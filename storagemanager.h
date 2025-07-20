#ifndef STORAGEMANAGER_H
#define STORAGEMANAGER_H

#include <QObject>
#include <QStorageInfo>
#include <QDir>
#include <QTimer>

/**
 * @brief 存储管理类 (StorageManager)
 * 
 * 该类继承自 QObject，负责监控和管理应用程序使用的存储空间，
 * 特别是针对视频录像文件的存储。它提供了自动检测存储空间、
 * 在空间不足时清理最旧录像（按天组织的目录）等功能。
 * 
 * 主要功能包括：
 * - 配置和查询存储根路径 (例如 TF 卡挂载点)。
 * - 设置和查询最小可用空间百分比阈值。
 * - 手动检查存储空间是否低于阈值。
 * - 自动（定时）检查存储空间。
 * - 当空间不足时，自动查找并删除包含最早录像数据的整个日期目录。
 * - 通过信号通知外部组件关于存储空间状态（不足、清理完成、清理失败）。
 */
class StorageManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param storagePath 存储管理的根路径。默认为 "/mnt/TFcard"。
     * @param parent 父对象指针。默认为 nullptr。
     *               `explicit` 关键字防止构造函数的隐式类型转换。
     */
    explicit StorageManager(const QString &storagePath = "/mnt/TFcard", QObject *parent = nullptr);
    
    /**
     * @brief 析构函数
     *
     * 负责在对象销毁前停止所有正在运行的定时器。
     * Qt的对象树机制会自动处理作为子对象创建的 QTimer 的内存释放。
     */
    ~StorageManager();
    
    /**
     * @brief 设置存储管理的根路径。
     * @param path 新的存储路径字符串。如果路径不存在，会尝试创建它。
     */
    void setStoragePath(const QString &path);
    
    /**
     * @brief 获取当前配置的存储管理根路径。
     * @return 返回存储路径的 QString。
     */
    QString storagePath() const;
    
    /**
     * @brief 设置最小可用存储空间百分比阈值。
     * @param percent 百分比值 (0-100)。如果超出此范围，会被校正到最近的边界。
     *                当实际可用空间低于此百分比时，会触发空间不足逻辑。
     */
    void setMinFreeSpacePercent(int percent);
    
    /**
     * @brief 获取当前配置的最小可用存储空间百分比阈值。
     * @return 返回百分比值 (0-100)。
     */
    int minFreeSpacePercent() const;
    
    /**
     * @brief 手动检查当前存储路径下的存储空间是否充足。
     * @return 如果可用空间百分比大于或等于设定阈值，则返回 true；
     *         否则（空间不足、设备无效或未就绪）返回 false。
     *         如果空间不足，会发出 `lowStorageSpace` 信号。
     */
    bool checkStorageSpace();
    
    /**
     * @brief 手动触发清理操作：删除存储路径下按日期命名的目录中最早的一天。
     * @return 如果成功找到并删除了一个目录，则返回 true；否则返回 false。
     *         操作结果会通过 `cleanupCompleted` 或 `cleanupFailed` 信号通知。
     */
    bool cleanupOldestDay();
    
    /**
     * @brief 启动存储空间的自动（定时）检查功能。
     * @param interval 自动检查的时间间隔，单位为毫秒。默认为 3,600,000 毫秒 (1小时)。
     *                 启动时会立即执行一次检查。
     */
    void startAutoCheck(int interval = 3600000); // 默认1小时 (1 * 60 * 60 * 1000 ms)
    
    /**
     * @brief 停止存储空间的自动（定时）检查功能。
     */
    void stopAutoCheck();

signals:
    /**
     * @brief 当检测到存储空间低于设定阈值时发出的信号。
     * @param availableBytes 当前可用的存储空间字节数。
     * @param totalBytes 存储设备的总字节数。
     * @param percent 当前可用空间占总空间的百分比。
     */
    void lowStorageSpace(qint64 availableBytes, qint64 totalBytes, double percent);
    
    /**
     * @brief 当自动清理操作成功删除一个旧目录后发出的信号。
     * @param path 被成功清理的目录的相对路径名 (例如 "20230115")。
     * @param freedBytes 通过删除该目录所释放的近似字节数。
     */
    void cleanupCompleted(const QString &path, qint64 freedBytes);
    
    /**
     * @brief 当自动清理操作失败时（例如找不到可清理目录，或删除操作因权限等问题失败）发出的信号。
     * @param errorMessage 描述清理失败原因的文本信息。
     */
    void cleanupFailed(const QString &errorMessage);

private slots:
    /**
     * @brief 私有槽函数：执行一次存储空间的自动检查和可能的清理操作。
     * 
     * 此槽函数由 `m_checkTimer` 定时器周期性触发，或在 `startAutoCheck()` 时被直接调用一次。
     * 它调用 `checkStorageSpace()`，如果空间不足，则调用 `cleanupOldestDay()`。
     */
    void performAutoCheck();

private:
    QString m_storagePath;       ///< 存储管理的根路径字符串 (例如 "/mnt/TFcard")。
    int m_minFreeSpacePercent;   ///< 最小可用存储空间百分比阈值 (0-100)。
    QTimer *m_checkTimer;        ///< QTimer 对象，用于实现自动（定时）检查存储空间的功能。
    
    // 私有辅助方法
    /**
     * @brief 计算指定目录的总大小（递归地包括所有子目录和文件）。
     * @param dir 要计算大小的 QDir 对象。
     * @return 目录的总大小，单位为字节。
     */
    qint64 getDirSize(const QDir &dir);
    
    /**
     * @brief 获取存储路径下符合 "yyyyMMdd" 命名格式且日期最早的子目录名。
     * @return 如果找到，返回目录名 (例如 "20230115")；否则返回空 QString。
     */
    QString getOldestDateDir();
};

#endif // STORAGEMANAGER_H