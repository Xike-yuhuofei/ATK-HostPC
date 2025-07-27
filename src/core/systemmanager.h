#pragma once

#include <QObject>
#include <QSettings>
#include <QDateTime>
#include <QTimer>
#include <QJsonObject>

// 前置声明
class LogManager;
class ConfigManager;
class DatabaseManager;
class SecurityManager;

/**
 * @brief 系统管理器类
 * 
 * 负责系统级别的管理功能，包括：
 * - 配置管理
 * - 日志管理
 * - 权限和安全管理
 * - 系统监控
 * - 备份和恢复
 * - 升级和维护
 */
class SystemManager : public QObject
{
    Q_OBJECT

public:
    explicit SystemManager(QObject* parent = nullptr);
    ~SystemManager();

    // 初始化和关闭
    void initialize();
    void shutdown();
    
    // 配置管理
    void loadConfiguration();
    void saveConfiguration();
    void resetConfiguration();
    void importConfiguration(const QString& filePath);
    void exportConfiguration(const QString& filePath);
    
    // 设置访问
    void setSetting(const QString& key, const QVariant& value);
    QVariant getSetting(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void removeSetting(const QString& key);
    bool hasSetting(const QString& key) const;
    
    // 日志管理
    void logMessage(const QString& message, const QString& level = "INFO");
    void logError(const QString& error, const QString& context = QString());
    void logWarning(const QString& warning, const QString& context = QString());
    void logDebug(const QString& debug, const QString& context = QString());
    void clearLogs();
    void archiveLogs();
    
    // 权限管理
    void setCurrentUser(const QString& username);
    QString getCurrentUser() const;
    void setUserPermissions(const QString& username, const QStringList& permissions);
    QStringList getUserPermissions(const QString& username) const;
    bool hasPermission(const QString& permission) const;
    void loginUser(const QString& username, const QString& password);
    void logoutUser();
    
    // 系统监控
    void startSystemMonitoring();
    void stopSystemMonitoring();
    double getCPUUsage() const;
    double getMemoryUsage() const;
    qint64 getDiskUsage() const;
    qint64 getAvailableDiskSpace() const;
    QJsonObject getSystemInfo() const;
    
    // 备份和恢复
    void createBackup(const QString& backupPath);
    void restoreBackup(const QString& backupPath);
    void scheduleAutoBackup(int intervalMinutes);
    void cancelAutoBackup();
    QStringList getAvailableBackups() const;
    
    // 升级和维护
    void checkForUpdates();
    void installUpdate(const QString& updatePath);
    void performMaintenance();
    void cleanupTempFiles();
    void optimizeDatabase();
    
    // 系统检查
    void checkSystemRequirements();
    void validateConfiguration();
    void runDiagnostics();
    bool isSystemHealthy() const;
    
    // 会话管理
    void startSession();
    void endSession();
    void refreshSession();
    bool isSessionActive() const;
    int getSessionTimeout() const;
    void setSessionTimeout(int timeoutMinutes);
    
    // 通知管理
    void showNotification(const QString& title, const QString& message, const QString& type = "INFO");
    void showCriticalError(const QString& error);
    void showWarning(const QString& warning);
    void showInfo(const QString& info);
    
    // 状态查询
    bool isInitialized() const;
    bool isMonitoringActive() const;
    bool isBackupScheduled() const;
    bool isMaintenanceMode() const;
    
public slots:
    // 定时任务
    void performPeriodicTasks();
    void updateSystemStats();
    void checkSystemHealth();
    void performAutoBackup();
    void checkSession();
    void cleanupResources();
    
    // 事件处理
    void onApplicationStarted();
    void onApplicationClosing();
    void onCriticalError(const QString& error);
    void onUserActivity();
    void onSystemResourcesLow();

signals:
    // 系统状态信号
    void systemInitialized();
    void systemShutdown();
    void configurationLoaded();
    void configurationSaved();
    void configurationReset();
    
    // 用户会话信号
    void userLoggedIn(const QString& username);
    void userLoggedOut();
    void sessionTimeout();
    void sessionRefreshed();
    void permissionChanged();
    
    // 系统监控信号
    void systemStatsUpdated(double cpuUsage, double memoryUsage, qint64 diskUsage);
    void systemHealthChanged(bool healthy);
    void criticalErrorOccurred(const QString& error);
    void warningIssued(const QString& warning);
    void resourceUsageHigh(const QString& resourceType);
    
    // 备份和恢复信号
    void backupStarted();
    void backupCompleted(bool success);
    void backupFailed(const QString& error);
    void restoreStarted();
    void restoreCompleted(bool success);
    void restoreFailed(const QString& error);
    
    // 升级和维护信号
    void updateAvailable(const QString& version);
    void updateDownloaded();
    void updateInstalled(bool success);
    void maintenanceStarted();
    void maintenanceCompleted();
    
    // 通知信号
    void notificationRequested(const QString& title, const QString& message, const QString& type);
    void criticalErrorNotification(const QString& error);
    void warningNotification(const QString& warning);
    void infoNotification(const QString& info);

private:
    void initializeComponents();
    void setupTimers();
    void loadSystemSettings();
    void saveSystemSettings();
    void createDirectories();
    void setupDefaultConfiguration();
    
    // 配置管理内部方法
    void validateSettings();
    void migrateConfiguration();
    void backupConfiguration();
    void restoreConfiguration();
    
    // 系统监控内部方法
    void collectSystemStats();
    void checkResourceUsage();
    void monitorSystemHealth();
    void handleResourceShortage();
    
    // 权限管理内部方法
    void loadUserDatabase();
    void saveUserDatabase();
    void validateUserCredentials(const QString& username, const QString& password);
    void updateUserActivity();
    
    // 备份内部方法
    void createFullBackup(const QString& backupPath);
    void createIncrementalBackup(const QString& backupPath);
    void compressBackup(const QString& backupPath);
    void verifyBackup(const QString& backupPath);
    
    // 维护内部方法
    void performDatabaseMaintenance();
    void performLogMaintenance();
    void performTempFileCleanup();
    void performRegistryCleanup();
    
    // 核心组件
    LogManager* logManager;
    ConfigManager* configManager;
    DatabaseManager* databaseManager;
    SecurityManager* securityManager;
    QSettings* settings;
    
    // 定时器
    QTimer* systemMonitorTimer;
    QTimer* backupTimer;
    QTimer* maintenanceTimer;
    QTimer* sessionTimer;
    QTimer* healthCheckTimer;
    
    // 系统状态
    bool initialized;
    bool monitoringActive;
    bool backupScheduled;
    bool maintenanceMode;
    bool systemHealthy;
    
    // 会话管理
    QString currentUser;
    QStringList currentUserPermissions;
    QDateTime sessionStartTime;
    QDateTime lastActivity;
    int sessionTimeoutMinutes;
    bool sessionActive;
    
    // 系统统计
    struct SystemStats {
        double cpuUsage;
        double memoryUsage;
        qint64 diskUsage;
        qint64 availableDiskSpace;
        QDateTime lastUpdate;
    } systemStats;
    
    // 配置路径
    QString configFilePath;
    QString logFilePath;
    QString backupDirectory;
    QString tempDirectory;
    QString userDataPath;
    
    // 配置参数
    bool autoSaveEnabled;
    bool autoBackupEnabled;
    bool systemMonitoringEnabled;
    bool maintenanceScheduled;
    int autoSaveInterval;
    int autoBackupInterval;
    int systemMonitorInterval;
    int maintenanceInterval;
    
    // 阈值设置
    double cpuUsageThreshold;
    double memoryUsageThreshold;
    qint64 diskUsageThreshold;
    int maxLogFileSize;
    int maxBackupCount;
    int maxTempFileAge;
}; 