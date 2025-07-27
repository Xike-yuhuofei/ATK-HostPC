#pragma once

#include <QObject>
#include <QSettings>
#include <QVariant>
#include <QString>
#include <QMutex>
#include <QDateTime>
#include <QFileSystemWatcher>

class ConfigManager : public QObject
{
    Q_OBJECT

public:
    static ConfigManager* getInstance();
    
    // 串口配置
    QString getSerialPort() const;
    void setSerialPort(const QString& port);
    
    int getBaudRate() const;
    void setBaudRate(int rate);
    
    int getDataBits() const;
    void setDataBits(int bits);
    
    int getParity() const;
    void setParity(int parity);
    
    int getStopBits() const;
    void setStopBits(int bits);
    
    int getSerialTimeout() const;
    void setSerialTimeout(int timeout);
    
    // TCP配置
    QString getTcpHost() const;
    void setTcpHost(const QString& host);
    
    int getTcpPort() const;
    void setTcpPort(int port);
    
    int getTcpTimeout() const;
    void setTcpTimeout(int timeout);
    
    // Modbus配置
    int getModbusSlaveId() const;
    void setModbusSlaveId(int id);
    
    int getModbusTimeout() const;
    void setModbusTimeout(int timeout);
    
    // 数据库配置
    QString getDatabasePath() const;
    void setDatabasePath(const QString& path);
    
    // 日志配置
    QString getLogLevel() const;
    void setLogLevel(const QString& level);
    
    int getLogMaxFiles() const;
    void setLogMaxFiles(int maxFiles);
    
    int getLogMaxSize() const;
    void setLogMaxSize(int maxSize);
    
    // 界面配置
    QString getLanguage() const;
    void setLanguage(const QString& language);
    
    QString getTheme() const;
    void setTheme(const QString& theme);
    
    // 通用配置方法
    QVariant getValue(const QString& key, const QVariant& defaultValue = QVariant()) const;
    void setValue(const QString& key, const QVariant& value);
    
    // 保存配置
    void sync();
    
    // 重置配置
    void resetToDefaults();
    
    // 配置验证
    bool validateConfig() const;
    bool validateSerialConfig() const;
    bool validateTcpConfig() const;
    bool validateModbusConfig() const;
    bool validateDatabaseConfig() const;
    bool validateLogConfig() const;
    bool validateUIConfig() const;
    
    // 配置导入导出
    bool exportConfig(const QString& filePath) const;
    bool importConfig(const QString& filePath);
    QString exportConfigToString() const;
    bool importConfigFromString(const QString& configString);
    
    // 配置备份
    bool backupConfig(const QString& backupPath = QString()) const;
    bool restoreConfig(const QString& backupPath);
    QStringList getBackupList() const;
    bool deleteBackup(const QString& backupName);
    
    // 配置模板
    bool saveConfigTemplate(const QString& templateName, const QString& description = QString());
    bool loadConfigTemplate(const QString& templateName);
    QStringList getConfigTemplates() const;
    bool deleteConfigTemplate(const QString& templateName);
    
    // 配置监控
    void startConfigMonitoring();
    void stopConfigMonitoring();
    bool isConfigMonitoring() const;
    
    // 配置统计
    int getConfigChangeCount() const;
    QDateTime getLastConfigChange() const;
    QString getConfigFilePath() const;
    qint64 getConfigFileSize() const;
    
    // 配置分组管理
    QStringList getConfigGroups() const;
    QStringList getConfigKeys(const QString& group) const;
    bool hasConfigGroup(const QString& group) const;
    bool removeConfigGroup(const QString& group);
    
    // 配置加密
    void setEncryptionEnabled(bool enabled);
    bool isEncryptionEnabled() const;
    void setEncryptionKey(const QString& key);

signals:
    void configChanged(const QString& key, const QVariant& value);
    void configValidationFailed(const QString& error);
    void configImported(const QString& filePath);
    void configExported(const QString& filePath);
    void configBackupCreated(const QString& backupPath);
    void configRestored(const QString& backupPath);
    void configTemplateLoaded(const QString& templateName);
    void configFileChanged();

private:
    explicit ConfigManager(QObject* parent = nullptr);
    ~ConfigManager();
    
    void loadDefaults();
    void setupConfigMonitoring();
    void onConfigFileChanged();
    
    bool validateValue(const QString& key, const QVariant& value) const;
    QString encryptValue(const QString& value) const;
    QString decryptValue(const QString& value) const;
    QString generateBackupName() const;
    QString getBackupDirectory() const;
    QString getTemplateDirectory() const;
    
    static ConfigManager* instance;
    static QMutex mutex;
    
    QSettings* settings;
    class QFileSystemWatcher* configWatcher;
    
    // 配置状态
    bool encryptionEnabled;
    QString encryptionKey;
    int configChangeCount;
    QDateTime lastConfigChange;
    bool configMonitoring;
}; 