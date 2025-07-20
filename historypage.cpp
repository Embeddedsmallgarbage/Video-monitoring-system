/**
 * @file historypage.cpp
 * @brief 历史记录页面类的实现文件
 * 
 * 本文件实现了视频监控系统的历史记录功能，包括：
 * - 显示指定目录下录制的视频文件列表（支持文件夹和MP4文件）。
 * - 提供文件和文件夹的浏览功能，双击文件夹可进入，双击MP4文件可播放。
 * - 提供返回上一级目录或返回首页的功能。
 * - 显示当前目录下的项目数量。
 * - 定期更新并显示TF卡的存储容量信息（总容量和可用容量）。
 */

#include "historypage.h"
#include "mainwindow.h"

#include <QVBoxLayout>   // 垂直布局类
#include <QHBoxLayout>   // 水平布局类
#include <QDir>          // 目录操作类
#include <QFileInfo>     // 文件信息类
#include <QIcon>         // 图标类
#include <QStorageInfo>  // 新增：用于获取存储设备信息

/**
 * @brief 历史记录页面类的构造函数
 * @param parent 父窗口指针，默认为nullptr
 * 
 * 初始化成员变量，调用 setupUI() 构建界面，并启动一个定时器用于周期性更新存储信息。
 */
HistoryPage::HistoryPage(MainWindow *parent)
    : QWidget(parent)                                 // 调用父类QWidget的构造函数
    , m_mainWindow(parent)                            // 初始化主窗口指针
    , m_historyLabel(nullptr)                         // 初始化历史记录标签为空指针
    , m_fileListWidget(nullptr)                       // 初始化文件列表控件为空指针
    , m_refreshButton(nullptr)                        // 初始化刷新按钮为空指针
    , m_backButton(nullptr)                           // 初始化返回按钮为空指针
    , m_fileInfoLabel(nullptr)                        // 初始化文件信息标签为空指针
    , m_storageInfoLabel(nullptr)                     // 初始化存储信息标签为空指针
    , m_storageTimer(nullptr)                         // 初始化存储信息更新定时器为空指针
    , m_currentVideoDir("")                           // 初始化当前视频目录为空字符串
{
    setupUI();  // 调用函数初始化用户界面
    
    // 创建并启动存储信息更新定时器
    m_storageTimer = new QTimer(this);                                              // 创建QTimer对象，用于定时更新存储信息
    connect(m_storageTimer, &QTimer::timeout, this, &HistoryPage::updateStorageInfo); // 连接定时器的timeout信号到updateStorageInfo槽函数
    m_storageTimer->start(5000);                                                    // 启动定时器，设置间隔为5000毫秒（5秒）
    
    // 初始更新存储信息
    updateStorageInfo();                                                            // 构造函数执行时立即调用一次，以显示初始存储信息
}

/**
 * @brief 初始化历史记录页面界面
 * 
 * 该函数负责创建和设置历史记录页面的所有UI组件，包括布局、标签、按钮和列表控件，
 * 并连接相关的信号与槽。
 */
void HistoryPage::setupUI()
{
    // 创建历史记录页面主垂直布局
    QVBoxLayout *historyLayout = new QVBoxLayout(this); // `this` 指定当前窗口为父窗口
    
    // 创建顶部水平布局，用于放置刷新按钮、标题和返回按钮
    QHBoxLayout *topLayout = new QHBoxLayout();
    
    // 创建刷新按钮（位于顶部布局左侧）
    m_refreshButton = new QPushButton();                                  // 创建QPushButton对象
    m_refreshButton->setIcon(QIcon(":/images/refresh.png"));              // 设置按钮图标
    m_refreshButton->setIconSize(QSize(32, 32));                          // 设置图标大小
    m_refreshButton->setToolTip("刷新文件列表");                           // 设置鼠标悬停提示信息
    m_refreshButton->setObjectName("m_refreshButton");                    // 设置对象名称，用于样式表选择
    m_refreshButton->setFlat(true);                                       // 设置按钮为扁平样式，无边框凸起
    
    // 创建标题标签（位于顶部布局中间）
    m_historyLabel = new QLabel("监控历史记录");                           // 创建QLabel对象并设置文本
    m_historyLabel->setObjectName("historyLabel");                        // 设置对象名称
    m_historyLabel->setAlignment(Qt::AlignCenter);                        // 设置文本居中对齐
    
    // 创建返回按钮（位于顶部布局右侧）
    m_backButton = new QPushButton();                                     // 创建QPushButton对象
    m_backButton->setIcon(QIcon(":/images/back.png"));                    // 设置按钮图标
    m_backButton->setIconSize(QSize(32, 32));                             // 设置图标大小
    m_backButton->setToolTip("返回首页");                                  // 设置鼠标悬停提示信息
    m_backButton->setObjectName("m_backButton");                          // 设置对象名称
    m_backButton->setFlat(true);                                          // 设置按钮为扁平样式
    
    // 将刷新按钮、标题标签和返回按钮添加到顶部布局
    topLayout->addWidget(m_refreshButton, 0, Qt::AlignLeft);              // 添加刷新按钮，左对齐，拉伸因子为0
    topLayout->addWidget(m_historyLabel, 1, Qt::AlignCenter);             // 添加标题标签，居中对齐，拉伸因子为1（占据更多空间）
    topLayout->addWidget(m_backButton, 0, Qt::AlignRight);                // 添加返回按钮，右对齐，拉伸因子为0
    
    // 创建文件列表控件
    m_fileListWidget = new QListWidget();                                 // 创建QListWidget对象
    m_fileListWidget->setSelectionMode(QAbstractItemView::SingleSelection); // 设置选择模式为单选(一次只能选择列表中的一个项目)
    
    // 设置文件列表项的图标大小
    QSize itemSize = m_fileListWidget->iconSize();                        // 获取默认图标大小
    m_fileListWidget->setIconSize(itemSize * 12);                         // 将图标尺寸增大为原来的12倍，使图标更清晰
    
    // 项目高度已在style.qss中通过 `QListWidget::item { min-height: 40px; }` 
    // 或 `#m_fileListWidget QListWidget::item { height: 50px; }` 进行设置
    
    // 设置文件列表项的字体大小
    QFont font = m_fileListWidget->font();                                // 获取当前字体
    font.setPointSize(26);                                              // 设置字体大小为26磅
    m_fileListWidget->setFont(font);                                      // 应用新字体
    
    // 连接文件列表项双击信号到Lambda表达式，处理文件/文件夹打开逻辑
    connect(m_fileListWidget, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *item) {
        if (!item) return; // 如果item为空指针，则直接返回
        
        // 获取存储在列表项中的文件/文件夹完整路径
        QString filePath = item->data(Qt::UserRole).toString(); // UserRole用于存储自定义数据
        QFileInfo fileInfo(filePath);                           // 创建QFileInfo对象以获取文件信息
        
        // 判断双击的是文件夹还是文件
        if (fileInfo.isDir()) { // 如果是文件夹
            // 保存当前文件夹路径
            QString currentPath = fileInfo.absoluteFilePath(); // 获取文件夹的绝对路径
            
            // 更新当前视频目录变量，记录用户进入的文件夹
            m_currentVideoDir = currentPath;
            
            // 清空文件列表控件中的所有项，准备显示新文件夹内容
            m_fileListWidget->clear();
            
            // 创建QDir对象，用于操作指定路径的文件夹
            QDir dir(currentPath);
            
            // 如果当前路径不是TF卡根目录，则添加 "..." 项用于返回上一级目录
            if (currentPath != "/mnt/TFcard") {
                // 创建 "..." 列表项，并设置文件夹图标
                QListWidgetItem *parentItem = new QListWidgetItem(QIcon(":/images/folder.png"), "...");
                // 将上一级目录的路径存储到UserRole中
                parentItem->setData(Qt::UserRole, dir.absolutePath() + "/.."); 
                m_fileListWidget->addItem(parentItem); // 将 "..." 项添加到列表控件
            }
            
            // 获取目录中的所有文件和文件夹条目列表
            // QDir::AllEntries: 包括文件、目录、符号链接等
            // QDir::NoDotAndDotDot: 排除 "." 和 ".." 条目
            // QDir::DirsFirst: 文件夹优先显示
            // QDir::Name: 按名称排序
            QFileInfoList entryList = dir.entryInfoList(QDir::AllEntries | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
            
            // 遍历获取到的所有条目，并添加到文件列表控件中
            for (const QFileInfo &entryInfo : entryList) {
                QString entryName = entryInfo.fileName(); // 获取文件名或文件夹名
                QIcon icon;                               // 用于存储条目图标
                
                if (entryInfo.isDir()) { // 如果是文件夹
                    entryName += "/";    // 在文件夹名称后添加 "/" 以示区分
                    icon = QIcon(":/images/folder.png"); // 设置文件夹图标
                } else if (entryInfo.suffix().toLower() == "mp4") { // 如果是MP4文件（忽略后缀大小写）
                    icon = QIcon(":/images/mp4.png");   // 设置MP4文件图标
                }
                // else { continue; } // 如果需要只显示文件夹和MP4文件，可以取消此行注释

                // 创建新的列表项，并设置图标和名称
                QListWidgetItem *newItem = new QListWidgetItem(icon, entryName);
                // 将文件/文件夹的绝对路径存储到UserRole中
                newItem->setData(Qt::UserRole, entryInfo.absoluteFilePath());
                m_fileListWidget->addItem(newItem); // 添加到列表控件
            }
            
            // 更新底部文件信息标签，显示当前目录下的项目总数
            m_fileInfoLabel->setText(QString("共找到 %1 个项目").arg(entryList.size()));
            m_fileInfoLabel->setAlignment(Qt::AlignCenter); // 文本居中对齐
        }
        // 如果双击的是MP4文件，则调用主窗口的showVideoPage方法播放视频
        else if (fileInfo.suffix().toLower() == "mp4") {
            m_mainWindow->showVideoPage(filePath); // 通知主窗口切换到视频播放页面并播放该文件
        }
    });
    
    // 创建文件信息标签（位于底部布局左侧）
    m_fileInfoLabel = new QLabel("请选择一个文件查看详细信息"); // 创建QLabel对象并设置初始文本
    m_fileInfoLabel->setObjectName("m_fileInfoLabel");    // 设置对象名称
    m_fileInfoLabel->setAlignment(Qt::AlignLeft);         // 文本左对齐
    m_fileInfoLabel->setWordWrap(true);                   // 允许文本自动换行
    
    // 创建存储容量信息标签（位于底部布局右侧）
    m_storageInfoLabel = new QLabel();                    // 创建QLabel对象
    m_storageInfoLabel->setObjectName("m_storageInfoLabel"); // 设置对象名称
    m_storageInfoLabel->setAlignment(Qt::AlignRight);     // 文本右对齐
    QFont storageFont = m_storageInfoLabel->font();       // 获取当前字体
    storageFont.setPointSize(16);                       // 设置字体大小为16磅
    m_storageInfoLabel->setFont(storageFont);             // 应用新字体
    
    // 创建底部水平布局，用于放置文件信息标签和存储信息标签
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    bottomLayout->addWidget(m_fileInfoLabel);             // 添加文件信息标签
    bottomLayout->addWidget(m_storageInfoLabel);          // 添加存储信息标签
    
    // 将顶部布局、文件列表控件和底部布局添加到主垂直布局中
    historyLayout->addLayout(topLayout);                  // 添加顶部布局
    historyLayout->addWidget(m_fileListWidget);           // 添加文件列表控件
    historyLayout->addLayout(bottomLayout);               // 添加底部布局
    
    // 连接各个按钮的点击信号到相应的槽函数
    connect(m_backButton, &QPushButton::clicked, this, &HistoryPage::handleBackButton);         // 返回按钮点击事件
    connect(m_refreshButton, &QPushButton::clicked, this, &HistoryPage::handleRefreshButton);   // 刷新按钮点击事件
    connect(m_refreshButton, &QPushButton::clicked, this, &HistoryPage::updateStorageInfo);     // 刷新按钮点击时也更新存储信息
}

/**
 * @brief 设置当前视频目录的路径
 * @param dirPath 要设置的目录路径字符串
 * 
 * 此函数用于外部（如从视频播放页面返回时）设置历史记录页面应显示的目录。
 */
void HistoryPage::setCurrentVideoDir(const QString &dirPath)
{
    m_currentVideoDir = dirPath; // 将传入的目录路径赋值给成员变量m_currentVideoDir
}

/**
 * @brief 获取当前视频目录的路径
 * @return 返回存储当前视频目录路径的QString对象
 * 
 * 此函数用于外部获取当前历史记录页面正在显示的目录路径。
 */
QString HistoryPage::getCurrentVideoDir() const
{
    return m_currentVideoDir; // 返回成员变量m_currentVideoDir的值
}

/**
 * @brief 刷新历史记录页面的文件列表
 * 
 * 该函数首先清空现有的文件列表，然后根据`m_currentVideoDir`重新扫描目录，
 * 获取文件和文件夹信息，并更新到`m_fileListWidget`中。
 * 如果`m_currentVideoDir`为空，则默认使用"/mnt/TFcard"作为根目录。
 * 同时，如果当前目录不是根目录，会自动添加一个返回上级目录的 "..." 项。
 */
void HistoryPage::refreshFileList()
{
    // 清空文件列表控件中的所有项
    m_fileListWidget->clear();
    
    // 如果当前视频目录路径为空，则默认设置为TF卡的根目录
    if (m_currentVideoDir.isEmpty()) {
        m_currentVideoDir = "/mnt/TFcard";
    }
    
    // 创建QDir对象，用于操作当前视频目录
    QDir tfDir(m_currentVideoDir);
    
    // 检查指定的目录是否存在
    if (!tfDir.exists()) {
        // 如果目录不存在，则在文件信息标签处显示错误信息，并返回
        m_fileInfoLabel->setText(QString("错误: 无法访问%1目录").arg(m_currentVideoDir));
        return;
    }
    
    // 获取目录中的所有文件和文件夹条目列表
    // QDir::Files: 包含文件
    // QDir::Dirs: 包含目录
    // QDir::NoDotAndDotDot: 排除 "." 和 ".." 条目
    // QDir::DirsFirst: 文件夹优先显示
    // QDir::Name: 按名称排序
    QFileInfoList fileList = tfDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot, QDir::DirsFirst | QDir::Name);
    
    // 检查获取到的文件列表是否为空
    if (fileList.isEmpty()) {
        // 如果列表为空，则在文件信息标签处显示目录中没有文件，并返回
        m_fileInfoLabel->setText(QString("%1目录中没有文件").arg(m_currentVideoDir));
        return;
    }
    
    // 如果当前目录不是TF卡的根目录，则添加一个返回上级目录的列表项 "..."
    if (m_currentVideoDir != "/mnt/TFcard") {
        QListWidgetItem *parentItem = new QListWidgetItem(QIcon(":/images/folder.png"), "..."); // 创建 "..." 项，并设置文件夹图标
        parentItem->setData(Qt::UserRole, tfDir.absolutePath() + "/..");                      // 将上一级目录的路径存储到UserRole中
        m_fileListWidget->addItem(parentItem);                                                 // 添加到列表控件
    }
    
    // 遍历获取到的所有文件和文件夹条目
    for (const QFileInfo &fileInfo : fileList) {
        QString displayName = fileInfo.fileName(); // 获取文件名或文件夹名
        QIcon icon;                                // 用于存储条目图标
        
        if (fileInfo.isDir()) { // 如果是文件夹
            displayName += "/"; // 在文件夹名称后添加 "/" 以示区分
            icon = QIcon(":/images/folder.png"); // 设置文件夹图标
        } else if (fileInfo.suffix().toLower() == "mp4") { // 如果是MP4文件（忽略后缀大小写）
            icon = QIcon(":/images/mp4.png");   // 设置MP4文件图标
        }
        // else { continue; } // 如果需要只显示文件夹和MP4，可以取消此行注释

        // 创建新的列表项，并设置图标和名称
        QListWidgetItem *item = new QListWidgetItem(icon, displayName);
        item->setData(Qt::UserRole, fileInfo.absoluteFilePath()); // 将文件/文件夹的绝对路径存储到UserRole中
        m_fileListWidget->addItem(item);                          // 添加到列表控件
    }
    
    // 更新底部文件信息标签，显示当前目录下的项目总数
    m_fileInfoLabel->setText(QString("共找到 %1 个项目").arg(fileList.size()));
    m_fileInfoLabel->setAlignment(Qt::AlignCenter); // 文本居中对齐
}



/**
 * @brief 处理返回按钮的点击事件
 * 
 * 此函数根据当前目录状态执行不同操作：
 * 1. 如果`m_currentVideoDir`为空，则将其设置为TF卡根目录并刷新列表。
 * 2. 如果当前目录已经是TF卡根目录("/mnt/TFcard")，则调用主窗口的`showHomePage()`方法返回首页。
 * 3. 否则，将当前目录更改为上一级目录，并刷新文件列表。
 *    特殊处理：如果返回到"/mnt"目录，则自动设置为"/mnt/TFcard"。
 */
void HistoryPage::handleBackButton()
{
    // 检查当前视频目录是否为空（通常是首次进入或未选择任何目录时）
    if (m_currentVideoDir.isEmpty()) {
        // 如果目录为空，将其设置为TF卡的根目录
        m_currentVideoDir = "/mnt/TFcard";
        refreshFileList(); // 刷新文件列表以显示根目录内容
        return;            // 处理完毕，返回
    }
    
    // 检查当前目录是否已经是TF卡的根目录
    if (m_currentVideoDir == "/mnt/TFcard") {
        // 如果是根目录，则调用主窗口的方法切换到首页
        m_mainWindow->showHomePage();
        return; // 处理完毕，返回
    }
    
    // 如果当前目录不是根目录，则尝试返回上一级目录
    QDir dir(m_currentVideoDir); // 创建QDir对象操作当前目录
    dir.cdUp();                  // 切换到上一级目录
    m_currentVideoDir = dir.absolutePath(); // 更新当前视频目录为上一级目录的绝对路径
    
    // 特殊处理：如果返回到了"/mnt"目录（TF卡的父目录），则将其规范化为TF卡根目录路径
    if (m_currentVideoDir == "/mnt") {
        m_currentVideoDir = "/mnt/TFcard";
    }
    
    // 刷新文件列表，以显示上一级目录的内容
    refreshFileList();
}

/**
 * @brief 处理刷新按钮的点击事件
 * 
 * 此函数用于刷新当前目录的文件列表，它不改变当前的目录层级。
 * 如果当前目录为空，则默认设置为TF卡根目录。
 */
void HistoryPage::handleRefreshButton()
{
    // 确保当前视频目录不为空，如果为空，则设置为TF卡根目录
    if (m_currentVideoDir.isEmpty()) {
        m_currentVideoDir = "/mnt/TFcard";
    }
    
    // 调用refreshFileList()方法刷新当前目录的文件列表
    refreshFileList();
}

/**
 * @brief 更新存储容量信息
 * 
 * 此函数尝试获取"/mnt/TFcard"路径下的存储设备信息。
 * 如果设备有效且就绪，则计算总容量和可用容量（转换为MB单位），
 * 并将格式化后的信息更新到`m_storageInfoLabel`标签上。
 * 如果设备未就绪或无效，则显示"存储设备未就绪"。
 */
void HistoryPage::updateStorageInfo()
{
    // 创建QStorageInfo对象，用于获取指定路径的存储信息（这里是TF卡挂载点）
    QStorageInfo storage("/mnt/TFcard");
    
    // 检查存储设备是否有效且已准备好（例如，是否已挂载）
    if (!storage.isValid() || !storage.isReady()) {
        m_storageInfoLabel->setText("存储设备未就绪"); // 如果无效或未就绪，显示提示信息
        return;                                      // 并返回
    }
    
    // 计算总存储空间和可用存储空间（单位：MB）
    // storage.bytesTotal() 返回总字节数
    // storage.bytesAvailable() 返回可用字节数
    // 除以 (1024.0 * 1024.0) 将字节转换为MB（使用浮点数以保证精度）
    double totalSpace = storage.bytesTotal() / (1024.0 * 1024.0);
    double availableSpace = storage.bytesAvailable() / (1024.0 * 1024.0);
    
    // 格式化要显示的文本字符串
    // QString::arg() 用于格式化字符串，类似printf
    // `%1` 和 `%2` 是占位符
    // `availableSpace, 0, 'f', 1` 表示将availableSpace格式化为浮点数('f')，小数点后保留1位，总宽度不限(0)
    QString storageText = QString("可用: %1 MB / 总容量: %2 MB").arg(availableSpace, 0, 'f', 1).arg(totalSpace, 0, 'f', 1);
    
    // 更新存储信息标签的文本
    m_storageInfoLabel->setText(storageText);
}