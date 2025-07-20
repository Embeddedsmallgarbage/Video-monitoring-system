/**
 * @file videopage.h
 * @brief 视频播放页面类 (VideoPage) 的头文件。
 * 
 * 此文件声明了 VideoPage 类，该类是视频监控系统的一部分，
 * 专门用于播放已录制的视频文件。它继承自 QWidget，并利用
 * QMediaPlayer 和 QVideoWidget 实现视频播放功能。
 * 
 * 主要特性包括：
 * - 播放指定的视频文件。
 * - 提供播放控制接口（播放/暂停、停止、跳转）。
 * - 显示视频播放进度和时间信息。
 * - (可选) 展示与当前视频同目录的其他视频文件列表，并允许切换。
 * - 提供返回到上一页（通常是历史记录页面）的导航。
 */
#ifndef VIDEOPAGE_H
#define VIDEOPAGE_H

#include <QWidget>       // QWidget 基类，所有UI元素的父类
#include <QLabel>        // QLabel 类，用于显示文本或图像 (此处用于显示时间)
#include <QPushButton>   // QPushButton 类，命令按钮控件
#include <QSlider>       // QSlider 类，滑动条控件 (此处用作播放进度条)
#include <QMediaPlayer>  // QMediaPlayer 类，用于媒体播放的核心功能
#include <QVideoWidget>  // QVideoWidget 类，用于显示 QMediaPlayer 播放的视频内容
#include <QListWidget>   // QListWidget 类，用于显示项目列表 (此处用于显示同目录视频文件)
#include <QTimer>        // QTimer 类 (虽然在此头文件中未直接作为成员变量声明，但在.cpp中可能用于UI更新等)

// 前向声明 (Forward Declarations)
// 用于声明类名，使得可以在不知道这些类的完整定义的情况下使用它们的指针或引用。
// 这有助于减少编译依赖，避免头文件之间的循环包含问题。
class MainWindow;        // 主窗口类，VideoPage 是其子页面之一，需要访问主窗口进行页面切换。
class QListWidgetItem;   // QListWidget 中的列表项类，在槽函数参数中用到。

/**
 * @brief 视频播放页面类 (VideoPage)
 * 
 * 继承自 QWidget，提供了视频播放的用户界面和控制逻辑。
 * 使用 QMediaPlayer 处理媒体播放，QVideoWidget 显示视频，
 * 并包含各种按钮、滑块和标签来与用户交互。
 */
class VideoPage : public QWidget
{
    Q_OBJECT // Qt元对象系统宏，使得类可以使用信号、槽以及其他Qt特性

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针，通常是 MainWindow 的实例。
     *               `explicit` 关键字防止构造函数的隐式类型转换。
     */
    explicit VideoPage(MainWindow *parent);

    /**
     * @brief 初始化视频播放页面的用户界面 (UI)。
     *
     * 此方法在构造函数中被调用，负责创建、配置和布局页面上的所有UI组件，
     * 并连接必要的信号和槽。
     */
    void setupUI();

    /**
     * @brief 加载并开始播放指定的视频文件。
     * @param filePath 要播放的视频文件的完整路径字符串。
     * 
     * 此函数会设置 QMediaPlayer 的媒体源为指定的视频文件，并开始播放。
     * 同时，它还会更新同目录视频列表。
     */
    void playVideo(const QString &filePath);

public slots: // 公共槽函数，可以从其他对象（如按钮点击、播放器状态变化）或通过信号连接调用
    /**
     * @brief 槽函数：切换视频的播放/暂停状态。
     *
     * 当播放/暂停按钮被点击时调用。如果当前正在播放，则暂停；如果已暂停，则继续播放。
     */
    void playPauseVideo();

    /**
     * @brief 槽函数：停止当前视频的播放。
     *
     * 当停止按钮被点击时调用。会使 QMediaPlayer 停止播放并将播放位置重置。
     */
    void stopVideo();

    /**
     * @brief 槽函数：设置视频的播放位置。
     * @param position 用户通过进度条选择的新的播放位置 (单位：毫秒)。
     *
     * 当用户拖动播放进度条时，由此信号 `QSlider::sliderMoved` 触发调用。
     */
    void setVideoPosition(int position);

    /**
     * @brief 槽函数：处理 QMediaPlayer 播放位置变化事件。
     * @param position 当前 QMediaPlayer 的播放位置 (单位：毫秒)。
     *
     * 当视频播放位置改变时 (正常播放、跳转等)，此槽被 `QMediaPlayer::positionChanged` 信号触发。
     * 用于更新UI上的进度条和时间显示。
     */
    void videoPositionChanged(qint64 position);

    /**
     * @brief 槽函数：处理 QMediaPlayer 视频总时长变化事件。
     * @param duration 视频的总时长 (单位：毫秒)。
     *
     * 当加载新视频或确定视频总时长后，此槽被 `QMediaPlayer::durationChanged` 信号触发。
     * 用于更新UI上的进度条范围和时间显示。
     */
    void videoDurationChanged(qint64 duration);

    /**
     * @brief 槽函数：处理视频文件列表项的双击事件。
     * @param item 被双击的 QListWidgetItem 对象。
     *
     * 当用户双击同目录视频列表中的一个文件时，此槽被 `QListWidget::itemDoubleClicked` 信号触发。
     * 用于加载并播放选中的新视频文件。
     */
    void videoItemDoubleClicked(QListWidgetItem *item);
    
    /**
     * @brief 获取当前正在播放的视频文件所在的目录路径。
     * @return 返回一个 QString，包含当前视频目录的绝对路径。
     *         主要用于在返回历史页面时，让历史页面能够定位到之前的目录。
     */
    QString getCurrentVideoDir() const;

private: // 私有成员变量，仅供 VideoPage 类内部访问
    MainWindow *m_mainWindow;  ///< 指向主窗口 (MainWindow) 实例的指针，用于页面导航等。
    
    // --- UI 组件指针 --- 
    QMediaPlayer *m_mediaPlayer;       ///< Qt的多媒体播放器核心对象，负责视频的解码和播放控制。
    QVideoWidget *m_videoWidget;       ///< Qt的视频显示控件，用于渲染 QMediaPlayer 输出的视频帧。
    QSlider *m_positionSlider;         ///< 水平滑动条，用作视频播放进度条，允许用户查看和调整播放位置。
    QLabel *m_durationLabel;           ///< 文本标签，用于显示视频的当前播放时间和总时长 (例如 "01:23 / 05:40")。
    QPushButton *m_playPauseButton;    ///< 按钮，用于控制视频的播放和暂停。
    QPushButton *m_stopButton;         ///< 按钮，用于停止视频播放。
    QPushButton *m_backButton;         ///< 按钮，用于从当前视频播放页面返回到上一级页面 (通常是历史记录页面)。
    QListWidget *m_videoListWidget;    ///< 列表控件，用于显示与当前播放视频同目录下的其他MP4视频文件。
    QPushButton *m_toggleListButton;   ///< 按钮，用于显示或隐藏旁边的 `m_videoListWidget`。
    QWidget *m_videoListContainer;     ///< QWidget容器，用于容纳 `m_videoListWidget` 及其标题，方便整体显示/隐藏。
    
    // --- 状态变量 --- 
    bool m_isVideoListVisible;         ///< 布尔标志，指示同目录视频列表当前是否可见。
    QString m_currentVideoDir;         ///< 字符串，存储当前正在播放的视频文件所在的完整目录路径。
};

#endif // VIDEOPAGE_H
