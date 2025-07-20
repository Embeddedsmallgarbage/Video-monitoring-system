/**
 * @file storagemanager.cpp
 * @brief 存储管理类 (StorageManager) 的实现文件。
 * 
 * 本文件负责实现视频监控系统中与存储空间管理相关的功能。
 * 主要职责包括：
 * - 监控指定存储路径（例如TF卡）的可用空间。
 * - 根据设定的最小可用空间百分比阈值，判断存储空间是否充足。
 * - 当检测到存储空间不足时，自动删除最早录制的视频文件（按天为单位的整个目录）。
 * - 提供手动和自动（定时）检查存储空间的功能。
 * - 通过信号 (`lowStorageSpace`, `cleanupCompleted`, `cleanupFailed`) 通知其他组件存储状态的变化和清理操作的结果。
 */

#include "storagemanager.h"

#include <QDebug>        // QDebug 类，用于输出调试信息。
#include <QFileInfo>     // QFileInfo 类，提供文件的元信息（如大小、类型等）。
#include <QDateTime>     // QDateTime 类，用于处理日期和时间（此处用于比较目录日期）。
#include <QStorageInfo>  // QStorageInfo 类，提供关于已挂载存储设备的信息（如总容量、可用空间）。
#include <QDirIterator>  // QDirIterator 类，用于遍历目录中的文件和子目录。

/**
 * @brief StorageManager 类的构造函数。
 * @param storagePath 存储管理的根路径 (例如 "/mnt/TFcard")。
 * @param parent 父对象指针，遵循Qt的对象树模型。
 * 
 * 初始化成员变量，包括存储路径、默认的最小可用空间百分比 (10%)，
 * 并创建一个 QTimer 实例 (`m_checkTimer`) 用于未来的自动检查功能。
 * 同时，它会确保指定的 `storagePath` 存在，如果不存在则尝试创建它。
 */
StorageManager::StorageManager(const QString &storagePath, QObject *parent)
    : QObject(parent)                         // 调用父类QObject的构造函数
    , m_storagePath(storagePath)              // 初始化存储路径
    , m_minFreeSpacePercent(10)               // 初始化最小可用空间百分比阈值为10%
    , m_checkTimer(nullptr)                   // 初始化定时器指针为空
{
    // 创建用于自动检查存储空间的定时器
    m_checkTimer = new QTimer(this);                                        // 创建QTimer实例，this作为其父对象
    connect(m_checkTimer, &QTimer::timeout, this, &StorageManager::performAutoCheck); // 连接定时器的timeout信号到performAutoCheck槽函数
    
    // 确保指定的存储根路径存在，如果不存在则尝试创建它
    QDir dir(m_storagePath);
    if (!dir.exists()) { // 检查目录是否存在
        qInfo() << "StorageManager: 存储路径 " << m_storagePath << " 不存在，尝试创建。";
        if (!dir.mkpath(".")) { // mkpath(".") 会创建路径中的所有必需的父目录
            qWarning() << "StorageManager: 创建存储路径 " << m_storagePath << " 失败！";
            // 此时可以考虑抛出异常或发出错误信号，因为存储管理器可能无法正常工作
        }
    }
}

/**
 * @brief StorageManager 类的析构函数。
 * 
 * 负责在对象销毁前停止自动检查定时器 `m_checkTimer` (如果它正在运行)，
 * 以防止悬挂的定时器事件。Qt的对象父子关系会自动处理 `m_checkTimer` 对象的删除。
 */
StorageManager::~StorageManager()
{
    // 如果定时器存在且正在运行，则停止它
    if (m_checkTimer && m_checkTimer->isActive()) {
        m_checkTimer->stop();
        qDebug() << "StorageManager: 自动检查定时器已在析构时停止。";
    }
    // m_checkTimer 作为 this 的子对象，会被Qt自动删除，无需显式 delete
}

/**
 * @brief 设置存储管理的根路径。
 * @param path 新的存储路径字符串。
 * 
 * 此函数会更新内部的 `m_storagePath` 成员变量。
 * 与构造函数类似，它也会检查新的存储路径是否存在，如果不存在则尝试创建。
 */
void StorageManager::setStoragePath(const QString &path)
{
    if (m_storagePath == path) return; // 如果路径未改变，则不执行任何操作
    
    qDebug() << "StorageManager: 存储路径已从 " << m_storagePath << " 更改为 " << path;
    m_storagePath = path; // 更新存储路径
    
    // 确保新的存储路径存在
    QDir dir(m_storagePath);
    if (!dir.exists()) {
        qInfo() << "StorageManager: 新的存储路径 " << m_storagePath << " 不存在，尝试创建。";
        if (!dir.mkpath(".")) {
            qWarning() << "StorageManager: 创建新的存储路径 " << m_storagePath << " 失败！";
        }
    }
}

/**
 * @brief 获取当前配置的存储管理根路径。
 * @return 返回存储路径的QString。
 */
QString StorageManager::storagePath() const
{
    return m_storagePath;
}

/**
 * @brief 设置最小可用存储空间百分比阈值。
 * @param percent 百分比值，有效范围是 0 到 100。
 *                如果传入值超出此范围，会被自动校正到最近的有效边界。
 * 
 * 当存储设备的可用空间低于此百分比时，会触发空间不足的逻辑（例如自动清理）。
 */
void StorageManager::setMinFreeSpacePercent(int percent)
{
    // 确保百分比值在有效范围内 (0-100)
    if (percent < 0) {
        percent = 0;
    } else if (percent > 100) {
        percent = 100;
    }
    
    if (m_minFreeSpacePercent != percent) {
        qDebug() << "StorageManager: 最小可用空间阈值已从 " << m_minFreeSpacePercent 
                 << "% 更改为 " << percent << "%";
        m_minFreeSpacePercent = percent; // 更新阈值
    }
}

/**
 * @brief 获取当前配置的最小可用存储空间百分比阈值。
 * @return 返回百分比值 (0-100)。
 */
int StorageManager::minFreeSpacePercent() const
{
    return m_minFreeSpacePercent;
}

/**
 * @brief 检查当前存储路径下的存储空间是否充足。
 * @return 如果可用空间百分比大于或等于设定的 `m_minFreeSpacePercent` 阈值，则返回 true；
 *         否则（空间不足、设备无效或未就绪），返回 false。
 * 
 * 此函数执行以下操作：
 * 1. 使用 `QStorageInfo` 获取指定存储路径的设备信息（总容量、可用容量）。
 * 2. 检查存储设备是否有效且已就绪。如果无效，则记录警告并返回 false。
 * 3. 计算可用空间的实际百分比。
 * 4. 如果可用百分比低于 `m_minFreeSpacePercent`，则发出 `lowStorageSpace` 信号，并返回 false。
 * 5. 否则，返回 true。
 */
bool StorageManager::checkStorageSpace()
{
    // 获取指定存储路径（例如 "/mnt/TFcard"）的存储设备信息
    QStorageInfo storage(m_storagePath);
    storage.refresh(); // 确保获取的是最新的存储信息
    
    // 检查存储设备是否有效且已准备好（例如，是否已挂载且可读写）
    if (!storage.isValid() || !storage.isReady()) {
        qWarning() << "StorageManager::checkStorageSpace: 存储设备无效或未就绪于路径: " << m_storagePath;
        // 在这种情况下，可以认为空间不足或无法确定，返回false
        emit lowStorageSpace(0, 0, 0.0); // 发送一个表示无效状态的信号
        return false;
    }
    
    // 获取总字节数和可用字节数
    qint64 availableBytes = storage.bytesAvailable();
    qint64 totalBytes = storage.bytesTotal();
    
    // 计算可用空间百分比
    // 必须确保 totalBytes 不为0，以避免除以零的错误
    double availablePercent = 0.0;
    if (totalBytes > 0) {
        availablePercent = static_cast<double>(availableBytes) / totalBytes * 100.0;
    } else {
        // 如果总字节数为0（可能是无效的存储设备或路径），则认为可用百分比也为0
        qWarning() << "StorageManager::checkStorageSpace: 获取到的总存储空间为0字节于路径: " << m_storagePath;
        // 此时也应视为空间不足
        emit lowStorageSpace(availableBytes, totalBytes, availablePercent);
        return false;
    }
    
    qDebug() << QString("StorageManager - 空间信息 for '%1': 总容量: %2 MB, 可用: %3 MB, 可用百分比: %4% (阈值: %5%)")
                 .arg(m_storagePath)
                 .arg(totalBytes / (1024.0 * 1024.0), 0, 'f', 2)       // 总容量MB，保留2位小数
                 .arg(availableBytes / (1024.0 * 1024.0), 0, 'f', 2)    // 可用容量MB，保留2位小数
                 .arg(availablePercent, 0, 'f', 1)                          // 可用百分比，保留1位小数
                 .arg(m_minFreeSpacePercent);
    
    // 将计算出的可用百分比与设定的最小阈值进行比较
    if (availablePercent < m_minFreeSpacePercent) {
        qInfo() << "StorageManager::checkStorageSpace: 存储空间不足！可用 (" << availablePercent 
                << ") 低于阈值 (" << m_minFreeSpacePercent << ").";
        // 发出存储空间不足的信号，传递详细信息
        emit lowStorageSpace(availableBytes, totalBytes, availablePercent);
        return false; // 空间不足
    }
    
    return true; // 空间充足
}

/**
 * @brief 清理（删除）存储路径下按日期命名的目录中最早的一天。
 * @return 如果成功找到并删除了一个目录，则返回 true；
 *         如果没有找到符合日期格式的目录可供删除，或删除操作失败，则返回 false。
 * 
 * 此函数执行以下操作：
 * 1. 调用 `getOldestDateDir()` 获取存储路径下名称符合 "yyyyMMdd" 格式且日期最早的子目录名。
 * 2. 如果没有找到这样的目录，则发出 `cleanupFailed` 信号并返回 false。
 * 3. 如果找到目录，则构造该目录的完整路径，并调用 `getDirSize()` 计算其大小（用于报告）。
 * 4. 使用 `QDir::removeRecursively()` 尝试递归删除该目录及其所有内容。
 * 5. 如果删除成功，则记录日志，发出 `cleanupCompleted` 信号（包含被删目录名和释放的空间大小），并返回 true。
 * 6. 如果删除失败，则记录警告，发出 `cleanupFailed` 信号，并返回 false。
 */
bool StorageManager::cleanupOldestDay()
{
    // 获取存储路径下按日期命名 (yyyyMMdd) 且最早的目录名
    QString oldestDirName = getOldestDateDir();
    
    // 如果没有找到符合条件的目录 (例如，目录为空或没有符合日期格式的子目录)
    if (oldestDirName.isEmpty()) {
        qInfo() << "StorageManager::cleanupOldestDay: 没有找到可清理的日期目录于路径: " << m_storagePath;
        emit cleanupFailed("没有找到可清理的日期目录");
        return false;
    }
    
    // 构建要删除的目录的完整路径
    QDir dirToRemove(m_storagePath + QDir::separator() + oldestDirName);
    if (!dirToRemove.exists()) { // 再次确认目录确实存在
        qWarning() << "StorageManager::cleanupOldestDay: 目录 " << dirToRemove.absolutePath() << " 已不存在，无法删除。";
        emit cleanupFailed(QString("目录 %1 已不存在").arg(dirToRemove.absolutePath()));
        return false;
    }
    
    // 计算要删除目录的大小，以便在成功后报告释放了多少空间
    qint64 dirSize = getDirSize(dirToRemove);
    
    qInfo() << "StorageManager: 准备删除最早的视频目录: " << dirToRemove.absolutePath() 
            << " (大小: " << dirSize / (1024.0*1024.0) << " MB)";
    
    // 尝试递归删除目录及其所有内容
    bool success = dirToRemove.removeRecursively();
    
    if (success) {
        qInfo() << "StorageManager: 已成功删除目录: " << dirToRemove.absolutePath() 
                << ", 释放空间约: " << dirSize / (1024.0 * 1024.0) << " MB";
        // 发出清理完成信号，传递被删除的目录名（相对路径）和释放的字节数
        emit cleanupCompleted(oldestDirName, dirSize);
    } else {
        qWarning() << "StorageManager: 删除目录失败: " << dirToRemove.absolutePath() 
                   << " (错误可能与文件锁定、权限等有关)";
        // 发出清理失败信号
        emit cleanupFailed(QString("删除目录 %1 失败").arg(dirToRemove.absolutePath()));
    }
    
    return success;
}

/**
 * @brief 启动存储空间的自动（定时）检查功能。
 * @param interval 自动检查的时间间隔，单位为毫秒。
 * 
 * 当启动时，会首先立即调用 `performAutoCheck()` 执行一次检查。
 * 然后，`m_checkTimer` 会以指定的 `interval` 周期性触发 `performAutoCheck()`。
 */
void StorageManager::startAutoCheck(int interval)
{
    if (interval <= 0) {
        qWarning() << "StorageManager::startAutoCheck: 无效的检查间隔: " << interval << "ms. 必须大于0。";
        return;
    }
    
    // 首次启动时，立即执行一次检查
    qDebug() << "StorageManager: 请求启动自动检查，将首先执行一次初始检查...";
    performAutoCheck();
    
    // 启动定时器，以指定的间隔重复执行检查
    m_checkTimer->start(interval);
    qInfo() << "StorageManager: 已启动存储空间自动检查，间隔: " << interval / 1000.0 << " 秒";
}

/**
 * @brief 停止存储空间的自动（定时）检查功能。
 * 
 * 此函数会停止 `m_checkTimer` 定时器。
 */
void StorageManager::stopAutoCheck()
{
    if (m_checkTimer && m_checkTimer->isActive()) {
        m_checkTimer->stop();
        qInfo() << "StorageManager: 已停止存储空间自动检查。";
    } else {
        qDebug() << "StorageManager::stopAutoCheck: 自动检查定时器未运行或未初始化。";
    }
}

/**
 * @brief 私有槽函数：执行一次存储空间的自动检查和可能的清理操作。
 * 
 * 此槽函数通常由 `m_checkTimer` 定时器触发，或在 `startAutoCheck()` 时被直接调用一次。
 * 它会调用 `checkStorageSpace()` 检查空间。如果发现空间不足，则调用 `cleanupOldestDay()`
 * 尝试清理最早一天的视频文件。清理后，会再次调用 `checkStorageSpace()` 确认空间是否已足够。
 */
void StorageManager::performAutoCheck()
{
    qDebug() << "StorageManager::performAutoCheck: 开始执行存储空间自动检查...";
    
    // 步骤1: 检查当前存储空间是否充足
    if (!checkStorageSpace()) { // checkStorageSpace会在空间不足时发出lowStorageSpace信号
        qInfo() << "StorageManager::performAutoCheck: 检测到存储空间不足，尝试清理最早一天的文件...";
        // 步骤2: 如果空间不足，尝试清理最早一天的视频文件
        bool cleaned = cleanupOldestDay(); // cleanupOldestDay会发出cleanupCompleted或cleanupFailed信号
        
        if (cleaned) {
            qInfo() << "StorageManager::performAutoCheck: 清理操作已执行。将再次检查空间...";
            // 步骤3: 清理后，再次检查存储空间是否已恢复到阈值以上
            if (!checkStorageSpace()) {
                qWarning() << "StorageManager::performAutoCheck: 清理后存储空间仍然不足！可能需要进一步清理或手动干预。";
                // 此时可以考虑是否要再次触发清理，或者发出更严重的警告信号
                // 例如，如果一次清理不解决问题，可能需要一个策略来决定是否连续清理多天
            } else {
                qInfo() << "StorageManager::performAutoCheck: 清理后存储空间已恢复正常。";
            }
        } else {
            qWarning() << "StorageManager::performAutoCheck: 自动清理操作失败。存储空间仍然不足。";
        }
    } else {
        qDebug() << "StorageManager::performAutoCheck: 存储空间充足，无需操作。";
    }
    qDebug() << "StorageManager::performAutoCheck: 存储空间自动检查执行完毕。";
}

/**
 * @brief 计算指定目录的总大小（递归地包括所有子目录和文件）。
 * @param dir 要计算大小的 QDir 对象。
 * @return 目录的总大小，单位为字节。
 * 
 * 使用 `QDirIterator` 遍历目录中的所有文件，并累加它们的大小。
 */
qint64 StorageManager::getDirSize(const QDir &dir)
{
    qint64 totalSize = 0; // 初始化总大小为0
    
    // 创建一个目录迭代器，用于递归遍历指定目录下的所有文件和子目录
    // QDir::Files: 包含文件
    // QDir::Dirs: 包含目录 (虽然我们只加文件大小，但需要遍历子目录以找到其中的文件)
    // QDir::NoDotAndDotDot: 排除特殊目录 "." (当前目录) 和 ".." (父目录)
    // QDirIterator::Subdirectories: 递归进入子目录进行遍历
    QDirIterator it(dir.absolutePath(), QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    // 注意：上面的过滤器 QDir::Files | QDir::NoDotAndDotDot 若配合 QDirIterator::Subdirectories，
    // 迭代器本身可能不会直接列出子目录条目，而是直接递归进入并列出子目录中的文件。
    // 如果需要明确区分文件和目录，或者有其他处理逻辑，过滤器和迭代器选项需要仔细调整。
    // 对于仅计算总文件大小，当前方式是有效的。
    
    while (it.hasNext()) { // 当迭代器还有下一个条目时
        it.next();           // 移动到下一个条目
        QFileInfo fileInfo = it.fileInfo(); // 获取当前条目的文件信息
        if (fileInfo.isFile()) {            // 如果当前条目是一个文件
            totalSize += fileInfo.size();   // 将文件大小累加到总大小中
        }
    }
    
    return totalSize; // 返回计算得到的总大小
}

/**
 * @brief 获取存储路径下符合 "yyyyMMdd" 命名格式且日期最早的子目录名。
 * @return 如果找到符合条件的目录，则返回其名称 (例如 "20230115")；
 *         如果没有找到，或者所有目录名都不符合格式，则返回空 QString。
 * 
 * 此函数用于确定在空间不足时应该删除哪个旧的视频数据目录。
 * 它会列出 `m_storagePath` 下的所有子目录，筛选出名称为8位数字（预期为yyyyMMdd格式）
 * 的目录，然后对这些有效日期目录进行排序，找出最早的一个。
 */
QString StorageManager::getOldestDateDir()
{
    QDir storageDir(m_storagePath); // 创建QDir对象操作存储根路径
    
    // 获取存储根路径下的所有子目录名称列表
    // QDir::Dirs: 只列出目录
    // QDir::NoDotAndDotDot: 排除特殊目录 "." 和 ".."
    QStringList dateDirs = storageDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    
    if (dateDirs.isEmpty()) {
        qDebug() << "StorageManager::getOldestDateDir: 存储路径 " << m_storagePath << " 中没有找到任何子目录。";
        return QString(); // 如果没有子目录，则返回空字符串
    }
    
    // 筛选出名称符合 "yyyyMMdd" 格式的有效日期目录
    QStringList validDateDirs;
    QRegExp dateRegex("^\\d{8}$"); // 正则表达式，匹配8位纯数字字符串 (例如 "20230115")
                                 // ^: 匹配字符串开头, \\d{8}: 匹配8个数字, $: 匹配字符串结尾
    
    for (const QString &dirName : dateDirs) {
        if (dateRegex.exactMatch(dirName)) { // 检查目录名是否完全匹配日期格式
            // 进一步验证是否是有效的日期，例如使用QDate::fromString
            // QDate date = QDate::fromString(dirName, "yyyyMMdd");
            // if (date.isValid()) { validDateDirs.append(dirName); }
            // 为简化，这里仅用正则匹配，假设符合8位数字的就是有效日期目录
            validDateDirs.append(dirName);
        }
    }
    
    if (validDateDirs.isEmpty()) {
        qDebug() << "StorageManager::getOldestDateDir: 在 " << m_storagePath << " 中没有找到符合 yyyyMMdd 格式的日期目录。";
        return QString(); // 如果没有符合格式的日期目录，则返回空字符串
    }
    
    //对有效的日期目录名进行升序排序（字符串排序等同于日期排序，因为格式是yyyyMMdd）
    std::sort(validDateDirs.begin(), validDateDirs.end());
    
    // 返回排序后的第一个目录名，即最早的日期目录
    qDebug() << "StorageManager::getOldestDateDir: 找到最早的日期目录为: " << validDateDirs.first();
    return validDateDirs.first();
}