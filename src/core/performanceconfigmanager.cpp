#include "performanceconfigmanager.h"
#include <QFile>
#include <QJsonParseError>
#include <QDebug>
#include <QDateTime>
#include <QProcess>
#include <QApplication>
#include <QDir>
#include <QRandomGenerator>
#include <cstdlib>
#include <functional>

PerformanceConfigManager::PerformanceConfigManager(QObject *parent)
    : QObject(parent)
    , m_monitoringTimer(new QTimer(this))
    , m_monitoringEnabled(false)
    , m_samplingIntervalMs(1000)
{
    // 连接监控定时器
    connect(m_monitoringTimer, &QTimer::timeout, this, &PerformanceConfigManager::onMonitoringTimer);
    
    // 初始化默认阈值
    m_thresholds["memory_usage_percent"] = 80.0;
    m_thresholds["cpu_usage_percent"] = 75.0;
    m_thresholds["database_query_time_ms"] = 100.0;
    m_thresholds["ui_response_time_ms"] = 50.0;
    m_thresholds["communication_timeout_ms"] = 5000.0;
}

PerformanceConfigManager::~PerformanceConfigManager()
{
    stopMonitoring();
}

bool PerformanceConfigManager::loadConfiguration(const QString &configPath)
{
    QMutexLocker locker(&m_configMutex);
    
    QFile file(configPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open performance config file:" << configPath;
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    
    if (error.error != QJsonParseError::NoError) {
        qWarning() << "Failed to parse performance config JSON:" << error.errorString();
        return false;
    }
    
    m_config = doc.object();
    m_configPath = configPath;
    
    // 解析配置
    parseConfiguration(m_config);
    
    qDebug() << "Performance configuration loaded successfully from:" << configPath;
    return true;
}

void PerformanceConfigManager::parseConfiguration(const QJsonObject &jsonObj)
{
    // 解析性能监控配置
    QJsonObject perfMonitoring = jsonObj["performance_monitoring"].toObject();
    m_monitoringEnabled = perfMonitoring["enabled"].toBool(true);
    m_samplingIntervalMs = perfMonitoring["sampling_interval_ms"].toInt(1000);
    
    // 解析阈值配置
    QJsonObject thresholds = perfMonitoring["thresholds"].toObject();
    for (auto it = thresholds.begin(); it != thresholds.end(); ++it) {
        m_thresholds[it.key()] = it.value().toDouble();
    }
    
    // 解析优化参数配置
    QJsonObject optimizationParams = jsonObj["optimization_parameters"].toObject();
    
    // 数据库连接池配置
    QJsonObject dbConfig = optimizationParams["database_connection_pool"].toObject();
    m_optimizationConfig.dbMinConnections = dbConfig["min_connections"].toInt(5);
    m_optimizationConfig.dbMaxConnections = dbConfig["max_connections"].toInt(20);
    m_optimizationConfig.dbConnectionTimeoutMs = dbConfig["connection_timeout_ms"].toInt(30000);
    m_optimizationConfig.dbIdleTimeoutMs = dbConfig["idle_timeout_ms"].toInt(300000);
    
    // 内存优化器配置
    QJsonObject memConfig = optimizationParams["memory_optimizer"].toObject();
    m_optimizationConfig.memoryObjectPoolEnabled = memConfig["enable_object_pool"].toBool(true);
    m_optimizationConfig.memoryTrackingEnabled = memConfig["enable_memory_tracking"].toBool(true);
    m_optimizationConfig.memoryAutoCleanupEnabled = memConfig["enable_auto_cleanup"].toBool(true);
    m_optimizationConfig.memoryCleanupIntervalMs = memConfig["cleanup_interval_ms"].toInt(60000);
    m_optimizationConfig.memoryThresholdMb = memConfig["memory_threshold_mb"].toInt(512);
    
    // UI更新优化器配置
    QJsonObject uiConfig = optimizationParams["ui_update_optimizer"].toObject();
    m_optimizationConfig.uiMaxFps = uiConfig["max_fps"].toInt(60);
    m_optimizationConfig.uiBatchSize = uiConfig["batch_size"].toInt(10);
    m_optimizationConfig.uiUpdateIntervalMs = uiConfig["update_interval_ms"].toInt(16);
    m_optimizationConfig.uiAdaptiveTuningEnabled = uiConfig["enable_adaptive_tuning"].toBool(true);
    
    // 通信缓冲配置
    QJsonObject commConfig = optimizationParams["communication_buffer"].toObject();
    m_optimizationConfig.commBufferSizeKb = commConfig["buffer_size_kb"].toInt(64);
    m_optimizationConfig.commMaxBuffers = commConfig["max_buffers"].toInt(100);
    m_optimizationConfig.commTimeoutMs = commConfig["timeout_ms"].toInt(5000);
    m_optimizationConfig.commCompressionEnabled = commConfig["compression_enabled"].toBool(true);
}

PerformanceConfigManager::OptimizationConfig PerformanceConfigManager::getOptimizationConfig() const
{
    QMutexLocker locker(&m_configMutex);
    return m_optimizationConfig;
}

void PerformanceConfigManager::updateMetrics(const PerformanceMetrics &metrics)
{
    QMutexLocker locker(&m_configMutex);
    
    // 添加到历史记录
    m_metricsHistory.append(metrics);
    
    // 保持历史记录在合理范围内（最多保留1000条记录）
    if (m_metricsHistory.size() > 1000) {
        m_metricsHistory.removeFirst();
    }
    
    // 检查阈值
    QStringList warnings = checkThresholds(metrics);
    for (const QString &warning : warnings) {
        qWarning() << "Performance threshold exceeded:" << warning;
    }
    
    // 发送更新信号
    emit metricsUpdated(metrics);
}

QStringList PerformanceConfigManager::checkThresholds(const PerformanceMetrics &metrics)
{
    QStringList warnings;
    
    if (metrics.memoryUsagePercent > m_thresholds["memory_usage_percent"]) {
        warnings << QString("Memory usage: %1% > %2%")
                    .arg(metrics.memoryUsagePercent)
                    .arg(m_thresholds["memory_usage_percent"]);
        emit performanceWarning("memory_usage", metrics.memoryUsagePercent, m_thresholds["memory_usage_percent"]);
    }
    
    if (metrics.cpuUsagePercent > m_thresholds["cpu_usage_percent"]) {
        warnings << QString("CPU usage: %1% > %2%")
                    .arg(metrics.cpuUsagePercent)
                    .arg(m_thresholds["cpu_usage_percent"]);
        emit performanceWarning("cpu_usage", metrics.cpuUsagePercent, m_thresholds["cpu_usage_percent"]);
    }
    
    if (metrics.databaseQueryTimeMs > m_thresholds["database_query_time_ms"]) {
        warnings << QString("Database query time: %1ms > %2ms")
                    .arg(metrics.databaseQueryTimeMs)
                    .arg(m_thresholds["database_query_time_ms"]);
        emit performanceWarning("database_query_time", metrics.databaseQueryTimeMs, m_thresholds["database_query_time_ms"]);
    }
    
    if (metrics.uiResponseTimeMs > m_thresholds["ui_response_time_ms"]) {
        warnings << QString("UI response time: %1ms > %2ms")
                    .arg(metrics.uiResponseTimeMs)
                    .arg(m_thresholds["ui_response_time_ms"]);
        emit performanceWarning("ui_response_time", metrics.uiResponseTimeMs, m_thresholds["ui_response_time_ms"]);
    }
    
    if (metrics.communicationLatencyMs > m_thresholds["communication_timeout_ms"]) {
        warnings << QString("Communication latency: %1ms > %2ms")
                    .arg(metrics.communicationLatencyMs)
                    .arg(m_thresholds["communication_timeout_ms"]);
        emit performanceWarning("communication_latency", metrics.communicationLatencyMs, m_thresholds["communication_timeout_ms"]);
    }
    
    return warnings;
}

void PerformanceConfigManager::startMonitoring()
{
    if (m_monitoringEnabled && !m_monitoringTimer->isActive()) {
        m_monitoringTimer->start(m_samplingIntervalMs);
        qDebug() << "Performance monitoring started with interval:" << m_samplingIntervalMs << "ms";
    }
}

void PerformanceConfigManager::stopMonitoring()
{
    if (m_monitoringTimer->isActive()) {
        m_monitoringTimer->stop();
        qDebug() << "Performance monitoring stopped";
    }
}

QVariant PerformanceConfigManager::getConfigValue(const QString &key, const QVariant &defaultValue) const
{
    QMutexLocker locker(&m_configMutex);
    
    QStringList keyParts = key.split('.');
    QJsonValue value = m_config;
    
    for (const QString &part : keyParts) {
        if (value.isObject()) {
            value = value.toObject()[part];
        } else {
            return defaultValue;
        }
    }
    
    return value.toVariant();
}

void PerformanceConfigManager::setConfigValue(const QString &key, const QVariant &value)
{
    QMutexLocker locker(&m_configMutex);
    
    QStringList keyParts = key.split('.');
    
    // 递归函数来设置嵌套值
    std::function<void(QJsonObject&, const QStringList&, int, const QVariant&)> setNestedValue = 
        [&](QJsonObject& obj, const QStringList& parts, int index, const QVariant& val) {
            if (index == parts.size() - 1) {
                obj[parts[index]] = QJsonValue::fromVariant(val);
                return;
            }
            
            const QString& part = parts[index];
            if (!obj.contains(part) || !obj[part].isObject()) {
                obj[part] = QJsonObject();
            }
            
            QJsonObject nestedObj = obj[part].toObject();
            setNestedValue(nestedObj, parts, index + 1, val);
            obj[part] = nestedObj;
        };
    
    setNestedValue(m_config, keyParts, 0, value);
    
    emit configurationUpdated(key, value);
}

bool PerformanceConfigManager::saveConfiguration()
{
    QMutexLocker locker(&m_configMutex);
    
    if (m_configPath.isEmpty()) {
        qWarning() << "No config path specified for saving";
        return false;
    }
    
    QFile file(m_configPath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "Failed to open config file for writing:" << m_configPath;
        return false;
    }
    
    QJsonDocument doc(m_config);
    file.write(doc.toJson());
    file.close();
    
    qDebug() << "Performance configuration saved to:" << m_configPath;
    return true;
}

void PerformanceConfigManager::onMonitoringTimer()
{
    PerformanceMetrics metrics = collectSystemMetrics();
    updateMetrics(metrics);
}

PerformanceConfigManager::PerformanceMetrics PerformanceConfigManager::collectSystemMetrics()
{
    PerformanceMetrics metrics;
    metrics.timestamp = QDateTime::currentMSecsSinceEpoch();
    
    // 获取内存使用情况（简化实现）
    QProcess memProcess;
    memProcess.start("ps", QStringList() << "-o" << "pid,pmem" << "-p" << QString::number(QApplication::applicationPid()));
    memProcess.waitForFinished(1000);
    QString memOutput = memProcess.readAllStandardOutput();
    QStringList memLines = memOutput.split('\n');
    if (memLines.size() > 1) {
        QStringList memParts = memLines[1].split(' ', Qt::SkipEmptyParts);
        if (memParts.size() >= 2) {
            metrics.memoryUsagePercent = memParts[1].toDouble();
        }
    }
    
    // 获取CPU使用情况（简化实现）
    QProcess cpuProcess;
    cpuProcess.start("ps", QStringList() << "-o" << "pid,pcpu" << "-p" << QString::number(QApplication::applicationPid()));
    cpuProcess.waitForFinished(1000);
    QString cpuOutput = cpuProcess.readAllStandardOutput();
    QStringList cpuLines = cpuOutput.split('\n');
    if (cpuLines.size() > 1) {
        QStringList cpuParts = cpuLines[1].split(' ', Qt::SkipEmptyParts);
        if (cpuParts.size() >= 2) {
            metrics.cpuUsagePercent = cpuParts[1].toDouble();
        }
    }
    
    // 模拟其他指标（实际应用中应该从相应组件获取）
    QRandomGenerator *generator = QRandomGenerator::global();
    metrics.databaseQueryTimeMs = generator->bounded(50) + 10; // 10-60ms
    metrics.uiResponseTimeMs = generator->bounded(30) + 5;      // 5-35ms
    metrics.communicationLatencyMs = generator->bounded(100) + 20; // 20-120ms
    
    return metrics;
}