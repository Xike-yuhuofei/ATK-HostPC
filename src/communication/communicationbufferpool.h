#ifndef COMMUNICATIONBUFFERPOOL_H
#define COMMUNICATIONBUFFERPOOL_H

#include <QObject>
#include <QMutex>
#include <QQueue>
#include <QTimer>
#include <QByteArray>
#include <QAtomicInt>
#include <QThread>
#include <QWaitCondition>
#include <QHash>
#include <memory>

/**
 * @brief 缓冲区类型枚举
 */
enum class BufferType {
    Small = 0,      // 小缓冲区 (64-512字节)
    Medium,         // 中缓冲区 (512-4KB)
    Large,          // 大缓冲区 (4KB-64KB)
    Huge            // 超大缓冲区 (64KB+)
};

/**
 * @brief 缓冲区信息结构
 */
struct BufferInfo {
    QByteArray* buffer;         // 缓冲区指针
    BufferType type;            // 缓冲区类型
    qint64 allocatedTime;       // 分配时间
    qint64 lastUsedTime;        // 最后使用时间
    int useCount;               // 使用次数
    bool inUse;                 // 是否正在使用
    QThread* ownerThread;       // 拥有线程
    
    BufferInfo() : buffer(nullptr), type(BufferType::Small), 
                   allocatedTime(0), lastUsedTime(0), useCount(0), 
                   inUse(false), ownerThread(nullptr) {}
};

/**
 * @brief 池统计信息结构
 */
struct PoolStatistics {
    QAtomicInt totalAllocated;      // 总分配数量
    QAtomicInt totalReleased;       // 总释放数量
    QAtomicInt currentInUse;        // 当前使用中
    QAtomicInt peakUsage;           // 峰值使用量
    QAtomicInt hitCount;            // 命中次数
    QAtomicInt missCount;           // 未命中次数
    qint64 totalMemoryUsage;        // 总内存使用量
    qint64 peakMemoryUsage;         // 峰值内存使用量
    double hitRatio;                // 命中率
    
    PoolStatistics() : totalAllocated(0), totalReleased(0), currentInUse(0),
                      peakUsage(0), hitCount(0), missCount(0),
                      totalMemoryUsage(0), peakMemoryUsage(0), hitRatio(0.0) {}
};

/**
 * @brief 池配置结构
 */
struct PoolConfig {
    int maxPoolSize;                // 最大池大小
    int initialPoolSize;            // 初始池大小
    int maxIdleTime;                // 最大空闲时间(秒)
    int cleanupInterval;            // 清理间隔(秒)
    bool enableAutoCleanup;         // 启用自动清理
    bool enableStatistics;          // 启用统计
    bool enableThreadSafety;        // 启用线程安全
    int growthFactor;               // 增长因子
    int shrinkThreshold;            // 收缩阈值
    
    // 各类型缓冲区的大小配置
    int smallBufferSize;            // 小缓冲区大小
    int mediumBufferSize;           // 中缓冲区大小
    int largeBufferSize;            // 大缓冲区大小
    int hugeBufferSize;             // 超大缓冲区大小
    
    PoolConfig() : maxPoolSize(1000), initialPoolSize(50), maxIdleTime(300),
                   cleanupInterval(60), enableAutoCleanup(true), enableStatistics(true),
                   enableThreadSafety(true), growthFactor(2), shrinkThreshold(10),
                   smallBufferSize(512), mediumBufferSize(4096), 
                   largeBufferSize(65536), hugeBufferSize(1048576) {}
};

/**
 * @brief 通信缓冲池管理器
 * 
 * 提供高效的缓冲区分配和回收机制，支持多种大小的缓冲区类型，
 * 具备自动清理、统计监控、线程安全等功能
 */
class CommunicationBufferPool : public QObject
{
    Q_OBJECT

public:
    explicit CommunicationBufferPool(QObject* parent = nullptr);
    ~CommunicationBufferPool();

    /**
     * @brief 获取单例实例
     * @return 单例指针
     */
    static CommunicationBufferPool* getInstance();

    /**
     * @brief 初始化缓冲池
     * @param config 池配置
     * @return 是否成功
     */
    bool initialize(const PoolConfig& config = PoolConfig());

    /**
     * @brief 关闭缓冲池
     */
    void shutdown();

    /**
     * @brief 获取缓冲区
     * @param size 所需大小
     * @param type 缓冲区类型(可选)
     * @return 缓冲区指针
     */
    QByteArray* acquireBuffer(int size, BufferType type = BufferType::Small);

    /**
     * @brief 释放缓冲区
     * @param buffer 缓冲区指针
     */
    void releaseBuffer(QByteArray* buffer);

    /**
     * @brief 预分配缓冲区
     * @param type 缓冲区类型
     * @param count 数量
     */
    void preallocateBuffers(BufferType type, int count);

    /**
     * @brief 清理空闲缓冲区
     */
    void cleanupIdleBuffers();

    /**
     * @brief 强制清理所有缓冲区
     */
    void forceCleanup();

    /**
     * @brief 设置池配置
     * @param config 新配置
     */
    void setPoolConfig(const PoolConfig& config);

    /**
     * @brief 获取池配置
     * @return 当前配置
     */
    PoolConfig getPoolConfig() const;

    /**
     * @brief 获取池统计信息
     * @return 统计信息
     */
    PoolStatistics getStatistics();

    /**
     * @brief 获取可用缓冲区数量
     * @param type 缓冲区类型
     * @return 可用数量
     */
    int getAvailableCount(BufferType type) const;

    /**
     * @brief 获取使用中缓冲区数量
     * @param type 缓冲区类型
     * @return 使用中数量
     */
    int getInUseCount(BufferType type) const;

    /**
     * @brief 获取总内存使用量
     * @return 内存使用量(字节)
     */
    qint64 getTotalMemoryUsage() const;

    /**
     * @brief 检查池健康状态
     * @return 是否健康
     */
    bool isHealthy() const;

    /**
     * @brief 重置统计信息
     */
    void resetStatistics();

public slots:
    /**
     * @brief 定时清理槽
     */
    void onCleanupTimer();

    /**
     * @brief 统计更新槽
     */
    void onStatisticsTimer();

signals:
    /**
     * @brief 内存使用警告信号
     * @param usage 当前使用量
     * @param threshold 阈值
     */
    void memoryUsageWarning(qint64 usage, qint64 threshold);

    /**
     * @brief 池状态变化信号
     * @param healthy 是否健康
     */
    void poolHealthChanged(bool healthy);

    /**
     * @brief 统计信息更新信号
     * @param stats 统计信息
     */
    void statisticsUpdated(const PoolStatistics& stats);

private:
    /**
     * @brief 创建新缓冲区
     * @param type 缓冲区类型
     * @return 缓冲区信息
     */
    BufferInfo* createBuffer(BufferType type);

    /**
     * @brief 销毁缓冲区
     * @param info 缓冲区信息
     */
    void destroyBuffer(BufferInfo* info);

    /**
     * @brief 根据大小确定缓冲区类型
     * @param size 所需大小
     * @return 缓冲区类型
     */
    BufferType determineBufferType(int size) const;

    /**
     * @brief 获取缓冲区大小
     * @param type 缓冲区类型
     * @return 缓冲区大小
     */
    int getBufferSize(BufferType type) const;

    /**
     * @brief 更新统计信息
     */
    void updateStatistics();

    /**
     * @brief 检查内存使用情况
     */
    void checkMemoryUsage();

    /**
     * @brief 扩展池大小
     * @param type 缓冲区类型
     */
    void expandPool(BufferType type);

    /**
     * @brief 收缩池大小
     * @param type 缓冲区类型
     */
    void shrinkPool(BufferType type);

private:
    static CommunicationBufferPool* s_instance;     // 单例实例
    static QMutex s_instanceMutex;                  // 实例互斥锁
    
    // 缓冲区池 - 按类型分组
    QQueue<BufferInfo*> m_availableBuffers[4];      // 可用缓冲区队列
    QList<BufferInfo*> m_inUseBuffers;              // 使用中缓冲区列表
    QHash<QByteArray*, BufferInfo*> m_bufferMap;    // 缓冲区映射表
    
    mutable QMutex m_poolMutex;                     // 池互斥锁
    QWaitCondition m_waitCondition;                 // 等待条件
    
    PoolConfig m_config;                            // 池配置
    PoolStatistics m_statistics;                    // 统计信息
    
    QTimer* m_cleanupTimer;                         // 清理定时器
    QTimer* m_statisticsTimer;                      // 统计定时器
    
    bool m_initialized;                             // 是否已初始化
    bool m_shutdown;                                // 是否已关闭
    
    qint64 m_memoryThreshold;                       // 内存阈值
    qint64 m_lastCleanupTime;                       // 最后清理时间
};

#endif // COMMUNICATIONBUFFERPOOL_H