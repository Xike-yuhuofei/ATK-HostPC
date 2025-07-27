#pragma once

#include <QObject>
#include <QByteArray>
#include <QQueue>
#include <QTimer>
#include <QDateTime>
#include <QVariant>
#include "constants.h"
#include "utils/checksum.h"

// 命令码定义
enum class ProtocolCommand : quint8 {
    // 设备控制命令
    DeviceStart = 0x01,
    DeviceStop = 0x02,
    DeviceReset = 0x03,
    DeviceStatus = 0x04,
    PauseDevice = 0x05,
    ResumeDevice = 0x06,
    HomeDevice = 0x07,
    EmergencyStop = 0x08,
    
    // 参数读写命令
    ReadParameter = 0x10,
    WriteParameter = 0x11,
    ReadAllParameters = 0x12,
    WriteAllParameters = 0x13,
    
    // 运动控制命令
    MoveToPosition = 0x15,
    JogMove = 0x16,
    SetOrigin = 0x17,
    GetPosition = 0x18,
    
    // 点胶控制命令
    StartGlue = 0x19,
    StopGlue = 0x1A,
    SetGlueParameters = 0x1B,
    GetGlueParameters = 0x1C,
    
    // 数据采集命令
    ReadSensorData = 0x20,
    ReadAllSensors = 0x21,
    StartDataCollection = 0x22,
    StopDataCollection = 0x23,
    
    // 系统命令
    GetDeviceInfo = 0x30,
    GetVersionInfo = 0x31,
    SetDateTime = 0x32,
    GetDateTime = 0x33,
    Heartbeat = 0x34,
    
    // 升级命令
    StartUpgrade = 0x40,
    UpgradeData = 0x41,
    EndUpgrade = 0x42,
    
    // 响应命令
    Response = 0x80,
    Error = 0xFF
};

// 错误码定义
enum class ProtocolError : quint8 {
    None = 0x00,
    InvalidCommand = 0x01,
    InvalidParameter = 0x02,
    ChecksumError = 0x03,
    DeviceNotReady = 0x04,
    DataTooLong = 0x05,
    TimeoutError = 0x06,
    UnknownError = 0xFF
};

// 协议帧结构
struct ProtocolFrame {
    quint16 header;      // 帧头 0xAA55
    ProtocolCommand command; // 命令码
    quint8 dataLength;   // 数据长度
    QByteArray data;     // 数据区
    quint8 checksum;     // 校验和
    quint8 tail;         // 帧尾 0x0D
    QDateTime timestamp; // 帧时间戳
    
    bool isValid() const {
        return header == 0xAA55 && tail == 0x0D && data.size() == dataLength;
    }
};

class ProtocolParser : public QObject
{
    Q_OBJECT

public:
    explicit ProtocolParser(QObject* parent = nullptr);
    
    // 数据解析
    void parseData(const QByteArray& data);
    
    // 基础帧构建
    QByteArray buildFrame(ProtocolCommand command, const QByteArray& data = QByteArray());
    QByteArray buildResponseFrame(ProtocolCommand originalCommand, const QByteArray& data = QByteArray());
    QByteArray buildErrorFrame(ProtocolError error, const QString& message = QString());
    
    // 高级帧构建
    QByteArray buildHeartbeatFrame();
    QByteArray buildParameterFrame(const QString& paramName, const QVariant& value);
    QByteArray buildMotionFrame(double x, double y, double z, double speed);
    QByteArray buildGlueFrame(double volume, double pressure, double temperature, int time);
    
    // 数据解析
    bool parseParameterResponse(const QByteArray& data, QString& paramName, QVariant& value);
    bool parseMotionResponse(const QByteArray& data, double& x, double& y, double& z, double& speed);
    bool parseGlueResponse(const QByteArray& data, double& volume, double& pressure, double& temperature, int& time);
    
    // 帧验证
    bool validateFrameIntegrity(const ProtocolFrame& frame);
    
    // 增强的帧完整性检查
    EnhancedChecksum::FrameIntegrityResult checkAdvancedFrameIntegrity(const QByteArray& frameData);
    
    // 校验类型管理
    void setChecksumType(ChecksumType type);
    ChecksumType getChecksumType() const;
    bool isEnhancedChecksumEnabled() const;
    void enableEnhancedChecksum(bool enabled);
    
    // 工具函数
    static quint8 calculateChecksum(const QByteArray& data);
    static QString commandToString(ProtocolCommand command);
    static QString errorToString(ProtocolError error);
    
    // 设置超时时间
    void setTimeout(int timeout);
    
    // 清空缓冲区
    void clearBuffer();
    
    // 性能统计
    QString getPerformanceStats() const;

signals:
    void frameReceived(const ProtocolFrame& frame);
    void parseError(const QString& error);
    void timeoutOccurred();
    
    // 高级信号
    void heartbeatReceived(qint64 delay);
    void motionDataReceived(double x, double y, double z, double speed);
    void glueDataReceived(double volume, double pressure, double temperature, int time);
    void parameterReceived(const QString& name, const QVariant& value);

private slots:
    void onTimeout();

private:
    bool findFrame(QByteArray& frame);
    bool validateFrame(const QByteArray& frameData, ProtocolFrame& frame);
    void processCompleteFrame(const ProtocolFrame& frame);
    
    // 高级帧处理
    void processAdvancedFrame(const ProtocolFrame& frame);
    void processHeartbeatFrame(const ProtocolFrame& frame);
    void processMotionFrame(const ProtocolFrame& frame);
    void processGlueFrame(const ProtocolFrame& frame);
    void processParameterFrame(const ProtocolFrame& frame);
    
    // 性能优化相关
    bool findFrameOptimized(QByteArray& frame);
    void optimizeBuffer();
    void preallocateMemory();
    
    QByteArray receiveBuffer;
    QQueue<ProtocolFrame> frameQueue;
    QTimer* timeoutTimer;
    int timeoutMs;
    
    // 增强校验相关
    ChecksumType m_checksumType;
    bool m_enhancedChecksumEnabled;
    
    // 性能优化相关成员
    int m_lastHeaderPos;           // 上次找到帧头的位置
    int m_bufferSearchStart;       // 缓冲区搜索起始位置
    QByteArray m_tempFrameBuffer;  // 临时帧缓冲区（预分配）
    bool m_bufferOptimized;        // 缓冲区是否已优化
    
    // 性能统计
    struct PerformanceStats {
        qint64 totalBytesProcessed;
        qint64 totalFramesProcessed;
        qint64 totalParseTime;
        qint64 averageParseTime;
        QDateTime lastStatsUpdate;
    } m_perfStats;
    
    // 协议常量已移动到 constants.h
}; 