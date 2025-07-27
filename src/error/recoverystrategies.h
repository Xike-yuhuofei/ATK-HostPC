#pragma once

#include <QObject>
#include <QTimer>
#include <QMutex>
#include <functional>
#include "errorhandler.h"

// 前置声明
class ICommunication;
class CommunicationManager;
class MainWindow;

// 恢复策略执行器
class RecoveryStrategies : public QObject
{
    Q_OBJECT

public:
    explicit RecoveryStrategies(QObject* parent = nullptr);
    ~RecoveryStrategies();
    
    // 单例模式
    static RecoveryStrategies* getInstance();
    
    // 策略注册
    void registerAllStrategies();
    void registerCommunicationStrategies();
    void registerDeviceStrategies();
    void registerSystemStrategies();
    void registerProtocolStrategies();
    
    // 通用恢复策略
    static bool retryOperation(const ErrorInfo& error);
    static bool reconnectCommunication(const ErrorInfo& error);
    static bool resetDevice(const ErrorInfo& error);
    static bool restartService(const ErrorInfo& error);
    static bool fallbackOperation(const ErrorInfo& error);
    static bool emergencyShutdown(const ErrorInfo& error);
    static bool userIntervention(const ErrorInfo& error);
    static bool ignoreError(const ErrorInfo& error);
    
    // 通讯错误恢复策略
    static bool handleSerialError(const ErrorInfo& error);
    static bool handleTcpError(const ErrorInfo& error);
    static bool handleNetworkError(const ErrorInfo& error);
    static bool handleTimeoutError(const ErrorInfo& error);
    static bool handleConnectionLost(const ErrorInfo& error);
    static bool handleDataCorruption(const ErrorInfo& error);
    static bool handleBufferOverflow(const ErrorInfo& error);
    
    // 设备错误恢复策略
    static bool handleDeviceNotReady(const ErrorInfo& error);
    static bool handleDeviceBusy(const ErrorInfo& error);
    static bool handleDeviceFault(const ErrorInfo& error);
    static bool handleEmergencyStop(const ErrorInfo& error);
    static bool handlePositionError(const ErrorInfo& error);
    static bool handleSensorError(const ErrorInfo& error);
    static bool handleMotorError(const ErrorInfo& error);
    
    // 协议错误恢复策略
    static bool handleChecksumError(const ErrorInfo& error);
    static bool handleInvalidCommand(const ErrorInfo& error);
    static bool handleInvalidParameter(const ErrorInfo& error);
    static bool handleFrameError(const ErrorInfo& error);
    static bool handleSequenceError(const ErrorInfo& error);
    
    // 系统错误恢复策略
    static bool handleMemoryError(const ErrorInfo& error);
    static bool handleFileSystemError(const ErrorInfo& error);
    static bool handleDatabaseError(const ErrorInfo& error);
    static bool handleConfigurationError(const ErrorInfo& error);
    static bool handlePermissionError(const ErrorInfo& error);
    static bool handleResourceError(const ErrorInfo& error);
    
    // 高级恢复策略
    static bool gradualDegradation(const ErrorInfo& error);
    static bool circuitBreaker(const ErrorInfo& error);
    static bool loadBalancing(const ErrorInfo& error);
    static bool serviceMigration(const ErrorInfo& error);
    static bool rollbackOperation(const ErrorInfo& error);
    static bool healthCheck(const ErrorInfo& error);
    
    // 策略组合
    static bool sequentialRecovery(const ErrorInfo& error, const QList<RecoveryAction>& actions);
    static bool parallelRecovery(const ErrorInfo& error, const QList<RecoveryAction>& actions);
    static bool conditionalRecovery(const ErrorInfo& error, const QMap<QString, RecoveryAction>& conditions);
    
    // 策略配置
    void setRetryAttempts(int maxAttempts);
    void setRetryDelay(int delayMs);
    void setTimeout(int timeoutMs);
    void setEmergencyThreshold(int errorCount);
    
    int getRetryAttempts() const;
    int getRetryDelay() const;
    int getTimeout() const;
    int getEmergencyThreshold() const;
    
    // 状态监控
    bool isSystemHealthy() const;
    void setSystemHealthy(bool healthy);
    int getCurrentErrorCount() const;
    void resetErrorCount();
    
    // 依赖注入
    void setCommunicationManager(CommunicationManager* manager);
    void setMainWindow(MainWindow* window);
    
    CommunicationManager* getCommunicationManager() const;
    MainWindow* getMainWindow() const;

signals:
    // 恢复事件信号
    void recoveryStarted(const ErrorInfo& error, RecoveryStrategy strategy);
    void recoveryCompleted(const ErrorInfo& error, bool success);
    void recoveryFailed(const ErrorInfo& error, const QString& reason);
    void emergencyActivated(const ErrorInfo& error);
    void systemHealthChanged(bool healthy);
    
    // 操作信号
    void retryRequested(const QString& operation, int attempt);
    void reconnectRequested(const QString& connectionName);
    void resetRequested(const QString& deviceName);
    void restartRequested(const QString& serviceName);
    void shutdownRequested(const QString& reason);

public slots:
    // 主要恢复操作
    void performSystemReset();
    void performEmergencyShutdown();
    void performHealthCheck();
    void performGracefulShutdown();
    
    // 通讯恢复操作
    void reconnectAllConnections();
    void resetCommunicationBuffers();
    void restartCommunicationService();
    
    // 设备恢复操作
    void resetAllDevices();
    void performDeviceCalibration();
    void returnToHomePosition();
    void stopAllMotors();

private slots:
    // 内部定时器
    void onRetryTimer();
    void onTimeoutTimer();
    void onHealthCheckTimer();

private:
    // 静态实例
    static RecoveryStrategies* s_instance;
    static QMutex s_mutex;
    
    // 配置参数
    int m_maxRetryAttempts;
    int m_retryDelay;
    int m_timeout;
    int m_emergencyThreshold;
    
    // 状态跟踪
    bool m_systemHealthy;
    int m_currentErrorCount;
    QMap<QString, int> m_operationRetryCount;
    
    // 定时器
    QTimer* m_retryTimer;
    QTimer* m_timeoutTimer;
    QTimer* m_healthCheckTimer;
    
    // 依赖对象
    CommunicationManager* m_communicationManager;
    MainWindow* m_mainWindow;
    
    // 线程安全
    mutable QMutex m_mutex;
    
    // 辅助方法
    static bool executeWithRetry(const std::function<bool()>& operation, int maxAttempts = 3);
    static bool waitForCondition(const std::function<bool()>& condition, int timeoutMs = 5000);
    static void delayExecution(int delayMs);
    static QString extractConnectionName(const ErrorInfo& error);
    static QString extractDeviceName(const ErrorInfo& error);
    static QString extractOperationName(const ErrorInfo& error);
    
    // 恢复帮助方法
    static bool tryReconnectSerial(const QString& portName);
    static bool tryReconnectTcp(const QString& address, quint16 port);
    static bool tryResetDevice(const QString& deviceName);
    static bool tryRestartService(const QString& serviceName);
    static bool checkSystemResources();
    static bool checkDiskSpace();
    static bool checkMemoryUsage();
    static bool checkNetworkConnectivity();
    
    // 通知方法
    void notifyRecoveryStart(const ErrorInfo& error, RecoveryStrategy strategy);
    void notifyRecoveryComplete(const ErrorInfo& error, bool success);
    void notifyEmergencyActivation(const ErrorInfo& error);
    
    // 禁用复制构造函数和赋值操作符
    RecoveryStrategies(const RecoveryStrategies&) = delete;
    RecoveryStrategies& operator=(const RecoveryStrategies&) = delete;
};

// 恢复策略工厂
class RecoveryStrategyFactory
{
public:
    // 创建标准恢复动作
    static RecoveryAction createRetryAction(int maxAttempts = 3);
    static RecoveryAction createReconnectAction(const QString& connectionName = QString());
    static RecoveryAction createResetAction(const QString& deviceName = QString());
    static RecoveryAction createRestartAction(const QString& serviceName = QString());
    static RecoveryAction createFallbackAction(const RecoveryAction& fallback);
    static RecoveryAction createIgnoreAction();
    static RecoveryAction createUserInterventionAction();
    static RecoveryAction createEmergencyAction();
    
    // 创建组合恢复动作
    static RecoveryAction createSequentialAction(const QList<RecoveryAction>& actions);
    static RecoveryAction createParallelAction(const QList<RecoveryAction>& actions);
    static RecoveryAction createConditionalAction(const std::function<bool(const ErrorInfo&)>& condition,
                                                  const RecoveryAction& ifTrue,
                                                  const RecoveryAction& ifFalse);
    
    // 创建延时恢复动作
    static RecoveryAction createDelayedAction(const RecoveryAction& action, int delayMs);
    static RecoveryAction createTimeoutAction(const RecoveryAction& action, int timeoutMs);
    
    // 创建自定义恢复动作
    static RecoveryAction createCustomAction(const std::function<bool(const ErrorInfo&)>& customFunction);

private:
    RecoveryStrategyFactory() = delete;  // 静态工厂类，禁止实例化
}; 