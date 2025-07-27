#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QVariant>
#include <QMap>
#include <QQueue>
#include <QTimer>
#include <QMutex>
#include <QJsonObject>
#include <functional>
#include "constants.h"

// 错误级别枚举
enum class ErrorLevel {
    Debug = 0,      // 调试信息
    Info = 1,       // 一般信息
    Warning = 2,    // 警告
    Error = 3,      // 错误
    Critical = 4,   // 严重错误
    Fatal = 5       // 致命错误
};

// 错误类型枚举
enum class ErrorType {
    Unknown = 0,
    Communication,  // 通讯错误
    Device,         // 设备错误
    Protocol,       // 协议错误
    Configuration,  // 配置错误
    System,         // 系统错误
    Database,       // 数据库错误
    FileSystem,     // 文件系统错误
    Network,        // 网络错误
    Hardware,       // 硬件错误
    Software,       // 软件错误
    User,           // 用户操作错误
    Security,       // 安全错误
    Performance     // 性能错误
};

// 恢复策略枚举
enum class RecoveryStrategy {
    None = 0,       // 无恢复策略
    Retry,          // 重试
    Reconnect,      // 重新连接
    Reset,          // 重置
    Restart,        // 重启
    Fallback,       // 降级处理
    Ignore,         // 忽略错误
    UserIntervention, // 需要用户干预
    Emergency       // 紧急处理
};

// 错误信息结构
struct ErrorInfo {
    int id;                      // 错误ID
    ErrorType type;             // 错误类型
    ErrorLevel level;           // 错误级别
    QString code;               // 错误代码
    QString message;            // 错误消息
    QString description;        // 详细描述
    QString source;             // 错误源
    QString context;            // 错误上下文
    QDateTime timestamp;        // 发生时间
    QVariantMap data;           // 附加数据
    int count;                  // 发生次数
    QDateTime firstOccurrence;  // 首次发生时间
    QDateTime lastOccurrence;   // 最后发生时间
    RecoveryStrategy strategy;  // 恢复策略
    bool isResolved;            // 是否已解决
    QString resolution;         // 解决方案
    
    ErrorInfo() 
        : id(0)
        , type(ErrorType::Unknown)
        , level(ErrorLevel::Error)
        , timestamp(QDateTime::currentDateTime())
        , count(1)
        , firstOccurrence(QDateTime::currentDateTime())
        , lastOccurrence(QDateTime::currentDateTime())
        , strategy(RecoveryStrategy::None)
        , isResolved(false)
    {}
    
    // 转换为JSON
    QJsonObject toJson() const;
    // 从JSON创建
    static ErrorInfo fromJson(const QJsonObject& json);
};

// 恢复动作定义
using RecoveryAction = std::function<bool(const ErrorInfo&)>;

// 错误处理器配置
struct ErrorHandlerConfig {
    bool enableAutoRecovery;     // 启用自动恢复
    bool enableLogging;          // 启用日志记录
    bool enableNotification;     // 启用通知
    int maxRetryAttempts;        // 最大重试次数
    int retryDelay;              // 重试延迟(ms)
    int maxErrorHistory;         // 最大错误历史记录数
    ErrorLevel logLevel;         // 日志级别
    ErrorLevel notificationLevel; // 通知级别
    bool enableStatistics;       // 启用统计
    bool enablePrediction;       // 启用错误预测
    
    ErrorHandlerConfig()
        : enableAutoRecovery(true)
        , enableLogging(true)
        , enableNotification(true)
        , maxRetryAttempts(3)
        , retryDelay(1000)
        , maxErrorHistory(1000)
        , logLevel(ErrorLevel::Warning)
        , notificationLevel(ErrorLevel::Error)
        , enableStatistics(true)
        , enablePrediction(false)
    {}
};

// 错误统计信息
struct ErrorStatistics {
    QMap<ErrorType, int> typeCount;        // 按类型统计
    QMap<ErrorLevel, int> levelCount;      // 按级别统计
    QMap<QString, int> sourceCount;        // 按来源统计
    int totalErrors;                       // 总错误数
    int resolvedErrors;                    // 已解决错误数
    int unresolvedErrors;                  // 未解决错误数
    double averageResolutionTime;          // 平均解决时间(秒)
    QDateTime lastErrorTime;               // 最后错误时间
    QDateTime statisticsStartTime;         // 统计开始时间
    
    ErrorStatistics() 
        : totalErrors(0)
        , resolvedErrors(0)
        , unresolvedErrors(0)
        , averageResolutionTime(0.0)
        , statisticsStartTime(QDateTime::currentDateTime())
    {}
    
    void reset() {
        typeCount.clear();
        levelCount.clear();
        sourceCount.clear();
        totalErrors = 0;
        resolvedErrors = 0;
        unresolvedErrors = 0;
        averageResolutionTime = 0.0;
        statisticsStartTime = QDateTime::currentDateTime();
    }
};

// 智能错误处理器
class ErrorHandler : public QObject
{
    Q_OBJECT

public:
    explicit ErrorHandler(QObject* parent = nullptr);
    ~ErrorHandler();
    
    // 单例模式
    static ErrorHandler* getInstance();
    
    // 错误报告
    int reportError(ErrorType type, ErrorLevel level, const QString& code, 
                   const QString& message, const QString& source = QString(),
                   const QVariantMap& data = QVariantMap());
    int reportError(const ErrorInfo& error);
    
    // 错误处理
    bool handleError(int errorId);
    bool handleError(const ErrorInfo& error);
    void handleAllPendingErrors();
    
    // 恢复策略管理
    void registerRecoveryAction(ErrorType type, RecoveryAction action);
    void registerRecoveryAction(const QString& errorCode, RecoveryAction action);
    void setDefaultRecoveryStrategy(ErrorType type, RecoveryStrategy strategy);
    RecoveryStrategy getRecoveryStrategy(ErrorType type) const;
    
    // 错误查询
    ErrorInfo getError(int errorId) const;
    QList<ErrorInfo> getErrors(ErrorType type = ErrorType::Unknown) const;
    QList<ErrorInfo> getUnresolvedErrors() const;
    QList<ErrorInfo> getRecentErrors(int minutes = 60) const;
    bool hasUnresolvedErrors() const;
    int getErrorCount(ErrorType type = ErrorType::Unknown) const;
    
    // 错误解决
    bool resolveError(int errorId, const QString& resolution = QString());
    void resolveAllErrors();
    void clearResolvedErrors();
    void clearAllErrors();
    
    // 配置管理
    void setConfig(const ErrorHandlerConfig& config);
    ErrorHandlerConfig getConfig() const;
    void updateConfig(const QString& key, const QVariant& value);
    
    // 统计信息
    ErrorStatistics getStatistics() const;
    void resetStatistics();
    QString generateReport() const;
    QString generateSummary() const;
    
    // 预测和分析
    void enablePredictiveAnalysis(bool enabled);
    bool isPredictiveAnalysisEnabled() const;
    QList<ErrorInfo> predictPotentialErrors() const;
    double getErrorTrend(ErrorType type) const;
    
    // 导入导出
    bool exportErrors(const QString& filePath) const;
    bool importErrors(const QString& filePath);
    QJsonObject exportToJson() const;
    bool importFromJson(const QJsonObject& json);
    
    // 日志管理
    void enableLogging(bool enabled);
    bool isLoggingEnabled() const;
    void setLogLevel(ErrorLevel level);
    ErrorLevel getLogLevel() const;

signals:
    // 错误事件信号
    void errorReported(const ErrorInfo& error);
    void errorResolved(int errorId, const QString& resolution);
    void errorHandled(int errorId, bool success);
    void recoveryAttempted(int errorId, RecoveryStrategy strategy, bool success);
    
    // 状态变更信号
    void criticalErrorOccurred(const ErrorInfo& error);
    void fatalErrorOccurred(const ErrorInfo& error);
    void errorThresholdExceeded(ErrorType type, int count);
    void systemHealthChanged(bool healthy);
    
    // 统计信号
    void statisticsUpdated(const ErrorStatistics& stats);
    void errorTrendChanged(ErrorType type, double trend);
    void predictionAvailable(const QList<ErrorInfo>& predictions);

public slots:
    // 系统操作
    void performHealthCheck();
    void clearOldErrors(int days = 30);
    void optimizeDatabase();
    
    // 恢复操作
    void retryFailedOperations();
    void resetToSafeState();
    void performEmergencyShutdown();

private slots:
    // 内部定时器
    void onRetryTimer();
    void onCleanupTimer();
    void onStatisticsTimer();
    void onHealthCheckTimer();

private:
    // 静态实例
    static ErrorHandler* s_instance;
    static QMutex s_mutex;
    
    // 错误存储
    QMap<int, ErrorInfo> m_errors;
    QQueue<int> m_pendingErrors;
    mutable QMutex m_errorsMutex;
    int m_nextErrorId;
    
    // 恢复策略
    QMap<ErrorType, RecoveryStrategy> m_defaultStrategies;
    QMap<ErrorType, RecoveryAction> m_typeActions;
    QMap<QString, RecoveryAction> m_codeActions;
    
    // 配置和统计
    ErrorHandlerConfig m_config;
    ErrorStatistics m_statistics;
    mutable QMutex m_statsMutex;
    
    // 定时器
    QTimer* m_retryTimer;
    QTimer* m_cleanupTimer;
    QTimer* m_statisticsTimer;
    QTimer* m_healthCheckTimer;
    
    // 系统状态
    bool m_systemHealthy;
    QDateTime m_lastHealthCheck;
    
    // 辅助方法
    void initializeDefaultStrategies();
    void initializeDefaultActions();
    void updateStatistics(const ErrorInfo& error);
    bool executeRecoveryAction(const ErrorInfo& error);
    void scheduleRetry(int errorId);
    void logError(const ErrorInfo& error);
    void notifyError(const ErrorInfo& error);
    bool isErrorThresholdExceeded(ErrorType type) const;
    void performPredictiveAnalysis();
    QString errorTypeToString(ErrorType type) const;
    QString errorLevelToString(ErrorLevel level) const;
    QString recoveryStrategyToString(RecoveryStrategy strategy) const;
    ErrorType stringToErrorType(const QString& typeStr) const;
    ErrorLevel stringToErrorLevel(const QString& levelStr) const;
    RecoveryStrategy stringToRecoveryStrategy(const QString& strategyStr) const;
    
    // 禁用复制构造函数和赋值操作符
    ErrorHandler(const ErrorHandler&) = delete;
    ErrorHandler& operator=(const ErrorHandler&) = delete;
}; 