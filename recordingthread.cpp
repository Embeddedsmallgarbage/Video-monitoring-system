#include "recordingthread.h"

#include <QDebug>
#include <QDir>
#include <QMutexLocker>

/**
 * @file recordingthread.cpp
 * @brief 视频录制线程类 (RecordingThread) 的实现文件。
 * 
 * 本文件负责实现一个独立的线程，用于从摄像头捕获的原始图像帧数据
 * 进行H.264视频编码，并将编码后的数据写入MP4文件。
 * 主要功能包括：
 * - 使用 FFmpeg 库进行视频编码和文件封装。
 * - 管理一个帧数据队列，主线程（如 MonitorPage）将捕获到的帧添加到此队列。
 * - 录制线程从队列中取出帧进行处理。
 * - 支持开始录制、停止录制操作。
 * - 支持视频的自动分段录制（例如每30分钟一段）。
 * - 在录制出错时通过信号通知主线程。
 * - 计算并输出录制视频的平均帧率。
 */

RecordingThread::RecordingThread(QObject *parent)
    : QThread(parent)
    , m_isRecording(false)
    , m_shouldExit(false)
    , m_frameCount(0)
    , m_width(0)
    , m_height(0)
    , m_formatContext(nullptr)
    , m_codecContext(nullptr)
    , m_swsContext(nullptr)
    , m_frame(nullptr)
    , m_packet(nullptr)
    , m_startTime(std::chrono::steady_clock::now())  // 初始化开始时间
    , m_lastFrameTime(std::chrono::steady_clock::now())  // 初始化上一帧时间
    , m_totalFrames(0)  // 总帧数
    , m_totalTime(0.0)  // 总时间（秒）
    , m_segmentTimer(nullptr)
    , m_autoSegmentation(true) // 默认启用自动分段
    , m_maxRecordingMinutes(30) // 默认30分钟自动分段
{
}

RecordingThread::~RecordingThread()
{
    // 确保停止录制并清理资源
    stopRecording();
    
    // 等待线程结束
    m_shouldExit = true;
    m_condition.wakeAll();
    wait();
    
    // 清空帧队列
    QMutexLocker locker(&m_queueMutex);
    while (!m_frameQueue.isEmpty()) {
        delete m_frameQueue.dequeue();
    }
    
    // 清理分段定时器
    if (m_segmentTimer) {
        m_segmentTimer->stop();
        delete m_segmentTimer;
        m_segmentTimer = nullptr;
    }
}

/**
 * @brief 开始一个新的视频录制会话。
 * @param filePath 要保存的MP4视频文件的完整路径。
 * @param width 视频帧的宽度 (像素)。
 * @param height 视频帧的高度 (像素)。
 * @return 如果成功初始化并开始录制，则返回 true；否则返回 false。
 * 
 * 此函数执行以下操作：
 * 1. 检查是否已在录制中，如果是则直接返回 false。
 * 2. 保存传入的文件路径、宽度和高度到成员变量。
 * 3. 重置帧计数器、总帧数、总时间，并记录当前时间为录制开始时间。
 * 4. 确保输出文件所在的目录存在，如果不存在则尝试创建它。
 * 5. 调用 `initRecorder()` 初始化FFmpeg编码器和相关上下文。
 * 6. 如果初始化失败，则返回 false。
 * 7. 设置 `m_isRecording` 标志为 true。
 * 8. 如果启用了自动分段 (`m_autoSegmentation`)，则初始化并启动分段定时器 `m_segmentTimer`。
 *    该定时器会在达到 `m_maxRecordingMinutes` 分钟后触发 `recordingTimeReached30Minutes` 信号。
 * 9. 如果线程尚未运行，则调用 `start()` 启动线程的 `run()` 方法；否则，唤醒已在运行的线程。
 */
bool RecordingThread::startRecording(const QString &filePath, int width, int height)
{
    QMutexLocker locker(&m_mutex);

    if (m_isRecording) {
        return false; // 已经在录制中
    }

    m_filePath = filePath;
    m_width = width;
    m_height = height;
    m_frameCount = 0;
    m_totalFrames = 0;  // 重置总帧数
    m_totalTime = 0.0;  // 重置总时间
    m_startTime = std::chrono::steady_clock::now();  // 记录开始时间
    m_lastFrameTime = m_startTime;  // 初始化上一帧时间
    m_shouldExit = false;

    // 确保输出目录存在
    QDir dir = QFileInfo(m_filePath).dir();
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 初始化录制器
    if (!initRecorder()) {
        return false;
    }

    m_isRecording = true;
    
    // 初始化并启动分段录制定时器
    if (m_autoSegmentation) {
        if (!m_segmentTimer) {
            m_segmentTimer = new QTimer(this);
            connect(m_segmentTimer, &QTimer::timeout, this, [this]() {
                // 发送录制时间达到30分钟的信号
                emit recordingTimeReached30Minutes(m_filePath);
            });
        }
        
        // 设置定时器为30分钟（1800000毫秒）
        m_segmentTimer->start(m_maxRecordingMinutes * 60 * 1000);
    }
    
    // 启动线程
    if (!isRunning()) {
        start();
    } else {
        // 唤醒线程
        m_condition.wakeAll();
    }
    
    return true;
}

/**
 * @brief 请求停止当前的视频录制会话。
 * 
 * 此函数执行以下操作：
 * 1. 检查是否未在录制，如果是则直接返回。
 * 2. 设置 `m_isRecording` 标志为 false。
 * 3. 如果分段定时器 `m_segmentTimer` 正在运行，则停止它。
 * 4. 唤醒录制线程 (`run()` 方法中的等待)，使其能够检测到 `m_isRecording` 变为 false，
 *    从而执行清理工作 (如 `cleanupRecorder()`) 并结束当前录制段。
 * 
 * 注意：此函数只是设置标志并唤醒线程，实际的编码和文件关闭操作在 `run()` 方法中异步完成。
 */
void RecordingThread::stopRecording()
{
    QMutexLocker locker(&m_mutex);

    if (!m_isRecording) {
        return; // 未在录制状态
    }

    m_isRecording = false;
    
    // 停止分段定时器
    if (m_segmentTimer && m_segmentTimer->isActive()) {
        m_segmentTimer->stop();
    }
    
    // 唤醒可能因等待新帧或等待开始录制而阻塞的 run() 循环
    // 这样 run() 可以检查到 m_isRecording 变为 false，并执行后续的清理和退出逻辑
    m_condition.wakeAll();
}

/**
 * @brief 将一帧原始图像数据添加到待处理队列中。
 * @param frameData 指向包含原始图像数据（例如RGB888格式）的缓冲区的指针。
 * @param size 图像数据的总字节大小。
 * @return 如果成功将帧数据添加到队列，则返回 true；否则返回 false。
 * 
 * 此函数由主线程（例如 `MonitorPage`）调用，用于将从摄像头捕获到的视频帧
 * 传递给录制线程进行编码。
 * 
 * 执行逻辑：
 * 1. 检查当前是否正在录制，以及传入的帧数据和大小是否有效。如果无效，则直接返回 false。
 * 2. 创建一个新的 `FrameData` 对象，将传入的 `frameData` 和 `size` 复制到其中。
 *    (`FrameData` 内部会分配内存并复制数据，以确保数据的生命周期由录制线程管理)。
 * 3. 将创建的 `FrameData` 对象添加到帧队列 `m_frameQueue` 的末尾。
 * 4. 唤醒录制线程 (`run()` 方法中的等待)，通知其有新的帧数据需要处理。
 */
bool RecordingThread::addFrameToQueue(const unsigned char *frameData, int size)
{
    if (!m_isRecording || !frameData || size <= 0) {
        return false;
    }

    // 创建帧数据副本并添加到队列
    QMutexLocker locker(&m_queueMutex);
    m_frameQueue.enqueue(new FrameData(frameData, size));
    
    // 唤醒线程处理新帧
    m_condition.wakeOne();
    
    return true;
}

/**
 * @brief 录制线程的主执行函数 (QThread::run() 的重写)。
 * 
 * 此函数是录制线程的入口点，在一个循环中运行，直到 `m_shouldExit` 标志被设置为 true。
 * 主要逻辑：
 * 1. 循环从帧队列 `m_frameQueue` 中获取待处理的帧数据 (`FrameData`)。
 * 2. 如果队列为空：
 *    - 检查 `m_isRecording` 状态。如果不再录制，则调用 `cleanupRecorder()` 清理FFmpeg资源，
 *      计算并输出平均帧率，然后线程进入等待状态，直到下一次 `startRecording()` 被调用或 `m_shouldExit` 为 true。
 *    - 如果仍在录制但队列为空，则线程进入等待状态，直到有新帧被添加到队列或 `m_isRecording` 变为 false。
 * 3. 如果成功从队列中获取到一帧数据：
 *    - 再次检查 `m_isRecording` 状态。如果仍在录制，则调用 `processFrame()` 处理该帧（进行颜色空间转换和编码）。
 *    - 删除已处理的 `FrameData` 对象以释放内存。
 * 4. 当 `m_shouldExit` 为 true 时，循环结束，在退出前调用 `cleanupRecorder()` 确保所有资源被释放。
 */
void RecordingThread::run()
{
    while (!m_shouldExit) {
        FrameData *frameData = nullptr;
        
        // 从队列获取帧
        {
            QMutexLocker locker(&m_queueMutex);
            if (m_frameQueue.isEmpty()) {
                // 如果队列为空，等待新帧或停止信号
                m_mutex.lock();
                bool isRecording = m_isRecording;
                m_mutex.unlock();
                
                if (!isRecording) {
                    // 如果不再录制，清理资源并退出循环
                    cleanupRecorder();
                    
                    // 计算并输出平均帧率
                    if (m_totalFrames > 0) {
                        double averageFPS = m_totalFrames / m_totalTime;
                        qDebug() << "平均帧率:" << averageFPS << "FPS";
                    } else {
                        qDebug() << "未录制任何帧";
                    }
                    
                    // 等待下一次录制开始
                    m_mutex.lock();
                    m_condition.wait(&m_mutex);
                    m_mutex.unlock();
                    continue;
                }
                
                // 等待新帧
                m_condition.wait(&m_queueMutex);
                
                // 再次检查队列
                if (m_frameQueue.isEmpty()) {
                    continue;
                }
            }
            
            frameData = m_frameQueue.dequeue();
        }
        
        // 处理帧
        if (frameData) {
            m_mutex.lock();
            bool isRecording = m_isRecording;
            m_mutex.unlock();
            
            if (isRecording) {
                processFrame(frameData);
            }
            
            delete frameData;
        }
    }
    
    // 线程退出前清理资源
    cleanupRecorder();
}

/**
 * @brief 初始化FFmpeg录制器相关的组件。
 * @return 如果所有组件成功初始化，则返回 true；否则返回 false。
 *
 * 此函数负责为一次新的录制会话（或一个新的分段）设置FFmpeg。
 * 主要步骤：
 * 1. 使用 `avformat_alloc_output_context2()` 根据输出文件路径和格式（MP4）分配封装格式上下文 (`m_formatContext`)。
 * 2. 使用 `avcodec_find_encoder()` 查找H.264编码器。
 * 3. 使用 `avcodec_alloc_context3()` 分配编码器上下文 (`m_codecContext`)。
 * 4. 设置编码器参数，如视频宽度、高度、时间基、帧率、像素格式 (YUV420P)、比特率等。
 *    - 为了提高性能，启用多线程编码 (4个线程，帧级并行)。
 *    - 设置编码预设为 "ultrafast" 和调优为 "zerolatency" 以追求更快的编码速度和低延迟。
 * 5. 使用 `avcodec_open2()` 打开编码器。
 * 6. 使用 `avformat_new_stream()` 在封装格式上下文中创建一个新的视频流。
 * 7. 使用 `avcodec_parameters_from_context()` 将编码器上下文的参数复制到视频流的参数中。
 * 8. 如果输出格式的标志包含 `AVFMT_GLOBALHEADER`，则请求编码器写入全局头信息。
 * 9. 使用 `avio_open()` 打开输出文件以供写入。
 * 10. 使用 `avformat_write_header()` 写入输出文件的头部信息。
 * 11. 分配 `AVFrame` (`m_frame`) 用于存储转换后的YUV420P图像数据，并为其分配图像缓冲区。
 * 12. 分配 `AVPacket` (`m_packet`) 用于存储编码后的H.264数据。
 * 13. 初始化SwsContext (`m_swsContext`) 用于将输入的RGB888帧数据转换为编码器所需的YUV420P格式。
 *
 * 如果任何步骤失败，会通过 `recordError` 信号发送错误信息，并返回 false。
 */
bool RecordingThread::initRecorder()
{
    // 初始化 FFmpeg 输出上下文
    avformat_alloc_output_context2(&m_formatContext, nullptr, nullptr, m_filePath.toStdString().c_str());
    if (!m_formatContext) {
        emit recordError("无法创建输出上下文");
        return false;
    }

    // 查找 H.264 编码器
    const AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec) {
        emit recordError("无法找到 H.264 编码器");
        avformat_free_context(m_formatContext);
        return false;
    }

    // 创建编码器上下文
    m_codecContext = avcodec_alloc_context3(codec);
    if (!m_codecContext) {
        emit recordError("无法创建编码器上下文");
        avformat_free_context(m_formatContext);
        return false;
    }

    // 设置编码参数
    m_codecContext->width = m_width;
    m_codecContext->height = m_height;
    m_codecContext->time_base = {1, 8};
    m_codecContext->framerate = {8, 1};
    m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
    m_codecContext->bit_rate = 800000; // 目标比特率 (800 kbps)，影响视频质量和文件大小
    
    // 启用多线程编码以提高性能
    if (codec->capabilities & AV_CODEC_CAP_FRAME_THREADS) {
        m_codecContext->thread_count = 4; // 尝试使用4个线程进行编码
        m_codecContext->thread_type = FF_THREAD_FRAME; // 帧级并行
        qDebug() << "RecordingThread: H.264编码器启用帧级多线程，线程数:" << m_codecContext->thread_count;
    } else if (codec->capabilities & AV_CODEC_CAP_SLICE_THREADS) {
        m_codecContext->thread_count = 4; // 尝试使用4个线程进行编码
        m_codecContext->thread_type = FF_THREAD_SLICE; // 片级并行
        qDebug() << "RecordingThread: H.264编码器启用片级多线程，线程数:" << m_codecContext->thread_count;
    } else {
        m_codecContext->thread_count = 1; // 编码器不支持多线程或仅支持单线程
        qDebug() << "RecordingThread: H.264编码器使用单线程编码。";
    }
    
    // 设置编码速度预设和调优参数 (H.264特有，通过AVDictionary传递)
    AVDictionary *codec_opts = nullptr;
    av_dict_set(&codec_opts, "preset", "ultrafast", 0); // "ultrafast" 提供最快的编码速度，但质量最低
                                                     // 其他可选值: "superfast", "veryfast", "faster", "fast", "medium" (默认), "slow", "slower", "veryslow", "placebo"
    av_dict_set(&codec_opts, "tune", "zerolatency", 0);  // "zerolatency" 针对低延迟场景优化，禁用B帧等
    // av_dict_set(&codec_opts, "crf", "23", 0); // 可选：恒定速率因子 (Constant Rate Factor)，值越小质量越高，范围0-51，默认23
    // 如果设置了bit_rate，CRF通常会被忽略或表现不同。这里以bit_rate为主。

    // 打开编码器
    int ret = avcodec_open2(m_codecContext, codec, &codec_opts);
    av_dict_free(&codec_opts); // 释放AVDictionary
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        QString errorMsg = QString("无法打开H.264编码器: %1").arg(errbuf);
        qWarning() << "RecordingThread::initRecorder: " << errorMsg;
        emit recordError(errorMsg);
        avcodec_free_context(&m_codecContext);
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
        return false;
    }

    // 创建视频流
    AVStream *stream = avformat_new_stream(m_formatContext, codec);
    if (!stream) {
        QString errorMsg = "无法创建新的视频流";
        qWarning() << "RecordingThread::initRecorder: " << errorMsg;
        emit recordError(errorMsg);
        avcodec_close(m_codecContext);
        avcodec_free_context(&m_codecContext);
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
        return false;
    }
    stream->id = m_formatContext->nb_streams - 1; // 设置流ID
    stream->time_base = m_codecContext->time_base; // 设置流的时间基与编码器一致

    // 将编码器上下文的参数复制到视频流的编解码器参数 (AVCodecParameters) 中
    ret = avcodec_parameters_from_context(stream->codecpar, m_codecContext);
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        QString errorMsg = QString("无法从编码器上下文复制参数到流: %1").arg(errbuf);
        qWarning() << "RecordingThread::initRecorder: " << errorMsg;
        emit recordError(errorMsg);
        // 此处 stream 是 m_formatContext 的一部分，avformat_free_context 会处理它
        avcodec_close(m_codecContext);
        avcodec_free_context(&m_codecContext);
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
        return false;
    }

    // 某些封装格式需要全局头信息 (例如 MP4 中的 SPS/PPS NAL单元)
    // 如果 AVFMT_GLOBALHEADER 标志被设置，则编码器需要能够提供这些信息。
    // FFmpeg的H.264编码器 (libx264) 通常会将SPS/PPS放在 extradata 中，avformat_write_header 会处理。
    if (m_formatContext->oformat->flags & AVFMT_GLOBALHEADER) {
        m_codecContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
    }

    // 打开输出文件以供写入
    if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
        ret = avio_open(&m_formatContext->pb, m_filePath.toStdString().c_str(), AVIO_FLAG_WRITE);
        if (ret < 0) {
            char errbuf[AV_ERROR_MAX_STRING_SIZE];
            av_strerror(ret, errbuf, sizeof(errbuf));
            QString errorMsg = QString("无法打开输出文件 '%1': %2").arg(m_filePath).arg(errbuf);
            qWarning() << "RecordingThread::initRecorder: " << errorMsg;
            emit recordError(errorMsg);
            avcodec_close(m_codecContext);
            avcodec_free_context(&m_codecContext);
            avformat_free_context(m_formatContext); // avformat_free_context 会处理 pb (如果已分配)
            m_formatContext = nullptr;
            return false;
        }
    }

    // 写入输出文件的头部信息 (例如 MP4的moov box等)
    ret = avformat_write_header(m_formatContext, nullptr); // 第二个参数是 AVDictionary** options
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        QString errorMsg = QString("写入文件头失败: %1 (文件: %2)").arg(errbuf).arg(m_filePath);
        qWarning() << "RecordingThread::initRecorder: " << errorMsg;
        emit recordError(errorMsg);
        if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) {
            avio_closep(&m_formatContext->pb); // 关闭已打开的文件IO上下文
        }
        avcodec_close(m_codecContext);
        avcodec_free_context(&m_codecContext);
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
        return false;
    }

    // 分配 AVFrame 用于存储待编码的YUV图像数据
    m_frame = av_frame_alloc();
    if (!m_frame) {
        QString errorMsg = "无法分配AVFrame";
        qWarning() << "RecordingThread::initRecorder: " << errorMsg;
        emit recordError(errorMsg);
        if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) avio_closep(&m_formatContext->pb);
        avcodec_close(m_codecContext);
        avcodec_free_context(&m_codecContext);
        avformat_free_context(m_formatContext); m_formatContext = nullptr;
        return false;
    }
    m_frame->format = m_codecContext->pix_fmt; // YUV420P
    m_frame->width = m_width;
    m_frame->height = m_height;
    // 为 m_frame 分配图像数据缓冲区
    ret = av_frame_get_buffer(m_frame, 0); // 第二个参数 align, 0表示默认对齐
    if (ret < 0) {
        char errbuf[AV_ERROR_MAX_STRING_SIZE];
        av_strerror(ret, errbuf, sizeof(errbuf));
        QString errorMsg = QString("无法为AVFrame分配缓冲区: %1").arg(errbuf);
        qWarning() << "RecordingThread::initRecorder: " << errorMsg;
        emit recordError(errorMsg);
        av_frame_free(&m_frame); // m_frame 已分配，需要释放
        if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) avio_closep(&m_formatContext->pb);
        avcodec_close(m_codecContext);
        avcodec_free_context(&m_codecContext);
        avformat_free_context(m_formatContext); m_formatContext = nullptr;
        return false;
    }

    // 分配 AVPacket 用于存储编码后的H.264数据
    m_packet = av_packet_alloc();
    if (!m_packet) {
        QString errorMsg = "无法分配AVPacket";
        qWarning() << "RecordingThread::initRecorder: " << errorMsg;
        emit recordError(errorMsg);
        av_frame_free(&m_frame); // 清理 m_frame
        if (!(m_formatContext->oformat->flags & AVFMT_NOFILE)) avio_closep(&m_formatContext->pb);
        avcodec_close(m_codecContext);
        avcodec_free_context(&m_codecContext);
        avformat_free_context(m_formatContext); m_formatContext = nullptr;
        return false;
    }

    // 初始化 SwsContext 用于将输入的RGB帧转换为编码器所需的YUV420P格式
    m_swsContext = sws_getContext(m_width, m_height, AV_PIX_FMT_RGB24,     // 输入: 宽度, 高度, 像素格式 (RGB888)
                                  m_width, m_height, AV_PIX_FMT_YUV420P, // 输出: 宽度, 高度, 像素格式 (YUV420P)
    // 初始化 swscale 上下文，用于 RGB 到 YUV 的转换，使用更快的算法
    m_swsContext = sws_getContext(m_width, m_height, AV_PIX_FMT_RGB24,
                                  m_width, m_height, AV_PIX_FMT_YUV420P,
                                  SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (!m_swsContext) {
        emit recordError("无法创建 swscale 上下文");
        av_packet_free(&m_packet);
        av_frame_free(&m_frame);
        avio_closep(&m_formatContext->pb);
        avformat_free_context(m_formatContext);
        return false;
    }

    return true;
}

void RecordingThread::cleanupRecorder()
{
    // 刷新编码器以处理剩余帧
    if (m_codecContext && m_formatContext) {
        encodeFrame(nullptr);

        // 写入 MP4 文件尾
        av_write_trailer(m_formatContext);

        // 关闭输出文件
        avio_closep(&m_formatContext->pb);
    }

    // 释放资源
    if (m_swsContext) {
        sws_freeContext(m_swsContext);
        m_swsContext = nullptr;
    }
    
    if (m_packet) {
        av_packet_free(&m_packet);
        m_packet = nullptr;
    }
    
    if (m_frame) {
        av_frame_free(&m_frame);
        m_frame = nullptr;
    }
    
    if (m_codecContext) {
        avcodec_free_context(&m_codecContext);
        m_codecContext = nullptr;
    }
    
    if (m_formatContext) {
        avformat_free_context(m_formatContext);
        m_formatContext = nullptr;
    }
}

bool RecordingThread::processFrame(FrameData *frameData)
{
    if (!m_isRecording || !frameData || !frameData->data || frameData->size <= 0) {
        return false;
    }

    // 记录当前时间
    auto now = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - m_lastFrameTime).count() / 1000000.0;  // 转换为秒
    m_lastFrameTime = now;

    // 更新总时间和帧数
    m_totalTime += elapsed;
    m_totalFrames++;

    // 计算瞬时帧率，但不每帧都输出日志（仅每30帧输出一次）
    double currentFPS = 1.0 / elapsed;
    if (m_totalFrames % 30 == 0) {
        qDebug() << "当前帧率:" << currentFPS << "FPS";
    }

    // 将 RGB888 转换为 YUV420P
    const uint8_t *srcSlice[1] = {frameData->data};
    int srcStride[1] = {3 * m_width};
    sws_scale(m_swsContext, srcSlice, srcStride, 0, m_height, m_frame->data, m_frame->linesize);

    // 设置帧的 pts（呈现时间戳）
    m_frame->pts = m_frameCount++;

    // 编码并写入帧
    if (!encodeFrame(m_frame)) {
        return false;
    }

    return true;
}

bool RecordingThread::encodeFrame(AVFrame *frame)
{
    // 发送帧到编码器
    int ret = avcodec_send_frame(m_codecContext, frame);
    if (ret < 0) {
        emit recordError("发送帧失败");
        return false;
    }

    // 接收编码后的数据包
    while (ret >= 0) {
        ret = avcodec_receive_packet(m_codecContext, m_packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break; // 需要更多帧或编码结束
        } else if (ret < 0) {
            emit recordError("接收数据包失败");
            return false;
        }

        // 调整时间戳并写入数据包
        av_packet_rescale_ts(m_packet, m_codecContext->time_base, m_formatContext->streams[0]->time_base);
        m_packet->stream_index = 0;
        if (av_interleaved_write_frame(m_formatContext, m_packet) < 0) {
            emit recordError("写入数据包失败");
            return false;
        }
        av_packet_unref(m_packet);
    }

    return true;
}