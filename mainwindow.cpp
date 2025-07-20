/**
 * @file mainwindow.cpp
 * @brief 主窗口类 (MainWindow) 的实现文件。
 * 
 * 本文件负责实现视频监控系统的主窗口逻辑。
 * 主要功能包括：
 * - 初始化应用程序的主界面，包括加载全局样式表、设置窗口标题和大小。
 * - 创建并管理一个 `QStackedWidget`，用于在不同的功能页面之间进行切换。
 * - 实例化各个功能页面：首页 (`HomePage`)、监控页面 (`MonitorPage`)、
 *   历史记录页面 (`HistoryPage`) 和视频播放页面 (`VideoPage`)。
 * - 提供公共槽函数来响应页面切换的请求，例如从首页导航到监控页或历史页，
 *   从历史页导航到视频播放页，以及从视频播放页返回历史页。
 * - 在页面切换时执行必要的逻辑，如开始/停止视频采集、刷新文件列表等。
 */

#include "mainwindow.h"       // MainWindow类的头文件
#include "ui_mainwindow.h"    // 由Qt uic根据mainwindow.ui生成的界面类头文件
#include "homepage.h"         // 首页类头文件
#include "monitorpage.h"      // 监控页面类头文件
#include "historypage.h"      // 历史记录页面类头文件
#include "videopage.h"        // 视频播放页面类头文件

#include <QFile>             // QFile类，用于文件操作（如此处用于读取样式表）
#include <QStackedWidget>    // QStackedWidget类，用于管理多个页面层叠显示 (在.h中已包含，此处为清晰可省略)
#include <QFileInfo>         // QFileInfo类，用于获取文件信息 (在.h中已包含，此处为清晰可省略)
#include <QDir>              // QDir类，用于目录操作 (在.h中已包含，此处为清晰可省略)

/**
 * @brief MainWindow 类的构造函数。
 * @param parent 父窗口部件指针，对于主窗口通常为 nullptr。
 * 
 * 在构造函数中，主要执行以下操作：
 * 1. 调用父类 `QMainWindow` 的构造函数。
 * 2. 初始化 `ui` 成员，它是由 `mainwindow.ui` 文件通过 `uic` 工具生成的界面类的实例。
 * 3. 调用 `ui->setupUi(this)` 来设置由 Qt Designer 设计的UI。
 * 4. 加载并应用全局样式表 `style.qss`。
 * 5. 设置主窗口的标题和初始大小。
 * 6. 创建一个 `QStackedWidget` 作为中心部件，用于管理和切换不同的功能页面。
 * 7. 实例化所有功能页面 (`HomePage`, `MonitorPage`, `HistoryPage`, `VideoPage`)，并将它们添加到 `QStackedWidget` 中。
 * 8. 默认显示首页 (`HomePage`)。
 */
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)               // 调用父类 QMainWindow 的构造函数
    , ui(new Ui::MainWindow)            // 创建 Ui::MainWindow 的实例，用于访问 .ui 文件中定义的控件
    , m_homePage(nullptr)               // 初始化首页指针为空
    , m_monitorPage(nullptr)            // 初始化监控页面指针为空
    , m_historyPage(nullptr)            // 初始化历史记录页面指针为空
    , m_videoPage(nullptr)              // 初始化视频播放页面指针为空
    , m_stackedWidget(nullptr)          // 初始化堆叠窗口部件指针为空 (在.h中未声明，应在此处或.h中添加)
    , m_currentVideoDir("")             // 初始化当前视频目录为空字符串 (在.h中声明了)
{
    ui->setupUi(this); // 初始化通过Qt Designer创建的UI元素
    
    // 加载并应用QSS样式表
    QFile styleFile(":/style.qss"); // 创建QFile对象，指向资源文件中的style.qss
    if (styleFile.open(QFile::ReadOnly)) { //尝试以只读方式打开样式表文件
        QString styleSheet = QLatin1String(styleFile.readAll()); // 读取文件全部内容为字符串
        setStyleSheet(styleSheet);                             // 将读取到的样式表应用到当前窗口及其子部件
        styleFile.close();                                     // 关闭文件
    }
    // else { qDebug() << "无法加载样式表: style.qss"; } // 可选：添加加载失败的调试信息
    
    // 设置窗口的基本属性
    setWindowTitle("视频监控系统");      // 设置主窗口标题
    resize(800, 600);                  // 设置主窗口的初始尺寸为800x600像素
    
    // 创建并设置中心堆叠部件 QStackedWidget，用于管理多个页面
    m_stackedWidget = new QStackedWidget(this); // 创建QStackedWidget实例，this作为其父对象
    setCentralWidget(m_stackedWidget);          // 将m_stackedWidget设置为主窗口的中心部件
    
    // 创建各个功能页面的实例
    m_homePage = new HomePage(this);            // 创建首页实例，this作为其父对象
    m_monitorPage = new MonitorPage(this);      // 创建监控页面实例
    m_historyPage = new HistoryPage(this);      // 创建历史记录页面实例
    m_videoPage = new VideoPage(this);          // 创建视频播放页面实例
    
    // 将创建的各个页面添加到堆叠部件中
    // addWidget()会返回页面的索引，但这里我们不需要使用它
    m_stackedWidget->addWidget(m_homePage);     // 添加首页
    m_stackedWidget->addWidget(m_monitorPage);  // 添加监控页面
    m_stackedWidget->addWidget(m_historyPage);  // 添加历史记录页面
    m_stackedWidget->addWidget(m_videoPage);    // 添加视频播放页面
    
    // 初始化时，默认显示首页
    m_stackedWidget->setCurrentWidget(m_homePage);
}

/**
 * @brief MainWindow 类的析构函数。
 * 
 * 负责释放在构造函数中通过 `new` 分配的 `ui` 对象。
 * Qt的父子对象机制会自动处理其他通过 `new` 创建并指定了父对象的 `QWidget` 派生类
 * (如 `m_homePage`, `m_monitorPage` 等) 的释放。
 */
MainWindow::~MainWindow()
{
    delete ui; // 释放由Qt Designer生成的ui类实例所占用的内存
               // m_homePage, m_monitorPage等由于设置了this作为parent，会被Qt自动管理和释放
}

/**
 * @brief 公共槽函数：切换到并显示首页。
 * 
 * 当需要返回或显示首页时调用此函数。
 * 如果当前页面是监控页面 (`m_monitorPage`)，则在切换前会先调用 `m_monitorPage->stopCapture()`
 * 来停止视频采集，以释放摄像头资源并停止相关处理。
 */
void MainWindow::showHomePage()
{
    // 检查当前显示的页面是否是监控页面
    if (m_stackedWidget->currentWidget() == m_monitorPage) {
        // 如果是监控页面，则调用其 stopCapture 方法停止视频采集
        m_monitorPage->stopCapture();
    }
    
    // 将堆叠部件的当前显示页面设置为首页
    m_stackedWidget->setCurrentWidget(m_homePage);
}

/**
 * @brief 公共槽函数：切换到并显示监控页面。
 * 
 * 当需要显示实时监控画面时调用此函数。
 * 切换到监控页面后，会尝试调用 `m_monitorPage->startCapture()` 来开始视频采集。
 * 如果视频采集启动失败（例如，摄像头无法打开），则会自动切回首页。
 */
void MainWindow::showMonitorPage()
{
    // 将堆叠部件的当前显示页面设置为监控页面
    m_stackedWidget->setCurrentWidget(m_monitorPage);
    
    // 尝试启动监控页面的视频采集功能
    if (!m_monitorPage->startCapture()) {
        // 如果启动视频采集失败 (例如，摄像头未连接或权限问题)
        // 则调用 showHomePage() 方法，将界面切换回首页
        showHomePage();
    }
}

/**
 * @brief 公共槽函数：切换到并显示历史记录页面。
 * 
 * 当需要浏览历史录像文件时调用此函数。
 * 切换到历史记录页面后，会调用 `m_historyPage->refreshFileList()` 来刷新文件列表，
 * 以显示最新的录像文件和目录结构。
 */
void MainWindow::showHistoryPage()
{
    // 将堆叠部件的当前显示页面设置为历史记录页面
    m_stackedWidget->setCurrentWidget(m_historyPage);
    
    // 调用历史记录页面的 refreshFileList 方法，刷新其文件列表显示
    m_historyPage->refreshFileList();
}

/**
 * @brief 公共槽函数：切换到并显示视频播放页面。
 * @param filePath 要播放的视频文件的完整路径。
 * 
 * 当用户从历史记录页面选择一个视频文件进行播放时调用此函数。
 * 1. 根据传入的 `filePath` 获取视频文件所在的目录路径，并保存到 `m_currentVideoDir`。
 * 2. 将此目录路径设置到历史记录页面 (`m_historyPage`)，以便从视频播放页返回时能恢复到正确的目录。
 * 3. 切换到视频播放页面 (`m_videoPage`)。
 * 4. 调用 `m_videoPage->playVideo(filePath)` 开始播放指定的视频文件。
 */
void MainWindow::showVideoPage(const QString &filePath)
{
    // 使用QFileInfo获取传入文件路径的目录信息
    QFileInfo fileInfo(filePath);
    m_currentVideoDir = fileInfo.dir().absolutePath(); // 保存视频文件所在目录的绝对路径
    
    // 将当前视频的目录路径传递给历史记录页面，
    // 这样当从视频播放页返回历史记录页时，历史记录页可以知道之前所在的目录
    m_historyPage->setCurrentVideoDir(m_currentVideoDir);
    
    // 将堆叠部件的当前显示页面设置为视频播放页面
    m_stackedWidget->setCurrentWidget(m_videoPage);
    
    // 调用视频播放页面的 playVideo 方法，传入文件路径以开始播放
    m_videoPage->playVideo(filePath);
}

/**
 * @brief 公共槽函数：从视频播放页面返回到历史记录页面。
 * 
 * 当用户在视频播放页面点击返回按钮时调用此函数。
 * 1. 从视频播放页面 (`m_videoPage`) 获取其当前显示的视频所在的目录路径。
 * 2. 切换回历史记录页面 (`m_historyPage`)。
 * 3. 如果获取到的目录路径不为空，则将此路径设置到历史记录页面，并刷新其文件列表，
 *    以确保历史记录页面显示的是之前视频文件所在的目录内容。
 */
void MainWindow::returnFromVideoPage()
{
    // 从视频播放页面获取当前视频文件所在的目录路径
    // 这是为了确保返回历史记录页面时，能正确显示之前浏览的文件夹内容
    QString videoDir = m_videoPage->getCurrentVideoDir();
    
    // 将堆叠部件的当前显示页面设置为历史记录页面
    m_stackedWidget->setCurrentWidget(m_historyPage);
    
    // 检查从视频播放页面获取的目录路径是否有效（不为空）
    if (!videoDir.isEmpty()) {
        // 如果路径有效，则将历史记录页面的当前目录设置为该路径
        m_historyPage->setCurrentVideoDir(videoDir);
        // 并刷新历史记录页面的文件列表
        m_historyPage->refreshFileList();
    }
    // 如果 videoDir 为空，历史记录页面将根据其内部逻辑（可能显示默认目录或上次的目录）刷新
    // 或者，也可以在此处添加逻辑，如果videoDir为空，则让m_historyPage显示一个默认目录，例如：
    // else { m_historyPage->setCurrentVideoDir("/mnt/TFcard"); m_historyPage->refreshFileList(); }
}