#pragma once

#include <QObject>
#include <QString>
#include <QDateTime>
#include <QMutex>
#include <QTimer>
#include <QQueue>
#include <functional>

/**
 * @brief 错误级别枚举
 */
enum class ErrorLevel {
    Info = 0,
    Warning = 1,
    Error = 2,
    Critical = 3,
    Fatal = 4
};

/**
 * @brief 错误条目结构
 */
struct ErrorEntry {
    QDateTime timestamp;
    ErrorLevel level;
    QString category;
    QString message;
    QString source;
    QString details;
    bool handled;
};

/**
 * @brief 错误处理回调函数类型
 */
using ErrorCallback = std::function<void(const ErrorEntry&)>;

/**
 * @brief 统一错误处理器
 * 
 * 提供应用程序级别的统一错误处理机制，包括：
 * - 错误分级处理
 * - 错误记录和统计
 * - 自动恢复机制
 * - 错误通知和回调
 * - 错误历史管理
 */
class ErrorHandler : public QObject
{
    Q_OBJECT

public:
    static ErrorHandler* getInstance();
    
    // 错误报告方法
    void reportError(ErrorLevel level, const QString& category, 
                    const QString& message, const QString& source = QString(),
                    const QString& details = QString());
    
    void reportInfo(const QString& message, const QString& category = "App");
    void reportWarning(const QString& message, const QString& category = "App");
    void reportError(const QString& message, const QString& category = "App");
    void reportCritical(const QString& message, const QString& category = "App");
    void reportFatal(const QString& message, const QString& category = "App");
    
    // 错误处理配置
    void setMaxErrorCount(int count) { maxErrorCount = count; }
    void setErrorThreshold(ErrorLevel level, int count);
    void setAutoRecoveryEnabled(bool enabled) { autoRecoveryEnabled = enabled; }
    
    // 错误回调管理
    void registerErrorCallback(const QString& name, ErrorCallback callback);
    void unregisterErrorCallback(const QString& name);
    
    // 错误统计和查询
    int getErrorCount(ErrorLevel level = ErrorLevel::Error) const;
    QList<ErrorEntry> getRecentErrors(int count = 100) const;
    QList<ErrorEntry> getErrorsByCategory(const QString& category) const;
    
    // 错误处理状态
    bool hasUnhandledErrors() const;
    void markErrorsAsHandled();
    void clearErrorHistory();
    
    // 自动恢复
    void triggerRecovery(const QString& reason = QString());
    bool isRecoveryInProgress() const { return recoveryInProgress; }
    
signals:
    void errorReported(const ErrorEntry& error);
    void criticalErrorOccurred(const QString& message);
    void fatalErrorOccurred(const QString& message);
    void recoveryTriggered(const QString& reason);
    void recoveryCompleted(bool success);
    
private slots:
    void processErrorQueue();
    void checkErrorThresholds();
    void performAutoRecovery();
    
private:
    explicit ErrorHandler(QObject* parent = nullptr);
    ~ErrorHandler();
    
    void processError(const ErrorEntry& error);
    void notifyCallbacks(const ErrorEntry& error);
    void checkCriticalConditions();
    
    static ErrorHandler* instance;
    static QMutex instanceMutex;
    
    mutable QMutex errorMutex;
    QQueue<ErrorEntry> errorQueue;
    QList<ErrorEntry> errorHistory;
    
    QTimer* processTimer;
    QTimer* thresholdTimer;
    
    // 错误统计
    QMap<ErrorLevel, int> errorCounts;
    QMap<ErrorLevel, int> errorThresholds;
    int maxErrorCount;
    int maxHistorySize;
    
    // 错误回调
    QMap<QString, ErrorCallback> errorCallbacks;
    
    // 自动恢复
    bool autoRecoveryEnabled;
    bool recoveryInProgress;
    QTimer* recoveryTimer;
    int recoveryAttempts;
    int maxRecoveryAttempts;
    
    // 禁用拷贝构造和赋值
    ErrorHandler(const ErrorHandler&) = delete;
    ErrorHandler& operator=(const ErrorHandler&) = delete;
};

// 便捷宏定义
#define REPORT_INFO(msg) ErrorHandler::getInstance()->reportInfo(msg, __FILE__)
#define REPORT_WARNING(msg) ErrorHandler::getInstance()->reportWarning(msg, __FILE__)
#define REPORT_ERROR(msg) ErrorHandler::getInstance()->reportError(msg, __FILE__)
#define REPORT_CRITICAL(msg) ErrorHandler::getInstance()->reportCritical(msg, __FILE__)
#define REPORT_FATAL(msg) ErrorHandler::getInstance()->reportFatal(msg, __FILE__)