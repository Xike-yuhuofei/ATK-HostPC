#include "configmanager.h"
#include <QStandardPaths>
#include <QDir>
#include <QDebug>
#include <QFileSystemWatcher>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QDateTime>
#include <QRegularExpression>
#include <QHostAddress>
#include <QUrl>

ConfigManager* ConfigManager::instance = nullptr;
QMutex ConfigManager::mutex;

ConfigManager* ConfigManager::getInstance()
{
    QMutexLocker locker(&mutex);
    if (!instance) {
        instance = new ConfigManager();
    }
    return instance;
}

ConfigManager::ConfigManager(QObject* parent)
    : QObject(parent)
    , configWatcher(nullptr)
    , encryptionEnabled(false)
    , configChangeCount(0)
    , configMonitoring(false)
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config/app.ini";
    
    // 确保配置目录存在
    QDir configDir = QFileInfo(configPath).dir();
    if (!configDir.exists()) {
        configDir.mkpath(".");
    }
    
    settings = new QSettings(configPath, QSettings::IniFormat);
    loadDefaults();
    setupConfigMonitoring();
    
    // 初始化状态
    lastConfigChange = QDateTime::currentDateTime();
}

ConfigManager::~ConfigManager()
{
    stopConfigMonitoring();
    if (settings) {
        settings->sync();
    }
}

void ConfigManager::loadDefaults()
{
    // 串口默认配置
    if (!settings->contains("Serial/Port")) {
        settings->setValue("Serial/Port", "/dev/tty.usbserial-1130");  // macOS格式
    }
    if (!settings->contains("Serial/BaudRate")) {
        settings->setValue("Serial/BaudRate", 115200);
    }
    if (!settings->contains("Serial/DataBits")) {
        settings->setValue("Serial/DataBits", 8);
    }
    if (!settings->contains("Serial/Parity")) {
        settings->setValue("Serial/Parity", 0); // NoParity
    }
    if (!settings->contains("Serial/StopBits")) {
        settings->setValue("Serial/StopBits", 1);
    }
    if (!settings->contains("Serial/Timeout")) {
        settings->setValue("Serial/Timeout", 3000);
    }
    
    // TCP默认配置
    if (!settings->contains("TCP/Host")) {
        settings->setValue("TCP/Host", "127.0.0.1");
    }
    if (!settings->contains("TCP/Port")) {
        settings->setValue("TCP/Port", 502);
    }
    if (!settings->contains("TCP/Timeout")) {
        settings->setValue("TCP/Timeout", 3000);
    }
    
    // Modbus默认配置
    if (!settings->contains("Modbus/SlaveId")) {
        settings->setValue("Modbus/SlaveId", 1);
    }
    if (!settings->contains("Modbus/Timeout")) {
        settings->setValue("Modbus/Timeout", 1000);
    }
    
    // 数据库默认配置
    if (!settings->contains("Database/Path")) {
        QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/data/industrial.db";
        settings->setValue("Database/Path", dbPath);
    }
    
    // 日志默认配置
    if (!settings->contains("Log/Level")) {
        settings->setValue("Log/Level", "Info");
    }
    if (!settings->contains("Log/MaxFiles")) {
        settings->setValue("Log/MaxFiles", 10);
    }
    if (!settings->contains("Log/MaxSize")) {
        settings->setValue("Log/MaxSize", 10 * 1024 * 1024); // 10MB
    }
    
    // 界面默认配置
    if (!settings->contains("UI/Language")) {
        settings->setValue("UI/Language", "zh_CN");
    }
    if (!settings->contains("UI/Theme")) {
        settings->setValue("UI/Theme", "Default");
    }
    
    settings->sync();
}

// 串口配置
QString ConfigManager::getSerialPort() const
{
    return settings->value("Serial/Port").toString();
}

void ConfigManager::setSerialPort(const QString& port)
{
    settings->setValue("Serial/Port", port);
    emit configChanged("Serial/Port", port);
}

int ConfigManager::getBaudRate() const
{
    return settings->value("Serial/BaudRate").toInt();
}

void ConfigManager::setBaudRate(int rate)
{
    settings->setValue("Serial/BaudRate", rate);
    emit configChanged("Serial/BaudRate", rate);
}

int ConfigManager::getDataBits() const
{
    return settings->value("Serial/DataBits").toInt();
}

void ConfigManager::setDataBits(int bits)
{
    settings->setValue("Serial/DataBits", bits);
    emit configChanged("Serial/DataBits", bits);
}

int ConfigManager::getParity() const
{
    return settings->value("Serial/Parity").toInt();
}

void ConfigManager::setParity(int parity)
{
    settings->setValue("Serial/Parity", parity);
    emit configChanged("Serial/Parity", parity);
}

int ConfigManager::getStopBits() const
{
    return settings->value("Serial/StopBits").toInt();
}

void ConfigManager::setStopBits(int bits)
{
    settings->setValue("Serial/StopBits", bits);
    emit configChanged("Serial/StopBits", bits);
}

int ConfigManager::getSerialTimeout() const
{
    return settings->value("Serial/Timeout").toInt();
}

void ConfigManager::setSerialTimeout(int timeout)
{
    settings->setValue("Serial/Timeout", timeout);
    emit configChanged("Serial/Timeout", timeout);
}

// TCP配置
QString ConfigManager::getTcpHost() const
{
    return settings->value("TCP/Host").toString();
}

void ConfigManager::setTcpHost(const QString& host)
{
    settings->setValue("TCP/Host", host);
    emit configChanged("TCP/Host", host);
}

int ConfigManager::getTcpPort() const
{
    return settings->value("TCP/Port").toInt();
}

void ConfigManager::setTcpPort(int port)
{
    settings->setValue("TCP/Port", port);
    emit configChanged("TCP/Port", port);
}

int ConfigManager::getTcpTimeout() const
{
    return settings->value("TCP/Timeout").toInt();
}

void ConfigManager::setTcpTimeout(int timeout)
{
    settings->setValue("TCP/Timeout", timeout);
    emit configChanged("TCP/Timeout", timeout);
}

// Modbus配置
int ConfigManager::getModbusSlaveId() const
{
    return settings->value("Modbus/SlaveId").toInt();
}

void ConfigManager::setModbusSlaveId(int id)
{
    settings->setValue("Modbus/SlaveId", id);
    emit configChanged("Modbus/SlaveId", id);
}

int ConfigManager::getModbusTimeout() const
{
    return settings->value("Modbus/Timeout").toInt();
}

void ConfigManager::setModbusTimeout(int timeout)
{
    settings->setValue("Modbus/Timeout", timeout);
    emit configChanged("Modbus/Timeout", timeout);
}

// 数据库配置
QString ConfigManager::getDatabasePath() const
{
    return settings->value("Database/Path").toString();
}

void ConfigManager::setDatabasePath(const QString& path)
{
    settings->setValue("Database/Path", path);
    emit configChanged("Database/Path", path);
}

// 日志配置
QString ConfigManager::getLogLevel() const
{
    return settings->value("Log/Level").toString();
}

void ConfigManager::setLogLevel(const QString& level)
{
    settings->setValue("Log/Level", level);
    emit configChanged("Log/Level", level);
}

int ConfigManager::getLogMaxFiles() const
{
    return settings->value("Log/MaxFiles").toInt();
}

void ConfigManager::setLogMaxFiles(int maxFiles)
{
    settings->setValue("Log/MaxFiles", maxFiles);
    emit configChanged("Log/MaxFiles", maxFiles);
}

int ConfigManager::getLogMaxSize() const
{
    return settings->value("Log/MaxSize").toInt();
}

void ConfigManager::setLogMaxSize(int maxSize)
{
    settings->setValue("Log/MaxSize", maxSize);
    emit configChanged("Log/MaxSize", maxSize);
}

// 界面配置
QString ConfigManager::getLanguage() const
{
    return settings->value("UI/Language").toString();
}

void ConfigManager::setLanguage(const QString& language)
{
    settings->setValue("UI/Language", language);
    emit configChanged("UI/Language", language);
}

QString ConfigManager::getTheme() const
{
    return settings->value("UI/Theme").toString();
}

void ConfigManager::setTheme(const QString& theme)
{
    settings->setValue("UI/Theme", theme);
    emit configChanged("UI/Theme", theme);
}

// 通用配置方法
QVariant ConfigManager::getValue(const QString& key, const QVariant& defaultValue) const
{
    return settings->value(key, defaultValue);
}

void ConfigManager::setValue(const QString& key, const QVariant& value)
{
    // 验证值
    if (!validateValue(key, value)) {
        emit configValidationFailed(QString("Invalid value for key: %1").arg(key));
        return;
    }
    
    QVariant finalValue = value;
    
    // 如果启用加密，加密敏感数据
    if (encryptionEnabled && (key.contains("Password") || key.contains("Key") || key.contains("Secret"))) {
        finalValue = encryptValue(value.toString());
    }
    
    settings->setValue(key, finalValue);
    configChangeCount++;
    lastConfigChange = QDateTime::currentDateTime();
    emit configChanged(key, value);
}

void ConfigManager::sync()
{
    settings->sync();
}

void ConfigManager::resetToDefaults()
{
    settings->clear();
    loadDefaults();
    configChangeCount++;
    lastConfigChange = QDateTime::currentDateTime();
    qDebug() << "Configuration reset to defaults";
}

// 配置验证实现
bool ConfigManager::validateConfig() const
{
    return validateSerialConfig() && validateTcpConfig() && validateModbusConfig() && 
           validateDatabaseConfig() && validateLogConfig() && validateUIConfig();
}

bool ConfigManager::validateSerialConfig() const
{
    // 验证串口配置
    QString port = getSerialPort();
    if (port.isEmpty()) return false;
    
    int baudRate = getBaudRate();
    if (baudRate <= 0) return false;
    
    int dataBits = getDataBits();
    if (dataBits < 5 || dataBits > 8) return false;
    
    int parity = getParity();
    if (parity < 0 || parity > 4) return false;
    
    int stopBits = getStopBits();
    if (stopBits < 1 || stopBits > 2) return false;
    
    int timeout = getSerialTimeout();
    if (timeout < 100 || timeout > 60000) return false;
    
    return true;
}

bool ConfigManager::validateTcpConfig() const
{
    // 验证TCP配置
    QString host = getTcpHost();
    QHostAddress address(host);
    if (address.isNull() && host != "localhost") return false;
    
    int port = getTcpPort();
    if (port <= 0 || port > 65535) return false;
    
    int timeout = getTcpTimeout();
    if (timeout < 100 || timeout > 60000) return false;
    
    return true;
}

bool ConfigManager::validateModbusConfig() const
{
    // 验证Modbus配置
    int slaveId = getModbusSlaveId();
    if (slaveId < 1 || slaveId > 247) return false;
    
    int timeout = getModbusTimeout();
    if (timeout < 100 || timeout > 60000) return false;
    
    return true;
}

bool ConfigManager::validateDatabaseConfig() const
{
    // 验证数据库配置
    QString dbPath = getDatabasePath();
    if (dbPath.isEmpty()) return false;
    
    QFileInfo fileInfo(dbPath);
    QDir dir = fileInfo.dir();
    if (!dir.exists() && !dir.mkpath(".")) return false;
    
    return true;
}

bool ConfigManager::validateLogConfig() const
{
    // 验证日志配置
    QString level = getLogLevel();
    QStringList validLevels = {"Debug", "Info", "Warning", "Error", "Critical"};
    if (!validLevels.contains(level)) return false;
    
    int maxFiles = getLogMaxFiles();
    if (maxFiles < 1 || maxFiles > 100) return false;
    
    int maxSize = getLogMaxSize();
    if (maxSize < 1024 || maxSize > 1024*1024*1024) return false; // 1KB - 1GB
    
    return true;
}

bool ConfigManager::validateUIConfig() const
{
    // 验证UI配置
    QString language = getLanguage();
    QStringList validLanguages = {"zh_CN", "en_US", "ja_JP", "ko_KR"};
    if (!validLanguages.contains(language)) return false;
    
    QString theme = getTheme();
    QStringList validThemes = {"Default", "Dark", "Light", "Modern"};
    if (!validThemes.contains(theme)) return false;
    
    return true;
}

// 配置导入导出实现
bool ConfigManager::exportConfig(const QString& filePath) const
{
    QJsonObject configObj;
    
    // 遍历所有配置组
    QStringList groups = getConfigGroups();
    for (const QString& group : groups) {
        QJsonObject groupObj;
        QStringList keys = getConfigKeys(group);
        for (const QString& key : keys) {
            QString fullKey = group + "/" + key;
            QVariant value = getValue(fullKey);
            
            // 不导出敏感信息
            if (key.contains("Password") || key.contains("Key") || key.contains("Secret")) {
                continue;
            }
            
            if (value.typeId() == QMetaType::QString) {
                groupObj[key] = value.toString();
            } else if (value.typeId() == QMetaType::Int) {
                groupObj[key] = value.toInt();
            } else if (value.typeId() == QMetaType::Bool) {
                groupObj[key] = value.toBool();
            } else if (value.typeId() == QMetaType::Double) {
                groupObj[key] = value.toDouble();
            }
        }
        configObj[group] = groupObj;
    }
    
    QJsonDocument doc(configObj);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        // emit configExported(filePath); // 移除const函数中的emit
        return true;
    }
    
    return false;
}

bool ConfigManager::importConfig(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        return false;
    }
    
    QJsonObject configObj = doc.object();
    
    // 导入配置
    for (auto it = configObj.begin(); it != configObj.end(); ++it) {
        QString group = it.key();
        QJsonObject groupObj = it.value().toObject();
        
        for (auto groupIt = groupObj.begin(); groupIt != groupObj.end(); ++groupIt) {
            QString key = groupIt.key();
            QJsonValue value = groupIt.value();
            QString fullKey = group + "/" + key;
            
            if (value.isString()) {
                setValue(fullKey, value.toString());
            } else if (value.isDouble()) {
                setValue(fullKey, value.toDouble());
            } else if (value.isBool()) {
                setValue(fullKey, value.toBool());
            }
        }
    }
    
    sync();
    emit configImported(filePath);
    return true;
}

QString ConfigManager::exportConfigToString() const
{
    QJsonObject configObj;
    
    QStringList groups = getConfigGroups();
    for (const QString& group : groups) {
        QJsonObject groupObj;
        QStringList keys = getConfigKeys(group);
        for (const QString& key : keys) {
            QString fullKey = group + "/" + key;
            QVariant value = getValue(fullKey);
            
            if (key.contains("Password") || key.contains("Key") || key.contains("Secret")) {
                continue;
            }
            
            if (value.typeId() == QMetaType::QString) {
                groupObj[key] = value.toString();
            } else if (value.typeId() == QMetaType::Int) {
                groupObj[key] = value.toInt();
            } else if (value.typeId() == QMetaType::Bool) {
                groupObj[key] = value.toBool();
            } else if (value.typeId() == QMetaType::Double) {
                groupObj[key] = value.toDouble();
            }
        }
        configObj[group] = groupObj;
    }
    
    QJsonDocument doc(configObj);
    return doc.toJson();
}

bool ConfigManager::importConfigFromString(const QString& configString)
{
    QJsonDocument doc = QJsonDocument::fromJson(configString.toUtf8());
    if (doc.isNull()) {
        return false;
    }
    
    QJsonObject configObj = doc.object();
    
    for (auto it = configObj.begin(); it != configObj.end(); ++it) {
        QString group = it.key();
        QJsonObject groupObj = it.value().toObject();
        
        for (auto groupIt = groupObj.begin(); groupIt != groupObj.end(); ++groupIt) {
            QString key = groupIt.key();
            QJsonValue value = groupIt.value();
            QString fullKey = group + "/" + key;
            
            if (value.isString()) {
                setValue(fullKey, value.toString());
            } else if (value.isDouble()) {
                setValue(fullKey, value.toDouble());
            } else if (value.isBool()) {
                setValue(fullKey, value.toBool());
            }
        }
    }
    
    sync();
    return true;
}

// 配置备份实现
bool ConfigManager::backupConfig(const QString& backupPath) const
{
    QString finalBackupPath = backupPath;
    if (finalBackupPath.isEmpty()) {
        finalBackupPath = getBackupDirectory() + "/" + generateBackupName();
    }
    
    QDir().mkpath(QFileInfo(finalBackupPath).dir().path());
    
    if (exportConfig(finalBackupPath)) {
        // emit configBackupCreated(finalBackupPath); // 移除const函数中的emit
        return true;
    }
    
    return false;
}

bool ConfigManager::restoreConfig(const QString& backupPath)
{
    if (importConfig(backupPath)) {
        emit configRestored(backupPath);
        return true;
    }
    
    return false;
}

QStringList ConfigManager::getBackupList() const
{
    QStringList backups;
    QDir backupDir(getBackupDirectory());
    
    if (backupDir.exists()) {
        QStringList filters;
        filters << "*.json";
        QFileInfoList files = backupDir.entryInfoList(filters, QDir::Files, QDir::Time);
        
        for (const QFileInfo& file : files) {
            backups.append(file.baseName());
        }
    }
    
    return backups;
}

bool ConfigManager::deleteBackup(const QString& backupName)
{
    QString backupPath = getBackupDirectory() + "/" + backupName + ".json";
    return QFile::remove(backupPath);
}

// 配置模板实现
bool ConfigManager::saveConfigTemplate(const QString& templateName, const QString& description)
{
    QString templatePath = getTemplateDirectory() + "/" + templateName + ".json";
    QDir().mkpath(getTemplateDirectory());
    
    QJsonObject templateObj;
    templateObj["name"] = templateName;
    templateObj["description"] = description;
    templateObj["createTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    templateObj["config"] = QJsonDocument::fromJson(exportConfigToString().toUtf8()).object();
    
    QJsonDocument doc(templateObj);
    QFile file(templatePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        return true;
    }
    
    return false;
}

bool ConfigManager::loadConfigTemplate(const QString& templateName)
{
    QString templatePath = getTemplateDirectory() + "/" + templateName + ".json";
    QFile file(templatePath);
    
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull()) {
        return false;
    }
    
    QJsonObject templateObj = doc.object();
    QJsonObject configObj = templateObj["config"].toObject();
    
    // 导入模板配置
    for (auto it = configObj.begin(); it != configObj.end(); ++it) {
        QString group = it.key();
        QJsonObject groupObj = it.value().toObject();
        
        for (auto groupIt = groupObj.begin(); groupIt != groupObj.end(); ++groupIt) {
            QString key = groupIt.key();
            QJsonValue value = groupIt.value();
            QString fullKey = group + "/" + key;
            
            if (value.isString()) {
                setValue(fullKey, value.toString());
            } else if (value.isDouble()) {
                setValue(fullKey, value.toDouble());
            } else if (value.isBool()) {
                setValue(fullKey, value.toBool());
            }
        }
    }
    
    sync();
    emit configTemplateLoaded(templateName);
    return true;
}

QStringList ConfigManager::getConfigTemplates() const
{
    QStringList templates;
    QDir templateDir(getTemplateDirectory());
    
    if (templateDir.exists()) {
        QStringList filters;
        filters << "*.json";
        QFileInfoList files = templateDir.entryInfoList(filters, QDir::Files, QDir::Time);
        
        for (const QFileInfo& file : files) {
            templates.append(file.baseName());
        }
    }
    
    return templates;
}

bool ConfigManager::deleteConfigTemplate(const QString& templateName)
{
    QString templatePath = getTemplateDirectory() + "/" + templateName + ".json";
    return QFile::remove(templatePath);
}

// 配置监控实现
void ConfigManager::startConfigMonitoring()
{
    if (!configWatcher) {
        configWatcher = new QFileSystemWatcher(this);
        connect(configWatcher, &QFileSystemWatcher::fileChanged, 
                this, &ConfigManager::onConfigFileChanged);
    }
    
    QString configPath = settings->fileName();
    if (!configWatcher->files().contains(configPath)) {
        configWatcher->addPath(configPath);
    }
    
    configMonitoring = true;
}

void ConfigManager::stopConfigMonitoring()
{
    if (configWatcher) {
        configWatcher->removePaths(configWatcher->files());
        configWatcher->deleteLater();
        configWatcher = nullptr;
    }
    
    configMonitoring = false;
}

bool ConfigManager::isConfigMonitoring() const
{
    return configMonitoring;
}

// 配置统计实现
int ConfigManager::getConfigChangeCount() const
{
    return configChangeCount;
}

QDateTime ConfigManager::getLastConfigChange() const
{
    return lastConfigChange;
}

QString ConfigManager::getConfigFilePath() const
{
    return settings->fileName();
}

qint64 ConfigManager::getConfigFileSize() const
{
    QFileInfo fileInfo(settings->fileName());
    return fileInfo.size();
}

// 配置分组管理实现
QStringList ConfigManager::getConfigGroups() const
{
    return settings->childGroups();
}

QStringList ConfigManager::getConfigKeys(const QString& group) const
{
    settings->beginGroup(group);
    QStringList keys = settings->childKeys();
    settings->endGroup();
    return keys;
}

bool ConfigManager::hasConfigGroup(const QString& group) const
{
    return settings->childGroups().contains(group);
}

bool ConfigManager::removeConfigGroup(const QString& group)
{
    settings->beginGroup(group);
    settings->remove("");
    settings->endGroup();
    configChangeCount++;
    lastConfigChange = QDateTime::currentDateTime();
    return true;
}

// 配置加密实现
void ConfigManager::setEncryptionEnabled(bool enabled)
{
    encryptionEnabled = enabled;
}

bool ConfigManager::isEncryptionEnabled() const
{
    return encryptionEnabled;
}

void ConfigManager::setEncryptionKey(const QString& key)
{
    encryptionKey = key;
}

// 私有方法实现
void ConfigManager::setupConfigMonitoring()
{
    startConfigMonitoring();
}

void ConfigManager::onConfigFileChanged()
{
    // 配置文件发生变化时重新加载
    settings->sync();
    emit configFileChanged();
}

bool ConfigManager::validateValue(const QString& key, const QVariant& value) const
{
    // 根据键名验证值
    if (key.contains("Port") && key.contains("TCP")) {
        int port = value.toInt();
        return port > 0 && port <= 65535;
    }
    
    if (key.contains("BaudRate")) {
        int rate = value.toInt();
        QList<int> validRates = {9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600};
        return validRates.contains(rate);
    }
    
    if (key.contains("Timeout")) {
        int timeout = value.toInt();
        return timeout >= 100 && timeout <= 60000;
    }
    
    if (key.contains("Host") || key.contains("IP")) {
        QString host = value.toString();
        QHostAddress address(host);
        return !address.isNull() || host == "localhost";
    }
    
    return true;
}

QString ConfigManager::encryptValue(const QString& value) const
{
    if (encryptionKey.isEmpty()) {
        return value;
    }
    
    QByteArray data = value.toUtf8();
    QByteArray key = encryptionKey.toUtf8();
    
    // 简单的XOR加密
    for (int i = 0; i < data.size(); ++i) {
        data[i] = data[i] ^ key[i % key.size()];
    }
    
    return data.toBase64();
}

QString ConfigManager::decryptValue(const QString& value) const
{
    if (encryptionKey.isEmpty()) {
        return value;
    }
    
    QByteArray data = QByteArray::fromBase64(value.toUtf8());
    QByteArray key = encryptionKey.toUtf8();
    
    // 简单的XOR解密
    for (int i = 0; i < data.size(); ++i) {
        data[i] = data[i] ^ key[i % key.size()];
    }
    
    return QString::fromUtf8(data);
}

QString ConfigManager::generateBackupName() const
{
    return QString("config_backup_%1.json")
           .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss"));
}

QString ConfigManager::getBackupDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/backups";
}

QString ConfigManager::getTemplateDirectory() const
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/templates";
} 