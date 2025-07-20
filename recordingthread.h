#ifndef RECORDINGTHREAD_H
#define RECORDINGTHREAD_H

#include <QThread>
#include <QMutex>
#include <QQueue>
#include <QWaitCondition>
#include <QString>
#include <chrono>
#include <QTimer>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
}

/**
 * @brief 视频录制线程类
 * 
 * 实现视频监控系统的视频录制功能：
 * - 将v4l2采集的视频帧保存为MP4文件
 * - 在单独的线程中运行，不阻塞UI
 * - 使用线程安全的帧队列机制
 */
class RecordingThread : public QThread
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象指针
     */
    explicit RecordingThread(QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     *
     * 负责在线程对象销毁前停止录制（如果正在进行），等待线程安全退出，
     * 并清理所有分配的资源，包括帧队列中的数据、FFmpeg上下文以及定时器。
     */
    ~RecordingThread();
    
    /**
     * @brief 开始一个新的录制会话。
     * @param filePath 要保存的MP4视频文件的完整路径。
     * @param width 视频帧的宽度 (像素)。
     * @param height 视频帧的高度 (像素)。
     * @return 如果成功初始化FFmpeg编码器、打开输出文件并启动线程（如果尚未运行），则返回 true；
     *         如果已在录制或初始化失败，则返回 false。
     */
    bool startRecording(const QString &filePath, int width, int height);
    
    /**
     * @brief 请求停止当前的录制会话。
     * 
     * 此方法设置内部标志以指示线程应停止录制，并唤醒线程（如果它正在等待）。
     * 实际的文件关闭和资源清理在线程的 `run()` 方法中异步完成。
     * 如果当前未在录制，则此方法不执行任何操作。
     */
    void stopRecording();
    
    /**
     * @brief 将一帧原始图像数据添加到待编码队列。
     * @param frameData 指向包含原始图像数据（通常为RGB888格式）的缓冲区的指针。
     *                  函数内部会复制此数据，调用者之后可以释放原始数据。
     * @param size 图像数据的总字节大小。
     * @return 如果当前正在录制且帧数据有效，并且成功将帧（的副本）添加到队列，则返回 true；
     *         否则（例如未在录制、数据无效或队列操作失败）返回 false。
     */
    bool addFrameToQueue(const unsigned char *frameData, int size);
    
    /**
     * @brief 查询当前是否正在进行录制。
     * @return 如果线程当前正在录制视频，则返回 true；否则返回 false。
     *         此方法是线程安全的。
     */
    bool isRecording() const { return m_isRecording; }
    
    /**
     * @brief 获取当前正在录制或最后一次录制的视频文件的完整路径。
     * @return 包含文件路径的 QString。如果尚未开始过录制，可能返回空字符串。
     *         此方法是线程安全的。
     */
    QString getFilePath() const { return m_filePath; }
    
    /**
     * @brief 启用或禁用视频的自动分段录制功能。
     * @param enable 如果为 true，则启用自动分段；如果为 false，则禁用。
     *               默认情况下，自动分段是启用的。
     */
    void setAutoSegmentation(bool enable);
    
    /**
     * @brief 设置自动分段的时长（单位：分钟）。
     * @param minutes 分段时长，单位为分钟。必须大于0。
     *                默认时长为30分钟。
     */
    void setMaxRecordingMinutes(int minutes);

signals:
    /**
     * @brief 录制过程中发生错误时发出的信号。
     * @param errorMsg 描述错误的文本信息。
     *                 例如，FFmpeg初始化失败、文件写入错误等。
     */
    void recordError(const QString &errorMsg);
    
    /**
     * @brief 当启用了自动分段且当前录制时长达到预设的最大值时发出的信号。
     * @param filePath 当前已完成录制的视频分段的文件路径。
     *                 接收此信号的对象（如 MonitorPage）通常会调用 `stopRecording()` 
     *                 结束当前段，然后立即调用 `startRecording()` 开始新的分段。
     */
    void recordingTimeReached30Minutes(const QString &filePath);

protected:
    /**
     * @brief QThread 的核心虚函数，线程启动后会执行此方法中的代码。
     * 
     * 包含一个主循环，该循环负责：
     * - 从帧队列 `m_frameQueue` 中安全地取出待处理的帧数据。
     * - 如果队列为空且不在录制状态，则等待下一次 `startRecording()` 或线程退出信号。
     * - 如果队列为空但在录制状态，则等待新帧的到来。
     * - 调用 `processFrame()` 对取出的帧进行编码和写入文件。
     * - 在录制停止或线程退出时，调用 `cleanupRecorder()` 清理FFmpeg资源。
     */
    void run() override;

private:
    bool m_isRecording;       ///< 原子布尔标志，指示当前是否正在录制。由 `m_mutex` 保护。
    bool m_shouldExit;        ///< 原子布尔标志，指示线程是否应准备退出其 `run()` 循环。由 `m_mutex` 保护。
    QString m_filePath;       ///< 当前录制会话（或分段）的输出文件完整路径。由 `m_mutex` 保护。
    int m_frameCount;         ///< 当前录制会话（或分段）已成功编码并写入文件的帧数。
    int m_width;              ///< 输入视频帧的宽度 (像素)。在 `startRecording()` 时设置。
    int m_height;             ///< 输入视频帧的高度 (像素)。在 `startRecording()` 时设置。
    
    // 线程同步原语
    mutable QMutex m_mutex;   ///< 互斥锁，用于保护对 `m_isRecording`, `m_shouldExit`, `m_filePath` 等共享状态变量的访问。
                              ///< `mutable` 允许在const成员函数（如 `isRecording()`）中锁定它。
    QWaitCondition m_condition;///< 条件变量，与 `m_mutex` 或 `m_queueMutex` 配合使用，
                              ///< 用于在帧队列为空或等待录制开始/停止时，使线程高效地挂起和被唤醒。
    
    // 帧数据队列相关
    /**
     * @brief 内部结构体，用于封装一帧原始图像数据及其大小。
     * 
     * 当帧数据从主线程传递到录制线程的队列时，会创建此结构的实例。
     * 构造函数负责分配内存并复制传入的图像数据，确保数据的生命周期
     * 由 `FrameData` 对象管理。析构函数负责释放分配的内存。
     */
    struct FrameData {
        unsigned char *data; ///< 指向存储原始图像数据的缓冲区的指针。
        int size;            ///< `data` 缓冲区中图像数据的总字节大小。
        
        /**
         * @brief FrameData 的构造函数。
         * @param src 指向要复制的原始图像数据的源缓冲区。
         * @param s   源图像数据的字节大小。
         */
        FrameData(const unsigned char *src, int s) {
            data = new unsigned char[s]; // 分配新内存
            memcpy(data, src, s);        // 复制数据
            size = s;                    // 保存大小
        }
        /**
         * @brief FrameData 的析构函数。
         * 负责释放构造函数中为 `data` 分配的内存。
         */
        ~FrameData() {
            delete[] data; // 释放内存
        }
    };
    QQueue<FrameData*> m_frameQueue; ///< 存储指向 `FrameData` 对象的指针的队列。
                                     ///< 这是生产者（`addFrameToQueue()`）和消费者（`run()`）之间的主要数据通道。
    QMutex m_queueMutex;             ///< 专门用于保护对 `m_frameQueue` 访问的互斥锁。
    
    // FFmpeg 相关核心组件的指针
    AVFormatContext *m_formatContext; ///< FFmpeg 封装格式上下文。管理输出文件的格式（如MP4）和I/O操作。
    AVCodecContext *m_codecContext;   ///< FFmpeg 编码器上下文。管理视频编码器（如H.264）的参数和状态。
    SwsContext *m_swsContext;         ///< FFmpeg 图像转换上下文。用于将输入的像素格式（如RGB24）转换为编码器所需的格式（如YUV420P）。
    AVFrame *m_frame;                 ///< FFmpeg AVFrame 对象。用于存储一帧待编码的原始（转换后为YUV）视频数据。
    AVPacket *m_packet;               ///< FFmpeg AVPacket 对象。用于存储一帧编码后的压缩视频数据。
    
    // 帧率和录制时间统计相关 (用于调试或信息显示)
    std::chrono::steady_clock::time_point m_startTime;    ///< 当前录制会话（或分段）的开始精确时间点。
    std::chrono::steady_clock::time_point m_lastFrameTime;///< 上一帧被成功处理的时间点，用于计算瞬时总时间。
    int m_totalFrames;      ///< 在一次完整的录制调用（可能跨越多个分段）中处理的总帧数。
    double m_totalTime;     ///< 在一次完整的录制调用中，从第一帧到最后一帧处理所花费的总时间（秒）。
    
    // 视频自动分段功能相关
    QTimer *m_segmentTimer;        ///< QTimer 对象，当启用自动分段时，用于定时触发分段信号。
    bool m_autoSegmentation;       ///< 布尔标志，指示是否启用视频自动分段功能。
    int m_maxRecordingMinutes;     ///< 自动分段的最大时长（单位：分钟）。
    mutable QMutex m_formatContextMutex; ///< 保护对m_formatContext的并发写入，主要用于av_interleaved_write_frame。
    
    // 私有辅助方法
    /**
     * @brief 初始化FFmpeg相关的编码器、封装器和转换器组件。
     * @return 如果所有FFmpeg组件成功初始化并准备好录制，则返回 true；否则返回 false。
     *         在失败时，会通过 `recordError` 信号发送错误信息。
     */
    bool initRecorder();
    
    /**
     * @brief 清理并释放在 `initRecorder()` 中分配的所有FFmpeg资源。
     * 
     * 此方法在每次录制停止或线程退出时调用，以确保没有内存泄漏或资源悬挂。
     * 包括刷新编码器、写入文件尾、关闭文件、释放各种上下文、帧和包。
     */
    void cleanupRecorder();
    
    /**
     * @brief 处理（编码并写入）单帧视频数据。
     * @param frameData 指向包含原始RGB图像数据的 `FrameData` 对象的指针。
     *                  函数处理完后不会释放 `frameData`，调用者（`run()`）负责。
     * 
     * 此方法执行颜色空间转换 (RGB -> YUV)，设置帧时间戳，然后调用 `encodeFrame()`。
     */
    void processFrame(const FrameData *frameData); // 改为 FrameData* 以避免不必要的拷贝，但注意生命周期管理
    
    /**
     * @brief 将准备好的 AVFrame（包含YUV数据）发送给编码器，并处理输出的 AVPacket。
     * @param frame 指向待编码的 AVFrame 的指针。如果传入 `nullptr`，则表示通知编码器已无更多帧（刷新操作）。
     * @return 如果帧成功发送、所有产生的包成功接收并写入文件，则返回 true；
     *         如果在任何步骤发生错误，则返回 false，并通过 `recordError` 信号报告。
     */
    bool encodeFrame(AVFrame *frame);
};

#endif // RECORDINGTHREAD_H