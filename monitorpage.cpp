/**
 * @file monitorpage.cpp
 * @brief 监控页面类 (MonitorPage) 的实现文件。
 * 
 * 本文件负责实现视频监控系统的实时监控功能页面。
 * 主要功能包括：
 * - 通过 V4L2 接口从摄像头设备 (`/dev/video0`) 采集视频帧。
 * - 在界面上实时显示摄像头画面，并计算和显示帧率 (FPS)。
 * - 提供开始/停止视频录制的功能，录制文件以H.264编码的MP4格式保存。
 * - 录制文件按日期 (yyyyMMdd) 和时间 (HHmmss) 自动分文件夹和文件命名。
 * - 录制过程中，实时更新录制时长显示。
 * - 实现录制文件自动分段功能（例如每30分钟一段）。
 * - 集成存储管理 (`StorageManager`)，在开始录制前检查存储空间，空间不足时尝试清理旧文件。
 * - 提供返回首页的按钮。
 * - 处理存储空间不足和清理完成的事件。
 */

#include "monitorpage.h"
#include "mainwindow.h"
#include "v4l2_wrapper.h"     // V4L2摄像头操作的C封装层
#include "recordingthread.h"  // 视频录制线程类
#include "storagemanager.h"   // 存储管理类

#include <QVBoxLayout>        // 垂直布局
#include <QHBoxLayout>        // 水平布局
#include <QStackedLayout>     // 堆叠布局，用于在视频上覆盖控件
#include <QMessageBox>        // 消息框，用于显示警告或信息
#include <QImage>             // QImage类，用于处理图像数据
#include <QDir>               // QDir类，用于目录操作
#include <QDateTime>          // QDateTime类，用于处理日期和时间
#include <QPixmap>            // QPixmap类，用于在标签上显示图像 (新增包含)
#include <QDebug>             // QDebug类，用于调试输出 (新增包含)

/**
 * @brief 监控页面类 (MonitorPage) 的构造函数。
 * @param parent 父窗口指针，通常是 MainWindow 实例。
 * 
 * 初始化成员变量，调用 `setupUI()` 构建界面，初始化存储管理器，
 * 并分配用于V4L2视频帧的缓冲区。
 */
MonitorPage::MonitorPage(MainWindow *parent)
    : QWidget(parent)                                 // 调用父类QWidget构造函数
    , m_mainWindow(parent)                            // 初始化主窗口指针
    , m_imageLabel(nullptr)                           // 初始化图像显示标签为空
    , m_backButton(nullptr)                           // 初始化返回按钮为空
    , m_recordButton(nullptr)                         // 初始化录制按钮为空
    , m_recordStatusLabel(nullptr)                    // 初始化录制状态标签为空
    , m_recordTimeLabel(nullptr)                      // 初始化录制时间标签为空
    , m_frameTimer(nullptr)                           // 初始化帧更新定时器为空
    , m_recordTimer(nullptr)                          // 初始化录制状态更新定时器为空
    , m_frameBuffer(nullptr)                          // 初始化V4L2帧缓冲区为空
    , m_frameWidth(0)                                 // 初始化帧宽度为0
    , m_frameHeight(0)                                // 初始化帧高度为0
    , m_videoRecorder(nullptr)                        // 初始化视频录制线程对象为空
    , m_isRecording(false)                            // 初始化录制状态为false (未在录制)
    , m_recordingSeconds(0)                           // 初始化已录制秒数为0
    , m_recordingPath("/mnt/TFcard")                  // 初始化默认录制路径为TF卡
    , m_recordingStartTime(QDateTime::currentDateTime()) // 初始化录制开始时间为当前时间
    , m_currentVideoFile("")                           // 初始化当前录制文件名为空
    , m_fpsLabel(nullptr)                             // 初始化FPS显示标签为空
    , m_lastFrameTime(std::chrono::steady_clock::now()) // 初始化上一帧的时间点为当前时间
    , m_currentFPS(0.0)                               // 初始化当前FPS为0.0
    , m_storageManager(nullptr)                       // 初始化存储管理器为空
{
    setupUI(); // 调用函数初始化用户界面
    
    // 初始化存储管理器 (StorageManager)
    m_storageManager = new StorageManager(m_recordingPath, this); // 创建StorageManager实例，指定存储路径和父对象
    m_storageManager->setMinFreeSpacePercent(10);                 // 设置最小可用磁盘空间百分比阈值为10%
    
    // 连接存储管理器发出的信号到本类的槽函数
    // 当存储空间不足时，会调用 onLowStorageSpace 方法
    connect(m_storageManager, &StorageManager::lowStorageSpace, this, &MonitorPage::onLowStorageSpace);
    // 当存储空间清理完成后，会调用 onCleanupCompleted 方法
    connect(m_storageManager, &StorageManager::cleanupCompleted, this, &MonitorPage::onCleanupCompleted);
    
    // 启动存储空间的自动检查功能，每600000毫秒（10分钟）检查一次
    m_storageManager->startAutoCheck(600000);
    
    // 为从V4L2摄像头捕获的原始帧数据分配缓冲区
    // 假设最大分辨率为1280x720，RGB888格式 (每个像素3字节)
    // 注意：v4l2_wrapper 中实际使用的是RGB565，然后转换为RGB888，所以这里缓冲区大小是针对RGB888的。
    m_frameBuffer = new unsigned char[1280 * 720 * 3]; // 分配内存
}

/**
 * @brief 初始化监控页面的用户界面 (UI)。
 * 
 * 该函数负责创建、配置和布局监控页面上的所有视觉元素，
 * 包括视频显示区域、返回按钮、录制控制按钮、状态标签等，并连接相关信号与槽。
 */
void MonitorPage::setupUI()
{
    // 创建主垂直布局，并设置边距和间距为0，使其填满整个页面
    QVBoxLayout *monitorLayout = new QVBoxLayout(this);
    monitorLayout->setContentsMargins(0, 0, 0, 0); // 设置外边距为0
    monitorLayout->setSpacing(0);                   // 设置内部控件间距为0
    
    // 创建用于显示摄像头画面的 QLabel 控件
    m_imageLabel = new QLabel();
    m_imageLabel->setObjectName("m_imageLabel");         // 设置对象名，用于QSS样式
    m_imageLabel->setAlignment(Qt::AlignCenter);          // 设置图像居中显示
    // 设置尺寸策略为Expanding，使其能随窗口大小变化而填充可用空间
    m_imageLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    
    // 创建返回首页的 QPushButton 控件
    m_backButton = new QPushButton();
    m_backButton->setIcon(QIcon(":/images/back.png")); // 设置按钮图标
    m_backButton->setIconSize(QSize(32, 32));          // 设置图标大小
    m_backButton->setToolTip("返回首页");               // 设置鼠标悬停提示
    m_backButton->setObjectName("m_backButton");       // 设置对象名
    m_backButton->setFlat(true);                       // 设置为扁平样式，无边框
    
    // 创建开始/停止录制的 QPushButton 控件
    m_recordButton = new QPushButton();
    m_recordButton->setIcon(QIcon(":/images/playback.png")); // 初始图标为"开始录制"
    m_recordButton->setIconSize(QSize(32, 32));             // 设置图标大小
    m_recordButton->setToolTip("开始录制");                   // 初始提示为"开始录制"
    m_recordButton->setObjectName("m_recordButton");          // 设置对象名
    m_recordButton->setFlat(true);                          // 设置为扁平样式
    
    // 创建显示录制状态的 QLabel 控件 (例如 "未录制", "正在录制视频...")
    m_recordStatusLabel = new QLabel("未录制");
    m_recordStatusLabel->setObjectName("m_recordStatusLabel"); // 设置对象名
    
    // 创建显示录制时长的 QLabel 控件 (例如 "00:00:00")
    m_recordTimeLabel = new QLabel("00:00:00");
    m_recordTimeLabel->setObjectName("m_recordTimeLabel");     // 设置对象名
    m_recordTimeLabel->setVisible(false);                      // 初始状态下隐藏
    
    // 创建显示帧率 (FPS) 的 QLabel 控件
    m_fpsLabel = new QLabel("FPS: 0.0");
    m_fpsLabel->setObjectName("m_fpsLabel");                   // 设置对象名
    
    // 创建一个QWidget作为覆盖层 (overlay)，用于在视频画面上显示控制按钮和信息
    QWidget *overlayWidget = new QWidget();
    overlayWidget->setObjectName("overlayWidget"); // 设置对象名，方便用QSS设置透明背景等
    QHBoxLayout *overlayLayout = new QHBoxLayout(overlayWidget); // 为覆盖层设置水平布局
    overlayLayout->setContentsMargins(10, 10, 10, 10);        // 设置覆盖层内边距
    
    // 在覆盖层左上角放置返回按钮
    overlayLayout->addWidget(m_backButton, 0, Qt::AlignLeft | Qt::AlignTop);
    
    // 创建一个垂直布局，用于在覆盖层右上角组织录制相关的控件
    QVBoxLayout *rightLayout = new QVBoxLayout();
    rightLayout->addWidget(m_recordButton, 0, Qt::AlignRight | Qt::AlignTop);       // 录制按钮，右上对齐
    rightLayout->addWidget(m_recordStatusLabel, 0, Qt::AlignRight);                 // 录制状态标签，右对齐
    rightLayout->addWidget(m_recordTimeLabel, 0, Qt::AlignRight);                   // 录制时间标签，右对齐
    rightLayout->addStretch(); // 添加一个弹性空间，将以上控件推向顶部
    
    // 创建一个垂直布局，用于在覆盖层左下角组织FPS显示控件
    QVBoxLayout *leftLayout = new QVBoxLayout();
    leftLayout->addStretch(); // 添加一个弹性空间，将FPS标签推向底部
    leftLayout->addWidget(m_fpsLabel, 0, Qt::AlignLeft | Qt::AlignBottom);          // FPS标签，左下对齐
    
    // 将左侧布局（含FPS）和右侧布局（含录制控件）添加到覆盖层的水平布局中
    overlayLayout->addLayout(leftLayout); // 左侧FPS
    overlayLayout->addStretch();          // 中间弹性空间，将左右两侧控件推开
    overlayLayout->addLayout(rightLayout); // 右侧录制控件
    
    // 创建 QStackedLayout，用于将视频显示标签 (m_imageLabel) 和覆盖层 (overlayWidget) 堆叠在一起
    // StackAll 模式意味着所有子控件都会显示，覆盖层会在视频层之上
    QStackedLayout *stackedLayout = new QStackedLayout();
    stackedLayout->setStackingMode(QStackedLayout::StackAll);
    stackedLayout->addWidget(m_imageLabel);     // 添加视频显示层到底部
    stackedLayout->addWidget(overlayWidget);  // 添加覆盖控制层到顶部
    
    // 将堆叠布局添加到主垂直布局中
    monitorLayout->addLayout(stackedLayout);
    
    // 连接各个按钮的点击信号到相应的槽函数
    connect(m_backButton, &QPushButton::clicked, m_mainWindow, &MainWindow::showHomePage); // 返回按钮 -> 显示首页
    connect(m_recordButton, &QPushButton::clicked, this, &MonitorPage::toggleRecording);   // _recordButton -> 切换录制状态
    
    // 创建帧更新定时器 (m_frameTimer)，用于定期从摄像头获取并显示新的一帧图像
    m_frameTimer = new QTimer(this);
    connect(m_frameTimer, &QTimer::timeout, this, &MonitorPage::updateFrame); // 定时器超时 -> 更新帧
    
    // 创建录制时间更新定时器 (m_recordTimer)，用于在录制时每秒更新录制时长显示
    m_recordTimer = new QTimer(this);
    m_recordTimer->setInterval(1000); // 设置时间间隔为1000毫秒 (1秒)
    connect(m_recordTimer, &QTimer::timeout, this, &MonitorPage::updateRecordingStatus); // 定时器超时 -> 更新录制状态（时间）
    
    // 初始化媒体录制相关的组件 (例如 RecordingThread)
    initMediaRecorder();
}

/**
 * @brief 开始视频采集流程。
 * @return 如果成功初始化摄像头、开始采集并启动帧更新定时器，则返回 true；否则返回 false。
 * 
 * 此函数负责：
 * 1. 调用 `v4l2_init()` 初始化摄像头设备 ("/dev/video0")。
 * 2. 调用 `v4l2_start_capture()` 开始 V4L2 视频流的捕获。
 * 3. 如果上述步骤成功，则启动 `m_frameTimer` 定时器，以大约 30 FPS (33ms间隔) 的频率更新画面。
 * 4. 如果任何步骤失败，会进行必要的清理 (如 `v4l2_cleanup()`) 并返回 false。
 */
bool MonitorPage::startCapture()
{
    // 初始化V4L2摄像头设备，指定设备节点为 "/dev/video0"
    if (v4l2_init("/dev/video0") < 0) { // v4l2_init 失败会返回负值
        qWarning() << "摄像头初始化失败 (v4l2_init)";
        return false; // 初始化失败，返回false
    }
    
    // 开始V4L2视频捕获流
    if (v4l2_start_capture() < 0) { // v4l2_start_capture 失败会返回负值
        qWarning() << "启动摄像头捕获失败 (v4l2_start_capture)";
        v4l2_cleanup(); // 清理已初始化的V4L2资源
        return false;   // 启动捕获失败，返回false
    }
    
    // 启动帧更新定时器，设置间隔为33毫秒（大约对应30帧/秒）
    m_frameTimer->start(33);
    qDebug() << "摄像头捕获已启动，帧更新定时器已启动 (33ms)";
    return true; // 视频采集成功启动
}

/**
 * @brief 停止视频采集流程。
 * 
 * 此函数负责：
 * 1. 如果当前正在录制视频 (`m_isRecording` 为 true)，则调用 `stopRecording()` 先停止录制。
 * 2. 停止帧更新定时器 `m_frameTimer`。
 * 3. 调用 `v4l2_stop_capture()` 停止 V4L2 视频流的捕获。
 * 4. 调用 `v4l2_cleanup()` 清理并释放 V4L2 相关的资源。
 */
void MonitorPage::stopCapture()
{
    // 检查当前是否正在录制视频
    if (m_isRecording) {
        qDebug() << "停止捕获时检测到正在录制，将先停止录制。";
        stopRecording(); // 如果是，则先调用停止录制的方法
    }
    
    // 停止帧更新定时器
    if (m_frameTimer && m_frameTimer->isActive()) { // 确保定时器存在且正在运行
        m_frameTimer->stop();
        qDebug() << "帧更新定时器已停止。";
    }
    
    // 停止V4L2视频捕获并清理相关资源
    v4l2_stop_capture(); // 停止捕获流
    v4l2_cleanup();      // 释放V4L2资源（关闭设备，解除映射等）
    qDebug() << "摄像头捕获已停止并清理资源。";
}

/**
 * @brief 更新监控页面的视频帧。
 * 
 * 此槽函数由 `m_frameTimer` 定时器周期性调用。
 * 它执行以下操作：
 * 1. 检查帧缓冲区 `m_frameBuffer` 是否有效。
 * 2. 调用 `v4l2_get_frame()` 从摄像头获取一帧原始图像数据到 `m_frameBuffer`，
 *    并获取实际的帧宽度 `m_frameWidth` 和高度 `m_frameHeight`。
 *    (v4l2_wrapper内部会将RGB565转换为RGB888)
 * 3. 计算瞬时帧率 (FPS) 并进行平滑处理后更新到 `m_fpsLabel`。
 * 4. 使用获取到的帧数据创建 QImage 对象 (格式为 RGB888)。
 * 5. 将 QImage 转换为 QPixmap，并按比例缩放以适应 `m_imageLabel` 的大小，然后显示。
 * 6. 如果当前正在录制视频 (`m_isRecording` 为 true) 且录制器 (`m_videoRecorder`) 有效，
 *    则将当前帧数据添加到录制线程的队列中进行处理。
 */
void MonitorPage::updateFrame()
{
    // 检查帧缓冲区是否已分配
    if (!m_frameBuffer) {
        qWarning() << "updateFrame: 帧缓冲区 (m_frameBuffer) 为空，无法获取帧。";
        return;
    }
    
    // 调用V4L2封装函数获取一帧图像数据
    // 数据存入 m_frameBuffer，宽度和高度分别存入 m_frameWidth 和 m_frameHeight
    // v4l2_get_frame 成功返回0，失败返回-1
    if (v4l2_get_frame(m_frameBuffer, &m_frameWidth, &m_frameHeight) == 0) {
        // -- 计算并更新帧率 (FPS) --
        auto now = std::chrono::steady_clock::now(); // 获取当前时间点
        // 计算距离上一帧的时间差（单位：秒）
        double elapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - m_lastFrameTime).count() / 1000000.0;
        m_lastFrameTime = now; // 更新上一帧的时间点为当前时间点
        
        // 计算瞬时FPS，并使用指数移动平均法进行平滑处理，以减少数值剧烈波动
        if (elapsed > 0) { // 防止除以零
            // 平滑公式: new_fps = alpha * current_instant_fps + (1 - alpha) * old_fps
            // 这里 alpha = 0.2
            m_currentFPS = 0.8 * m_currentFPS + 0.2 * (1.0 / elapsed);
        }
        
        // 更新界面上的FPS显示标签，保留一位小数
        m_fpsLabel->setText(QString("FPS: %1").arg(m_currentFPS, 0, 'f', 1));
        
        // -- 将获取到的帧数据显示到UI上 --
        // 使用帧数据、宽度、高度和行字节数创建QImage对象
        // v4l2_wrapper 保证输出的是RGB888格式，每个像素3字节，所以行字节数为 m_frameWidth * 3
        QImage image(m_frameBuffer, m_frameWidth, m_frameHeight, 
                    m_frameWidth * 3, QImage::Format_RGB888);
        
        // 将QImage转换为QPixmap，然后进行缩放以适应m_imageLabel的大小，并保持宽高比
        // Qt::FastTransformation 提供较快的缩放，但可能牺牲一些图像质量
        QPixmap pixmap = QPixmap::fromImage(image);
        m_imageLabel->setPixmap(pixmap.scaled(m_imageLabel->size(), 
                                            Qt::KeepAspectRatio, 
                                            Qt::FastTransformation));
        
        // -- 如果正在录制，则将当前帧添加到录制队列 --
        if (m_isRecording && m_videoRecorder) {
            // 将帧数据 (m_frameBuffer) 和其大小 (宽度*高度*3字节/像素) 添加到录制线程的队列
            m_videoRecorder->addFrameToQueue(m_frameBuffer, m_frameWidth * m_frameHeight * 3);
        }
    } else {
        // 获取帧失败，可以添加一些错误处理或日志记录
        // qDebug() << "v4l2_get_frame 获取帧失败";
    }
}

/**
 * @brief 初始化媒体录制相关的组件。
 *
 * 此函数主要负责：
 * 1. 创建视频录制线程 `RecordingThread` 的实例 (`m_videoRecorder`)。
 * 2. 连接 `m_videoRecorder` 发出的 `recordError` 信号到本类的一个Lambda表达式槽函数，
 *    该槽函数会在录制出错时显示错误消息并停止录制。
 * 3. 连接 `m_videoRecorder` 发出的 `recordingTimeReached30Minutes` 信号到本类的
 *    `onRecordingTimeReached30Minutes` 槽函数，用于处理视频自动分段逻辑。
 */
void MonitorPage::initMediaRecorder()
{
    // 创建视频录制线程 RecordingThread 的实例，this作为其父对象
    m_videoRecorder = new RecordingThread(this);
    
    // 连接录制线程的 recordError 信号到当前对象的槽函数 (Lambda表达式)
    // 当录制线程发出 recordError 信号时，会执行Lambda中的代码
    connect(m_videoRecorder, &RecordingThread::recordError, this, [this](const QString &errorString) {
        // 弹出警告对话框显示错误信息
        QMessageBox::warning(this, "视频录制错误", "视频录制过程中发生错误: " + errorString);
        qWarning() << "视频录制错误:" << errorString;
        // 调用 stopRecording 停止当前的录制过程
        stopRecording();
    });
    
    // 连接录制线程的 recordingTimeReached30Minutes 信号到 onRecordingTimeReached30Minutes 槽函数
    // 当录制时长达到预设值（如30分钟），录制线程会发出此信号，触发自动分段逻辑
    connect(m_videoRecorder, &RecordingThread::recordingTimeReached30Minutes, 
            this, &MonitorPage::onRecordingTimeReached30Minutes);
    qDebug() << "媒体录制组件 (RecordingThread) 初始化完成。";
}

/**
 * @brief 开始录制视频。
 * 
 * 此函数执行以下操作：
 * 1. 检查是否已在录制中，如果是则直接返回。
 * 2. 调用存储管理器 `m_storageManager` 检查TF卡存储空间是否足够。
 *    - 如果空间不足，尝试调用 `m_storageManager->cleanupOldestDay()` 清理最早一天的视频文件。
 *    - 如果清理后空间仍然不足，则显示警告信息并返回，不开始录制。
 * 3. 记录录制开始时间 `m_recordingStartTime`。
 * 4. 根据当前日期和时间生成录制文件的目录名 (yyyyMMdd) 和初始文件名 (record_HHmmss.mp4)。
 *    确保日期目录存在，如果不存在则创建。
 * 5. 构造完整的视频文件保存路径 `m_currentVideoFile`。
 * 6. 调用 `m_videoRecorder->startRecording()` 启动录制线程开始录制视频，传入文件路径、帧宽度和高度。
 * 7. 如果录制成功启动：
 *    - 重置录制秒数 `m_recordingSeconds`，更新录制时间标签为 "00:00:00" 并使其可见。
 *    - 更新录制按钮的图标为停止图标，提示文本为 "停止录制"。
 *    - 更新录制状态标签为 "正在录制视频..."，并设置其QSS类为 "recording" (用于改变样式)。
 *    - 设置 `m_isRecording` 标志为 true。
 *    - 启动录制时间更新定时器 `m_recordTimer`。
 * 8. 如果录制启动失败，则显示警告信息。
 */
void MonitorPage::startRecording()
{
    // 如果当前已经在录制中，则直接返回，避免重复启动
    if (m_isRecording) {
        qDebug() << "尝试开始录制，但已处于录制状态。";
        return; 
    }
    
    qDebug() << "请求开始录制视频...";
    // 检查TF卡存储空间是否足够
    if (!m_storageManager->checkStorageSpace()) { // checkStorageSpace 返回false表示空间不足
        qDebug() << "存储空间不足，尝试清理旧文件...";
        // 空间不足，尝试清理最早一天的视频文件
        bool cleaned = m_storageManager->cleanupOldestDay();
        
        // 再次检查存储空间
        if (!cleaned || !m_storageManager->checkStorageSpace()) {
            // 如果清理失败，或者清理后空间仍然不足
            qWarning() << "清理后存储空间仍然不足，无法开始录制。";
            QMessageBox::warning(this, "存储空间不足", 
                "TF卡存储空间不足，无法开始录制。\n已尝试清理最早的视频文件，但空间仍然不足。");
            return; // 不开始录制
        }
        qDebug() << "旧文件清理完成，存储空间已足够。";
    }
    
    // 保存录制开始的精确时间点
    m_recordingStartTime = QDateTime::currentDateTime();
    
    // 根据当前日期和时间生成目录名和初始文件名
    QString dateDirName = m_recordingStartTime.toString("yyyyMMdd"); // 日期目录，格式：年年月月日日
    QString timeName = m_recordingStartTime.toString("HHmmss");      // 初始时间名，格式：时时分分秒秒
    
    // 确保根录制路径存在
    QDir recordRootDir(m_recordingPath);
    if (!recordRootDir.exists()) {
        qInfo() << "根录制目录 " << m_recordingPath << " 不存在，尝试创建。";
        recordRootDir.mkpath("."); // mkpath会创建所有必需的父目录
    }
    
    // 确保日期子目录存在，如果不存在则创建它
    QDir dateDir(m_recordingPath + "/" + dateDirName);
    if (!dateDir.exists()) {
        qInfo() << "日期子目录 " << dateDirName << " 不存在，尝试创建。";
        recordRootDir.mkdir(dateDirName); // 在根录制目录下创建日期子目录
    }
    
    // 设置当前视频录制文件的完整路径和初始文件名
    // 初始文件名格式: /mnt/TFcard/yyyyMMdd/record_HHmmss.mp4
    // 最终文件名会在停止录制时根据开始和结束时间重命名为 HH:mm-HH:mm.mp4
    m_currentVideoFile = m_recordingPath + "/" + dateDirName + "/" + "record_" + timeName + ".mp4";
    qDebug() << "视频将保存至 (初始):" << m_currentVideoFile;
    
    // 调用视频录制线程的 startRecording 方法，传入文件路径、当前帧宽度和高度
    // 注意：m_frameWidth 和 m_frameHeight 由 updateFrame 中的 v4l2_get_frame 更新
    // 如果此时摄像头还未捕获到第一帧，它们可能是0，这可能导致录制问题。
    // 确保在调用此函数前，m_frameWidth和m_frameHeight已经被有效设置。
    if (m_frameWidth == 0 || m_frameHeight == 0) {
        qWarning() << "帧宽度或高度为0，可能导致录制失败。请确保摄像头已捕获到有效帧。";
        // 可以选择在这里等待一小段时间或几帧，或者在startCapture成功后才允许录制
    }
    bool videoStarted = m_videoRecorder->startRecording(m_currentVideoFile, m_frameWidth, m_frameHeight);
    
    // 根据视频录制是否成功启动，更新UI和内部状态
    if (videoStarted) {
        qInfo() << "视频录制已成功启动。";
        // 重置录制时间计数器
        m_recordingSeconds = 0;
        m_recordTimeLabel->setText("00:00:00"); // 将录制时间标签重置为0
        m_recordTimeLabel->setVisible(true);    // 使录制时间标签可见
        
        // 更新录制按钮的UI状态：图标变为停止，提示文本变为"停止录制"
        m_recordButton->setIcon(QIcon(":/images/stop.png"));
        m_recordButton->setToolTip("停止录制");
        // 更新录制状态标签的文本和样式
        m_recordStatusLabel->setText("正在录制视频...");
        m_recordStatusLabel->setProperty("class", "recording"); // 设置QSS class属性，用于应用特定样式
        style()->unpolish(m_recordStatusLabel); // 确保样式表更新
        style()->polish(m_recordStatusLabel);
        
        m_isRecording = true; // 更新内部录制状态标志
        
        // 启动录制时间更新定时器 (m_recordTimer)，每秒触发一次 updateRecordingStatus
        m_recordTimer->start();
    } else {
        qWarning() << "无法启动视频录制。";
        QMessageBox::warning(this, "录制错误", "无法开始录制视频。请检查日志获取更多信息。");
    }
}

/**
 * @brief 停止录制视频。
 * 
 * 此函数执行以下操作：
 * 1. 检查是否未在录制中，如果是则直接返回。
 * 2. 调用 `m_videoRecorder->stopRecording()` 停止录制线程的工作。
 * 3. 停止录制时间更新定时器 `m_recordTimer`。
 * 4. 更新UI元素状态：
 *    - 录制按钮图标恢复为播放图标，提示文本恢复为 "开始录制"。
 *    - 录制状态标签恢复为 "未录制"，并移除 "recording" QSS类。
 *    - 隐藏录制时间标签。
 * 5. 设置 `m_isRecording` 标志为 false。
 * 6. 获取录制结束时间，并根据录制开始和结束时间（时:分）生成新的文件名，格式为 "HH:mm-HH:mm.mp4"。
 * 7. 尝试将初始录制的视频文件重命名为新的、包含时间段的文件名。
 * 8. 显示录制完成的消息框，包含视频保存路径。
 */
void MonitorPage::stopRecording()
{
    // 如果当前未处于录制状态，则直接返回
    if (!m_isRecording) {
        qDebug() << "尝试停止录制，但当前未在录制状态。";
        return; 
    }
    qInfo() << "请求停止视频录制...";
    
    // 调用视频录制线程的 stopRecording 方法，通知其完成当前文件的写入并停止
    if (m_videoRecorder) { // 确保录制器对象存在
        m_videoRecorder->stopRecording();
    }
    
    // 停止录制时间更新定时器
    if (m_recordTimer && m_recordTimer->isActive()) { // 确保定时器存在且正在运行
        m_recordTimer->stop();
    }
    
    // 更新UI元素状态以反映录制已停止
    m_recordButton->setIcon(QIcon(":/images/playback.png")); // 恢复录制按钮图标为"开始录制"
    m_recordButton->setToolTip("开始录制");                   // 恢复提示文本
    m_recordStatusLabel->setText("未录制");                   // 更新状态标签文本
    m_recordStatusLabel->setProperty("class", "");             // 移除QSS的"recording"类，恢复默认样式
    style()->unpolish(m_recordStatusLabel); // 确保样式表更新
    style()->polish(m_recordStatusLabel);
    m_recordTimeLabel->setVisible(false);                   // 隐藏录制时间标签
    
    m_isRecording = false; // 更新内部录制状态标志
    
    // 获取录制结束的精确时间点
    QDateTime recordingEndTime = QDateTime::currentDateTime();
    
    // -- 重命名视频文件 --
    // 根据录制的开始时间和结束时间，生成更具描述性的文件名
    // 新文件名格式: HH:mm-HH:mm.mp4 (例如: 14:30-15:00.mp4)
    QString startTimeStr = m_recordingStartTime.toString("HH:mm"); // 开始时间 时:分
    QString endTimeStr = recordingEndTime.toString("HH:mm");     // 结束时间 时:分
    QString newVideoFileName = startTimeStr + "-" + endTimeStr + ".mp4"; // 拼接新文件名
    
    // 检查初始视频文件名是否有效
    if (!m_currentVideoFile.isEmpty()) {
        QFileInfo videoFileInfo(m_currentVideoFile); // 获取初始文件的信息
        // 构建新的完整文件路径 (目录保持不变，仅文件名更改)
        QString newVideoFilePath = videoFileInfo.dir().absolutePath() + "/" + newVideoFileName;
        
        QFile videoFile(m_currentVideoFile); // 创建QFile对象代表初始文件
        // 尝试重命名文件，如果目标文件已存在，则Qt的rename行为可能因平台而异 (通常会失败或覆盖)
        // 为避免覆盖，可以先检查 newVideoFilePath 是否存在
        if (QFile::exists(newVideoFilePath)) {
            qWarning() << "重命名失败：目标文件 " << newVideoFilePath << " 已存在。将使用原始文件名：" << m_currentVideoFile;
        } else {
            bool videoRenamed = videoFile.rename(newVideoFilePath); // 执行重命名操作
            
            // 如果重命名成功，则更新 m_currentVideoFile 为新的路径
            if (videoRenamed) {
                qInfo() << "视频文件已成功重命名为:" << newVideoFilePath;
                m_currentVideoFile = newVideoFilePath;
            } else {
                qWarning() << "重命名视频文件失败: 从 " << m_currentVideoFile << " 到 " << newVideoFilePath << ". 错误: " << videoFile.errorString();
                // 如果重命名失败，m_currentVideoFile 仍保持为初始文件名
            }
        }
    }
    
    // 显示录制完成的消息框，告知用户视频已保存及保存路径
    QString message = "录制完成\n";
    message += "视频已保存到: " + m_currentVideoFile; // 使用最终的文件路径（可能是重命名后的，也可能是初始的）
    QMessageBox::information(this, "录制完成", message);
    qInfo() << "录制流程已停止。" << message;
}

/**
 * @brief 切换录制状态 (开始/停止)。
 *
 * 此槽函数通常连接到录制按钮的 `clicked` 信号。
 * 如果当前正在录制 (`m_isRecording` 为 true)，则调用 `stopRecording()`。
 * 否则 (未在录制)，调用 `startRecording()`。
 */
void MonitorPage::toggleRecording()
{
    if (m_isRecording) { // 如果当前正在录制
        stopRecording();   // 则停止录制
    } else {               // 否则（当前未录制）
        startRecording();  // 则开始录制
    }
}

/**
 * @brief 更新录制状态和时间显示。
 *
 * 此槽函数由 `m_recordTimer` 定时器每秒调用一次（当录制正在进行时）。
 * 它负责递增已录制秒数 `m_recordingSeconds`，并将其格式化为 "HH:MM:SS" 
 * 的形式更新到 `m_recordTimeLabel` 标签上。
 */
void MonitorPage::updateRecordingStatus()
{
    if (m_isRecording) { // 仅当正在录制时执行
        // 递增已录制的总秒数
        m_recordingSeconds++;
        
        // 将总秒数转换为 时:分:秒 的格式
        int hours = m_recordingSeconds / 3600;                     // 计算小时数
        int minutes = (m_recordingSeconds % 3600) / 60;            // 计算分钟数
        int seconds = m_recordingSeconds % 60;                     // 计算秒数
        
        // 使用QString::arg()格式化时间字符串，确保时、分、秒都以两位数字显示，不足则补零
        // 例如：1小时5分3秒 -> "01:05:03"
        QString timeText = QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))   // %1: 小时，宽度2，十进制，不足补'0'
            .arg(minutes, 2, 10, QChar('0')) // %2: 分钟
            .arg(seconds, 2, 10, QChar('0'));// %3: 秒数
        
        // 更新录制时间显示标签的文本
        m_recordTimeLabel->setText(timeText);
    }
}

/**
 * @brief 处理存储管理器发出的存储空间不足事件。
 * @param availableBytes 当前可用的存储空间字节数。
 * @param totalBytes     总存储空间字节数。
 * @param percent        可用空间占总空间的百分比。
 *
 * 此槽函数在 `StorageManager` 检测到存储空间低于预设阈值时被调用。
 * 1. 使用 `qDebug()` 输出一条警告信息到控制台，包含详细的存储空间数据。
 * 2. 如果当前正在录制视频，则更新 `m_recordStatusLabel` 的文本，追加 "(存储空间不足)" 提示，
 *    但录制本身不立即中断。
 * 3. 调用 `m_storageManager->cleanupOldestDay()` 尝试自动清理最早一天的视频文件以释放空间。
 */
void MonitorPage::onLowStorageSpace(qint64 availableBytes, qint64 totalBytes, double percent)
{
    // 输出详细的存储空间不足警告日志
    qWarning() << QString("存储空间不足警告 - 可用: %1 MB (%3%), 总容量: %2 MB")
                  .arg(availableBytes / (1024.0 * 1024.0), 0, 'f', 2)
                  .arg(totalBytes / (1024.0 * 1024.0), 0, 'f', 2)
                  .arg(percent, 0, 'f', 1);
    
    // 如果当前正在录制视频，则在UI上给出提示，但不立即停止录制
    if (m_isRecording) {
        // 更新录制状态标签，显示存储空间不足的警告信息
        m_recordStatusLabel->setText("正在录制视频... (存储空间不足)");
        style()->unpolish(m_recordStatusLabel); // 确保样式表更新
        style()->polish(m_recordStatusLabel);
    }
    
    // 尝试自动清理最早一天的视频文件以释放空间
    qInfo() << "由于空间不足，尝试自动清理最早一天的视频文件...";
    m_storageManager->cleanupOldestDay();
}

/**
 * @brief 处理存储管理器发出的清理完成事件。
 * @param path       被成功清理的目录的路径。
 * @param freedBytes 通过清理该目录所释放的字节数。
 *
 * 此槽函数在 `StorageManager` 成功删除一个旧的视频目录后被调用。
 * 1. 使用 `qDebug()` 输出一条信息到控制台，包含被清理的目录路径和释放的空间大小。
 * 2. 如果当前正在录制视频，并且之前因为空间不足而更新了状态标签，
 *    则此时可以将状态标签恢复为正常的 "正在录制视频..."。
 */
void MonitorPage::onCleanupCompleted(const QString &path, qint64 freedBytes)
{
    // 输出清理完成的日志信息
    qInfo() << QString("已自动清理最早的视频目录: %1, 释放空间: %2 MB")
               .arg(path)
               .arg(freedBytes / (1024.0 * 1024.0), 0, 'f', 2);
    
    // 如果正在录制，并且状态标签之前可能显示了空间不足的警告，
    // 此时可以考虑恢复正常的录制状态显示（假设清理后空间已足够）
    if (m_isRecording) {
        // 检查当前状态文本是否包含"存储空间不足"
        if (m_recordStatusLabel->text().contains("(存储空间不足)")) {
             // 如果存储空间现在足够了，可以恢复正常显示
            if (m_storageManager->checkStorageSpace()) { // 再次检查空间
                 m_recordStatusLabel->setText("正在录制视频...");
                 style()->unpolish(m_recordStatusLabel);
                 style()->polish(m_recordStatusLabel);
                 qInfo() << "存储空间清理后已恢复正常，继续录制。";
            } else {
                qWarning() << "存储空间清理后仍然不足！录制状态标签将继续显示警告。";
            }
        }
    }
}

/**
 * @brief 处理录制线程发出的录制时间达到预设值（如30分钟）的事件。
 * @param filePath 当前正在录制的文件的路径。
 *
 * 此槽函数由 `RecordingThread` 在录制达到一定时长（由 `RecordingThread` 内部定时器控制）
 * 后发出 `recordingTimeReached30Minutes` 信号时调用，用于实现视频的自动分段录制。
 * 
 * 执行逻辑：
 * 1. 检查是否当前真的在录制状态，如果不是，则直接返回。
 * 2. 调用 `stopRecording()` 停止当前的录制段。这会完成当前文件的写入、重命名等操作。
 * 3. 立即调用 `startRecording()` 开始一个新的录制段。这会创建新文件、重置计时等。
 * 4. 输出调试信息表明已完成自动分段。
 */
void MonitorPage::onRecordingTimeReached30Minutes(const QString &filePath)
{
    qInfo() << "录制时间达到预设分段点 (来自文件: " << filePath << ")，准备自动分段...";
    
    // 确保当前确实处于录制状态，以避免不必要的操作或错误
    if (!m_isRecording) {
        qWarning() << "收到分段信号，但当前不在录制状态，忽略。";
        return; 
    }
    
    // 1. 停止当前的录制段 (完成文件写入、重命名等)
    qDebug() << "自动分段：正在停止当前录制段...";
    stopRecording(); 
    // stopRecording 会将 m_isRecording 设置为 false，startRecording会再次检查并设置为true
    
    // 2. 立即开始一个新的录制段
    qDebug() << "自动分段：正在开始新的录制段...";
    startRecording();
    
    qInfo() << "自动分段：已成功完成录制分段操作。";
}