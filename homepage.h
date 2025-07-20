#ifndef HOMEPAGE_H
#define HOMEPAGE_H

#include <QWidget>      // QWidget 基类，所有UI元素的父类
#include <QLabel>       // 用于显示文本或图像的标签控件
#include <QPushButton>  // 命令按钮控件
#include <QTimer>       // 定时器类，用于周期性事件

// 前向声明 MainWindow 类，以避免循环包含头文件的问题
// HomePage 类需要知道 MainWindow 的存在，以便进行交互（例如页面切换）
class MainWindow;

/**
 * @brief 首页类 (HomePage)
 * 
 * 该类继承自 QWidget，是视频监控系统的主要入口界面 (首页)。
 * 它负责展示系统标题、实时更新并显示当前日期时间，
 * 并提供导航到其他主要功能模块（如实时监控、历史记录）的按钮。
 */
class HomePage : public QWidget
{
    Q_OBJECT // Qt元对象系统宏，使得类可以使用信号和槽机制

public:
    /**
     * @brief 构造函数
     * @param parent 父窗口指针，通常是 MainWindow 的实例。默认为 nullptr。
     *               `explicit` 关键字防止构造函数的隐式类型转换。
     */
    explicit HomePage(MainWindow *parent = nullptr);

    /**
     * @brief 初始化首页的用户界面 (UI)。
     *
     * 此方法负责创建、配置和布局首页上的所有视觉元素，
     * 例如标题标签、日期时间标签以及导航按钮，并连接必要的信号和槽。
     */
    void setupUI();

public slots:
    /**
     * @brief 公共槽函数：更新并显示当前的日期和时间。
     *
     * 此槽函数通常由一个 QTimer 周期性触发（例如每秒一次），
     * 以确保界面上显示的日期时间信息保持最新。
     */
    void updateDateTime();

private:
    MainWindow *m_mainWindow;      ///< 指向主窗口 (MainWindow) 实例的指针。
                                   ///< 用于在点击首页按钮时，请求主窗口切换到其他页面。
    
    // 首页 UI 组件的指针成员变量
    QLabel *m_title;               ///< 用于显示 "视频监控系统" 标题的标签。
    QLabel *m_dateTimeLabel;       ///< 用于显示当前日期和时间的标签。
    QPushButton *m_monitorButton;  ///< "实时监控" 按钮，点击后切换到监控页面。
    QPushButton *m_historyButton;  ///< "历史记录" 按钮，点击后切换到历史记录页面。
    
    // 定时器成员变量
    QTimer *m_dateTimeTimer;       ///< QTimer 对象，用于定期触发 `updateDateTime()` 槽函数，
                                   ///< 以实现日期时间标签的实时更新。
};

#endif // HOMEPAGE_H