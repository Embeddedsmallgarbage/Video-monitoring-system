#include "mainwindow.h" // 包含主窗口类的头文件

#include <QApplication>    // Qt应用程序核心类

/**
 * @brief 主函数 (main)
 * @param argc 命令行参数个数
 * @param argv 命令行参数数组
 * @return 应用程序退出状态码
 * 
 * 这是Qt应用程序的入口点。
 * 它执行以下操作：
 * 1. 创建一个 QApplication 对象 `a`，它是管理GUI应用程序控制流和主要设置所必需的。
 * 2. 创建一个 MainWindow 对象 `w`，这是应用程序的主窗口。
 * 3. 调用 `w.show()` 来显示主窗口。
 * 4. 调用 `a.exec()` 进入Qt事件循环，应用程序将在此处等待和处理用户交互及其他事件，
 *    直到应用程序退出（例如，关闭主窗口）。
 * 5. `a.exec()` 的返回值将作为程序的退出状态码。
 */
int main(int argc, char *argv[])
{
    // 1. 创建 QApplication 对象
    // QApplication 管理着整个应用程序的事件循环和全局设置。
    // argc 和 argv 是从命令行传递给应用程序的参数。
    QApplication a(argc, argv);
    
    // 2. 创建 MainWindow 对象
    // MainWindow 是我们应用程序的主窗口，定义在 "mainwindow.h" 中。
    MainWindow w;
    
    // 3. 显示主窗口
    //调用 show() 方法使主窗口在屏幕上可见。
    w.show();
    
    // 4. 进入 Qt 事件循环并返回退出状态码
    // a.exec() 启动了 Qt 的事件处理机制。程序会在这里暂停，
    // 响应用户的操作（如点击按钮、窗口移动等）以及其他系统事件。
    // 当最后一个窗口关闭或调用 QApplication::quit() 时，事件循环结束，
    // exec() 方法会返回一个状态码。
    return a.exec();
}
