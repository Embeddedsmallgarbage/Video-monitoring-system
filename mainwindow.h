#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>     // QMainWindow 基类，提供标准主窗口功能
#include <QStackedWidget>  // QStackedWidget 类，用于管理层叠的页面
#include <QString>         // QString 类，用于处理字符串
#include <QFileInfo>       // QFileInfo 类，用于获取文件信息
#include <QDir>            // QDir 类，用于目录操作

// 前向声明 (Forward Declarations)
// 用于声明类名，使得可以在不知道这些类的完整定义的情况下使用它们的指针或引用。
// 这有助于减少编译依赖，避免头文件之间的循环包含问题。
class HomePage;          // 首页类
class MonitorPage;       // 监控页面类
class HistoryPage;       // 历史记录页面类
class VideoPage;         // 视频播放页面类
class QListWidgetItem;   // QListWidget的列表项类 (虽然在此头文件中MainWindow不直接使用，但可能子页面会使用，或为未来扩展)

// Qt Designer 生成的 UI 类在 Ui 命名空间中
QT_BEGIN_NAMESPACE  // 确保在 Qt 自己的命名空间宏内定义
namespace Ui { class MainWindow; } // Ui::MainWindow 是由 mainwindow.ui 文件通过 uic 工具生成的类
QT_END_NAMESPACE

/**
 * @brief 主窗口类 (MainWindow)
 * 
 * 该类继承自 QMainWindow，是整个视频监控应用程序的核心窗口。
 * 它负责管理和协调各个功能子页面 (HomePage, MonitorPage, HistoryPage, VideoPage)，
 * 并提供在这些页面之间进行切换的接口和逻辑。
 * 主要功能包括：
 * - 作为应用程序的主框架，承载所有其他UI组件。
 * - 使用 QStackedWidget 实现多页面管理和切换。
 * - 提供槽函数以响应来自各页面的导航请求。
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT // Qt元对象系统宏，用于支持信号、槽以及其他Qt特性

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口部件指针。对于主窗口，通常为 nullptr。
     *               `explicit` 关键字防止构造函数的隐式类型转换。
     */
    explicit MainWindow(QWidget *parent = nullptr);
    
    /**
     * @brief 析构函数
     *
     * 负责清理在 MainWindow 对象生命周期内分配的资源，特别是 `ui` 对象。
     * 其他由 Qt 父子对象机制管理的子部件会被自动销毁。
     */
    ~MainWindow();

public slots:
    // 页面切换相关的公共槽函数
    // 这些槽函数可以被其他类（如各个子页面）的信号连接，以请求主窗口切换到指定的页面。
    void showHomePage();      ///< 切换并显示首页。
    void showMonitorPage();   ///< 切换并显示实时监控页面。
    void showHistoryPage();   ///< 切换并显示历史记录页面。
    void showVideoPage(const QString &filePath);  ///< 切换并显示视频播放页面，并播放指定路径的视频文件。
    void returnFromVideoPage(); ///< 从视频播放页面返回到历史记录页面。

private:
    Ui::MainWindow *ui; ///< 指向由Qt Designer生成的UI类的实例。
                        ///< 通过它可以访问在 `.ui` 文件中定义的控件。
    
    // 堆叠部件和各个功能页面的指针成员变量
    QStackedWidget *m_stackedWidget; ///< QStackedWidget实例，用于管理和显示不同的功能页面。
    HomePage *m_homePage;            ///< 指向首页 (HomePage) 实例的指针。
    MonitorPage *m_monitorPage;      ///< 指向监控页面 (MonitorPage) 实例的指针。
    HistoryPage *m_historyPage;      ///< 指向历史记录页面 (HistoryPage) 实例的指针。
    VideoPage *m_videoPage;          ///< 指向视频播放页面 (VideoPage) 实例的指针。
    
    // 状态变量
    QString m_currentVideoDir;       ///< 存储当前正在播放或最后播放的视频文件所在的目录的绝对路径。
                                     ///< 用于在从视频播放页返回历史记录页时，能够恢复到正确的目录上下文。
};

#endif // MAINWINDOW_H
