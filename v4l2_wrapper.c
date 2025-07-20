/**
 * @file v4l2_wrapper.c
 * @brief V4L2 功能封装层实现文件
 * 
 * 本文件提供了对 Video4Linux2 (V4L2) API 的一层简单封装，
 * 用于简化摄像头视频数据的捕获流程。
 * 主要功能包括：
 * - 初始化摄像头设备 (打开设备、查询能力、设置格式、请求并映射缓冲区)。
 * - 开始和停止视频流的捕获。
 * - 获取捕获到的视频帧 (通常需要进行格式转换，例如从RGB565到RGB888)。
 * - 清理和释放相关资源。
 */
#include "v4l2_wrapper.h"

#include <stdio.h>      // 标准输入输出库，用于 printf, fprintf, perror 等
#include <stdlib.h>     // 标准库，用于 exit, malloc, free 等 (在此文件中未使用明显函数)
#include <sys/types.h>  // 基本系统数据类型，例如用于 open, stat
#include <sys/stat.h>   // 文件状态信息，用于 open
#include <fcntl.h>      // 文件控制选项，用于 open
#include <unistd.h>     // POSIX 操作系统API，用于 close, read, write, ioctl 等
#include <sys/ioctl.h>  // I/O 控制操作，V4L2核心交互接口
#include <string.h>     // 字符串操作函数，例如 strcpy, strerror
#include <errno.h>      // 系统错误号定义，用于 perror, strerror
#include <sys/mman.h>   // 内存管理声明，用于 mmap, munmap
#include <linux/videodev2.h> // V4L2核心头文件，定义了所有V4L2相关的结构体、宏和常量

#define FRAMEBUFFER_COUNT   3               // 定义帧缓冲区的数量。通常设置3-5个以保证流畅性。

// 摄像头像素格式及其描述信息
/**
 * @brief 描述摄像头支持的像素格式。
 * 
 * 用于存储通过 VIDIOC_ENUM_FMT ioctl 调用枚举到的摄像头像素格式信息。
 */
typedef struct camera_format {
    unsigned char description[32];  /**< 格式的文本描述 (例如 "RGB565", "YUYV 4:2:2")。 */
    unsigned int pixelformat;       /**< V4L2定义的像素格式四字符码 (FourCC) (例如 V4L2_PIX_FMT_RGB565)。 */
} cam_fmt;

// 描述一个帧缓冲的信息
/**
 * @brief 描述一个内存映射的帧缓冲区。
 * 
 * 用于存储通过 mmap 映射到用户空间的内核帧缓冲区的相关信息。
 */
typedef struct cam_buf_info {
    unsigned short *start;      /**< 指向映射到用户空间的帧缓冲区的起始地址。类型假设为RGB565，故为unsigned short*。 */
    unsigned long length;       /**< 帧缓冲区的长度 (字节数)。 */
} cam_buf_info;

static int v4l2_fd = -1;                // 摄像头设备文件描述符。初始化为-1表示未打开。
static cam_buf_info buf_infos[FRAMEBUFFER_COUNT]; // 存储所有帧缓冲区信息的数组。
static cam_fmt cam_fmts[10];            // 存储摄像头支持的像素格式的数组 (假设最多10种)。
static int frm_width, frm_height;       // 存储实际设置的视频帧的宽度和高度 (像素)。
static int is_capturing = 0;            // 标志位，指示当前是否正在进行视频采集 (0: 未采集, 1: 正在采集)。

// RGB565转RGB888
/**
 * @brief 将RGB565格式的像素数据转换为RGB888格式。
 * 
 * RGB565每个像素用16位表示 (R:5位, G:6位, B:5位)。
 * RGB888每个像素用24位表示 (R:8位, G:8位, B:8位)。
 * 此函数负责将输入的RGB565数据逐像素转换为RGB888格式并存入目标缓冲区。
 * 
 * @param src 指向源RGB565数据缓冲区的指针。
 * @param dst 指向目标RGB888数据缓冲区的指针 (调用者需确保空间足够，大小为 pixels * 3 字节)。
 * @param pixels 要转换的像素数量。
 */
static void rgb565_to_rgb888(unsigned short *src, unsigned char *dst, int pixels)
{
    int i;                          // 循环计数器
    unsigned short rgb565_pixel;    // 存储当前处理的RGB565像素值
    unsigned char r, g, b;          // 存储从RGB565提取出的R, G, B分量 (5或6位)
    
    for (i = 0; i < pixels; i++) {  // 遍历所有像素
        rgb565_pixel = src[i];      // 获取当前RGB565像素
        
        // 从RGB565提取RGB分量
        r = (rgb565_pixel >> 11) & 0x1F;        // 提取高5位作为R分量 (0x1F = 00011111b)
        g = (rgb565_pixel >> 5) & 0x3F;         // 提取中间6位作为G分量 (0x3F = 00111111b)
        b = rgb565_pixel & 0x1F;                // 提取低5位作为B分量 (0x1F = 00011111b)
        
        // 转换为RGB888 (通过位移和补位将5/6位分量扩展到8位)
        // 一种常见的扩展方法是将高位复制到低位，以保持色彩的相对强度
        // R分量: (r << 3) | (r >> 2)  => rrrrrrrr (将5位rrrrr扩展为rrrrr rrr)
        // G分量: (g << 2) | (g >> 4)  => gggggggg (将6位gggggg扩展为gggggg gg)
        // B分量: (b << 3) | (b >> 2)  => bbbbbbbb (将5位bbbbb扩展为bbbbb bbb)
        dst[i*3 + 0] = (r << 3) | (r >> 2);  // R分量 (占3字节中的第1个字节)
        dst[i*3 + 1] = (g << 2) | (g >> 4);  // G分量 (占3字节中的第2个字节)
        dst[i*3 + 2] = (b << 3) | (b >> 2);  // B分量 (占3字节中的第3个字节)
    }
}

// 枚举摄像头支持的格式
/**
 * @brief 使用 VIDIOC_ENUM_FMT ioctl 调用枚举摄像头支持的所有像素格式。
 * 
 * 将查询到的格式信息（描述字符串和像素格式ID）存储在全局的 `cam_fmts` 数组中。
 * 此函数通常在摄像头初始化时调用，以了解设备能力。
 */
static void v4l2_enum_formats(void)
{
    struct v4l2_fmtdesc fmtdesc = {0}; // V4L2格式描述结构体，用于与ioctl交互

    // 枚举摄像头所支持的所有像素格式以及描述信息
    fmtdesc.index = 0;  // 从索引0开始枚举
    fmtdesc.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // 指定要枚举的是视频捕获类型的格式
    
    // 循环调用 VIDIOC_ENUM_FMT，直到ioctl返回错误 (通常表示没有更多格式)
    while (0 == ioctl(v4l2_fd, VIDIOC_ENUM_FMT, &fmtdesc)) {
        // 成功获取到一个格式信息
        if (fmtdesc.index < (sizeof(cam_fmts) / sizeof(cam_fmts[0]))) { // 检查数组边界
            // 将枚举出来的格式以及描述信息存放在全局数组 `cam_fmts` 中
            cam_fmts[fmtdesc.index].pixelformat = fmtdesc.pixelformat; // 存储像素格式ID
            strcpy((char*)cam_fmts[fmtdesc.index].description, (char*)fmtdesc.description); // 存储格式描述
        } else {
            // 如果支持的格式太多，超出数组大小，则停止枚举并打印警告
            fprintf(stderr, "Warning: Too many formats supported by camera, some were not stored.\n");
            break;
        }
        fmtdesc.index++; // 准备枚举下一个格式
    }
    // ioctl失败（非0返回值）通常意味着枚举结束或发生错误 (如EINVAL表示索引超出范围)
}

// 设置视频格式
/**
 * @brief 设置摄像头的视频捕获格式 (宽度、高度、像素格式) 和帧率。
 * 
 * 此函数尝试将摄像头配置为期望的参数（例如640x480, RGB565, 30fps）。
 * 它会：
 * 1. 设置宽度、高度和像素格式 (VIDIOC_S_FMT)。
 * 2. 验证设备是否成功接受了请求的像素格式。
 * 3. 获取实际应用的宽度和高度，并存储在全局变量 `frm_width`, `frm_height` 中。
 * 4. 尝试设置帧率 (VIDIOC_S_PARM)。
 * 
 * @return 成功返回0，失败返回-1。
 */
static int v4l2_set_format(void)
{
    struct v4l2_format fmt = {0};            // V4L2视频格式结构体，用于 VIDIOC_S_FMT 和 VIDIOC_G_FMT
    struct v4l2_streamparm streamparm = {0}; // V4L2流参数结构体，用于配置帧率等

    // 1. 设置帧格式
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // 指定为视频捕获类型
    // 设置期望的宽度、高度和像素格式
    // 在此示例中，硬编码为 640x480 RGB565。实际应用中可能需要更灵活的配置。
    fmt.fmt.pix.width       = 640;  // 期望的视频帧宽度 (像素)
    fmt.fmt.pix.height      = 480; // 期望的视频帧高度 (像素)
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_RGB565;  // 期望的像素格式 (RGB565)
    // 其他字段 (如 field, bytesperline, sizeimage) 通常由驱动程序在S_FMT调用后填充或根据需要设置
    // fmt.fmt.pix.field       = V4L2_FIELD_INTERLACED; // 如果是隔行扫描，需要设置

    // 通过 ioctl VIDIOC_S_FMT 应用设置
    if (0 > ioctl(v4l2_fd, VIDIOC_S_FMT, &fmt)) {
        fprintf(stderr, "ioctl error: VIDIOC_S_FMT: %s\n", strerror(errno));
        return -1; // 设置格式失败
    }

    // 2. 验证像素格式
    // 驱动可能不会完全接受请求的格式，而是选择一个最接近的。因此需要检查实际应用的格式。
    // 在此示例中，我们严格要求RGB565。如果驱动不支持或改成了其他格式，则报错。
    if (V4L2_PIX_FMT_RGB565 != fmt.fmt.pix.pixelformat) {
        fprintf(stderr, "Error: the device does not support RGB565 format or it was changed by driver! Actual format: %c%c%c%c\n",
                fmt.fmt.pix.pixelformat & 0xFF, (fmt.fmt.pix.pixelformat >> 8) & 0xFF,
                (fmt.fmt.pix.pixelformat >> 16) & 0xFF, (fmt.fmt.pix.pixelformat >> 24) & 0xFF);
        return -1; // 像素格式不匹配
    }

    // 3. 获取并存储实际的帧宽度和高度
    // 即使像素格式匹配，宽度和高度也可能被驱动调整。
    frm_width = fmt.fmt.pix.width;  // 从驱动返回的 `fmt` 结构中获取实际应用的宽度
    frm_height = fmt.fmt.pix.height;// 获取实际应用的高度
    printf("V4L2: Actual video frame size set to <%d x %d>\n", frm_width, frm_height);
    // fmt.fmt.pix.bytesperline 和 fmt.fmt.pix.sizeimage 也会被驱动填充，可用于缓冲区大小计算

    // 4. 设置帧率 (可选，但推荐)
    // 首先获取当前的流参数
    streamparm.type = V4L2_BUF_TYPE_VIDEO_CAPTURE; // 指定为视频捕获类型
    if (0 > ioctl(v4l2_fd, VIDIOC_G_PARM, &streamparm)) {
        // 获取流参数失败，可能不严重，可以继续，但帧率可能不是期望的
        fprintf(stderr, "Warning: ioctl VIDIOC_G_PARM failed: %s. Frame rate might not be configurable.\n", strerror(errno));
    } else {
        // 检查设备是否支持通过 timeperframe 设置帧率
        if (V4L2_CAP_TIMEPERFRAME & streamparm.parm.capture.capability) {
            streamparm.parm.capture.timeperframe.numerator = 1;      // 帧间隔的分子 (1)
            streamparm.parm.capture.timeperframe.denominator = 30;   // 帧间隔的分母 (30)，即期望30 FPS
            // 通过 ioctl VIDIOC_S_PARM 应用设置
            if (0 > ioctl(v4l2_fd, VIDIOC_S_PARM, &streamparm)) {
                fprintf(stderr, "ioctl error: VIDIOC_S_PARM to set frame rate: %s\n", strerror(errno));
                // 设置帧率失败，不一定是致命错误，可以继续，但帧率可能不是30fps
            } else {
                 printf("V4L2: Attempted to set frame rate to %d/%d FPS.\n", 
                        streamparm.parm.capture.timeperframe.denominator, 
                        streamparm.parm.capture.timeperframe.numerator);
            }
        } else {
            // 设备不支持通过 timeperframe 设置帧率，可能支持其他方式或帧率固定
            printf("V4L2: Device does not support setting frame rate via timeperframe.\n");
        }
    }

    return 0; // 格式设置成功（或主要部分成功）
}

// 初始化缓冲区
/**
 * @brief 请求并映射V4L2视频捕获缓冲区。
 * 
 * 此函数执行以下步骤：
 * 1. 使用 VIDIOC_REQBUFS 请求指定数量的缓冲区 (FRAMEBUFFER_COUNT)。
 *    这些缓冲区由驱动在内核空间分配，用于存储捕获到的视频帧。
 * 2. 对每一个请求到的缓冲区：
 *    a. 使用 VIDIOC_QUERYBUF 查询该缓冲区的元数据 (如长度、在设备内存中的偏移量)。
 *    b. 使用 mmap 将该内核缓冲区映射到用户进程的地址空间，
 *       使得用户程序可以直接访问捕获到的视频数据，而无需数据拷贝。
 * 3. 将映射后的缓冲区的起始地址和长度存储在全局的 `buf_infos` 数组中。
 * 
 * @note 内存映射 (V4L2_MEMORY_MMAP) 是V4L2中高效获取视频数据的常用方式。
 * 
 * @return 成功返回0，失败返回-1。
 */
static int v4l2_init_buffer(void)
{
    struct v4l2_requestbuffers reqbuf = {0}; // V4L2缓冲区请求结构体
    struct v4l2_buffer buf_query = {0};       // V4L2单个缓冲区信息结构体，用于 VIDIOC_QUERYBUF
    int i; // 循环计数器
    
    // 1. 请求帧缓冲
    reqbuf.count = FRAMEBUFFER_COUNT;           // 请求的缓冲区数量
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;  // 缓冲区类型为视频捕获
    reqbuf.memory = V4L2_MEMORY_MMAP;           // 指定内存模式为内存映射 (MMAP)
    
    if (0 > ioctl(v4l2_fd, VIDIOC_REQBUFS, &reqbuf)) {
        fprintf(stderr, "ioctl error: VIDIOC_REQBUFS: %s\n", strerror(errno));
        return -1; // 请求缓冲区失败
    }

    // 驱动可能分配少于请求数量的缓冲区，检查实际分配的数量
    if (reqbuf.count < FRAMEBUFFER_COUNT) {
        // 如果驱动分配的缓冲区数量少于期望，可能无法正常工作或性能下降
        fprintf(stderr, "Warning: VIDIOC_REQBUFS allocated fewer buffers (%d) than requested (%d).\n", 
                reqbuf.count, FRAMEBUFFER_COUNT);
        // 根据实际需求，这里可以决定是失败退出还是继续 (如果允许少于期望的缓冲区)
        // 为简单起见，如果严格要求FRAMEBUFFER_COUNT个，则应该返回错误
        // if (reqbuf.count < 2) { // 例如，至少需要2个才能正常工作
        //    fprintf(stderr, "Error: Not enough buffers allocated by driver.\n");
        //    return -1;
        // }
    }
    // 注意：实际项目中，应使用 reqbuf.count 作为后续循环的上限，而不是 FRAMEBUFFER_COUNT
    // 但为保持与原代码逻辑一致（使用全局FRAMEBUFFER_COUNT），这里不做修改，假设驱动总能满足请求。
    // 更好的做法是：调整buf_infos数组大小或在循环中检查索引。
    
    // 2. 建立内存映射
    // 为每个已分配的缓冲区获取信息并进行内存映射
    for (i = 0; i < FRAMEBUFFER_COUNT; i++) { // 理想情况下应为 for (i = 0; i < reqbuf.count; i++)
        // 准备查询缓冲区信息
        buf_query.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
        buf_query.memory = V4L2_MEMORY_MMAP;
        buf_query.index = i; // 要查询的缓冲区的索引
        
        // 使用 VIDIOC_QUERYBUF 获取缓冲区的物理地址信息 (主要是偏移量和长度)
        if (0 > ioctl(v4l2_fd, VIDIOC_QUERYBUF, &buf_query)) {
            fprintf(stderr, "ioctl error: VIDIOC_QUERYBUF for buffer %d: %s\n", i, strerror(errno));
            // 如果一个缓冲区查询失败，可能需要回滚已映射的缓冲区并返回错误
            // 为简单起见，这里直接返回错误
            // TODO: 添加清理逻辑，munmap已映射的缓冲区
            return -1;
        }
        
        // 记录缓冲区长度
        buf_infos[i].length = buf_query.length;
        
        // 将内核缓冲区映射到用户空间
        // mmap参数：
        //   NULL: 由内核选择映射地址
        //   buf_query.length: 映射区域的长度
        //   PROT_READ | PROT_WRITE: 映射区域可读可写
        //   MAP_SHARED: 对映射区域的修改对其他映射同一文件的进程可见，并且会写回到底层文件 (对设备文件意味着写回设备)
        //   v4l2_fd: 设备文件描述符
        //   buf_query.m.offset: 缓冲区在设备内存中的偏移量 (从VIDIOC_QUERYBUF获得)
        buf_infos[i].start = (unsigned short*)mmap(NULL, buf_query.length,
                                                PROT_READ | PROT_WRITE, MAP_SHARED,
                                                v4l2_fd, buf_query.m.offset);
        if (MAP_FAILED == buf_infos[i].start) {
            perror("mmap error");
            // mmap失败，可能需要回滚其他已映射的缓冲区并返回错误
            // TODO: 添加清理逻辑，munmap已映射的缓冲区
            return -1;
        }
        printf("V4L2: Buffer %d mapped at %p, length %lu\n", i, buf_infos[i].start, buf_infos[i].length);
    }
    
    return 0; // 所有缓冲区初始化并映射成功
}

// 开启视频流
/**
 * @brief 将所有已映射的缓冲区加入驱动的输入队列并启动视频流。
 * 
 * 此函数执行以下步骤：
 * 1. 对于每一个映射的缓冲区：
 *    a. 使用 VIDIOC_QBUF 将其"排队"到驱动程序的输入队列中。
 *       这意味着告诉驱动这个缓冲区现在可用于填充捕获到的视频数据。
 * 2. 使用 VIDIOC_STREAMON 启动视频捕获过程。
 *    一旦启动，摄像头开始采集数据，并将数据填充到已排队的缓冲区中。
 * 3. 设置全局标志 `is_capturing` 为1。
 * 
 * @return 成功返回0，失败返回-1。如果失败，会尝试清理资源并返回。
 */
static int v4l2_stream_on(void)
{
    struct v4l2_buffer qbuf = {0}; // V4L2缓冲区结构体，用于 VIDIOC_QBUF
    int i;                        // 循环计数器
    int ret = 0;                  // 函数返回值

    // 1. 将所有缓冲区加入队列
    qbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    qbuf.memory = V4L2_MEMORY_MMAP;
    
    for (i = 0; i < FRAMEBUFFER_COUNT; i++) { // 理想情况: for (i = 0; i < reqbuf.count from v4l2_init_buffer; i++)
        qbuf.index = i; // 要排队的缓冲区的索引
        if (0 > ioctl(v4l2_fd, VIDIOC_QBUF, &qbuf)) {
            fprintf(stderr, "ioctl error: VIDIOC_QBUF for buffer %d: %s\n", i, strerror(errno));
            ret = -1; // 标记失败
            goto cleanup_stream_on; // 跳转到清理逻辑
        }
    }

    // 2. 启动视频流
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (0 > ioctl(v4l2_fd, VIDIOC_STREAMON, &type)) {
        fprintf(stderr, "ioctl error: VIDIOC_STREAMON: %s\n", strerror(errno));
        ret = -1; // 标记失败
        goto cleanup_stream_on; // 跳转到清理逻辑 (虽然此时已排队的缓冲区可能无法轻易取消)
    }

    is_capturing = 1; // 设置采集标志为真
    printf("V4L2: Stream started successfully.\n");
    return 0; // 启动成功

cleanup_stream_on:
    // 异常处理：如果启动流的过程中发生错误
    if (ret < 0) {
        // 理论上，如果STREAMON失败，已经QBUF的缓冲区可能需要特殊处理，
        // 但V4L2规范中，通常在STREAMON失败后，STREAMOFF然后关闭设备是标准做法。
        // 如果STREAMON成功前QBUF失败，那么munmap和close是必要的。
        // 此处简单地解除映射并关闭文件描述符，与v4l2_cleanup的逻辑部分重叠。
        // 更好的错误处理可能需要更细致的状态管理。
        
        // 尝试关闭视频流 (如果已经意外开启)
        // ioctl(v4l2_fd, VIDIOC_STREAMOFF, &type); // 谨慎：如果STREAMON本身失败，此调用也可能失败或不必要
        
        // 解除内存映射 (仅当映射成功且未在v4l2_cleanup中处理时)
        // 此处假设buf_infos是全局有效的，如果v4l2_init_buffer部分失败，这里可能不安全。
        // 为避免与v4l2_cleanup重复且简化，原代码的cleanup更侧重于整体清理。
        // 原代码的cleanup部分在 `main.c` 或调用者处处理，这里保持与原代码风格一致，
        // 即高级函数失败时，由更高级别的清理函数（如v4l2_cleanup）处理资源释放。
        // 因此，这里的`cleanup`标签下的代码在原版中是空的，依赖于外部调用`v4l2_cleanup`。
        // 但为了更健壮，至少应该标记错误，让调用者知道需要清理。
        // 以下是原代码中的清理逻辑，如果在此处执行，需谨慎其副作用。
        /*
        for (int k = 0; k < FRAMEBUFFER_COUNT; k++) {
            if (buf_infos[k].start && buf_infos[k].start != MAP_FAILED) { // 检查是否已映射
                munmap(buf_infos[k].start, buf_infos[k].length);
                buf_infos[k].start = MAP_FAILED; // 标记为已解除映射
            }
        }
        if (v4l2_fd >= 0) {
            close(v4l2_fd);
            v4l2_fd = -1;
        }
        is_capturing = 0;
        */
        // 由于此函数是static的，错误应该向上传播，让公共API的调用者决定如何处理。
        // 原始的 goto cleanup 目标是空的，这暗示着清理责任可能在调用栈的更高层，
        // 或者这是一个未完成的错误处理路径。
        // 鉴于其他函数结构，返回-1后，v4l2_init会关闭fd。
    }
    return ret; // 返回错误码
}

// 关闭视频流
/**
 * @brief 使用 VIDIOC_STREAMOFF 停止视频捕获流。
 * 
 * 如果当前正在捕获 (`is_capturing` 为1)，则向驱动发送停止流的命令，
 * 并将 `is_capturing` 标志置为0。
 * 如果未在捕获，则此函数不执行任何操作。
 */
static void v4l2_stream_off(void)
{
    if (!is_capturing) { // 如果当前未在采集，则直接返回
        return;
    }
    
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if (0 > ioctl(v4l2_fd, VIDIOC_STREAMOFF, &type)) {
        // STREAMOFF 失败通常不常见，但如果发生，打印错误信息。
        // 此时设备状态可能不稳定，后续清理可能也受影响。
        fprintf(stderr, "ioctl error: VIDIOC_STREAMOFF: %s\n", strerror(errno));
    } else {
        printf("V4L2: Stream stopped successfully.\n");
    }
    is_capturing = 0; // 更新采集状态标志
}

// 公开API实现

// 初始化摄像头
/**
 * @brief 初始化V4L2摄像头设备。
 * 
 * 这是V4L2封装层的主要入口点之一。它执行完整的摄像头初始化序列：
 * 1. 打开指定的摄像头设备文件 (例如 "/dev/video0")。
 * 2. 使用 VIDIOC_QUERYCAP 查询设备能力，并验证它是一个视频捕获设备。
 * 3. 调用 `v4l2_enum_formats()` 枚举设备支持的像素格式 (主要用于调试或动态格式选择)。
 * 4. 调用 `v4l2_set_format()` 设置期望的视频格式 (分辨率、像素格式、帧率)。
 * 5. 调用 `v4l2_init_buffer()` 请求并内存映射视频捕获缓冲区。
 * 
 * @param device 摄像头设备文件的路径字符串 (例如 "/dev/video0")。
 * @return 成功返回0，失败返回-1。如果失败，会尝试关闭已打开的设备文件描述符。
 */
int v4l2_init(const char *device)
{
    struct v4l2_capability cap = {0}; // V4L2设备能力结构体

    // 1. 打开摄像头设备
    // O_RDWR: 以读写模式打开。对于V4L2，通常需要读写权限。
    v4l2_fd = open(device, O_RDWR /* | O_NONBLOCK */); // O_NONBLOCK 可用于非阻塞操作，但ioctl通常是阻塞的
    if (0 > v4l2_fd) { // open 返回-1表示失败
        fprintf(stderr, "open error for device %s: %s\n", device, strerror(errno));
        return -1;
    }

    // 2. 查询设备功能 (VIDIOC_QUERYCAP)
    // 此调用获取设备的基本信息，如驱动名称、卡名称、总线信息以及支持的能力。
    if (0 > ioctl(v4l2_fd, VIDIOC_QUERYCAP, &cap)) {
        fprintf(stderr, "ioctl error: VIDIOC_QUERYCAP: %s\n", strerror(errno));
        close(v4l2_fd); // 关闭已打开的文件描述符
        v4l2_fd = -1;   // 重置文件描述符
        return -1;
    }
    printf("V4L2: Device opened: %s (%s)\n", cap.card, cap.driver);

    // 3. 判断是否是视频采集设备
    // 检查 `cap.capabilities` 标志位，确保设备支持视频捕获。
    if (!(V4L2_CAP_VIDEO_CAPTURE & cap.capabilities)) {
        fprintf(stderr, "Error: %s is not a video capture device! Capabilities: 0x%x\n", device, cap.capabilities);
        close(v4l2_fd);
        v4l2_fd = -1;
        return -1;
    }
    // 还可以检查是否支持流式I/O (V4L2_CAP_STREAMING)，这对于MMAP是必需的。
    if (!(V4L2_CAP_STREAMING & cap.capabilities)) {
         fprintf(stderr, "Error: %s does not support streaming I/O (required for MMAP)!\n", device);
         close(v4l2_fd);
         v4l2_fd = -1;
         return -1;
    }

    // (可选) 枚举摄像头支持的格式 (主要用于信息展示或动态选择格式)
    v4l2_enum_formats(); // 内部调用，填充 cam_fmts 数组
    
    // 4. 设置视频格式 (分辨率、像素格式、帧率)
    if (0 > v4l2_set_format()) { // 内部调用，设置 frm_width, frm_height
        // v4l2_set_format 内部已打印错误信息
        close(v4l2_fd);
        v4l2_fd = -1;
        return -1;
    }
    
    // 5. 初始化缓冲区 (请求并映射)
    if (0 > v4l2_init_buffer()) { // 内部调用，填充 buf_infos 数组
        // v4l2_init_buffer 内部已打印错误信息
        // 如果缓冲区初始化失败，需要munmap已经映射的部分 (v4l2_init_buffer应处理部分失败的清理)
        // 然后关闭文件描述符
        close(v4l2_fd);
        v4l2_fd = -1;
        return -1;
    }
    
    printf("V4L2: Initialization successful for %s.\n", device);
    return 0; // 初始化成功
}

// 开始视频采集
/**
 * @brief 开始V4L2视频捕获流。
 * 
 * 此函数是对内部静态函数 `v4l2_stream_on()` 的简单封装，
 * `v4l2_stream_on()` 负责将缓冲区排队并启动流。
 * 
 * @return 成功返回0，失败返回-1。
 */
int v4l2_start_capture()
{
    if (v4l2_fd < 0) { // 检查设备是否已初始化
        fprintf(stderr, "Error: v4l2_start_capture called before successful v4l2_init.\n");
        return -1;
    }
    if (is_capturing) { // 检查是否已在采集
        fprintf(stderr, "Warning: v4l2_start_capture called while already capturing.\n");
        return 0; // 或者返回错误，取决于期望行为
    }
    return v4l2_stream_on();
}

// 停止视频采集
/**
 * @brief 停止V4L2视频捕获流。
 * 
 * 此函数是对内部静态函数 `v4l2_stream_off()` 的简单封装。
 */
void v4l2_stop_capture()
{
    if (v4l2_fd < 0) { // 检查设备是否已初始化
        // 如果未初始化就调用stop，通常不执行任何操作或打印警告
        return;
    }
    v4l2_stream_off();
}

// 获取一帧RGB888格式的图像数据
/**
 * @brief 从V4L2捕获流中获取一帧视频数据，并将其转换为RGB888格式。
 * 
 * 执行流程：
 * 1. 检查是否正在捕获以及输入参数是否有效。
 * 2. 使用 VIDIOC_DQBUF 从驱动的输出队列中取出一个已填充数据的缓冲区。
 *    此调用会阻塞，直到有可用的帧，或者如果以非阻塞模式打开设备且无可用帧，则返回EAGAIN。
 * 3. 如果成功获取到缓冲区 (包含RGB565数据)，调用 `rgb565_to_rgb888()` 将其转换为RGB888格式，
 *    存入用户提供的 `data` 缓冲区中。
 * 4. 将处理完的原始V4L2缓冲区使用 VIDIOC_QBUF 重新排队到驱动的输入队列，使其可被再次使用。
 * 5. 通过指针参数返回图像的实际宽度和高度。
 * 
 * @param data 指向用户分配的缓冲区的指针，用于存储转换后的RGB888图像数据。
 *             调用者必须确保此缓冲区足够大 (frm_width * frm_height * 3 字节)。
 * @param width 输出参数，指向一个int变量，用于接收图像的宽度 (像素)。
 * @param height 输出参数，指向一个int变量，用于接收图像的高度 (像素)。
 * @return 成功获取并转换一帧数据返回0；如果无数据可用 (EAGAIN) 或发生其他错误则返回-1。
 */
int v4l2_get_frame(unsigned char *data, int *width, int *height)
{
    struct v4l2_buffer dqbuf = {0}; // V4L2缓冲区结构体，用于 VIDIOC_DQBUF 和 VIDIOC_QBUF
    
    // 1. 参数检查和状态检查
    if (v4l2_fd < 0 || !is_capturing) { // 检查设备是否初始化并在采集中
        // fprintf(stderr, "Error: v4l2_get_frame called when not capturing or not initialized.\n");
        return -1;
    }
    if (!data || !width || !height) { // 检查输出参数指针是否有效
        fprintf(stderr, "Error: v4l2_get_frame called with NULL output parameters.\n");
        return -1;
    }
    
    // 2. 设置缓冲区类型以准备出队 (DQBUF)
    dqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    dqbuf.memory = V4L2_MEMORY_MMAP;
    
    // 3. 从驱动的输出队列中取出一个已填充数据的缓冲区
    // VIDIOC_DQBUF: Dequeue Buffer. 获取一个已填充数据的缓冲区。
    // 此调用通常是阻塞的，直到一个缓冲区可用。
    if (0 > ioctl(v4l2_fd, VIDIOC_DQBUF, &dqbuf)) {
        if (errno == EAGAIN) { // 如果以非阻塞模式打开，并且当前没有可用的帧
            // fprintf(stderr, "V4L2_GET_FRAME: VIDIOC_DQBUF: No data available (EAGAIN).\n");
            return -1; // 没有数据可用，调用者应稍后重试
        }
        // 其他错误
        fprintf(stderr, "ioctl error: VIDIOC_DQBUF: %s\n", strerror(errno));
        // 发生严重错误，可能需要停止捕获并清理
        // v4l2_stop_capture(); // 或由调用者决定
        return -1;
    }
    // dqbuf.index 现在包含了刚出队的缓冲区的索引
    // dqbuf.bytesused 包含了缓冲区中实际有效数据的字节数
    
    // 检查出队的缓冲区索引是否有效
    if (dqbuf.index >= FRAMEBUFFER_COUNT) { // 理想情况: dqbuf.index >= reqbuf.count
        fprintf(stderr, "Error: VIDIOC_DQBUF returned invalid buffer index %d.\n", dqbuf.index);
        // 这是一个严重错误，需要将缓冲区尝试重新入队（如果状态允许）或停止采集
        // 简单处理：返回错误，后续可能需要手动QBUF这个非法索引的buffer或重启采集
        return -1;
    }

    // 4. 将捕获到的数据 (RGB565) 转换为RGB888格式
    // buf_infos[dqbuf.index].start 是指向映射内存区域的指针 (包含RGB565数据)
    // frm_width * frm_height 是总像素数
    // data 是用户提供的输出缓冲区 (用于RGB888数据)
    if (buf_infos[dqbuf.index].start) { // 确保缓冲区指针有效
        rgb565_to_rgb888(buf_infos[dqbuf.index].start, data, frm_width * frm_height);
    } else {
        fprintf(stderr, "Error: Buffer %d for DQBUF has NULL start pointer.\n", dqbuf.index);
        // 尝试将此（可能损坏的）缓冲区重新入队以避免驱动程序状态不一致
        ioctl(v4l2_fd, VIDIOC_QBUF, &dqbuf); // 即使转换失败，也要尝试归还缓冲区
        return -1;
    }
    
    // 5. 返回图像尺寸 (这些是在v4l2_set_format时确定的)
    *width = frm_width;
    *height = frm_height;
    
    // 6. 将处理完的缓冲区重新放入驱动的输入队列 (QBUF)
    // VIDIOC_QBUF: Queue Buffer. 将缓冲区归还给驱动，使其可被再次用于填充数据。
    // 必须在处理完数据后尽快将缓冲区归还，否则驱动最终会用完所有缓冲区而停止采集。
    // dqbuf 结构体在DQBUF调用后仍然包含正确的index, type, memory等信息，可以直接用于QBUF。
    if (0 > ioctl(v4l2_fd, VIDIOC_QBUF, &dqbuf)) {
        fprintf(stderr, "ioctl error: VIDIOC_QBUF for buffer %d after processing: %s\n", dqbuf.index, strerror(errno));
        // QBUF失败是严重问题，可能导致驱动停止工作或资源泄漏
        // 可能需要停止捕获并进行更彻底的清理
        return -1;
    }
    
    return 0; // 成功获取并处理一帧
}

// 清理资源
/**
 * @brief 清理所有V4L2相关的资源。
 * 
 * 此函数应在程序退出前或不再需要摄像头时调用。
 * 它执行以下操作：
 * 1. 调用 `v4l2_stop_capture()` 停止视频流 (如果正在运行)。
 * 2. 对于每一个映射的缓冲区，调用 `munmap()` 解除其内存映射。
 * 3. 关闭打开的摄像头设备文件描述符 `v4l2_fd`。
 * 4. 重置相关全局变量 (如 `v4l2_fd`, `is_capturing`)。
 */
void v4l2_cleanup()
{
    int i; // 循环计数器

    // 1. 停止视频采集 (如果正在进行)
    // v4l2_stop_capture() 内部会检查 is_capturing 和 v4l2_fd
    v4l2_stop_capture();
    
    // 2. 解除内存映射
    // 只有当设备已成功打开 (v4l2_fd >=0) 且缓冲区已映射时才需要解除映射
    if (v4l2_fd >= 0) { // 隐含了 v4l2_init 可能已成功执行到映射步骤
        for (i = 0; i < FRAMEBUFFER_COUNT; i++) { // 理想情况: for (i = 0; i < reqbuf.count; i++)
            // 检查 buf_infos[i].start 是否有效且非 MAP_FAILED (mmap失败的返回值)
            // MAP_FAILED通常是 (void *)-1，但直接与NULL比较在原代码中是常见的简化
            // 更安全的检查是 buf_infos[i].start && buf_infos[i].start != MAP_FAILED
            if (buf_infos[i].start != NULL && buf_infos[i].start != MAP_FAILED) {
                if (0 > munmap(buf_infos[i].start, buf_infos[i].length)) {
                    // munmap 失败通常不常见，但如果发生，打印错误
                    fprintf(stderr, "munmap error for buffer %d: %s\n", i, strerror(errno));
                }
                buf_infos[i].start = NULL; // 标记为已解除映射 (或 MAP_FAILED)
                buf_infos[i].length = 0;
            }
        }
    }
    
    // 3. 关闭设备文件描述符
    if (v4l2_fd >= 0) { // 检查文件描述符是否有效 (即设备已打开)
        if (0 > close(v4l2_fd)) {
            fprintf(stderr, "close error for V4L2 device: %s\n", strerror(errno));
        }
        v4l2_fd = -1; // 重置文件描述符，表示设备已关闭
    }

    // 重置其他状态变量 (可选，但良好实践)
    is_capturing = 0;
    frm_width = 0;
    frm_height = 0;
    // cam_fmts 和 buf_infos 的内容不需要显式清除，因为它们是静态分配的，
    // 并且在下一次成功的v4l2_init中会被重新填充。

    printf("V4L2: Cleanup completed.\n");
}
