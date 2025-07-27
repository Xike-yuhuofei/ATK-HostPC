#pragma once

#include <QObject>
#include <QString>
#include <QMap>
#include <QVariant>

/**
 * @brief 业务逻辑管理器类
 * 
 * 负责处理应用程序的核心业务逻辑，包括：
 * - 设备连接管理
 * - 数据处理
 * - 报警系统
 * - 业务规则执行
 */
class BusinessLogicManager : public QObject
{
    Q_OBJECT

public:
    explicit BusinessLogicManager(QObject* parent = nullptr);
    ~BusinessLogicManager();

    // 初始化和关闭
    void initialize();
    void shutdown();
    
    // 设备管理
    void connectDevice();
    void disconnectDevice();
    QString getDeviceStatus() const;
    bool isDeviceConnected() const;
    
    // 数据处理
    void processData(const QByteArray& data);
    void exportData(const QString& filename);
    void importData(const QString& filename);
    
    // 报警管理
    void triggerAlarm(const QString& alarmType, const QString& message);
    void acknowledgeAlarm(const QString& alarmId);
    
    // 参数管理
    void setParameter(const QString& name, const QVariant& value);
    QVariant getParameter(const QString& name) const;
    void saveParameters();
    void loadParameters();

signals:
    // 设备状态信号
    void deviceStatusChanged(const QString& status);
    void deviceConnected();
    void deviceDisconnected();
    void deviceError(const QString& error);
    
    // 数据处理信号
    void dataProcessed(const QByteArray& data);
    void dataExported(const QString& filename);
    void dataImported(const QString& filename);
    
    // 报警信号
    void alarmTriggered(const QString& alarmType, const QString& message);
    void alarmAcknowledged(const QString& alarmId);
    
    // 参数信号
    void parameterChanged(const QString& name, const QVariant& value);
    void parametersSaved();
    void parametersLoaded();

private:
    // 初始化方法
    void initializeDeviceLogic();
    void initializeDataProcessing();
    void initializeAlarmSystem();
    
    // 关闭方法
    void shutdownDeviceLogic();
    void shutdownDataProcessing();
    void shutdownAlarmSystem();
    
    // 成员变量
    bool m_initialized;
    bool m_deviceConnected;
    QString m_currentStatus;
    QMap<QString, QVariant> m_parameters;
}; 