#ifndef HISTORYPAGE_H
#define HISTORYPAGE_H

#include <QWidget>         // QWidget 基类
#include <QLabel>          // 标签控件
#include <QPushButton>     // 按钮控件
#include <QListWidget>     // 列表控件
#include <QDateTime>       // 日期时间类 (在此文件中未直接使用，但可能被包含的头文件间接依赖或为未来扩展预留)
#include <QTimer>          // 定时器类
#include <QStorageInfo>    // 存储信息类

// 前向声明，避免循环包含头文件问题
class MainWindow;        // 主窗口类
class QListWidgetItem;   // 列表控件项类

/**
 * @brief 历史记录页面类 (HistoryPage)
 * 
 * 该类继承自 QWidget，用于实现视频监控系统的历史记录浏览功能。
 * 主要功能包括：
 * - 显示指定目录下录制的视频文件列表（支持文件夹和MP4文件）。
 * - 提供文件和文件夹的浏览功能，允许用户通过双击导航。
 * - 提供返回上一级目录或返回主页面的功能。
 * - 定期更新并显示存储设备（如TF卡）的容量信息。
 */
class HistoryPage : public QWidget
{
    Q_OBJECT // 宏，用于使类能够使用Qt的信号和槽机制

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针，通常是 MainWindow 实例。默认为 nullptr。
     */
    explicit HistoryPage(MainWindow *parent = nullptr); // explicit 防止隐式类型转换

    /**
     * @brief 初始化历史记录页面的用户界面 (UI)
     * 
     * 创建并配置此页面上的所有UI元素，如布局、标签、按钮、列表等，并连接信号槽。
     */
    void setupUI();

    /**
     * @brief 设置当前文件浏览器显示的视频目录路径。
     * @param dirPath 要设置为当前目录的路径字符串。
     */
    void setCurrentVideoDir(const QString &dirPath);

    /**
     * @brief 获取当前文件浏览器正在显示的视频目录路径。
     * @return 返回一个 QString 对象，包含当前目录的绝对路径。
     */
    QString getCurrentVideoDir() const;

public slots:
    /**
     * @brief 公共槽函数：刷新文件列表。
     * 
     * 当需要更新文件列表显示时（例如，目录更改或外部请求刷新），调用此槽函数。
     * 它会重新扫描当前目录并更新列表控件。
     */
    void refreshFileList();
    
    /**
     * @brief 公共槽函数：处理返回按钮的点击事件。
     * 
     * 根据当前目录状态，实现返回上一级目录或返回到主页面的逻辑。
     */
    void handleBackButton();
    
    /**
     * @brief 公共槽函数：处理刷新按钮的点击事件。
     * 
     * 当用户点击刷新按钮时调用此槽函数，它会重新加载并显示当前目录的内容，
     * 但不改变当前的目录层级。
     */
    void handleRefreshButton();

private slots: // Qt 5 以后 private slots 和普通 private 成员函数在信号槽连接上行为一致，但仍可用于组织代码
    /**
     * @brief 私有槽函数：更新存储容量信息。
     * 
     * 此槽函数由 `m_storageTimer` 定时器触发，用于获取TF卡的当前总容量和可用容量，
     * 并在界面上的 `m_storageInfoLabel` 标签中显示这些信息。
     */
    void updateStorageInfo();

private:
    MainWindow *m_mainWindow;       ///< 指向主窗口的指针，用于页面切换等交互操作。
    
    // UI 组件指针
    QLabel *m_historyLabel;         ///< 显示 "监控历史记录" 标题的标签。
    QListWidget *m_fileListWidget;  ///< 用于显示文件和文件夹列表的控件。
    QPushButton *m_refreshButton;   ///< 刷新文件列表的按钮。
    QPushButton *m_backButton;      ///< 返回上一级或主页的按钮。
    QLabel *m_fileInfoLabel;        ///< 显示当前目录下项目数量或提示信息的标签。
    QLabel *m_storageInfoLabel;     ///< 显示TF卡存储容量信息的标签。
    QTimer *m_storageTimer;         ///< 定时器，用于定期调用 `updateStorageInfo()` 更新存储信息。
    
    QString m_currentVideoDir;      ///< 存储当前文件列表显示的目录的绝对路径。
};

#endif // HISTORYPAGE_H
