#include "errorhandler.h"
#include "../logger/logmanager.h"
#include <QDebug>
#include <QCoreApplication>
#include <QMessageBox>
#include <QMutexLocker>

ErrorHandler* ErrorHandler::instance = nullptr;
QMutex ErrorHandler::instanceMutex;

ErrorHandler* ErrorHandler::getInstance()
{
    QMutexLocker locker(&instanceMutex);
    if (!instance) {
        instance = new ErrorHandler();
    }
    return instance;
}

ErrorHandler::ErrorHandler(QObject* parent)
    : QObject(parent)
    , maxErrorCount(100)
    , maxHistorySize(1000)
    , autoRecoveryEnabled(true)
    , recoveryInProgress(false)
    , recoveryAttempts(0)
    , maxRecoveryAttempts(3)
{
    // 初始化错误计数
    errorCounts[ErrorLevel::Info] = 0;
    errorCounts[ErrorLevel::Warning] = 0;
    errorCounts[ErrorLevel::Error] = 0;
    errorCounts[ErrorLevel::Critical] = 0;
    errorCounts[ErrorLevel::Fatal] = 0;
    
    // 设置默认错误阈值
    errorThresholds[ErrorLevel::Warning] = 50;
    errorThresholds[ErrorLevel::Error] = 20;
    errorThresholds[ErrorLevel::Critical] = 5;
    errorThresholds[ErrorLevel::Fatal] = 1;
    
    // 创建定时器
    processTimer = new QTimer(this);
    connect(processTimer, &QTimer::timeout, this, &ErrorHandler::processErrorQueue);
    processTimer->start(50); // 每50ms处理一次错误队列
    
    thresholdTimer = new QTimer(this);
    connect(thresholdTimer, &QTimer::timeout, this, &ErrorHandler::checkErrorThresholds);
    thresholdTimer->start(5000); // 每5秒检查一次错误阈值
    
    recoveryTimer = new QTimer(this);
    recoveryTimer->setSingleShot(true);
    connect(recoveryTimer, &QTimer::timeout, this, &ErrorHandler::performAutoRecovery);
    
    qDebug() << "ErrorHandler initialized";
}

ErrorHandler::~ErrorHandler()
{
    qDebug() << "ErrorHandler destroyed";
}

void ErrorHandler::reportError(ErrorLevel level, const QString& category, 
                              const QString& message, const QString& source,
                              const QString& details)
{
    ErrorEntry entry;
    entry.timestamp = QDateTime::currentDateTime();
    entry.level = level;
    entry.category = category;
    entry.message = message;
    entry.source = source;
    entry.details = details;
    entry.handled = false;
    
    {
        QMutexLocker locker(&errorMutex);
        errorQueue.enqueue(entry);
        errorCounts[level]++;
        
        // 添加到历史记录
        errorHistory.append(entry);
        if (errorHistory.size() > maxHistorySize) {
            errorHistory.removeFirst();
        }
    }
    
    // 立即处理致命错误
    if (level == ErrorLevel::Fatal) {
        processError(entry);
    }
    
    emit errorReported(entry);
}

void ErrorHandler::reportInfo(const QString& message, const QString& category)
{
    reportError(ErrorLevel::Info, category, message);
}

void ErrorHandler::reportWarning(const QString& message, const QString& category)
{
    reportError(ErrorLevel::Warning, category, message);
}

void ErrorHandler::reportError(const QString& message, const QString& category)
{
    reportError(ErrorLevel::Error, category, message);
}

void ErrorHandler::reportCritical(const QString& message, const QString& category)
{
    reportError(ErrorLevel::Critical, category, message);
}

void ErrorHandler::reportFatal(const QString& message, const QString& category)
{
    reportError(ErrorLevel::Fatal, category, message);
}

void ErrorHandler::setErrorThreshold(ErrorLevel level, int count)
{
    QMutexLocker locker(&errorMutex);
    errorThresholds[level] = count;
}

void ErrorHandler::registerErrorCallback(const QString& name, ErrorCallback callback)
{
    QMutexLocker locker(&errorMutex);
    errorCallbacks[name] = callback;
}

void ErrorHandler::unregisterErrorCallback(const QString& name)
{
    QMutexLocker locker(&errorMutex);
    errorCallbacks.remove(name);
}

int ErrorHandler::getErrorCount(ErrorLevel level) const
{
    QMutexLocker locker(&errorMutex);
    return errorCounts.value(level, 0);
}

QList<ErrorEntry> ErrorHandler::getRecentErrors(int count) const
{
    QMutexLocker locker(&errorMutex);
    
    if (count >= errorHistory.size()) {
        return errorHistory;
    }
    
    return errorHistory.mid(errorHistory.size() - count);
}

QList<ErrorEntry> ErrorHandler::getErrorsByCategory(const QString& category) const
{
    QMutexLocker locker(&errorMutex);
    
    QList<ErrorEntry> result;
    for (const ErrorEntry& entry : errorHistory) {
        if (entry.category == category) {
            result.append(entry);
        }
    }
    
    return result;
}

bool ErrorHandler::hasUnhandledErrors() const
{
    QMutexLocker locker(&errorMutex);
    
    for (const ErrorEntry& entry : errorHistory) {
        if (!entry.handled && entry.level >= ErrorLevel::Error) {
            return true;
        }
    }
    
    return false;
}

void ErrorHandler::markErrorsAsHandled()
{
    QMutexLocker locker(&errorMutex);
    
    for (ErrorEntry& entry : errorHistory) {
        entry.handled = true;
    }
}

void ErrorHandler::clearErrorHistory()
{
    QMutexLocker locker(&errorMutex);
    
    errorHistory.clear();
    
    // 重置错误计数
    for (auto& count : errorCounts) {
        count = 0;
    }
}

void ErrorHandler::triggerRecovery(const QString& reason)
{
    if (recoveryInProgress) {
        qWarning() << "Recovery already in progress";
        return;
    }
    
    recoveryInProgress = true;
    recoveryAttempts++;
    
    qWarning() << "Triggering recovery:" << reason;
    emit recoveryTriggered(reason);
    
    // 延迟执行恢复操作
    recoveryTimer->start(1000);
}

void ErrorHandler::processErrorQueue()
{
    QList<ErrorEntry> entriesToProcess;
    
    {
        QMutexLocker locker(&errorMutex);
        while (!errorQueue.isEmpty()) {
            entriesToProcess.append(errorQueue.dequeue());
        }
    }
    
    for (const ErrorEntry& entry : entriesToProcess) {
        processError(entry);
    }
}

void ErrorHandler::checkErrorThresholds()
{
    QMutexLocker locker(&errorMutex);
    
    for (auto it = errorThresholds.begin(); it != errorThresholds.end(); ++it) {
        ErrorLevel level = it.key();
        int threshold = it.value();
        int currentCount = errorCounts.value(level, 0);
        
        if (currentCount >= threshold) {
            QString message = QString("Error threshold exceeded for level %1: %2/%3")
                            .arg(static_cast<int>(level))
                            .arg(currentCount)
                            .arg(threshold);
            
            qCritical() << message;
            
            // 触发自动恢复
            if (autoRecoveryEnabled && !recoveryInProgress) {
                triggerRecovery(message);
            }
        }
    }
}

void ErrorHandler::performAutoRecovery()
{
    qInfo() << "Performing auto recovery, attempt:" << recoveryAttempts;
    
    try {
        // 清理错误历史
        clearErrorHistory();
        
        // 通知恢复完成
        recoveryInProgress = false;
        emit recoveryCompleted(true);
        
        qInfo() << "Auto recovery completed successfully";
        
    } catch (const std::exception& e) {
        qCritical() << "Auto recovery failed:" << e.what();
        
        if (recoveryAttempts < maxRecoveryAttempts) {
            // 重试恢复
            recoveryTimer->start(5000); // 5秒后重试
        } else {
            recoveryInProgress = false;
            emit recoveryCompleted(false);
            qCritical() << "Auto recovery failed after maximum attempts";
        }
    } catch (...) {
        qCritical() << "Auto recovery failed with unknown error";
        recoveryInProgress = false;
        emit recoveryCompleted(false);
    }
}

void ErrorHandler::processError(const ErrorEntry& error)
{
    // 记录到日志系统
    LogManager* logger = LogManager::getInstance();
    
    QString logMessage = QString("%1 [%2]: %3")
                        .arg(error.category, error.source, error.message);
    
    if (!error.details.isEmpty()) {
        logMessage += QString(" Details: %1").arg(error.details);
    }
    
    switch (error.level) {
        case ErrorLevel::Info:
            logger->info(logMessage, error.category);
            break;
        case ErrorLevel::Warning:
            logger->warning(logMessage, error.category);
            break;
        case ErrorLevel::Error:
            logger->error(logMessage, error.category);
            break;
        case ErrorLevel::Critical:
            logger->critical(logMessage, error.category);
            emit criticalErrorOccurred(error.message);
            break;
        case ErrorLevel::Fatal:
            logger->critical("FATAL: " + logMessage, error.category);
            emit fatalErrorOccurred(error.message);
            break;
    }
    
    // 通知回调函数
    notifyCallbacks(error);
    
    // 检查关键条件
    checkCriticalConditions();
}

void ErrorHandler::notifyCallbacks(const ErrorEntry& error)
{
    QMutexLocker locker(&errorMutex);
    
    for (auto it = errorCallbacks.begin(); it != errorCallbacks.end(); ++it) {
        try {
            it.value()(error);
        } catch (const std::exception& e) {
            qCritical() << "Error in callback" << it.key() << ":" << e.what();
        } catch (...) {
            qCritical() << "Unknown error in callback" << it.key();
        }
    }
}

void ErrorHandler::checkCriticalConditions()
{
    // 检查是否有太多错误
    int totalErrors = getErrorCount(ErrorLevel::Error) + 
                     getErrorCount(ErrorLevel::Critical) + 
                     getErrorCount(ErrorLevel::Fatal);
    
    if (totalErrors > maxErrorCount) {
        qCritical() << "Too many errors detected:" << totalErrors;
        
        if (autoRecoveryEnabled && !recoveryInProgress) {
            triggerRecovery("Too many errors");
        }
    }
}