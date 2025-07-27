#include "adaptiveconfigmanager.h"
#include "continuousoptimizer.h"
#include "intelligentanalyzer.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFile>
#include <QStandardPaths>
#include <QDir>
#include <QtMath>
#include <QRandomGenerator>
#include <algorithm>

AdaptiveConfigManager::AdaptiveConfigManager(QObject *parent)
    : QObject(parent)
    , m_optimizer(nullptr)
    , m_analyzer(nullptr)
    , m_adjustmentTimer(new QTimer(this))
    , m_evaluationTimer(new QTimer(this))
    , m_isRunning(false)
    , m_strategy(AdjustmentStrategy::Moderate)
    , m_adjustmentInterval(300000)  // 5分钟
    , m_evaluationInterval(600000)  // 10分钟
    , m_maxHistorySize(500)
    , m_improvementThreshold(0.05)
    , m_learningRate(0.1)
    , m_adaptationFactor(0.8)
    , m_totalAdjustments(0)
    , m_successfulAdjustments(0)
    , m_failedAdjustments(0)
{
    // 连接定时器
    connect(m_adjustmentTimer, &QTimer::timeout, this, &AdaptiveConfigManager::performPeriodicAdjustment);
    connect(m_evaluationTimer, &QTimer::timeout, this, &AdaptiveConfigManager::evaluateAdjustmentEffects);
    
    // 注册默认参数
    registerDefaultParameters();
    
    qDebug() << "[AdaptiveConfigManager] 自适应配置管理器已创建";
}

AdaptiveConfigManager::~AdaptiveConfigManager()
{
    stopAdaptiveAdjustment();
    saveConfigurationState();
    qDebug() << "[AdaptiveConfigManager] 自适应配置管理器已销毁";
}

bool AdaptiveConfigManager::initialize(ContinuousOptimizer* optimizer, IntelligentAnalyzer* analyzer)
{
    if (!optimizer || !analyzer) {
        qWarning() << "[AdaptiveConfigManager] 初始化失败：组件为空";
        return false;
    }
    
    m_optimizer = optimizer;
    m_analyzer = analyzer;
    
    // 加载配置状态
    loadConfigurationState();
    
    qDebug() << "[AdaptiveConfigManager] 初始化成功";
    return true;
}

bool AdaptiveConfigManager::registerParameter(const ParameterDefinition& definition)
{
    QMutexLocker locker(&m_parametersMutex);
    
    if (definition.name.isEmpty()) {
        qWarning() << "[AdaptiveConfigManager] 参数名称不能为空";
        return false;
    }
    
    if (m_parameters.contains(definition.name)) {
        qWarning() << "[AdaptiveConfigManager] 参数已存在:" << definition.name;
        return false;
    }
    
    m_parameters[definition.name] = definition;
    
    qDebug() << "[AdaptiveConfigManager] 已注册参数:" << definition.name 
             << "默认值:" << definition.defaultValue;
    
    return true;
}

QVariant AdaptiveConfigManager::getParameterValue(const QString& name) const
{
    QMutexLocker locker(&m_parametersMutex);
    
    if (!m_parameters.contains(name)) {
        qWarning() << "[AdaptiveConfigManager] 参数不存在:" << name;
        return QVariant();
    }
    
    return m_parameters[name].currentValue;
}

bool AdaptiveConfigManager::setParameterValue(const QString& name, const QVariant& value, const QString& reason)
{
    QMutexLocker locker(&m_parametersMutex);
    
    if (!m_parameters.contains(name)) {
        qWarning() << "[AdaptiveConfigManager] 参数不存在:" << name;
        return false;
    }
    
    auto& parameter = m_parameters[name];
    
    if (!validateParameterValue(parameter, value)) {
        qWarning() << "[AdaptiveConfigManager] 参数值无效:" << name << value;
        return false;
    }
    
    QVariant oldValue = parameter.currentValue;
    parameter.currentValue = value;
    
    // 记录调整
    AdjustmentRecord record;
    record.parameterName = name;
    record.oldValue = oldValue;
    record.newValue = value;
    record.reason = reason.isEmpty() ? "手动设置" : reason;
    record.timestamp = QDateTime::currentDateTime();
    record.expectedImprovement = 0.0;
    record.actualImprovement = 0.0;
    record.successful = true;
    
    QMutexLocker historyLocker(&m_historyMutex);
    m_adjustmentHistory.append(record);
    
    // 限制历史记录大小
    while (m_adjustmentHistory.size() > m_maxHistorySize) {
        m_adjustmentHistory.removeFirst();
    }
    
    emit parameterChanged(name, oldValue, value, record.reason);
    
    qDebug() << "[AdaptiveConfigManager] 参数已更新:" << name 
             << oldValue << "->" << value << "原因:" << record.reason;
    
    return true;
}

void AdaptiveConfigManager::startAdaptiveAdjustment()
{
    if (m_isRunning) {
        qDebug() << "[AdaptiveConfigManager] 自适应调整已在运行中";
        return;
    }
    
    m_isRunning = true;
    
    // 启动定时器
    m_adjustmentTimer->start(m_adjustmentInterval);
    m_evaluationTimer->start(m_evaluationInterval);
    
    qDebug() << "[AdaptiveConfigManager] 自适应调整已启动";
}

void AdaptiveConfigManager::stopAdaptiveAdjustment()
{
    if (!m_isRunning) {
        return;
    }
    
    m_isRunning = false;
    
    // 停止定时器
    m_adjustmentTimer->stop();
    m_evaluationTimer->stop();
    
    qDebug() << "[AdaptiveConfigManager] 自适应调整已停止";
}

void AdaptiveConfigManager::setAdjustmentStrategy(AdjustmentStrategy strategy)
{
    m_strategy = strategy;
    
    // 根据策略调整间隔
    switch (strategy) {
    case AdjustmentStrategy::Conservative:
        m_adjustmentInterval = 600000;  // 10分钟
        m_improvementThreshold = 0.1;
        break;
    case AdjustmentStrategy::Moderate:
        m_adjustmentInterval = 300000;  // 5分钟
        m_improvementThreshold = 0.05;
        break;
    case AdjustmentStrategy::Aggressive:
        m_adjustmentInterval = 120000;  // 2分钟
        m_improvementThreshold = 0.02;
        break;
    case AdjustmentStrategy::Adaptive:
        // 自适应策略会根据效果动态调整间隔
        break;
    }
    
    if (m_isRunning && strategy != AdjustmentStrategy::Adaptive) {
        m_adjustmentTimer->setInterval(m_adjustmentInterval);
    }
    
    qDebug() << "[AdaptiveConfigManager] 调整策略已设置为:" << static_cast<int>(strategy);
}

void AdaptiveConfigManager::updatePerformanceState(const PerformanceState& state)
{
    QMutexLocker locker(&m_performanceMutex);
    
    m_performanceHistory.append(state);
    
    // 限制历史记录大小
    while (m_performanceHistory.size() > m_maxHistorySize) {
        m_performanceHistory.removeFirst();
    }
    
    // 如果是自适应策略，根据性能变化调整间隔
    if (m_strategy == AdjustmentStrategy::Adaptive && m_performanceHistory.size() >= 2) {
        auto& current = m_performanceHistory.last();
        auto& previous = m_performanceHistory[m_performanceHistory.size() - 2];
        
        double improvement = calculatePerformanceImprovement(previous, current);
        
        if (improvement > 0.1) {
            // 性能改善显著，可以减少调整频率
            m_adjustmentInterval = qMin(600000, static_cast<int>(m_adjustmentInterval * 1.2));
        } else if (improvement < -0.05) {
            // 性能下降，增加调整频率
            m_adjustmentInterval = qMax(60000, static_cast<int>(m_adjustmentInterval * 0.8));
        }
        
        if (m_isRunning) {
            m_adjustmentTimer->setInterval(m_adjustmentInterval);
        }
    }
}

int AdaptiveConfigManager::triggerParameterAdjustment(ParameterType parameterType)
{
    if (m_performanceHistory.isEmpty()) {
        qWarning() << "[AdaptiveConfigManager] 无性能数据，无法进行调整";
        return 0;
    }
    
    auto currentState = m_performanceHistory.last();
    int adjustedCount = 0;
    
    QMutexLocker locker(&m_parametersMutex);
    
    for (auto it = m_parameters.begin(); it != m_parameters.end(); ++it) {
        auto& parameter = it.value();
        
        // 检查参数类型匹配
        if (parameterType != ParameterType::System && parameter.type != parameterType) {
            continue;
        }
        
        // 检查是否允许自动调整
        if (!parameter.autoAdjust) {
            continue;
        }
        
        // 计算调整值
        QVariant newValue = calculateAdjustmentValue(parameter, currentState);
        
        if (newValue != parameter.currentValue) {
            QString reason = QString("自适应调整 - 基于%1策略")
                           .arg(static_cast<int>(m_strategy));
            
            if (applyParameterAdjustment(it.key(), newValue, reason)) {
                adjustedCount++;
            }
        }
    }
    
    m_totalAdjustments += adjustedCount;
    m_lastAdjustmentTime = QDateTime::currentDateTime();
    
    emit adaptiveAdjustmentCompleted(adjustedCount, m_parameters.size());
    
    qDebug() << "[AdaptiveConfigManager] 参数调整完成，调整了" << adjustedCount << "个参数";
    
    return adjustedCount;
}

QList<AdaptiveConfigManager::AdjustmentRecord> AdaptiveConfigManager::getAdjustmentHistory(
    const QString& parameterName, int limit) const
{
    QMutexLocker locker(&m_historyMutex);
    
    QList<AdjustmentRecord> result;
    
    for (int i = m_adjustmentHistory.size() - 1; i >= 0 && result.size() < limit; --i) {
        const auto& record = m_adjustmentHistory[i];
        
        if (parameterName.isEmpty() || record.parameterName == parameterName) {
            result.append(record);
        }
    }
    
    return result;
}

QJsonObject AdaptiveConfigManager::getParameterStatistics() const
{
    QJsonObject stats;
    
    // 基本统计
    stats["timestamp"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    stats["total_parameters"] = m_parameters.size();
    stats["total_adjustments"] = m_totalAdjustments;
    stats["successful_adjustments"] = m_successfulAdjustments;
    stats["failed_adjustments"] = m_failedAdjustments;
    stats["success_rate"] = m_totalAdjustments > 0 ? 
        static_cast<double>(m_successfulAdjustments) / m_totalAdjustments : 0.0;
    stats["last_adjustment"] = m_lastAdjustmentTime.toString(Qt::ISODate);
    
    // 参数类型统计
    QJsonObject typeStats;
    QHash<ParameterType, int> typeCounts;
    
    QMutexLocker locker(&m_parametersMutex);
    for (const auto& parameter : m_parameters) {
        typeCounts[parameter.type]++;
    }
    
    typeStats["memory"] = typeCounts[ParameterType::Memory];
    typeStats["database"] = typeCounts[ParameterType::Database];
    typeStats["ui"] = typeCounts[ParameterType::UI];
    typeStats["communication"] = typeCounts[ParameterType::Communication];
    typeStats["performance"] = typeCounts[ParameterType::Performance];
    typeStats["system"] = typeCounts[ParameterType::System];
    
    stats["parameter_types"] = typeStats;
    
    // 最近调整统计
    QJsonArray recentAdjustments;
    auto recentHistory = getAdjustmentHistory(QString(), 10);
    for (const auto& record : recentHistory) {
        QJsonObject recordObj;
        recordObj["parameter"] = record.parameterName;
        recordObj["timestamp"] = record.timestamp.toString(Qt::ISODate);
        recordObj["successful"] = record.successful;
        recordObj["improvement"] = record.actualImprovement;
        recentAdjustments.append(recordObj);
    }
    stats["recent_adjustments"] = recentAdjustments;
    
    return stats;
}

bool AdaptiveConfigManager::exportConfiguration(const QString& filePath) const
{
    QJsonObject config;
    
    // 导出参数定义和当前值
    QJsonObject parameters;
    QMutexLocker locker(&m_parametersMutex);
    
    for (auto it = m_parameters.begin(); it != m_parameters.end(); ++it) {
        const auto& param = it.value();
        
        QJsonObject paramObj;
        paramObj["type"] = static_cast<int>(param.type);
        paramObj["default_value"] = param.defaultValue.toString();
        paramObj["min_value"] = param.minValue.toString();
        paramObj["max_value"] = param.maxValue.toString();
        paramObj["current_value"] = param.currentValue.toString();
        paramObj["description"] = param.description;
        paramObj["sensitivity"] = param.sensitivity;
        paramObj["auto_adjust"] = param.autoAdjust;
        
        parameters[it.key()] = paramObj;
    }
    
    config["parameters"] = parameters;
    config["strategy"] = static_cast<int>(m_strategy);
    config["adjustment_interval"] = m_adjustmentInterval;
    config["evaluation_interval"] = m_evaluationInterval;
    config["improvement_threshold"] = m_improvementThreshold;
    config["learning_rate"] = m_learningRate;
    config["adaptation_factor"] = m_adaptationFactor;
    config["exported_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    
    QJsonDocument doc(config);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[AdaptiveConfigManager] 无法导出配置到" << filePath;
        return false;
    }
    
    file.write(doc.toJson());
    file.close();
    
    qDebug() << "[AdaptiveConfigManager] 配置已导出到" << filePath;
    return true;
}

bool AdaptiveConfigManager::importConfiguration(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "[AdaptiveConfigManager] 无法导入配置从" << filePath;
        return false;
    }
    
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    file.close();
    
    if (doc.isNull() || !doc.isObject()) {
        qWarning() << "[AdaptiveConfigManager] 配置文件格式错误";
        return false;
    }
    
    QJsonObject config = doc.object();
    
    // 导入参数
    if (config.contains("parameters")) {
        QJsonObject parameters = config["parameters"].toObject();
        
        QMutexLocker locker(&m_parametersMutex);
        
        for (auto it = parameters.begin(); it != parameters.end(); ++it) {
            QString paramName = it.key();
            QJsonObject paramObj = it.value().toObject();
            
            if (m_parameters.contains(paramName)) {
                auto& param = m_parameters[paramName];
                
                // 只更新当前值，保持其他定义不变
                QVariant newValue = paramObj["current_value"].toVariant();
                if (validateParameterValue(param, newValue)) {
                    param.currentValue = newValue;
                }
            }
        }
    }
    
    // 导入其他设置
    if (config.contains("strategy")) {
        m_strategy = static_cast<AdjustmentStrategy>(config["strategy"].toInt());
    }
    if (config.contains("adjustment_interval")) {
        m_adjustmentInterval = config["adjustment_interval"].toInt();
    }
    if (config.contains("evaluation_interval")) {
        m_evaluationInterval = config["evaluation_interval"].toInt();
    }
    if (config.contains("improvement_threshold")) {
        m_improvementThreshold = config["improvement_threshold"].toDouble();
    }
    if (config.contains("learning_rate")) {
        m_learningRate = config["learning_rate"].toDouble();
    }
    if (config.contains("adaptation_factor")) {
        m_adaptationFactor = config["adaptation_factor"].toDouble();
    }
    
    qDebug() << "[AdaptiveConfigManager] 配置已导入从" << filePath;
    return true;
}

void AdaptiveConfigManager::resetToDefaults()
{
    QMutexLocker locker(&m_parametersMutex);
    
    int resetCount = 0;
    
    for (auto it = m_parameters.begin(); it != m_parameters.end(); ++it) {
        auto& parameter = it.value();
        
        if (parameter.currentValue != parameter.defaultValue) {
            QVariant oldValue = parameter.currentValue;
            parameter.currentValue = parameter.defaultValue;
            
            emit parameterChanged(it.key(), oldValue, parameter.defaultValue, "重置为默认值");
            resetCount++;
        }
    }
    
    qDebug() << "[AdaptiveConfigManager] 已重置" << resetCount << "个参数为默认值";
}

QStringList AdaptiveConfigManager::getOptimizationSuggestions() const
{
    QStringList suggestions;
    
    if (m_performanceHistory.isEmpty()) {
        suggestions.append("需要更多性能数据来生成建议");
        return suggestions;
    }
    
    auto currentState = m_performanceHistory.last();
    
    // 基于当前性能状态生成建议
    if (currentState.cpuUsage > 80.0) {
        suggestions.append("CPU使用率过高，建议优化CPU密集型参数");
    }
    
    if (currentState.memoryUsage > 2000.0) {
        suggestions.append("内存使用量过高，建议调整内存相关参数");
    }
    
    if (currentState.dbResponseTime > 20.0) {
        suggestions.append("数据库响应时间过长，建议优化数据库参数");
    }
    
    if (currentState.uiResponseTime > 2.0) {
        suggestions.append("UI响应时间过长，建议调整UI更新参数");
    }
    
    if (currentState.communicationLatency > 5.0) {
        suggestions.append("通信延迟过高，建议优化通信参数");
    }
    
    // 基于调整历史生成建议
    auto recentHistory = getAdjustmentHistory(QString(), 10);
    int failedCount = 0;
    for (const auto& record : recentHistory) {
        if (!record.successful) {
            failedCount++;
        }
    }
    
    if (failedCount > recentHistory.size() * 0.3) {
        suggestions.append("最近调整失败率较高，建议采用更保守的调整策略");
    }
    
    if (suggestions.isEmpty()) {
        suggestions.append("当前系统运行良好，无需特殊优化");
    }
    
    return suggestions;
}

void AdaptiveConfigManager::applyIntelligentRecommendations(const QJsonArray& recommendations)
{
    int appliedCount = 0;
    
    for (const auto& recValue : recommendations) {
        QJsonObject rec = recValue.toObject();
        
        QString parameterName = rec["parameter"].toString();
        QVariant recommendedValue = rec["recommended_value"].toVariant();
        QString reason = rec["reason"].toString();
        
        if (setParameterValue(parameterName, recommendedValue, "智能建议: " + reason)) {
            appliedCount++;
        }
    }
    
    qDebug() << "[AdaptiveConfigManager] 应用智能建议完成:" << appliedCount << "/" << recommendations.size();
}

int AdaptiveConfigManager::rollbackRecentAdjustments(int count)
{
    QMutexLocker historyLocker(&m_historyMutex);
    QMutexLocker paramLocker(&m_parametersMutex);
    
    int rolledBackCount = 0;
    
    // 从最近的调整开始回滚
    for (int i = m_adjustmentHistory.size() - 1; i >= 0 && rolledBackCount < count; --i) {
        const auto& record = m_adjustmentHistory[i];
        
        if (m_parameters.contains(record.parameterName)) {
            auto& parameter = m_parameters[record.parameterName];
            
            // 回滚到旧值
            QVariant currentValue = parameter.currentValue;
            parameter.currentValue = record.oldValue;
            
            emit parameterChanged(record.parameterName, currentValue, record.oldValue, 
                                "回滚调整");
            
            rolledBackCount++;
        }
    }
    
    qDebug() << "[AdaptiveConfigManager] 已回滚" << rolledBackCount << "个调整";
    
    return rolledBackCount;
}

void AdaptiveConfigManager::performPeriodicAdjustment()
{
    if (!m_optimizer || !m_analyzer) {
        return;
    }
    
    // 触发所有类型的参数调整
    int adjustedCount = triggerParameterAdjustment(ParameterType::System);
    
    // 学习调整模式
    learnAdjustmentPatterns();
    
    qDebug() << "[AdaptiveConfigManager] 定期调整完成，调整了" << adjustedCount << "个参数";
}

void AdaptiveConfigManager::evaluateAdjustmentEffects()
{
    if (m_performanceHistory.size() < 2) {
        return;
    }
    
    // 评估最近调整的效果
    auto recentAdjustments = getAdjustmentHistory(QString(), 5);
    
    for (auto& record : recentAdjustments) {
        if (record.actualImprovement == 0.0) { // 尚未评估
            // 查找调整前后的性能数据
            PerformanceState beforeState, afterState;
            bool foundBefore = false, foundAfter = false;
            
            QMutexLocker locker(&m_performanceMutex);
            
            for (const auto& state : m_performanceHistory) {
                if (state.timestamp <= record.timestamp && !foundBefore) {
                    beforeState = state;
                    foundBefore = true;
                } else if (state.timestamp > record.timestamp && !foundAfter) {
                    afterState = state;
                    foundAfter = true;
                    break;
                }
            }
            
            if (foundBefore && foundAfter) {
                double improvement = calculatePerformanceImprovement(beforeState, afterState);
                
                // 更新调整记录
                QMutexLocker historyLocker(&m_historyMutex);
                for (auto& historyRecord : m_adjustmentHistory) {
                    if (historyRecord.parameterName == record.parameterName && 
                        historyRecord.timestamp == record.timestamp) {
                        historyRecord.actualImprovement = improvement;
                        historyRecord.successful = improvement >= -m_improvementThreshold;
                        
                        if (improvement > m_improvementThreshold) {
                            m_successfulAdjustments++;
                            emit performanceImproved(record.parameterName, improvement);
                        } else if (improvement < -m_improvementThreshold) {
                            m_failedAdjustments++;
                            emit adjustmentFailed(record.parameterName, "性能下降");
                        }
                        
                        break;
                    }
                }
            }
        }
    }
    
    qDebug() << "[AdaptiveConfigManager] 调整效果评估完成";
}

// 私有方法实现
QVariant AdaptiveConfigManager::calculateAdjustmentValue(const ParameterDefinition& parameter, 
                                                        const PerformanceState& performanceState) const
{
    QVariant newValue = parameter.currentValue;
    
    // 根据参数类型和性能状态计算调整值
    switch (parameter.type) {
    case ParameterType::Memory:
        if (performanceState.memoryUsage > 2000.0) {
            // 内存使用过高，尝试减少相关参数
            if (parameter.name.contains("threshold") || parameter.name.contains("limit")) {
                double currentVal = parameter.currentValue.toDouble();
                double adjustment = currentVal * 0.1 * getStrategyFactor();
                newValue = qMax(parameter.minValue.toDouble(), currentVal - adjustment);
            }
        }
        break;
        
    case ParameterType::Database:
        if (performanceState.dbResponseTime > 15.0) {
            // 数据库响应慢，尝试增加连接池等参数
            if (parameter.name.contains("pool") || parameter.name.contains("connection")) {
                int currentVal = parameter.currentValue.toInt();
                int adjustment = qMax(1, static_cast<int>(currentVal * 0.2 * getStrategyFactor()));
                newValue = qMin(parameter.maxValue.toInt(), currentVal + adjustment);
            }
        }
        break;
        
    case ParameterType::UI:
        if (performanceState.uiResponseTime > 2.0) {
            // UI响应慢，尝试增加更新间隔
            if (parameter.name.contains("interval") || parameter.name.contains("delay")) {
                int currentVal = parameter.currentValue.toInt();
                int adjustment = qMax(10, static_cast<int>(currentVal * 0.15 * getStrategyFactor()));
                newValue = qMin(parameter.maxValue.toInt(), currentVal + adjustment);
            }
        }
        break;
        
    case ParameterType::Communication:
        if (performanceState.communicationLatency > 3.0) {
            // 通信延迟高，尝试增加缓冲区大小
            if (parameter.name.contains("buffer") || parameter.name.contains("size")) {
                int currentVal = parameter.currentValue.toInt();
                int adjustment = qMax(64, static_cast<int>(currentVal * 0.25 * getStrategyFactor()));
                newValue = qMin(parameter.maxValue.toInt(), currentVal + adjustment);
            }
        }
        break;
        
    default:
        break;
    }
    
    return newValue;
}

bool AdaptiveConfigManager::validateParameterValue(const ParameterDefinition& parameter, const QVariant& value) const
{
    // 根据参数类型进行比较
    switch (parameter.type) {
        case ParameterType::Memory:
        case ParameterType::Database:
        case ParameterType::UI:
        case ParameterType::Communication:
        case ParameterType::Performance:
        case ParameterType::System: {
            // 对于数值类型参数
            if (value.type() == QVariant::Int || value.type() == QVariant::UInt) {
                int intValue = value.toInt();
                if (parameter.minValue.isValid() && intValue < parameter.minValue.toInt()) {
                    return false;
                }
                if (parameter.maxValue.isValid() && intValue > parameter.maxValue.toInt()) {
                    return false;
                }
            } else if (value.type() == QVariant::Double) {
                double doubleValue = value.toDouble();
                if (parameter.minValue.isValid() && doubleValue < parameter.minValue.toDouble()) {
                    return false;
                }
                if (parameter.maxValue.isValid() && doubleValue > parameter.maxValue.toDouble()) {
                    return false;
                }
            } else if (value.type() == QVariant::String) {
                // 字符串类型可以添加长度检查
                if (parameter.maxValue.isValid() && value.toString().length() > parameter.maxValue.toInt()) {
                    return false;
                }
            }
            break;
        }
    }
    
    return true;
}

bool AdaptiveConfigManager::applyParameterAdjustment(const QString& name, const QVariant& newValue, const QString& reason)
{
    // 预测调整效果
    double predictedEffect = predictAdjustmentEffect(name, newValue);
    
    // 记录调整
    AdjustmentRecord record;
    record.parameterName = name;
    record.oldValue = m_parameters[name].currentValue;
    record.newValue = newValue;
    record.reason = reason;
    record.timestamp = QDateTime::currentDateTime();
    record.expectedImprovement = predictedEffect;
    record.actualImprovement = 0.0; // 稍后评估
    record.successful = true;
    
    // 应用调整
    QVariant oldValue = m_parameters[name].currentValue;
    m_parameters[name].currentValue = newValue;
    
    QMutexLocker locker(&m_historyMutex);
    m_adjustmentHistory.append(record);
    
    emit parameterChanged(name, oldValue, newValue, reason);
    
    return true;
}

double AdaptiveConfigManager::calculatePerformanceImprovement(const PerformanceState& beforeState, 
                                                             const PerformanceState& afterState) const
{
    // 计算综合性能改善
    double cpuImprovement = (beforeState.cpuUsage - afterState.cpuUsage) / 100.0;
    double memoryImprovement = (beforeState.memoryUsage - afterState.memoryUsage) / 3000.0;
    double dbImprovement = (beforeState.dbResponseTime - afterState.dbResponseTime) / 50.0;
    double uiImprovement = (beforeState.uiResponseTime - afterState.uiResponseTime) / 5.0;
    double commImprovement = (beforeState.communicationLatency - afterState.communicationLatency) / 10.0;
    double overallImprovement = (afterState.overallScore - beforeState.overallScore) / 100.0;
    
    // 加权平均
    double totalImprovement = (cpuImprovement * 0.2 + memoryImprovement * 0.2 + 
                              dbImprovement * 0.25 + uiImprovement * 0.15 + 
                              commImprovement * 0.1 + overallImprovement * 0.1);
    
    return qBound(-1.0, totalImprovement, 1.0);
}

void AdaptiveConfigManager::learnAdjustmentPatterns()
{
    // 简化的模式学习
    auto recentHistory = getAdjustmentHistory(QString(), 20);
    
    if (recentHistory.size() < 10) {
        return;
    }
    
    // 分析成功调整的模式
    int successfulCount = 0;
    double avgImprovement = 0.0;
    
    for (const auto& record : recentHistory) {
        if (record.successful && record.actualImprovement > 0) {
            successfulCount++;
            avgImprovement += record.actualImprovement;
        }
    }
    
    if (successfulCount > 0) {
        avgImprovement /= successfulCount;
        
        // 根据成功率调整学习参数
        double successRate = static_cast<double>(successfulCount) / recentHistory.size();
        
        if (successRate > 0.8) {
            // 成功率高，可以更激进
            m_adaptationFactor = qMin(1.0, m_adaptationFactor * 1.1);
        } else if (successRate < 0.4) {
            // 成功率低，需要更保守
            m_adaptationFactor = qMax(0.1, m_adaptationFactor * 0.9);
        }
    }
    
    qDebug() << "[AdaptiveConfigManager] 学习完成，适应因子:" << m_adaptationFactor;
}

double AdaptiveConfigManager::predictAdjustmentEffect(const QString& parameterName, const QVariant& newValue) const
{
    // 简化的效果预测
    if (!m_parameters.contains(parameterName)) {
        return 0.0;
    }
    
    const auto& parameter = m_parameters[parameterName];
    double currentVal = parameter.currentValue.toDouble();
    double newVal = newValue.toDouble();
    double change = qAbs(newVal - currentVal) / qMax(1.0, currentVal);
    
    // 基于参数敏感度和历史数据预测效果
    double predictedEffect = change * parameter.sensitivity * m_adaptationFactor;
    
    return qBound(-0.5, predictedEffect, 0.5);
}

void AdaptiveConfigManager::saveConfigurationState()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/adaptive_config.json";
    
    QDir().mkpath(QFileInfo(configPath).absolutePath());
    
    exportConfiguration(configPath);
}

void AdaptiveConfigManager::loadConfigurationState()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/adaptive_config.json";
    
    if (QFile::exists(configPath)) {
        importConfiguration(configPath);
    }
}

double AdaptiveConfigManager::getStrategyFactor() const
{
    switch (m_strategy) {
    case AdjustmentStrategy::Conservative:
        return 0.5;
    case AdjustmentStrategy::Moderate:
        return 1.0;
    case AdjustmentStrategy::Aggressive:
        return 2.0;
    case AdjustmentStrategy::Adaptive:
        return m_adaptationFactor;
    default:
        return 1.0;
    }
}

void AdaptiveConfigManager::registerDefaultParameters()
{
    // 注册默认的系统参数
    
    // 内存相关参数
    ParameterDefinition memoryThreshold;
    memoryThreshold.name = "memory_cleanup_threshold";
    memoryThreshold.type = ParameterType::Memory;
    memoryThreshold.defaultValue = 1800;
    memoryThreshold.minValue = 1000;
    memoryThreshold.maxValue = 3000;
    memoryThreshold.currentValue = 1800;
    memoryThreshold.description = "内存清理阈值(MB)";
    memoryThreshold.sensitivity = 0.8;
    memoryThreshold.autoAdjust = true;
    registerParameter(memoryThreshold);
    
    // 数据库相关参数
    ParameterDefinition dbPoolSize;
    dbPoolSize.name = "database_connection_pool_size";
    dbPoolSize.type = ParameterType::Database;
    dbPoolSize.defaultValue = 5;
    dbPoolSize.minValue = 2;
    dbPoolSize.maxValue = 20;
    dbPoolSize.currentValue = 5;
    dbPoolSize.description = "数据库连接池大小";
    dbPoolSize.sensitivity = 0.7;
    dbPoolSize.autoAdjust = true;
    registerParameter(dbPoolSize);
    
    // UI相关参数
    ParameterDefinition uiUpdateInterval;
    uiUpdateInterval.name = "ui_update_interval";
    uiUpdateInterval.type = ParameterType::UI;
    uiUpdateInterval.defaultValue = 100;
    uiUpdateInterval.minValue = 50;
    uiUpdateInterval.maxValue = 500;
    uiUpdateInterval.currentValue = 100;
    uiUpdateInterval.description = "UI更新间隔(ms)";
    uiUpdateInterval.sensitivity = 0.6;
    uiUpdateInterval.autoAdjust = true;
    registerParameter(uiUpdateInterval);
    
    // 通信相关参数
    ParameterDefinition commBufferSize;
    commBufferSize.name = "communication_buffer_size";
    commBufferSize.type = ParameterType::Communication;
    commBufferSize.defaultValue = 1024;
    commBufferSize.minValue = 512;
    commBufferSize.maxValue = 8192;
    commBufferSize.currentValue = 1024;
    commBufferSize.description = "通信缓冲区大小(bytes)";
    commBufferSize.sensitivity = 0.5;
    commBufferSize.autoAdjust = true;
    registerParameter(commBufferSize);
    
    qDebug() << "[AdaptiveConfigManager] 默认参数已注册";
}