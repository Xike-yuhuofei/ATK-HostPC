#ifndef COMMUNICATIONWIDGET_H
#define COMMUNICATIONWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QProgressBar>
#include <QSlider>
#include <QTimer>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTcpSocket>
#include <QTcpServer>
#include <QUdpSocket>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QModbusClient>
#include <QModbusServer>
#include <QtSerialBus/qmodbusrtuserialclient.h>
#include <QModbusTcpClient>
#include <QModbusDataUnit>
#include <QCanBusDevice>
#include <QCanBusDeviceInfo>
#include <QCanBusFrame>
#include "../communication/canworker.h"

// 通讯协议类型
enum class ProtocolType {
    SerialPort = 0,             // 串口
    TcpClient = 1,              // TCP客户端
    TcpServer = 2,              // TCP服务器
    UdpSocket = 3,              // UDP套接字
    ModbusRtu = 4,              // Modbus RTU
    ModbusTcp = 5,              // Modbus TCP
    CanBus = 6,                 // CAN总线
    Custom = 7                  // 自定义协议
};

// 连接状态
enum class ConnectionStatus {
    Disconnected = 0,           // 未连接
    Connecting = 1,             // 连接中
    Connected = 2,              // 已连接
    Error = 3,                  // 错误
    Timeout = 4                 // 超时
};

#include "../communication/icommunication.h"

// 串口配置
struct CommSerialConfig {
    QString portName;           // 端口名
    int baudRate;               // 波特率
    QSerialPort::DataBits dataBits;     // 数据位
    QSerialPort::Parity parity;         // 校验位
    QSerialPort::StopBits stopBits;     // 停止位
    QSerialPort::FlowControl flowControl; // 流控制
};

#include "../communication/tcpcommunication.h"

// Modbus配置
struct ModbusConfig {
    int serverAddress;          // 服务器地址
    int timeout;               // 超时时间
    int numberOfRetries;       // 重试次数
    QJsonObject registerMap;   // 寄存器映射
};

// CAN配置
struct CanConfig {
    QString plugin;            // 插件名称
    QString interface;         // 接口名称
    int bitrate;              // 波特率
    bool loopback;            // 回环模式
    bool receiveOwn;          // 接收自己的消息
    QList<quint32> filters;   // 过滤器
};

// 消息统计
struct MessageStatistics {
    int totalSent;             // 发送总数
    int totalReceived;         // 接收总数
    int totalErrors;           // 错误总数
    double averageLatency;     // 平均延迟
    QDateTime lastMessageTime; // 最后消息时间
    int bytesPerSecond;        // 每秒字节数
};

class CommunicationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CommunicationWidget(QWidget *parent = nullptr);
    ~CommunicationWidget();

    // 连接管理
    bool addConnection(const CommunicationConfig& config);
    bool removeConnection(const QString& name);
    bool updateConnection(const CommunicationConfig& config);
    CommunicationConfig getConnection(const QString& name) const;
    QList<CommunicationConfig> getAllConnections() const;
    
    // 连接控制
    bool connectToDevice(const QString& name);
    void disconnectFromDevice(const QString& name);
    void disconnectAll();
    bool isConnected(const QString& name) const;
    ConnectionStatus getConnectionStatus(const QString& name) const;
    
    // 数据传输
    bool sendData(const QString& connectionName, const QByteArray& data);
    bool sendMessage(const QString& connectionName, const QJsonObject& message);
    bool broadcastMessage(const QJsonObject& message);
    
    // Modbus操作
    bool readCoils(const QString& connectionName, int startAddress, quint16 quantity);
    bool readDiscreteInputs(const QString& connectionName, int startAddress, quint16 quantity);
    bool readHoldingRegisters(const QString& connectionName, int startAddress, quint16 quantity);
    bool readInputRegisters(const QString& connectionName, int startAddress, quint16 quantity);
    bool writeSingleCoil(const QString& connectionName, int address, bool value);
    bool writeSingleRegister(const QString& connectionName, int address, quint16 value);
    bool writeMultipleCoils(const QString& connectionName, int startAddress, const QVector<bool>& values);
    bool writeMultipleRegisters(const QString& connectionName, int startAddress, const QVector<quint16>& values);
    
    // CAN操作
    bool sendCanFrame(const QString& connectionName, quint32 canId, const QByteArray& data);
    bool addCanFilter(const QString& connectionName, quint32 canId, quint32 mask = 0xFFFFFFFF);
    bool removeCanFilter(const QString& connectionName, quint32 canId);
    
    // 统计信息
    MessageStatistics getStatistics(const QString& connectionName) const;
    void resetStatistics(const QString& connectionName);
    void resetAllStatistics();
    
    // 配置管理
    bool saveConfiguration(const QString& filename);
    bool loadConfiguration(const QString& filename);
    void exportConfiguration();
    void importConfiguration();

public slots:
    void onDataReceived(const QString& connectionName, const QByteArray& data);
    void onConnectionStatusChanged(const QString& connectionName, ConnectionStatus status);
    void onErrorOccurred(const QString& connectionName, const QString& error);
    void onStatisticsUpdated(const QString& connectionName, const MessageStatistics& stats);

private slots:
    void onAddConnectionClicked();
    void onEditConnectionClicked();
    void onDeleteConnectionClicked();
    void onConnectClicked();
    void onDisconnectClicked();
    void onDisconnectAllClicked();
    void onRefreshClicked();
    void onConnectionSelectionChanged();
    void onProtocolChanged();
    void onTestConnectionClicked();
    void onSendDataClicked();
    void onClearLogClicked();
    void onSaveLogClicked();
    void onConfigurationChanged();
    void onImportConfigClicked();
    void onExportConfigClicked();
    void onResetStatsClicked();
    void onAutoConnectChanged();
    void onUpdateTimer();
    void onSerialPortChanged();
    void onTcpConfigChanged();
    void onModbusConfigChanged();
    void onCanConfigChanged();
    void onMessageFilterChanged();
    
    // 通讯连接槽函数
    void refreshSerialPorts();
    void connectSerial();
    void disconnectSerial();
    void connectTcp();
    void disconnectTcp();
    void connectCan();
    void disconnectCan();
    void connectModbus();
    void disconnectModbus();
    
    // 设备管理函数
    void addDevice();
    void removeDevice();
    void refreshDevices();
    void resetStatistics();
    void exportStatistics();
    
    // 配置管理函数
    void loadConfiguration();
    void saveConfiguration();

private:
    void setupUI();
    void setupConnections();
    void setupConnectionTab();
    void setupSerialTab();
    void setupTcpTab();
    void setupModbusTab();
    void setupCanTab();
    
    // Tab创建函数
    QWidget* createSerialTab();
    QWidget* createTcpTab();
    QWidget* createCanTab();
    QWidget* createModbusTab();
    QWidget* createDeviceManagementTab();
    QWidget* createStatisticsTab();
    void setupMonitorTab();
    void setupConfigTab();
    
    void loadConnections();
    void updateConnectionList();
    void updateConnectionDetails();
    void updateProtocolConfig();
    void updateStatisticsDisplay();
    void updateMonitorDisplay();
    
    void showConnectionDialog(const CommunicationConfig& config = CommunicationConfig());
    void showSerialConfigDialog(const CommSerialConfig& config);
    void showTcpConfigDialog(const TcpConfig& config);
    void showModbusConfigDialog(const ModbusConfig& config);
    void showCanConfigDialog(const CanConfig& config);
    
    bool validateConfiguration(const CommunicationConfig& config);
    bool validateSerialConfig(const CommSerialConfig& config);
    bool validateTcpConfig(const TcpConfig& config);
    bool validateModbusConfig(const ModbusConfig& config);
    bool validateCanConfig(const CanConfig& config);
    
    void initializeSerialConnection(const QString& name, const CommSerialConfig& config);
    void initializeTcpConnection(const QString& name, const TcpConfig& config);
    void initializeModbusConnection(const QString& name, const ModbusConfig& config);
    void initializeCanConnection(const QString& name, const CanConfig& config);
    
    void closeConnection(const QString& name);
    void handleConnectionError(const QString& name, const QString& error);
    void updateConnectionStatus(const QString& name, ConnectionStatus status);
    
    void logMessage(const QString& connectionName, const QString& message, bool sent = false);
    void updateStatistics(const QString& connectionName, int bytesSent, int bytesReceived, bool error = false);
    
    QStringList getAvailableSerialPorts();
    QStringList getAvailableNetworkInterfaces();
    QStringList getAvailableCanInterfaces();
    
    QString formatConnectionStatus(ConnectionStatus status);
    QString formatProtocolType(ProtocolType protocol);
    QString formatByteArray(const QByteArray& data);
    QString formatTimestamp(const QDateTime& timestamp);
    QColor getStatusColor(ConnectionStatus status);
    QIcon getProtocolIcon(ProtocolType protocol);

private:
    // UI组件
    QTabWidget* m_tabWidget;
    
    // 连接管理标签页
    QWidget* m_connectionTab;
    QTableWidget* m_connectionTable;
    QPushButton* m_addConnectionBtn;
    QPushButton* m_editConnectionBtn;
    QPushButton* m_deleteConnectionBtn;
    QPushButton* m_connectBtn;
    QPushButton* m_disconnectBtn;
    QPushButton* m_disconnectAllBtn;
    QPushButton* m_refreshBtn;
    QPushButton* m_testConnectionBtn;
    QLabel* m_connectionStatusLabel;
    QLabel* m_connectionCountLabel;
    
    // 串口配置标签页
    QWidget* m_serialTab;
    QComboBox* m_serialPortCombo;
    QComboBox* m_serialBaudRateCombo;
    QComboBox* m_serialDataBitsCombo;
    QComboBox* m_serialParityCombo;
    QComboBox* m_serialStopBitsCombo;
    QComboBox* m_serialFlowControlCombo;
    QPushButton* m_serialRefreshBtn;
    QPushButton* m_serialConnectBtn;
    QPushButton* m_serialDisconnectBtn;
    QLabel* m_serialStatusLabel;
    QTextEdit* m_serialLogEdit;
    QLineEdit* m_serialSendEdit;
    QPushButton* m_serialSendBtn;
    QPushButton* m_serialClearBtn;
    
    // TCP配置标签页
    QWidget* m_tcpTab;
    QComboBox* m_tcpModeCombo;
    QLineEdit* m_tcpHostEdit;
    QSpinBox* m_tcpPortSpinBox;
    QSpinBox* m_tcpPortSpin;
    QSpinBox* m_tcpTimeoutSpin;
    QCheckBox* m_tcpAutoReconnectCheck;
    QCheckBox* m_tcpServerCheckBox;
    QSpinBox* m_tcpMaxConnectionsSpinBox;
    QCheckBox* m_tcpKeepAliveCheckBox;
    QPushButton* m_tcpConnectBtn;
    QPushButton* m_tcpDisconnectBtn;
    QLabel* m_tcpStatusLabel;
    QTextEdit* m_tcpLogEdit;
    QLineEdit* m_tcpSendEdit;
    QPushButton* m_tcpSendBtn;
    QPushButton* m_tcpClearBtn;
    QTableWidget* m_tcpClientsTable;
    
    // Modbus配置标签页
    QWidget* m_modbusTab;
    QComboBox* m_modbusModeCombo;
    QSpinBox* m_modbusSlaveIdSpin;
    QSpinBox* m_modbusServerAddressSpinBox;
    QSpinBox* m_modbusTimeoutSpinBox;
    QSpinBox* m_modbusTimeoutSpin;
    QSpinBox* m_modbusRetriesSpinBox;
    QSpinBox* m_modbusRetriesSpin;
    QPushButton* m_modbusConnectBtn;
    QPushButton* m_modbusDisconnectBtn;
    QLabel* m_modbusStatusLabel;
    QTableWidget* m_modbusRegisterTable;
    QSpinBox* m_modbusStartAddressSpinBox;
    QSpinBox* m_modbusQuantitySpinBox;
    QPushButton* m_modbusReadCoilsBtn;
    QPushButton* m_modbusReadRegistersBtn;
    QPushButton* m_modbusWriteCoilBtn;
    QPushButton* m_modbusWriteRegisterBtn;
    QTextEdit* m_modbusLogEdit;
    
    // CAN配置标签页
    QWidget* m_canTab;
    QComboBox* m_canPluginCombo;
    QComboBox* m_canInterfaceCombo;
    QLineEdit* m_canInterfaceEdit;
    QComboBox* m_canBitrateCombo;
    QSpinBox* m_canBitrateSpin;
    QDoubleSpinBox* m_canSamplePointSpin;
    QCheckBox* m_canLoopbackCheckBox;
    QCheckBox* m_canLoopbackCheck;
    QCheckBox* m_canReceiveOwnCheckBox;
    QCheckBox* m_canReceiveOwnCheck;
    QPushButton* m_canConnectBtn;
    QPushButton* m_canDisconnectBtn;
    QLabel* m_canStatusLabel;
    QTableWidget* m_canFiltersTable;
    QLineEdit* m_canIdEdit;
    QLineEdit* m_canDataEdit;
    QPushButton* m_canSendBtn;
    QTextEdit* m_canLogEdit;
    QPushButton* m_canClearBtn;
    QPushButton* m_canAddFilterBtn;
    QPushButton* m_canRemoveFilterBtn;
    
    // 监控标签页
    QWidget* m_monitorTab;
    QTableWidget* m_statisticsTable;
    QTextEdit* m_allMessagesEdit;
    QPushButton* m_clearAllLogsBtn;
    QPushButton* m_saveAllLogsBtn;
    QPushButton* m_resetStatsBtn;
    QProgressBar* m_dataRateBar;
    QLabel* m_totalConnectionsLabel;
    QLabel* m_activeConnectionsLabel;
    QLabel* m_totalMessagesLabel;
    QLabel* m_errorRateLabel;
    
    // 配置标签页
    QWidget* m_configTab;
    QCheckBox* m_autoConnectCheckBox;
    QSpinBox* m_retryCountSpinBox;
    QSpinBox* m_timeoutSpinBox;
    QSpinBox* m_updateIntervalSpinBox;
    QCheckBox* m_enableLoggingCheckBox;
    QLineEdit* m_logFilePathEdit;
    QPushButton* m_browseLogPathBtn;
    QPushButton* m_importConfigBtn;
    QPushButton* m_exportConfigBtn;
    QPushButton* m_resetConfigBtn;
    QPushButton* m_saveConfigBtn;
    
    // 数据模型
    QStandardItemModel* m_connectionModel;
    QStandardItemModel* m_statisticsModel;
    QStandardItemModel* m_tcpClientsModel;
    QStandardItemModel* m_modbusRegisterModel;
    QStandardItemModel* m_canFiltersModel;
    QSortFilterProxyModel* m_connectionProxy;
    
    // 通讯对象
    QMap<QString, QSerialPort*> m_serialPorts;
    QMap<QString, QTcpSocket*> m_tcpClients;
    QMap<QString, QTcpServer*> m_tcpServers;
    QMap<QString, QUdpSocket*> m_udpSockets;
    QMap<QString, QModbusClient*> m_modbusClients;
    QMap<QString, CanWorker*> m_canWorkers;
    
    // 配置数据
    QList<CommunicationConfig> m_connections;
    QMap<QString, MessageStatistics> m_statistics;
    QMap<QString, ConnectionStatus> m_connectionStatus;
    
    // 定时器
    QTimer* m_updateTimer;
    
    // 配置参数
    bool m_autoConnect;
    int m_retryCount;
    int m_timeout;
    int m_updateInterval;
    bool m_enableLogging;
    QString m_logFilePath;
    QString m_configFilePath;
    
    // 状态变量
    int m_totalConnections;
    int m_activeConnections;
    int m_totalMessages;
    double m_errorRate;

signals:
    void connectionAdded(const QString& name);
    void connectionRemoved(const QString& name);
    void connectionStatusChanged(const QString& name, ConnectionStatus status);
    void dataReceived(const QString& connectionName, const QByteArray& data);
    void dataSent(const QString& connectionName, const QByteArray& data);
    void errorOccurred(const QString& connectionName, const QString& error);
    void messageReceived(const QString& connectionName, const QJsonObject& message);
    void statisticsUpdated(const QString& connectionName, const MessageStatistics& stats);
    void configurationChanged();
    void logMessage(const QString& message);
    
    // 通讯连接信号
    void serialConnected();
    void serialDisconnected();
    void tcpConnected();
    void tcpDisconnected();
    void canConnected();
    void canDisconnected();
    void modbusConnected();
    void modbusDisconnected();
};

#endif // COMMUNICATIONWIDGET_H 