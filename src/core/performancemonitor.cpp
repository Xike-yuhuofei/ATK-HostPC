#include "performancemonitor.h"
#include "../logger/logmanager.h"
#include "errorhandler.h"
#include "memoryoptimizer.h"
#include <QDebug>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QTextStream>
#include <QMutexLocker>
#include <QSysInfo>
#include <QStorageInfo>
#include <QNetworkInterface>

#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#include <pdh.h>
#elif defined(Q_OS_LINUX)
#include <unistd.h>
#include <sys/sysinfo.h>
#include <fstream>
#elif defined(Q_OS_MAC)
#include <sys/types.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#endif

PerformanceMonitor* PerformanceMonitor::instance = nullptr;
QMutex PerformanceMonitor::instanceMutex;

PerformanceMonitor* PerformanceMonitor::getInstance()
{
    QMutexLocker locker(&instanceMutex);
    if (!instance) {
        instance = new PerformanceMonitor();
    }
    return instance;
}

PerformanceMonitor::PerformanceMonitor(QObject* parent)
    : QObject(parent)
    , monitoring(false)
    , paused(false)
    , alertsEnabled(true)
    , monitoringInterval(1000) // 1秒
    , historySize(3600) // 1小时的数据
    , m_memoryOptimizer(nullptr)
    , m_memoryThreshold(80.0)
    , systemProcess(nullptr)
    , lastNetworkBytesIn(0)
    , lastNetworkBytesOut(0)
{
    // 创建定时器
    collectTimer = new QTimer(this);
    connect(collectTimer, &QTimer::timeout, this, &PerformanceMonitor::collectMetrics);
    
    alertTimer = new QTimer(this);
    connect(alertTimer, &QTimer::timeout, this, &PerformanceMonitor::checkAlerts);
    alertTimer->start(5000); // 每5秒检查一次告警
    
    processTimer = new QTimer(this);
    connect(processTimer, &QTimer::timeout, this, &PerformanceMonitor::processMetricsQueue);
    processTimer->start(100); // 每100ms处理一次队列
    
    // 设置默认启用的指标
    enabledMetrics << "cpu" << "memory" << "disk" << "app";
    
    // 添加默认告警
    PerformanceAlert cpuAlert;
    cpuAlert.name = "High CPU Usage";
    cpuAlert.metric = "cpuUsage";
    cpuAlert.threshold = 80.0;
    cpuAlert.enabled = true;
    cpuAlert.duration = 30;
    cpuAlert.action = "warning";
    alerts.append(cpuAlert);
    
    PerformanceAlert memoryAlert;
    memoryAlert.name = "High Memory Usage";
    memoryAlert.metric = "memoryUsage";
    memoryAlert.threshold = 85.0;
    memoryAlert.enabled = true;
    memoryAlert.duration = 30;
    memoryAlert.action = "warning";
    alerts.append(memoryAlert);
    
    // 初始化内存优化器
    MemoryOptimizerConfig config;
    config.enableObjectPools = true;
    config.enableMemoryTracking = true;
    config.enableAutoCleanup = true;
    config.memoryThreshold = static_cast<qint64>(m_memoryThreshold * 1024 * 1024);
    config.cleanupInterval = 30; // 30秒
    
    m_memoryOptimizer = new MemoryOptimizer(this);
    m_memoryOptimizer->initialize(config);
    
    qDebug() << "PerformanceMonitor initialized";
}

PerformanceMonitor::~PerformanceMonitor()
{
    stopMonitoring();
    
    // 清理内存优化器
    if (m_memoryOptimizer) {
        m_memoryOptimizer->shutdown();
        delete m_memoryOptimizer;
        m_memoryOptimizer = nullptr;
    }
    
    qDebug() << "PerformanceMonitor destroyed";
}

void PerformanceMonitor::startMonitoring()
{
    if (monitoring) {
        qWarning() << "Performance monitoring already started";
        return;
    }
    
    monitoring = true;
    paused = false;
    
    collectTimer->start(monitoringInterval);
    
    // 立即收集一次指标
    collectMetrics();
    
    emit monitoringStarted();
    qDebug() << "Performance monitoring started";
}

void PerformanceMonitor::stopMonitoring()
{
    if (!monitoring) {
        return;
    }
    
    monitoring = false;
    paused = false;
    
    collectTimer->stop();
    
    emit monitoringStopped();
    qDebug() << "Performance monitoring stopped";
}

void PerformanceMonitor::pauseMonitoring()
{
    if (!monitoring || paused) {
        return;
    }
    
    paused = true;
    collectTimer->stop();
    
    qDebug() << "Performance monitoring paused";
}

void PerformanceMonitor::resumeMonitoring()
{
    if (!monitoring || !paused) {
        return;
    }
    
    paused = false;
    collectTimer->start(monitoringInterval);
    
    qDebug() << "Performance monitoring resumed";
}

void PerformanceMonitor::setMonitoringInterval(int intervalMs)
{
    monitoringInterval = qMax(100, intervalMs); // 最小100ms
    
    if (monitoring && !paused) {
        collectTimer->setInterval(monitoringInterval);
    }
}

void PerformanceMonitor::setHistorySize(int size)
{
    QMutexLocker locker(&metricsMutex);
    
    historySize = qMax(10, size);
    
    // 如果当前历史记录超过新的大小限制，删除旧记录
    while (metricsHistory.size() > historySize) {
        metricsHistory.removeFirst();
    }
}

void PerformanceMonitor::collectMetrics()
{
    if (!monitoring || paused) {
        return;
    }
    
    try {
        PerformanceMetrics metrics;
        metrics.timestamp = QDateTime::currentDateTime();
        
        // 收集系统指标
        if (enabledMetrics.contains("cpu")) {
            metrics.cpuUsage = getCpuUsage();
            metrics.cpuTemperature = getCpuTemperature();
        }
        
        if (enabledMetrics.contains("memory")) {
            getMemoryInfo(metrics.memoryUsed, metrics.memoryTotal);
            metrics.memoryUsage = (metrics.memoryTotal > 0) ? 
                (double(metrics.memoryUsed) / metrics.memoryTotal * 100.0) : 0.0;
        }
        
        if (enabledMetrics.contains("disk")) {
            getDiskInfo(metrics.diskUsed, metrics.diskTotal);
            metrics.diskUsage = (metrics.diskTotal > 0) ? 
                (double(metrics.diskUsed) / metrics.diskTotal * 100.0) : 0.0;
        }
        
        if (enabledMetrics.contains("network")) {
            getNetworkInfo(metrics.networkBytesIn, metrics.networkBytesOut);
        }
        
        if (enabledMetrics.contains("app")) {
            metrics.appMemoryUsage = getAppMemoryUsage();
            metrics.appCpuUsage = getAppCpuUsage();
            metrics.threadCount = getThreadCount();
            metrics.handleCount = getHandleCount();
        }
        
        // 添加自定义指标
        {
            QMutexLocker locker(&metricsMutex);
            metrics.customMetrics = customMetrics;
        }
        
        // 更新当前指标
        {
            QMutexLocker locker(&metricsMutex);
            currentMetrics = metrics;
            
            // 添加到队列
            metricsQueue.enqueue(metrics);
            
            // 添加到历史记录
            metricsHistory.append(metrics);
            if (metricsHistory.size() > historySize) {
                metricsHistory.removeFirst();
            }
        }
        
        // 检查内存使用率并触发优化
        if (m_memoryOptimizer && metrics.memoryUsage > m_memoryThreshold) {
            m_memoryOptimizer->performCleanup();
            qDebug() << "Memory optimization triggered due to high usage:" << metrics.memoryUsage << "%";
        }
        
        emit metricsUpdated(metrics);
        
    } catch (const std::exception& e) {
        ErrorHandler::getInstance()->reportError(
            QString("Failed to collect performance metrics: %1").arg(e.what()), 
            "PerformanceMonitor");
    } catch (...) {
        ErrorHandler::getInstance()->reportError(
            "Unknown error while collecting performance metrics", 
            "PerformanceMonitor");
    }
}

void PerformanceMonitor::checkAlerts()
{
    if (!alertsEnabled || alerts.isEmpty()) {
        return;
    }
    
    PerformanceMetrics metrics = getCurrentMetrics();
    
    for (const PerformanceAlert& alert : alerts) {
        if (!alert.enabled) {
            continue;
        }
        
        if (checkAlertCondition(alert, metrics)) {
            processAlert(alert, metrics);
        }
    }
}

// 内存优化器相关方法实现
bool PerformanceMonitor::initializeMemoryOptimizer(const MemoryOptimizerConfig& config)
{
    if (m_memoryOptimizer) {
        return m_memoryOptimizer->initialize(config);
    }
    return false;
}

MemoryStatistics PerformanceMonitor::getMemoryOptimizerStatistics() const
{
    if (m_memoryOptimizer) {
        return m_memoryOptimizer->getStatistics();
    }
    return MemoryStatistics();
}

void PerformanceMonitor::setMemoryOptimizerConfiguration(const MemoryOptimizerConfig& config)
{
    if (m_memoryOptimizer) {
        m_memoryOptimizer->setConfig(config);
    }
}

bool PerformanceMonitor::isMemoryOptimizerEnabled() const
{
    return m_memoryOptimizer != nullptr;
}

void PerformanceMonitor::enableMemoryOptimizer(bool enabled)
{
    if (!m_memoryOptimizer && enabled) {
        // 如果内存优化器不存在且需要启用，则创建并初始化
        MemoryOptimizerConfig config;
        config.enableObjectPools = true;
        config.enableMemoryTracking = true;
        config.enableAutoCleanup = true;
        config.memoryThreshold = static_cast<qint64>(m_memoryThreshold * 1024 * 1024);
        config.cleanupInterval = 30;
        
        m_memoryOptimizer = new MemoryOptimizer(this);
        m_memoryOptimizer->initialize(config);
    } else if (m_memoryOptimizer) {
        if (enabled) {
            // 内存优化器已在初始化时启用
        } else {
            m_memoryOptimizer->shutdown();
        }
    }
}

void PerformanceMonitor::triggerMemoryOptimization()
{
    if (m_memoryOptimizer) {
        m_memoryOptimizer->performCleanup();
        qDebug() << "Manual memory optimization triggered";
    }
}

void PerformanceMonitor::setMemoryThreshold(double threshold)
{
    m_memoryThreshold = qBound(0.0, threshold, 100.0);
    
    if (m_memoryOptimizer) {
        MemoryOptimizerConfig config = m_memoryOptimizer->getConfig();
        config.memoryThreshold = static_cast<qint64>(threshold * 1024 * 1024);
        m_memoryOptimizer->setConfig(config);
    }
}

void PerformanceMonitor::processMetricsQueue()
{
    QList<PerformanceMetrics> metricsToProcess;
    
    {
        QMutexLocker locker(&metricsMutex);
        while (!metricsQueue.isEmpty()) {
            metricsToProcess.append(metricsQueue.dequeue());
        }
    }
    
    for (const PerformanceMetrics& metrics : metricsToProcess) {
        notifyCallbacks(metrics);
    }
}

PerformanceMetrics PerformanceMonitor::getCurrentMetrics() const
{
    QMutexLocker locker(&metricsMutex);
    return currentMetrics;
}

QList<PerformanceMetrics> PerformanceMonitor::getHistoryMetrics(int count) const
{
    QMutexLocker locker(&metricsMutex);
    
    if (count >= metricsHistory.size()) {
        return metricsHistory;
    }
    
    return metricsHistory.mid(metricsHistory.size() - count);
}

void PerformanceMonitor::addCustomMetric(const QString& name, double value)
{
    QMutexLocker locker(&metricsMutex);
    customMetrics[name] = value;
}

void PerformanceMonitor::removeCustomMetric(const QString& name)
{
    QMutexLocker locker(&metricsMutex);
    customMetrics.remove(name);
}

QMap<QString, double> PerformanceMonitor::getCustomMetrics() const
{
    QMutexLocker locker(&metricsMutex);
    return customMetrics;
}

void PerformanceMonitor::addAlert(const PerformanceAlert& alert)
{
    QMutexLocker locker(&metricsMutex);
    alerts.append(alert);
}

void PerformanceMonitor::registerCallback(const QString& name, PerformanceCallback callback)
{
    QMutexLocker locker(&metricsMutex);
    callbacks[name] = callback;
}

void PerformanceMonitor::unregisterCallback(const QString& name)
{
    QMutexLocker locker(&metricsMutex);
    callbacks.remove(name);
}

// 平台相关的系统指标收集实现
double PerformanceMonitor::getCpuUsage()
{
#ifdef Q_OS_WIN
    // Windows实现
    static ULARGE_INTEGER lastCPU, lastSysCPU, lastUserCPU;
    static int numProcessors = 0;
    static bool first = true;
    
    if (first) {
        SYSTEM_INFO sysInfo;
        FILETIME ftime, fsys, fuser;
        
        GetSystemInfo(&sysInfo);
        numProcessors = sysInfo.dwNumberOfProcessors;
        
        GetSystemTimeAsFileTime(&ftime);
        memcpy(&lastCPU, &ftime, sizeof(FILETIME));
        
        GetProcessTimes(GetCurrentProcess(), &ftime, &ftime, &fsys, &fuser);
        memcpy(&lastSysCPU, &fsys, sizeof(FILETIME));
        memcpy(&lastUserCPU, &fuser, sizeof(FILETIME));
        
        first = false;
        return 0.0;
    }
    
    FILETIME ftime, fsys, fuser;
    ULARGE_INTEGER now, sys, user;
    
    GetSystemTimeAsFileTime(&ftime);
    memcpy(&now, &ftime, sizeof(FILETIME));
    
    GetProcessTimes(GetCurrentProcess(), &ftime, &ftime, &fsys, &fuser);
    memcpy(&sys, &fsys, sizeof(FILETIME));
    memcpy(&user, &fuser, sizeof(FILETIME));
    
    double percent = (sys.QuadPart - lastSysCPU.QuadPart) + (user.QuadPart - lastUserCPU.QuadPart);
    percent /= (now.QuadPart - lastCPU.QuadPart);
    percent /= numProcessors;
    
    lastCPU = now;
    lastUserCPU = user;
    lastSysCPU = sys;
    
    return percent * 100.0;
    
#elif defined(Q_OS_LINUX)
    // Linux实现
    static unsigned long long lastTotalUser = 0, lastTotalUserLow = 0, lastTotalSys = 0, lastTotalIdle = 0;
    
    std::ifstream file("/proc/stat");
    if (!file.is_open()) {
        return 0.0;
    }
    
    std::string line;
    std::getline(file, line);
    file.close();
    
    unsigned long long totalUser, totalUserLow, totalSys, totalIdle, total;
    sscanf(line.c_str(), "cpu %llu %llu %llu %llu", &totalUser, &totalUserLow, &totalSys, &totalIdle);
    
    if (lastTotalUser == 0) {
        lastTotalUser = totalUser;
        lastTotalUserLow = totalUserLow;
        lastTotalSys = totalSys;
        lastTotalIdle = totalIdle;
        return 0.0;
    }
    
    total = (totalUser - lastTotalUser) + (totalUserLow - lastTotalUserLow) + (totalSys - lastTotalSys);
    double percent = total;
    total += (totalIdle - lastTotalIdle);
    percent /= total;
    
    lastTotalUser = totalUser;
    lastTotalUserLow = totalUserLow;
    lastTotalSys = totalSys;
    lastTotalIdle = totalIdle;
    
    return percent * 100.0;
    
#else
    // 其他平台的简化实现
    return 0.0;
#endif
}

double PerformanceMonitor::getCpuTemperature()
{
    // 简化实现，返回模拟温度
    return 45.0 + (getCpuUsage() * 0.3);
}

void PerformanceMonitor::getMemoryInfo(qint64& used, qint64& total)
{
#ifdef Q_OS_WIN
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    
    total = memInfo.ullTotalPhys;
    used = total - memInfo.ullAvailPhys;
    
#elif defined(Q_OS_LINUX)
    struct sysinfo memInfo;
    sysinfo(&memInfo);
    
    total = memInfo.totalram * memInfo.mem_unit;
    used = (memInfo.totalram - memInfo.freeram) * memInfo.mem_unit;
    
#else
    // 其他平台的简化实现
    total = 8LL * 1024 * 1024 * 1024; // 8GB
    used = total * 0.6; // 假设使用60%
#endif
}

void PerformanceMonitor::getDiskInfo(qint64& used, qint64& total)
{
    QStorageInfo storage = QStorageInfo::root();
    total = storage.bytesTotal();
    used = total - storage.bytesAvailable();
}

void PerformanceMonitor::getNetworkInfo(qint64& bytesIn, qint64& bytesOut)
{
    // 简化实现，累计所有网络接口的流量
    bytesIn = 0;
    bytesOut = 0;
    
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    for (const QNetworkInterface& interface : interfaces) {
        if (interface.flags().testFlag(QNetworkInterface::IsUp) &&
            !interface.flags().testFlag(QNetworkInterface::IsLoopBack)) {
            // 这里需要平台相关的实现来获取实际的网络统计
            // 简化实现，返回模拟数据
            bytesIn += 1024 * 1024; // 1MB
            bytesOut += 512 * 1024; // 512KB
        }
    }
}

qint64 PerformanceMonitor::getAppMemoryUsage()
{
#ifdef Q_OS_WIN
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
#elif defined(Q_OS_LINUX)
    std::ifstream file("/proc/self/status");
    if (file.is_open()) {
        std::string line;
        while (std::getline(file, line)) {
            if (line.substr(0, 6) == "VmRSS:") {
                int value;
                sscanf(line.c_str(), "VmRSS: %d kB", &value);
                return value * 1024; // 转换为字节
            }
        }
    }
#endif
    return 0;
}

double PerformanceMonitor::getAppCpuUsage()
{
    // 简化实现，返回系统CPU使用率的一部分
    return getCpuUsage() * 0.1;
}

int PerformanceMonitor::getThreadCount()
{
    return QThread::idealThreadCount();
}

int PerformanceMonitor::getHandleCount()
{
    // 简化实现
    return 100;
}

bool PerformanceMonitor::checkAlertCondition(const PerformanceAlert& alert, const PerformanceMetrics& metrics)
{
    double value = 0.0;
    
    if (alert.metric == "cpuUsage") {
        value = metrics.cpuUsage;
    } else if (alert.metric == "memoryUsage") {
        value = metrics.memoryUsage;
    } else if (alert.metric == "diskUsage") {
        value = metrics.diskUsage;
    } else if (alert.metric == "appCpuUsage") {
        value = metrics.appCpuUsage;
    } else if (metrics.customMetrics.contains(alert.metric)) {
        value = metrics.customMetrics[alert.metric];
    }
    
    return value >= alert.threshold;
}

void PerformanceMonitor::processAlert(const PerformanceAlert& alert, const PerformanceMetrics& metrics)
{
    QDateTime now = QDateTime::currentDateTime();
    QDateTime lastTriggered = alertLastTriggered.value(alert.name);
    
    // 检查是否在冷却期内
    if (lastTriggered.isValid() && lastTriggered.secsTo(now) < alert.duration) {
        return;
    }
    
    alertLastTriggered[alert.name] = now;
    
    QString message = QString("Performance alert '%1' triggered: %2 = %3 (threshold: %4)")
                     .arg(alert.name)
                     .arg(alert.metric)
                     .arg(metrics.customMetrics.value(alert.metric, 0.0))
                     .arg(alert.threshold);
    
    LogManager::getInstance()->warning(message, "PerformanceMonitor");
    
    emit alertTriggered(alert, metrics);
    notifyAlertCallbacks(alert, metrics);
}

void PerformanceMonitor::notifyCallbacks(const PerformanceMetrics& metrics)
{
    QMutexLocker locker(&metricsMutex);
    
    for (auto it = callbacks.begin(); it != callbacks.end(); ++it) {
        try {
            it.value()(metrics);
        } catch (const std::exception& e) {
            qCritical() << "Error in performance callback" << it.key() << ":" << e.what();
        } catch (...) {
            qCritical() << "Unknown error in performance callback" << it.key();
        }
    }
}

void PerformanceMonitor::notifyAlertCallbacks(const PerformanceAlert& alert, const PerformanceMetrics& metrics)
{
    QMutexLocker locker(&metricsMutex);
    
    for (auto it = alertCallbacks.begin(); it != alertCallbacks.end(); ++it) {
        try {
            it.value()(alert, metrics);
        } catch (const std::exception& e) {
            qCritical() << "Error in alert callback" << it.key() << ":" << e.what();
        } catch (...) {
            qCritical() << "Unknown error in alert callback" << it.key();
        }
    }
}