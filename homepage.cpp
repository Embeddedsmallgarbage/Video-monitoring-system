/**
 * @file homepage.cpp
 * @brief 首页类的实现文件
 * 
 * 本文件实现了视频监控系统的首页用户界面和基本功能。
 * 主要功能包括：
 * - 显示系统标题 "视频监控系统"。
 * - 实时显示当前的日期和时间，每秒更新一次。
 * - 提供两个主要功能按钮："实时监控" 和 "历史记录"。
 * - 点击按钮会分别切换到监控页面和历史记录页面。
 */

#include "homepage.h"
#include "mainwindow.h"

#include <QVBoxLayout>   // 垂直布局类
#include <QDateTime>     // 日期时间类
#include <QLabel>        // 标签控件 (新增包含，因为m_title和m_dateTimeLabel是QLabel类型)
#include <QPushButton>   // 按钮控件 (新增包含，因为m_monitorButton和m_historyButton是QPushButton类型)
#include <QTimer>        // 定时器类 (新增包含，因为m_dateTimeTimer是QTimer类型)

/**
 * @brief 首页类 (HomePage) 的构造函数。
 * @param parent 父窗口指针，通常是 MainWindow 实例。
 * 
 * 初始化成员变量，并调用 `setupUI()` 方法来构建和设置首页的用户界面。
 */
HomePage::HomePage(MainWindow *parent)
    : QWidget(parent)               // 调用父类QWidget的构造函数，并传递parent
    , m_mainWindow(parent)          // 初始化主窗口指针
    , m_title(nullptr)              // 初始化标题标签指针为空
    , m_dateTimeLabel(nullptr)      // 初始化日期时间标签指针为空
    , m_monitorButton(nullptr)      // 初始化监控按钮指针为空
    , m_historyButton(nullptr)      // 初始化历史记录按钮指针为空
    , m_dateTimeTimer(nullptr)      // 初始化日期时间更新定时器指针为空
{
    setupUI(); // 调用成员函数来设置用户界面
}

/**
 * @brief 初始化首页的用户界面 (UI)。
 * 
 * 该函数负责创建和配置首页的所有UI元素，包括：
 * - 一个垂直布局 (`QVBoxLayout`) 作为主布局。
 * - 一个标题标签 (`m_title`) 显示 "视频监控系统"。
 * - 一个日期时间标签 (`m_dateTimeLabel`) 用于显示实时时间。
 * - 两个功能按钮 (`m_monitorButton` 和 `m_historyButton`) 分别用于进入实时监控和历史记录页面。
 * - 一个定时器 (`m_dateTimeTimer`) 用于每秒更新日期时间显示。
 * 同时，它还将按钮的点击信号连接到主窗口的相应槽函数，以实现页面切换。
 */
void HomePage::setupUI()
{
    // 创建主垂直布局，并将当前 HomePage 实例作为其父对象
    QVBoxLayout *homeLayout = new QVBoxLayout(this);
    
    // 创建日期时间标签
    m_dateTimeLabel = new QLabel();                             // 实例化 QLabel 对象
    m_dateTimeLabel->setObjectName("m_dateTimeLabel");        // 设置对象名称，用于 QSS 样式表中选择和应用样式
    m_dateTimeLabel->setAlignment(Qt::AlignCenter);           // 设置标签文本居中对齐

    // 创建系统标题标签
    m_title = new QLabel();                                     // 实例化 QLabel 对象
    m_title->setObjectName("title");                          // 设置对象名称，用于 QSS
    m_title->setText("视频监控系统");                           // 设置标签显示的文本内容
    m_title->setAlignment(Qt::AlignCenter);                   // 设置标签文本居中对齐
    
    // 创建功能按钮
    m_monitorButton = new QPushButton("实时监控");              // 实例化 QPushButton 对象，并设置按钮文本为 "实时监控"
    m_historyButton = new QPushButton("历史记录");              // 实例化 QPushButton 对象，并设置按钮文本为 "历史记录"
    m_monitorButton->setObjectName("m_monitorButton");        // 设置监控按钮的对象名称
    m_historyButton->setObjectName("m_historyButton");        // 设置历史记录按钮的对象名称
    
    // 将创建的UI组件按顺序添加到垂直布局中
    homeLayout->addStretch(1);                                // 在顶部添加一个可伸缩的空间，参数 1 表示拉伸因子
    homeLayout->addWidget(m_title, 0, Qt::AlignCenter);       // 添加标题标签，不参与拉伸 (因子0)，并居中对齐
    homeLayout->addSpacing(20);                               // 在标题和日期时间标签之间添加 20 像素的固定间距
    homeLayout->addWidget(m_dateTimeLabel, 0, Qt::AlignCenter); // 添加日期时间标签，不参与拉伸，并居中对齐
    homeLayout->addStretch(1);                                // 在日期时间标签和监控按钮之间添加一个可伸缩的空间
    homeLayout->addWidget(m_monitorButton, 0, Qt::AlignCenter); // 添加监控按钮，不参与拉伸，并居中对齐
    homeLayout->addSpacing(20);                               // 在监控按钮和历史记录按钮之间添加 20 像素的固定间距
    homeLayout->addWidget(m_historyButton, 0, Qt::AlignCenter); // 添加历史记录按钮，不参与拉伸，并居中对齐
    homeLayout->addStretch(1);                                // 在底部添加一个可伸缩的空间
    
    // 设置主布局的边距 (上, 下, 左, 右) 都为 50 像素
    homeLayout->setContentsMargins(50, 50, 50, 50);
    
    // 连接按钮的 clicked 信号到 MainWindow 中相应的槽函数，以实现页面切换
    // 当 m_monitorButton 被点击时，会调用 m_mainWindow 的 showMonitorPage 方法
    connect(m_monitorButton, &QPushButton::clicked, m_mainWindow, &MainWindow::showMonitorPage);
    // 当 m_historyButton 被点击时，会调用 m_mainWindow 的 showHistoryPage 方法
    connect(m_historyButton, &QPushButton::clicked, m_mainWindow, &MainWindow::showHistoryPage);
    
    // 创建并配置用于更新日期时间的定时器
    m_dateTimeTimer = new QTimer(this);                         // 实例化 QTimer 对象，this 作为其父对象
    // 连接定时器的 timeout 信号到 HomePage 的 updateDateTime 槽函数
    // 每当定时器超时（即达到设定的时间间隔），就会调用 updateDateTime 方法
    connect(m_dateTimeTimer, &QTimer::timeout, this, &HomePage::updateDateTime);
    m_dateTimeTimer->start(1000);                             // 启动定时器，设置时间间隔为 1000 毫秒 (1秒)
    
    // 界面初始化完成后，立即调用一次 updateDateTime，以显示初始的日期和时间
    updateDateTime();
}

/**
 * @brief 更新并显示系统当前的日期和时间。
 * 
 * 此方法被 `m_dateTimeTimer` 定时器每秒调用一次。
 * 它获取当前的系统日期和时间，将其格式化为 "yyyy年MM月dd日 hh:mm:ss" 的字符串，
 * 然后更新 `m_dateTimeLabel` 标签的文本。
 */
void HomePage::updateDateTime()
{
    // 获取当前系统的日期和时间
    QDateTime currentDateTime = QDateTime::currentDateTime();
    // 将日期时间对象格式化为指定的字符串格式
    // yyyy: 四位年份, MM: 两位月份, dd: 两位日期
    // hh: 两位小时 (24小时制), mm: 两位分钟, ss: 两位秒
    QString dateTimeText = currentDateTime.toString("yyyy年MM月dd日 hh:mm:ss");
    // 将格式化后的日期时间字符串设置到 m_dateTimeLabel 标签上进行显示
    m_dateTimeLabel->setText(dateTimeText);
}