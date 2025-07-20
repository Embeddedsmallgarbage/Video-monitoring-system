#ifndef MONITORPAGE_H
#define MONITORPAGE_H

#include <QWidget>         // QWidget 基类，所有UI元素的父类
#include <QLabel>          // QLabel 类，用于显示文本或图像
#include <QPushButton>     // QPushButton 类，命令按钮控件
#include <QTimer>          // QTimer 类，提供重复性和单次定时器
#include <QThread>         // QThread 类 (在此文件中未直接使用，但可能被包含的头文件间接依赖，或为未来扩展预留)
#include <QDir>            // QDir 类，用于目录操作和文件系统导航
#include <QDateTime>       // QDateTime 类，用于处理日期和时间
#include <QDebug>          // QDebug 类，用于输出调试信息 (通常在开发阶段使用)
#include <chrono>          // C++ 标准库 <chrono>，用于高精度时间测量 (此处用于FPS计算)

// 前向声明 (Forward Declarations)
// 用于声明类名，使得可以在不知道这些类的完整定义的情况下使用它们的指针或引用。
// 这有助于减少编译依赖，避免头文件之间的循环包含问题。
class RecordingThread;   // 视频录制线程类，负责将视频帧编码并写入文件
class MainWindow;        // 主窗口类，MonitorPage 是其子页面之一
class StorageManager;    // 存储管理类，负责监控和管理录像文件的存储空间

/**
 * @brief 监控页面类 (MonitorPage)
 * 
 * 该类继承自 QWidget，是视频监控系统中的实时监控功能模块。
 * 主要职责包括：
 * - 通过V4L2接口从摄像头捕获视频帧。
 * - 在界面上实时显示捕获到的视频画面。
 * - 计算并显示实时帧率 (FPS)。
 * - 提供用户界面控件，用于开始/停止视频录制。
 * - 管理视频录制过程，包括文件命名、自动分段、存储空间检查等。
 * - 与 RecordingThread 交互以执行实际的视频编码和文件写入。
 * - 与 StorageManager 交互以监控存储空间并在必要时执行清理。
 * - 提供返回到主页面的导航功能。
 */
class MonitorPage : public QWidget
{
    Q_OBJECT // Qt元对象系统宏，使得类可以使用信号、槽以及其他Qt特性

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针，通常是 MainWindow 的实例。默认为 nullptr。
     *               `explicit` 关键字防止构造函数的隐式类型转换。
     */
    explicit MonitorPage(MainWindow *parent = nullptr);

    /**
     * @brief 初始化监控页面的用户界面 (UI)。
     *
     * 此方法负责创建、配置和布局监控页面上的所有视觉元素，
     * 如视频显示区域、控制按钮、状态标签等，并连接必要的信号和槽。
     */
    void setupUI();

    /**
     * @brief 开始视频采集流程。
     * @return 如果成功初始化摄像头、开始V4L2捕获并启动帧更新定时器，则返回 true；否则返回 false。
     *
     * 此函数会尝试初始化摄像头设备，启动视频流捕获，并启动一个定时器
     * 以固定间隔 (`updateFrame()`) 从摄像头拉取并显示视频帧。
     */
    bool startCapture();

    /**
     * @brief 停止视频采集流程。
     *
     * 此函数会停止帧更新定时器，停止V4L2视频流捕获，并清理相关资源。
     * 如果当前正在录制视频，也会先调用 `stopRecording()` 来停止录制。
     */
    void stopCapture();

public slots: // 公共槽函数，可以从其他对象（如定时器、按钮）或通过信号连接调用
    /**
     * @brief 槽函数：更新并显示一帧视频图像。
     *
     * 通常由 `m_frameTimer` 定时器周期性调用。
     * 此函数从摄像头获取最新一帧的图像数据，将其显示在 `m_imageLabel` 上，
     * 计算并更新FPS，如果正在录制，则将该帧数据传递给 `m_videoRecorder` 进行处理。
     */
    void updateFrame();
    
    /**
     * @brief 槽函数：开始视频录制。
     *
     * 当用户点击录制按钮（且当前未在录制时）或系统需要自动开始录制（如分段后）时调用。
     * 此函数会检查存储空间，创建录制文件，并启动 `m_videoRecorder` 线程开始录制。
     * 同时更新UI状态以反映正在录制。
     */
    void startRecording();
    
    /**
     * @brief 槽函数：停止视频录制。
     *
     * 当用户点击停止录制按钮（且当前正在录制时）或系统需要自动停止录制（如分段前）时调用。
     * 此函数会通知 `m_videoRecorder` 线程停止录制并完成文件写入，然后更新UI状态。
     * 录制的文件会根据开始和结束时间进行重命名。
     */
    void stopRecording();
    
    /**
     * @brief 槽函数：切换录制状态 (开始/停止)。
     *
     * 通常连接到录制按钮的 `clicked()` 信号。
     * 如果当前正在录制，则调用 `stopRecording()`；否则调用 `startRecording()`。
     */
    void toggleRecording();
    
    /**
     * @brief 槽函数：更新录制状态和已录制时间显示。
     *
     * 由 `m_recordTimer` 定时器每秒调用一次（在录制期间）。
     * 它会递增已录制秒数，并格式化为 HH:MM:SS 的形式更新到 `m_recordTimeLabel`。
     */
    void updateRecordingStatus();
    
    /**
     * @brief 槽函数：处理因录制时长达到预设值（如30分钟）而需要自动分段的事件。
     * @param filePath 当前录制段的视频文件路径 (由 RecordingThread 发送)。
     *
     * 当 `m_videoRecorder` 检测到录制时长达到分段阈值时，会发出信号触发此槽函数。
     * 此函数会先调用 `stopRecording()` 结束当前录制段，然后立即调用 `startRecording()`
     * 开始一个新的录制段，从而实现视频的无缝分段录制。
     */
    void onRecordingTimeReached30Minutes(const QString &filePath);

private: // 私有成员函数和变量，仅供 MonitorPage 类内部访问
    /**
     * @brief 私有辅助函数：初始化媒体录制相关的组件。
     *
     * 此函数负责创建 `RecordingThread` 的实例 (`m_videoRecorder`)，并连接其
     * `recordError` 和 `recordingTimeReached30Minutes` 等信号到本类的相应槽函数。
     */
    void initMediaRecorder();
    
    MainWindow *m_mainWindow;      ///< 指向主窗口 (MainWindow) 实例的指针，用于页面导航等。
    
    // UI 组件指针
    QLabel *m_imageLabel;          ///< 用于显示实时视频画面的 QLabel 控件。
    QPushButton *m_backButton;     ///< "返回首页"按钮。
    QPushButton *m_recordButton;   ///< "开始/停止录制"按钮。
    QLabel *m_recordStatusLabel;   ///< 显示当前录制状态的标签 (例如 "未录制", "正在录制...")。
    QLabel *m_recordTimeLabel;     ///< 显示当前录制时长的标签 (格式 HH:MM:SS)。
    
    // 定时器
    QTimer *m_frameTimer;          ///< 定时器，用于周期性触发 `updateFrame()` 以刷新视频画面。
    QTimer *m_recordTimer;         ///< 定时器，用于在录制期间每秒触发 `updateRecordingStatus()` 更新录制时长。
    
    // V4L2 视频帧相关
    unsigned char *m_frameBuffer;  ///< 指向存储从摄像头获取的原始帧数据的缓冲区。
    int m_frameWidth;              ///< 当前视频帧的宽度 (像素)。
    int m_frameHeight;             ///< 当前视频帧的高度 (像素)。
    
    // 视频录制相关状态和数据
    RecordingThread *m_videoRecorder; ///< 指向视频录制线程 (RecordingThread) 的实例。
    bool m_isRecording;            ///< 标志位，指示当前是否正在进行视频录制 (true 表示正在录制)。
    int m_recordingSeconds;        ///< 当前录制段已持续的秒数。
    QString m_recordingPath;       ///< 录像文件保存的根目录路径 (例如 "/mnt/TFcard")。
    QDateTime m_recordingStartTime;  ///< 当前录制段的开始时间。
    QString m_currentVideoFile;    ///< 当前正在录制或刚刚完成录制的视频文件的完整路径。
    
    // 实时帧率 (FPS) 计算和显示相关
    QLabel *m_fpsLabel;            ///< 用于显示实时帧率 (FPS) 的 QLabel 控件。
    std::chrono::steady_clock::time_point m_lastFrameTime; ///< 上一帧被处理的时间点，用于计算FPS。
    double m_currentFPS;           ///< 当前计算并平滑处理后的帧率值。
    
    // 存储空间管理相关
    StorageManager *m_storageManager; ///< 指向存储管理器 (StorageManager) 的实例。
    
private slots: // 私有槽函数，通常用于响应来自类内部或其他紧密关联对象的信号
    /**
     * @brief 私有槽函数：处理存储空间不足的信号。
     * @param availableBytes 当前可用的存储空间字节数。
     * @param totalBytes TF卡总存储空间字节数。
     * @param percent 当前可用空间占总空间的百分比。
     *
     * 当 `m_storageManager` 检测到存储空间低于阈值时，会发出信号触发此槽。
     * 此函数会更新UI提示用户空间不足，并尝试调用 `m_storageManager` 清理旧文件。
     */
    void onLowStorageSpace(qint64 availableBytes, qint64 totalBytes, double percent);
    
    /**
     * @brief 私有槽函数：处理存储空间清理完成的信号。
     * @param path 被成功清理的目录的路径。
     * @param freedBytes 通过清理该目录所释放的字节数。
     *
     * 当 `m_storageManager` 成功删除一个旧的视频目录后，会发出信号触发此槽。
     * 此函数会记录日志，并可能更新UI状态（例如，如果之前有空间不足的警告，现在可以移除）。
     */
    void onCleanupCompleted(const QString &path, qint64 freedBytes);
};

#endif // MONITORPAGE_H