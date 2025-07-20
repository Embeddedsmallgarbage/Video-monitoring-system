/**
 * @file v4l2_wrapper.h
 * @brief V4L2 功能封装层的头文件
 * 
 * 此头文件声明了与 Video4Linux2 (V4L2) 摄像头交互的C函数接口。
 * 这些函数封装了V4L2的复杂性，提供了一个更简单的API来初始化摄像头、
 * 开始/停止视频捕获、获取视频帧以及清理资源。
 * 设计为纯C接口，以便于C和C++项目调用。
 */
#ifndef V4L2_WRAPPER_H
#define V4L2_WRAPPER_H

// CPLUSPLUS宏用于条件编译，确保C++代码可以正确链接C语言实现的函数
#ifdef __cplusplus
extern "C" { // "C" linkage specification for C++ compilers
#endif

/**
 * @brief 初始化摄像头设备。
 * 
 * 打开指定的摄像头设备，查询其能力，设置视频格式（分辨率、像素格式、帧率），
 * 并初始化用于视频捕获的内存映射缓冲区。
 * 
 * @param device 字符串，表示摄像头设备文件的路径 (例如 "/dev/video0")。
 * @return 成功时返回0；发生错误时返回-1。
 */
int v4l2_init(const char *device);

/**
 * @brief 开始视频采集流程。
 * 
 * 将所有先前初始化的缓冲区排入驱动程序的队列，并启动视频流。
 * 此函数必须在 `v4l2_init` 成功调用之后才能调用。
 * 
 * @return 成功时返回0；发生错误时返回-1。
 */
int v4l2_start_capture();

/**
 * @brief 停止视频采集流程。
 * 
 * 停止视频流。此函数之后，`v4l2_get_frame` 将不再返回新的数据。
 */
void v4l2_stop_capture();

/**
 * @brief 获取一帧RGB888格式的图像数据。
 * 
 * 从摄像头捕获流中取出一帧数据，该数据通常是摄像头原始格式（例如RGB565或YUYV），
 * 然后将其转换为RGB888格式，并存入用户提供的缓冲区。
 * 
 * @param data 指向用户分配的缓冲区的指针，用于存储转换后的RGB888图像数据。
 *             调用者必须确保此缓冲区足够大以容纳一帧图像 (宽度 * 高度 * 3字节)。
 * @param width 指向int类型变量的指针，函数将通过此指针返回捕获图像的实际宽度 (像素)。
 * @param height 指向int类型变量的指针，函数将通过此指针返回捕获图像的实际高度 (像素)。
 * @return 成功获取并转换一帧数据时返回0；
 *         如果暂时没有数据可用 (例如，在非阻塞模式下)，或发生错误时返回-1。
 */
int v4l2_get_frame(unsigned char *data, int *width, int *height);

/**
 * @brief 清理所有已分配的V4L2相关资源。
 * 
 * 包括停止视频流（如果正在运行），解除所有缓冲区的内存映射，
 * 并关闭打开的摄像头设备文件描述符。
 * 此函数应在程序不再需要使用摄像头时调用，以避免资源泄漏。
 */
void v4l2_cleanup();

#ifdef __cplusplus
} // extern "C"
#endif

#endif // V4L2_WRAPPER_H