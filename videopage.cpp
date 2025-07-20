/**
 * @file videopage.cpp
 * @brief 视频播放页面类 (VideoPage) 的实现文件。
 * 
 * 本文件负责实现视频监控系统中与视频播放相关的功能。
 * 主要职责包括：
 * - 加载并播放用户从历史记录中选择的视频文件。
 * - 提供标准的视频播放控制，如播放、暂停、停止。
 * - 显示视频播放的当前进度（通过滑块）和时间信息（当前时间/总时长）。
 * - 在视频播放界面提供返回到历史记录页面的导航功能。
 * - (可选特性) 显示与当前播放视频位于同一目录下的其他视频文件列表，并允许用户切换播放。
 * - (可选特性) 提供一个可切换显示/隐藏状态的侧边栏用于展示同目录视频列表。
 */

#include "videopage.h"
#include "mainwindow.h" // 包含主窗口头文件，用于页面切换和交互

#include <QVBoxLayout>     // Qt布局类，用于垂直排列控件
#include <QHBoxLayout>     // Qt布局类，用于水平排列控件
#include <QStackedLayout>  // Qt布局类，用于堆叠控件，通常只显示一个
#include <QDir>            // Qt目录操作类，用于文件系统导航和列出文件
#include <QFileInfo>       // Qt文件信息类，用于获取文件的属性（如路径、名称、目录）
#include <QIcon>           // Qt图标类，用于在按钮等控件上显示图标
#include <QTime>           // Qt时间处理类，用于格式化和显示时间

/**
 * @brief VideoPage 类的构造函数。
 * @param parent 父窗口指针，通常是 MainWindow 的实例。
 *               遵循Qt的对象父子关系模型，有助于内存管理。
 *
 * 初始化视频播放页面的所有成员变量，包括UI组件指针 (初始为nullptr)、
 * 媒体播放器实例、以及与视频列表相关的状态变量。
 * 最后调用 `setupUI()` 方法来构建和布局用户界面。
 */
VideoPage::VideoPage(MainWindow *parent)
    : QWidget(parent)                         // 调用父类QWidget的构造函数，并设置父对象
    , m_mainWindow(parent)                    // 初始化主窗口指针
    , m_mediaPlayer(nullptr)                  // 初始化媒体播放器指针为空
    , m_videoWidget(nullptr)                  // 初始化视频显示控件指针为空
    , m_positionSlider(nullptr)               // 初始化播放进度条指针为空
    , m_durationLabel(nullptr)                // 初始化时长标签指针为空
    , m_playPauseButton(nullptr)              // 初始化播放/暂停按钮指针为空
    , m_stopButton(nullptr)                   // 初始化停止按钮指针为空
    , m_backButton(nullptr)                   // 初始化返回按钮指针为空
    , m_videoListWidget(nullptr)              // 初始化视频文件列表控件指针为空
    , m_toggleListButton(nullptr)             // 初始化切换列表显示按钮指针为空
    , m_videoListContainer(nullptr)           // 初始化视频列表容器控件指针为空
    , m_isVideoListVisible(false)             // 初始化视频列表可见状态为false (隐藏)
    , m_currentVideoDir("")                  // 初始化当前视频目录路径为空字符串
{
    setupUI(); // 调用UI设置函数，构建界面
}

/**
 * @brief 初始化视频播放页面的用户界面 (UI)。
 *
 * 此方法负责创建、配置和布局视频播放页面上的所有视觉元素，
 * 包括视频显示区域、播放控制按钮 (播放/暂停、停止)、播放进度条、
 * 时间显示标签、返回按钮，以及可选的同目录视频列表和其切换按钮。
 * 同时，它还负责连接各个UI组件的信号到相应的槽函数，以实现交互逻辑。
 */
void VideoPage::setupUI()
{
    // --- 整体布局 --- 
    // 创建主水平布局 (mainLayout)，作为整个页面的根布局
    QHBoxLayout *mainLayout = new QHBoxLayout(this); // `this` 使mainLayout成为VideoPage的子布局
    mainLayout->setSpacing(0);                       // 设置布局内控件间距为0
    mainLayout->setContentsMargins(0, 0, 0, 0);    // 设置布局的四个外边距为0
    
    // --- 左侧：视频播放器区域 --- 
    // 创建视频播放器区域的容器 QWidget (videoPlayerWidget)
    QWidget *videoPlayerWidget = new QWidget();
    // 为 videoPlayerWidget 创建垂直布局 (videoLayout)
    QVBoxLayout *videoLayout = new QVBoxLayout(videoPlayerWidget);
    
    // 1. 媒体播放器核心组件初始化
    m_mediaPlayer = new QMediaPlayer(this); // 创建QMediaPlayer实例，this作为其父对象
    m_videoWidget = new QVideoWidget(this); // 创建QVideoWidget实例 (用于显示视频)，this作为其父对象
    m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio); // 设置视频显示时保持宽高比
    
    // 设置视频控件的大小策略，使其可以在水平和垂直方向上扩展以填充可用空间
    QSizePolicy sizePolicy = m_videoWidget->sizePolicy();
    sizePolicy.setVerticalPolicy(QSizePolicy::Expanding);   // 垂直方向可扩展
    sizePolicy.setHorizontalPolicy(QSizePolicy::Expanding); // 水平方向可扩展
    m_videoWidget->setSizePolicy(sizePolicy);
    // 注意: 最小高度已在 style.qss 文件中通过 #m_videoWidget { min-height: 400px; } 设置
    
    m_mediaPlayer->setVideoOutput(m_videoWidget); // 将 QVideoWidget 设置为 QMediaPlayer 的视频输出目标
    
    // 2. 播放控制条：进度条和时间标签
    m_positionSlider = new QSlider(Qt::Horizontal); // 创建水平 QSlider 作为播放进度条
    m_positionSlider->setRange(0, 0);               // 初始范围设为0-0，在加载视频后更新
    
    m_durationLabel = new QLabel("00:00 / 00:00"); // 创建 QLabel 用于显示播放时间和总时长
    m_durationLabel->setAlignment(Qt::AlignCenter); // 设置文本居中对齐
    
    // 3. 播放控制按钮 (播放/暂停、停止)
    QHBoxLayout *controlLayout = new QHBoxLayout(); // 创建水平布局用于放置控制按钮
    m_playPauseButton = new QPushButton();          // 创建播放/暂停按钮
    m_playPauseButton->setIcon(QIcon(":/images/playback.png")); // 从资源文件设置初始图标 (播放状态)
    m_playPauseButton->setIconSize(QSize(32, 32));  // 设置图标大小
    m_playPauseButton->setToolTip(tr("播放/暂停"));   // 设置鼠标悬停提示
    
    m_stopButton = new QPushButton();               // 创建停止按钮
    m_stopButton->setIcon(QIcon(":/images/stop.png"));    // 从资源文件设置图标
    m_stopButton->setIconSize(QSize(32, 32));       // 设置图标大小
    m_stopButton->setToolTip(tr("停止"));            // 设置鼠标悬停提示
    
    // (返回按钮稍后会放置在视频覆盖层上)
    m_backButton = new QPushButton();               // 创建返回按钮 (实际布局位置不同)
    m_backButton->setIcon(QIcon(":/images/back.png"));     // 从资源文件设置图标
    m_backButton->setIconSize(QSize(32, 32));       // 设置图标大小
    m_backButton->setToolTip(tr("返回"));            // 设置鼠标悬停提示
    
    // --- 设置对象名称 (主要用于 QSS 样式选择和查找子对象) --- 
    m_videoWidget->setObjectName("m_videoWidget");
    m_positionSlider->setObjectName("m_positionSlider");
    m_durationLabel->setObjectName("m_durationLabel");
    m_playPauseButton->setObjectName("m_playPauseButton");
    m_stopButton->setObjectName("m_stopButton");
    m_backButton->setObjectName("m_backButton"); // 虽然布局位置不同，但仍可设置对象名
    
    // 将播放/暂停和停止按钮添加到 controlLayout
    controlLayout->addWidget(m_playPauseButton);
    controlLayout->addWidget(m_stopButton);
    
    // --- 中间：视频显示区 (包含视频本身和可选的右侧列表) --- 
    QHBoxLayout *videoAndListLayout = new QHBoxLayout(); // 创建水平布局容纳视频区和列表切换部分
    videoAndListLayout->setSpacing(0);                   // 控件间无间距
    
    // --- 右侧：可切换的视频列表区域 --- 
    m_videoListContainer = new QWidget(); // 创建视频列表的容器 QWidget
    m_videoListContainer->setObjectName("m_videoListContainer"); // 用于QSS样式
    // 注意: m_videoListContainer 的固定宽度已在 style.qss 中通过 #m_videoListContainer { width: 200px; } 设置
    QVBoxLayout *listLayout = new QVBoxLayout(m_videoListContainer); // 为列表容器创建垂直布局
    // 注意: listLayout 的内边距可能已在 style.qss 中为 #m_videoListContainer 设置
    
    QLabel *listTitle = new QLabel(tr("同目录视频列表")); // 创建列表标题标签
    listTitle->setAlignment(Qt::AlignCenter);           // 标题居中
    listTitle->setObjectName("listTitle");             // 用于QSS样式
    
    m_videoListWidget = new QListWidget();            // 创建视频文件列表 QListWidget
    m_videoListWidget->setObjectName("m_videoListWidget"); // 用于QSS样式
    m_videoListWidget->setSelectionMode(QAbstractItemView::SingleSelection); // 设置为单选模式
    // 注意: m_videoListWidget 的图标大小和项目高度已在 style.qss 中设置
    
    listLayout->addWidget(listTitle);                 // 添加标题到列表布局
    listLayout->addWidget(m_videoListWidget);         // 添加列表控件到列表布局
    
    // 创建显示/隐藏视频列表的按钮 (m_toggleListButton)
    m_toggleListButton = new QPushButton("◀");       // 初始文本为向左箭头 (表示列表当前隐藏，点击可显示)
    m_toggleListButton->setObjectName("m_toggleListButton"); // 用于QSS样式
    // 注意: m_toggleListButton 的固定大小和样式已在 style.qss 中设置
    m_toggleListButton->setToolTip(tr("显示/隐藏视频列表")); // 鼠标悬停提示
    
    // 初始化视频列表为隐藏状态
    m_isVideoListVisible = false;
    m_videoListContainer->setVisible(false); // 初始不显示视频列表容器
    
    // --- 视频覆盖层与堆叠布局 (用于在视频上显示返回按钮) --- 
    // 创建一个透明的覆盖层QWidget (overlayWidget)，用于放置返回按钮
    QWidget *overlayWidget = new QWidget();
    overlayWidget->setObjectName("overlayWidget"); // 用于QSS样式 (使其背景透明)
    QHBoxLayout *overlayLayout = new QHBoxLayout(overlayWidget); // 为覆盖层创建布局
    // 注意: overlayWidget 的内边距 (padding) 已在 style.qss 中设置
    overlayLayout->addWidget(m_backButton, 0, Qt::AlignLeft | Qt::AlignTop); // 返回按钮左上角对齐
    overlayLayout->addStretch(); // 添加伸缩项，使返回按钮保持在左上角
    
    // 创建堆叠布局 (stackedLayout)，将视频控件 (m_videoWidget) 和覆盖层 (overlayWidget) 堆叠起来
    QStackedLayout *stackedLayout = new QStackedLayout();
    stackedLayout->setStackingMode(QStackedLayout::StackAll); // 设置堆叠模式为 StackAll，使所有控件可见 (上层透明则下层可见)
    stackedLayout->addWidget(m_videoWidget);  // 先添加视频控件 (在底层)
    stackedLayout->addWidget(overlayWidget);  // 再添加覆盖层 (在顶层)
    
    // 将堆叠布局放入一个新的容器QWidget (stackContainer)，然后将此容器添加到 videoAndListLayout
    QWidget *stackContainer = new QWidget();
    stackContainer->setLayout(stackedLayout);
    // videoAndListLayout 原本直接添加 m_videoWidget，现改为添加包含堆叠布局的 stackContainer
    // videoAndListLayout->addWidget(m_videoWidget); // 旧的添加方式，将被替换
    videoAndListLayout->addWidget(stackContainer); // 添加包含视频和覆盖层的堆叠容器 (在左侧)
    
    // 将切换按钮和视频列表容器添加到 videoAndListLayout (在右侧)
    videoAndListLayout->addWidget(m_toggleListButton); // 紧邻视频区右侧的是切换按钮
    videoAndListLayout->addWidget(m_videoListContainer); // 再右侧是视频列表容器
    
    // --- 组装视频播放器区域的垂直布局 (videoLayout) --- 
    videoLayout->addLayout(videoAndListLayout, 1); // 添加包含视频和列表的水平布局 (设置拉伸因子为1，使其优先占据垂直空间)
    videoLayout->addWidget(m_positionSlider);      // 在视频下方添加进度条
    videoLayout->addWidget(m_durationLabel);       // 在进度条下方添加时间标签
    videoLayout->addLayout(controlLayout);         // 在最下方添加播放控制按钮布局
    
    // --- 将视频播放器区域 (videoPlayerWidget) 添加到主布局 (mainLayout) --- 
    mainLayout->addWidget(videoPlayerWidget);
    
    // --- 连接信号和槽 --- 
    // 播放/暂停按钮的 clicked 信号连接到 playPauseVideo 槽函数
    connect(m_playPauseButton, &QPushButton::clicked, this, &VideoPage::playPauseVideo);
    // 停止按钮的 clicked 信号连接到 stopVideo 槽函数
    connect(m_stopButton, &QPushButton::clicked, this, &VideoPage::stopVideo);
    // 返回按钮的 clicked 信号连接到主窗口的 returnFromVideoPage 槽函数 (实现页面切换)
    connect(m_backButton, &QPushButton::clicked, m_mainWindow, &MainWindow::returnFromVideoPage);
    // 进度条的 sliderMoved 信号 (用户拖动滑块时) 连接到 setVideoPosition 槽函数
    connect(m_positionSlider, &QSlider::sliderMoved, this, &VideoPage::setVideoPosition);
    // QMediaPlayer 的 positionChanged 信号 (播放位置改变时) 连接到 videoPositionChanged 槽函数
    connect(m_mediaPlayer, &QMediaPlayer::positionChanged, this, &VideoPage::videoPositionChanged);
    // QMediaPlayer 的 durationChanged 信号 (视频总时长确定或改变时) 连接到 videoDurationChanged 槽函数
    connect(m_mediaPlayer, &QMediaPlayer::durationChanged, this, &VideoPage::videoDurationChanged);
    // 视频列表控件的 itemDoubleClicked 信号 (列表项被双击时) 连接到 videoItemDoubleClicked 槽函数
    connect(m_videoListWidget, &QListWidget::itemDoubleClicked, this, &VideoPage::videoItemDoubleClicked);
    // 切换列表显示按钮的 clicked 信号连接到一个 lambda 表达式，用于处理列表的显示/隐藏逻辑
    connect(m_toggleListButton, &QPushButton::clicked, this, [this]() {
        // 切换视频列表的可见性状态
        m_isVideoListVisible = !m_isVideoListVisible;
        m_videoListContainer->setVisible(m_isVideoListVisible);
        
        // 更新切换按钮的文本 (箭头方向)
        m_toggleListButton->setText(m_isVideoListVisible ? "▶" : "◀"); // 可见时为向右箭头，隐藏时为向左箭头
        
        // 当列表隐藏时，确保视频控件能正确扩展填充空间 (这是一个可选的UI微调)
        if (!m_isVideoListVisible) {
            // 尝试强制更新视频控件及其父控件的几何形状和布局
            m_videoWidget->updateGeometry(); 
            if (m_videoWidget->parentWidget()) { // 确保父控件存在
                 m_videoWidget->parentWidget()->updateGeometry();
                 // 进一步尝试强制更新父控件的布局
                 QTimer::singleShot(0, this, [this]() { // 使用QTimer::singleShot延迟执行，确保在事件循环中处理
                     if (m_videoWidget->parentWidget() && m_videoWidget->parentWidget()->layout()) {
                         m_videoWidget->parentWidget()->layout()->update();
                     }
                 });
            }
        }
    });
}

/**
 * @brief 开始播放指定的视频文件。
 * @param filePath 要播放的视频文件的完整路径。
 * 
 * 此函数执行以下操作：
 * 1. 使用 QMediaPlayer 加载并开始播放指定的视频文件。
 * 2. 更新播放/暂停按钮的图标为"暂停"状态。
 * 3. 清空当前的视频文件列表 (`m_videoListWidget`)。
 * 4. 获取当前播放视频所在的目录。
 * 5. 将此目录路径保存到 `m_currentVideoDir` 成员变量。
 * 6. 更新视频列表的标题，显示为当前目录的名称。
 * 7. 扫描该目录下的所有 MP4 文件。
 * 8. 将扫描到的MP4文件添加到 `m_videoListWidget` 中，并为每个列表项设置图标和文件路径数据。
 * 9. 如果列表中的某个文件是当前正在播放的视频，则将其设为选中状态并滚动到可视区域。
 */
void VideoPage::playVideo(const QString &filePath)
{
    // 1. 设置媒体源并开始播放
    m_mediaPlayer->setMedia(QUrl::fromLocalFile(filePath)); // 从本地文件路径创建QUrl作为媒体源
    m_mediaPlayer->play();                                  // 开始播放
    m_playPauseButton->setIcon(QIcon(":/images/pause.png")); // 更新按钮图标为"暂停"
    
    // 2. 更新同目录视频列表
    m_videoListWidget->clear(); // 清空现有列表项
    
    // 获取当前视频文件的信息和所在目录
    QFileInfo fileInfo(filePath);
    QDir videoDir = fileInfo.dir(); // 获取文件所在的QDir对象
    
    m_currentVideoDir = videoDir.absolutePath(); // 保存当前视频所在目录的绝对路径
    
    // 更新视频列表的标题，显示当前目录名
    QLabel* listTitle = m_videoListContainer->findChild<QLabel*>("listTitle"); // 通过对象名查找标题标签
    if (listTitle) {
        listTitle->setText(videoDir.dirName() + tr(" 目录视频列表")); // 设置标题文本
    }
    
    // 定义文件过滤器，只查找.mp4文件 (不区分大小写)
    QStringList filters;
    filters << "*.mp4" << "*.MP4";
    // 获取目录中所有符合条件的视频文件信息列表，按名称排序
    QFileInfoList videoFiles = videoDir.entryInfoList(filters, QDir::Files, QDir::Name);
    
    // 遍历找到的视频文件，添加到列表控件中
    for (const QFileInfo &videoFileInfo : videoFiles) {
        QString displayName = videoFileInfo.fileName(); // 获取文件名作为显示名称
        // 创建列表项，设置图标 (来自资源) 和显示名称
        QListWidgetItem *item = new QListWidgetItem(QIcon(":/images/mp4.png"), displayName);
        // 将视频文件的完整路径存储在列表项的 UserRole 数据中，方便后续引用
        item->setData(Qt::UserRole, videoFileInfo.absoluteFilePath());
        
        // 如果当前列表项对应的是正在播放的视频文件，则将其设为选中状态，并确保其可见
        if (videoFileInfo.absoluteFilePath() == filePath) {
            item->setSelected(true);
            m_videoListWidget->scrollToItem(item, QAbstractItemView::EnsureVisible); // 滚动到该项
        }
        
        m_videoListWidget->addItem(item); // 将创建的列表项添加到视频列表控件
    }
}

/**
 * @brief 槽函数：处理播放/暂停按钮的点击事件。
 * 
 * 根据当前 QMediaPlayer 的播放状态 (PlayingState)，切换播放/暂停状态：
 * - 如果当前正在播放，则调用 `m_mediaPlayer->pause()` 暂停播放，并更新按钮图标为"播放"。
 * - 如果当前已暂停或停止，则调用 `m_mediaPlayer->play()` 开始或继续播放，并更新按钮图标为"暂停"。
 */
void VideoPage::playPauseVideo()
{
    if (m_mediaPlayer->state() == QMediaPlayer::PlayingState) { // 如果正在播放
        m_mediaPlayer->pause();                                  // 暂停播放
        m_playPauseButton->setIcon(QIcon(":/images/playback.png")); // 更新图标为"播放"
    } else { // 如果已暂停、停止或未加载媒体
        m_mediaPlayer->play();                                   // 开始/继续播放
        m_playPauseButton->setIcon(QIcon(":/images/pause.png"));    // 更新图标为"暂停"
    }
}

/**
 * @brief 槽函数：处理停止按钮的点击事件。
 * 
 * 调用 `m_mediaPlayer->stop()` 停止视频播放，并将播放/暂停按钮的图标恢复为"播放"状态。
 */
void VideoPage::stopVideo()
{
    m_mediaPlayer->stop(); // 停止播放
    m_playPauseButton->setIcon(QIcon(":/images/playback.png")); // 更新图标为"播放"
}

/**
 * @brief 槽函数：处理播放进度条的拖动事件 (`sliderMoved`)。
 * @param position 用户通过拖动滑块选择的新的播放位置 (单位：毫秒)。
 * 
 * 当用户拖动进度条滑块时，此函数被调用。它将 QMediaPlayer 的播放位置
 * 设置为用户指定的新位置。
 */
void VideoPage::setVideoPosition(int position)
{
    m_mediaPlayer->setPosition(position); // 设置播放器的播放位置
}

/**
 * @brief 槽函数：处理 QMediaPlayer 播放位置变化事件 (`positionChanged`)。
 * @param position 当前的播放位置 (单位：毫秒)。
 * 
 * 当 QMediaPlayer 的播放位置发生变化时（例如，视频正常播放、跳转或拖动进度条后），
 * 此函数被调用。它会：
 * 1. 更新播放进度条滑块 (`m_positionSlider`) 的当前值。
 * 2. 更新时间显示标签 (`m_durationLabel`)，显示格式为 "当前时间 / 总时长"。
 *    时间的格式 (mm:ss 或 hh:mm:ss) 会根据视频总时长自动调整。
 */
void VideoPage::videoPositionChanged(qint64 position)
{
    // 只有当用户没有拖动滑块时，才更新滑块位置 (避免冲突)
    if (!m_positionSlider->isSliderDown()) {
        m_positionSlider->setValue(position); // 更新进度条滑块的值
    }
    
    // 更新时间显示标签
    qint64 duration = m_mediaPlayer->duration(); // 获取视频总时长
    // 将毫秒转换为QTime对象，方便格式化
    QTime currentTime((position / 3600000) % 60, (position / 60000) % 60, (position / 1000) % 60);
    QTime totalTime((duration / 3600000) % 60, (duration / 60000) % 60, (duration / 1000) % 60);
    
    // 根据总时长选择时间格式
    QString timeFormat = "mm:ss";
    if (duration > 3600000) { // 如果总时长超过1小时 (3,600,000毫秒)
        timeFormat = "hh:mm:ss";
    }
    m_durationLabel->setText(currentTime.toString(timeFormat) + " / " + totalTime.toString(timeFormat));
}

/**
 * @brief 槽函数：处理 QMediaPlayer 视频总时长变化事件 (`durationChanged`)。
 * @param duration 新的视频总时长 (单位：毫秒)。
 * 
 * 当加载新的视频文件或视频元数据解析完成，确定了总时长后，此函数被调用。
 * 它会：
 * 1. 更新播放进度条滑块 (`m_positionSlider`) 的最大值 (范围的上限)。
 * 2. 更新时间显示标签 (`m_durationLabel`)，以反映新的总时长。
 */
void VideoPage::videoDurationChanged(qint64 duration)
{
    m_positionSlider->setRange(0, duration); // 设置进度条的范围为 0 到 总时长
    
    // 更新时间显示标签 (与videoPositionChanged中的逻辑类似，但基于新的总时长)
    qint64 currentPosition = m_mediaPlayer->position(); // 获取当前播放位置
    QTime currentTime((currentPosition / 3600000) % 60, (currentPosition / 60000) % 60, (currentPosition / 1000) % 60);
    QTime totalTime((duration / 3600000) % 60, (duration / 60000) % 60, (duration / 1000) % 60);
    
    QString timeFormat = "mm:ss";
    if (duration > 3600000) {
        timeFormat = "hh:mm:ss";
    }
    m_durationLabel->setText(currentTime.toString(timeFormat) + " / " + totalTime.toString(timeFormat));
}

/**
 * @brief 槽函数：处理视频文件列表项的双击事件 (`itemDoubleClicked`)。
 * @param item 被双击的 QListWidgetItem 对象。
 * 
 * 当用户在同目录视频列表 (`m_videoListWidget`) 中双击一个列表项时，此函数被调用。
 * 它会：
 * 1. 检查 `item` 是否有效。
 * 2. 从被双击的列表项中获取存储的视频文件完整路径 (之前通过 `setData(Qt::UserRole, ...)` 设置)。
 * 3. 停止当前正在播放的视频 (`m_mediaPlayer->stop()`)。
 * 4. 将 QMediaPlayer 的媒体源设置为新选中的视频文件。
 * 5. 开始播放新的视频 (`m_mediaPlayer->play()`)。
 * 6. 更新播放/暂停按钮的图标为"暂停"状态。
 */
void VideoPage::videoItemDoubleClicked(QListWidgetItem *item)
{
    if (!item) return; // 如果item为空指针，则不执行任何操作
    
    // 从列表项的用户数据中获取视频文件的完整路径
    QString filePath = item->data(Qt::UserRole).toString();
    
    if (filePath.isEmpty()) return; // 如果路径为空，则不执行任何操作

    // 停止当前播放
    m_mediaPlayer->stop();
    
    // 设置新的媒体源并开始播放
    m_mediaPlayer->setMedia(QUrl::fromLocalFile(filePath));
    m_mediaPlayer->play();
    m_playPauseButton->setIcon(QIcon(":/images/pause.png")); // 更新图标为"暂停"
}

/**
 * @brief 获取当前正在播放的视频文件所在的目录路径。
 * @return 返回一个 QString，包含当前视频目录的绝对路径。
 *         如果尚未播放过视频或无法确定目录，可能返回空字符串。
 */
QString VideoPage::getCurrentVideoDir() const
{
    return m_currentVideoDir; // 返回存储的当前视频目录路径
}