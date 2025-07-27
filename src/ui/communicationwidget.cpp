#include "communicationwidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>
#include <QDebug>

CommunicationWidget::CommunicationWidget(QWidget *parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_updateTimer(new QTimer(this))
    , m_autoConnect(false)
    , m_retryCount(3)
    , m_timeout(5000)
    , m_updateInterval(1000)
    , m_enableLogging(true)
    , m_totalConnections(0)
    , m_activeConnections(0)
    , m_totalMessages(0)
    , m_errorRate(0.0)
{
    setupUI();
    // 暂时跳过setupConnections，避免空指针连接
    // setupConnections();
    // 暂时跳过配置加载
    // loadConfiguration();
}

CommunicationWidget::~CommunicationWidget()
{
    // 调用无参数的私有方法
    saveConfiguration();
}

void CommunicationWidget::setupUI()
{
    auto mainLayout = new QVBoxLayout(this);
    
    // 创建标签页
    m_tabWidget = new QTabWidget;
    
    // 暂时使用简化的标签页，避免复杂组件导致崩溃
    QWidget* serialPlaceholder = new QWidget();
    QVBoxLayout* serialLayout = new QVBoxLayout(serialPlaceholder);
    serialLayout->addWidget(new QLabel("串口通讯功能"));
    m_tabWidget->addTab(serialPlaceholder, "串口通讯");
    
    QWidget* tcpPlaceholder = new QWidget();
    QVBoxLayout* tcpLayout = new QVBoxLayout(tcpPlaceholder);
    tcpLayout->addWidget(new QLabel("TCP/IP通讯功能"));
    m_tabWidget->addTab(tcpPlaceholder, "TCP/IP通讯");
    
    // 暂时简化其他标签页
    QWidget* canPlaceholder = new QWidget();
    QVBoxLayout* canLayout = new QVBoxLayout(canPlaceholder);
    canLayout->addWidget(new QLabel("CAN总线功能"));
    m_tabWidget->addTab(canPlaceholder, "CAN总线");
    
    QWidget* modbusPlaceholder = new QWidget();
    QVBoxLayout* modbusLayout = new QVBoxLayout(modbusPlaceholder);
    modbusLayout->addWidget(new QLabel("Modbus通讯功能"));
    m_tabWidget->addTab(modbusPlaceholder, "Modbus通讯");
    
    QWidget* devicePlaceholder = new QWidget();
    QVBoxLayout* deviceLayout = new QVBoxLayout(devicePlaceholder);
    deviceLayout->addWidget(new QLabel("设备管理功能"));
    m_tabWidget->addTab(devicePlaceholder, "设备管理");
    
    QWidget* statsPlaceholder = new QWidget();
    QVBoxLayout* statsLayout = new QVBoxLayout(statsPlaceholder);
    statsLayout->addWidget(new QLabel("统计监控功能"));
    m_tabWidget->addTab(statsPlaceholder, "统计监控");
    
    mainLayout->addWidget(m_tabWidget);
}

QWidget* CommunicationWidget::createSerialTab()
{
    auto widget = new QWidget;
    auto layout = new QVBoxLayout(widget);
    
    // 串口配置组
    auto configGroup = new QGroupBox("串口配置");
    auto configLayout = new QFormLayout(configGroup);
    
    m_serialPortCombo = new QComboBox;
    m_serialBaudRateCombo = new QComboBox;
    m_serialBaudRateCombo->addItems({"9600", "19200", "38400", "57600", "115200", "230400", "460800", "921600"});
    m_serialBaudRateCombo->setCurrentText("115200");
    
    m_serialDataBitsCombo = new QComboBox;
    m_serialDataBitsCombo->addItems({"5", "6", "7", "8"});
    m_serialDataBitsCombo->setCurrentText("8");
    
    m_serialParityCombo = new QComboBox;
    m_serialParityCombo->addItems({"None", "Even", "Odd", "Space", "Mark"});
    
    m_serialStopBitsCombo = new QComboBox;
    m_serialStopBitsCombo->addItems({"1", "1.5", "2"});
    
    m_serialFlowControlCombo = new QComboBox;
    m_serialFlowControlCombo->addItems({"None", "RTS/CTS", "XON/XOFF"});
    
    configLayout->addRow("端口:", m_serialPortCombo);
    configLayout->addRow("波特率:", m_serialBaudRateCombo);
    configLayout->addRow("数据位:", m_serialDataBitsCombo);
    configLayout->addRow("校验位:", m_serialParityCombo);
    configLayout->addRow("停止位:", m_serialStopBitsCombo);
    configLayout->addRow("流控制:", m_serialFlowControlCombo);
    
    // 控制按钮
    auto buttonLayout = new QHBoxLayout;
    m_serialRefreshBtn = new QPushButton("刷新端口");
    m_serialConnectBtn = new QPushButton("连接");
    m_serialDisconnectBtn = new QPushButton("断开");
    m_serialDisconnectBtn->setEnabled(false);
    
    buttonLayout->addWidget(m_serialRefreshBtn);
    buttonLayout->addWidget(m_serialConnectBtn);
    buttonLayout->addWidget(m_serialDisconnectBtn);
    buttonLayout->addStretch();
    
    // 状态显示
    m_serialStatusLabel = new QLabel("状态: 未连接");
    
    layout->addWidget(configGroup);
    layout->addLayout(buttonLayout);
    layout->addWidget(m_serialStatusLabel);
    layout->addStretch();
    
    return widget;
}

QWidget* CommunicationWidget::createTcpTab()
{
    auto widget = new QWidget;
    auto layout = new QVBoxLayout(widget);
    
    // TCP配置组
    auto configGroup = new QGroupBox("TCP/IP配置");
    auto configLayout = new QFormLayout(configGroup);
    
    m_tcpModeCombo = new QComboBox;
    m_tcpModeCombo->addItems({"客户端", "服务器"});
    
    m_tcpHostEdit = new QLineEdit("192.168.1.100");
    m_tcpPortSpin = new QSpinBox;
    m_tcpPortSpin->setRange(1, 65535);
    m_tcpPortSpin->setValue(8080);
    
    m_tcpTimeoutSpin = new QSpinBox;
    m_tcpTimeoutSpin->setRange(1000, 30000);
    m_tcpTimeoutSpin->setValue(5000);
    m_tcpTimeoutSpin->setSuffix(" ms");
    
    m_tcpAutoReconnectCheck = new QCheckBox("自动重连");
    m_tcpAutoReconnectCheck->setChecked(true);
    
    configLayout->addRow("工作模式:", m_tcpModeCombo);
    configLayout->addRow("主机地址:", m_tcpHostEdit);
    configLayout->addRow("端口号:", m_tcpPortSpin);
    configLayout->addRow("超时时间:", m_tcpTimeoutSpin);
    configLayout->addRow("", m_tcpAutoReconnectCheck);
    
    // 控制按钮
    auto buttonLayout = new QHBoxLayout;
    m_tcpConnectBtn = new QPushButton("连接");
    m_tcpDisconnectBtn = new QPushButton("断开");
    m_tcpDisconnectBtn->setEnabled(false);
    
    buttonLayout->addWidget(m_tcpConnectBtn);
    buttonLayout->addWidget(m_tcpDisconnectBtn);
    buttonLayout->addStretch();
    
    // 状态显示
    m_tcpStatusLabel = new QLabel("状态: 未连接");
    
    layout->addWidget(configGroup);
    layout->addLayout(buttonLayout);
    layout->addWidget(m_tcpStatusLabel);
    layout->addStretch();
    
    return widget;
}

QWidget* CommunicationWidget::createCanTab()
{
    auto widget = new QWidget;
    auto layout = new QVBoxLayout(widget);
    
    // CAN配置组
    auto configGroup = new QGroupBox("CAN总线配置");
    auto configLayout = new QFormLayout(configGroup);
    
    m_canPluginCombo = new QComboBox;
    m_canPluginCombo->addItems({"socketcan", "peakcan", "tinycan", "vectorcan"});
    
    m_canInterfaceEdit = new QLineEdit("can0");
    
    m_canBitrateSpin = new QSpinBox;
    m_canBitrateSpin->setRange(10000, 1000000);
    m_canBitrateSpin->setValue(250000);
    
    m_canSamplePointSpin = new QDoubleSpinBox;
    m_canSamplePointSpin->setRange(0.1, 0.9);
    m_canSamplePointSpin->setValue(0.75);
    m_canSamplePointSpin->setSingleStep(0.01);
    
    m_canLoopbackCheck = new QCheckBox("环回模式");
    m_canReceiveOwnCheck = new QCheckBox("接收自己的消息");
    
    configLayout->addRow("CAN插件:", m_canPluginCombo);
    configLayout->addRow("接口名称:", m_canInterfaceEdit);
    configLayout->addRow("波特率:", m_canBitrateSpin);
    configLayout->addRow("采样点:", m_canSamplePointSpin);
    configLayout->addRow("", m_canLoopbackCheck);
    configLayout->addRow("", m_canReceiveOwnCheck);
    
    // 控制按钮
    auto buttonLayout = new QHBoxLayout;
    m_canConnectBtn = new QPushButton("连接");
    m_canDisconnectBtn = new QPushButton("断开");
    m_canDisconnectBtn->setEnabled(false);
    
    buttonLayout->addWidget(m_canConnectBtn);
    buttonLayout->addWidget(m_canDisconnectBtn);
    buttonLayout->addStretch();
    
    // 状态显示
    m_canStatusLabel = new QLabel("状态: 未连接");
    
    layout->addWidget(configGroup);
    layout->addLayout(buttonLayout);
    layout->addWidget(m_canStatusLabel);
    layout->addStretch();
    
    return widget;
}

QWidget* CommunicationWidget::createModbusTab()
{
    auto modbusTab = new QWidget();
    auto layout = new QVBoxLayout(modbusTab);
    
    // 连接配置组
    auto configGroup = new QGroupBox("Modbus连接配置");
    auto configLayout = new QGridLayout(configGroup);
    
    int row = 0;
    
    // 连接类型
    configLayout->addWidget(new QLabel("连接类型:"), row, 0);
    auto connectionTypeCombo = new QComboBox();
    connectionTypeCombo->addItems({"TCP", "RTU串口"});
    configLayout->addWidget(connectionTypeCombo, row++, 1);
    
    // TCP配置
    configLayout->addWidget(new QLabel("IP地址:"), row, 0);
    auto ipEdit = new QLineEdit("192.168.1.100");
    configLayout->addWidget(ipEdit, row++, 1);
    
    configLayout->addWidget(new QLabel("端口:"), row, 0);
    auto portSpinBox = new QSpinBox();
    portSpinBox->setRange(1, 65535);
    portSpinBox->setValue(502);
    configLayout->addWidget(portSpinBox, row++, 1);
    
    // RTU配置
    configLayout->addWidget(new QLabel("串口:"), row, 0);
    auto serialPortCombo = new QComboBox();
    serialPortCombo->addItems({"COM1", "COM2", "COM3", "COM4"});
    configLayout->addWidget(serialPortCombo, row++, 1);
    
    configLayout->addWidget(new QLabel("波特率:"), row, 0);
    auto baudRateCombo = new QComboBox();
    baudRateCombo->addItems({"9600", "19200", "38400", "57600", "115200"});
    baudRateCombo->setCurrentText("9600");
    configLayout->addWidget(baudRateCombo, row++, 1);
    
    // 从站地址
    configLayout->addWidget(new QLabel("从站地址:"), row, 0);
    auto slaveIdSpinBox = new QSpinBox();
    slaveIdSpinBox->setRange(1, 247);
    slaveIdSpinBox->setValue(1);
    configLayout->addWidget(slaveIdSpinBox, row++, 1);
    
    // 连接按钮
    auto buttonLayout = new QHBoxLayout();
    auto connectBtn = new QPushButton("连接");
    auto disconnectBtn = new QPushButton("断开");
    auto testBtn = new QPushButton("测试连接");
    
    connectBtn->setProperty("class", "success");
    disconnectBtn->setProperty("class", "danger");
    testBtn->setProperty("class", "info");
    
    buttonLayout->addWidget(connectBtn);
    buttonLayout->addWidget(disconnectBtn);
    buttonLayout->addWidget(testBtn);
    buttonLayout->addStretch();
    
    // 寄存器操作组
    auto registerGroup = new QGroupBox("寄存器操作");
    auto registerLayout = new QGridLayout(registerGroup);
    
    row = 0;
    
    // 功能码
    registerLayout->addWidget(new QLabel("功能码:"), row, 0);
    auto functionCodeCombo = new QComboBox();
    functionCodeCombo->addItems({
        "01 - 读线圈",
        "02 - 读离散输入",
        "03 - 读保持寄存器",
        "04 - 读输入寄存器",
        "05 - 写单个线圈",
        "06 - 写单个寄存器",
        "15 - 写多个线圈",
        "16 - 写多个寄存器"
    });
    registerLayout->addWidget(functionCodeCombo, row++, 1);
    
    // 起始地址
    registerLayout->addWidget(new QLabel("起始地址:"), row, 0);
    auto startAddressSpinBox = new QSpinBox();
    startAddressSpinBox->setRange(0, 65535);
    startAddressSpinBox->setValue(0);
    registerLayout->addWidget(startAddressSpinBox, row++, 1);
    
    // 数量/值
    registerLayout->addWidget(new QLabel("数量/值:"), row, 0);
    auto quantitySpinBox = new QSpinBox();
    quantitySpinBox->setRange(1, 125);
    quantitySpinBox->setValue(1);
    registerLayout->addWidget(quantitySpinBox, row++, 1);
    
    // 操作按钮
    auto operationLayout = new QHBoxLayout();
    auto readBtn = new QPushButton("读取");
    auto writeBtn = new QPushButton("写入");
    auto clearBtn = new QPushButton("清除");
    
    readBtn->setProperty("class", "primary");
    writeBtn->setProperty("class", "warning");
    
    operationLayout->addWidget(readBtn);
    operationLayout->addWidget(writeBtn);
    operationLayout->addWidget(clearBtn);
    operationLayout->addStretch();
    
    // 结果显示
    auto resultGroup = new QGroupBox("操作结果");
    auto resultLayout = new QVBoxLayout(resultGroup);
    
    auto resultTable = new QTableWidget(0, 4);
    resultTable->setHorizontalHeaderLabels({"地址", "值(十进制)", "值(十六进制)", "值(二进制)"});
    resultTable->horizontalHeader()->setStretchLastSection(true);
    resultLayout->addWidget(resultTable);
    
    // 状态显示
    auto statusGroup = new QGroupBox("连接状态");
    auto statusLayout = new QVBoxLayout(statusGroup);
    
    auto statusLabel = new QLabel("未连接");
    statusLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    statusLayout->addWidget(statusLabel);
    
    auto statisticsLayout = new QGridLayout();
    statisticsLayout->addWidget(new QLabel("发送帧数:"), 0, 0);
    statisticsLayout->addWidget(new QLabel("0"), 0, 1);
    statisticsLayout->addWidget(new QLabel("接收帧数:"), 1, 0);
    statisticsLayout->addWidget(new QLabel("0"), 1, 1);
    statisticsLayout->addWidget(new QLabel("错误次数:"), 2, 0);
    statisticsLayout->addWidget(new QLabel("0"), 2, 1);
    statusLayout->addLayout(statisticsLayout);
    
    // 布局组装
    layout->addWidget(configGroup);
    layout->addLayout(buttonLayout);
    layout->addWidget(registerGroup);
    layout->addLayout(operationLayout);
    layout->addWidget(resultGroup);
    layout->addWidget(statusGroup);
    layout->addStretch();
    
    return modbusTab;
}

QWidget* CommunicationWidget::createDeviceManagementTab()
{
    auto deviceTab = new QWidget();
    auto layout = new QVBoxLayout(deviceTab);
    
    // 设备列表组
    auto deviceListGroup = new QGroupBox("设备列表");
    auto deviceListLayout = new QVBoxLayout(deviceListGroup);
    
    // 工具栏
    auto toolBar = new QHBoxLayout();
    auto addDeviceBtn = new QPushButton("添加设备");
    auto editDeviceBtn = new QPushButton("编辑设备");
    auto removeDeviceBtn = new QPushButton("删除设备");
    auto refreshBtn = new QPushButton("刷新");
    auto connectAllBtn = new QPushButton("连接所有");
    auto disconnectAllBtn = new QPushButton("断开所有");
    
    addDeviceBtn->setProperty("class", "success");
    editDeviceBtn->setProperty("class", "primary");
    removeDeviceBtn->setProperty("class", "danger");
    connectAllBtn->setProperty("class", "info");
    disconnectAllBtn->setProperty("class", "warning");
    
    toolBar->addWidget(addDeviceBtn);
    toolBar->addWidget(editDeviceBtn);
    toolBar->addWidget(removeDeviceBtn);
    toolBar->addWidget(refreshBtn);
    toolBar->addStretch();
    toolBar->addWidget(connectAllBtn);
    toolBar->addWidget(disconnectAllBtn);
    
    // 设备表格
    auto deviceTable = new QTableWidget(0, 8);
    QStringList headers = {"设备名称", "类型", "地址", "端口", "状态", "最后通信", "错误次数", "操作"};
    deviceTable->setHorizontalHeaderLabels(headers);
    deviceTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    deviceTable->setAlternatingRowColors(true);
    deviceTable->horizontalHeader()->setStretchLastSection(true);
    
    // 添加示例数据
    deviceTable->insertRow(0);
    deviceTable->setItem(0, 0, new QTableWidgetItem("点胶控制器"));
    deviceTable->setItem(0, 1, new QTableWidgetItem("串口"));
    deviceTable->setItem(0, 2, new QTableWidgetItem("COM1"));
    deviceTable->setItem(0, 3, new QTableWidgetItem("115200"));
    deviceTable->setItem(0, 4, new QTableWidgetItem("已连接"));
    deviceTable->setItem(0, 5, new QTableWidgetItem("2024-01-01 12:00:00"));
    deviceTable->setItem(0, 6, new QTableWidgetItem("0"));
    
    auto operationWidget = new QWidget();
    auto operationLayout = new QHBoxLayout(operationWidget);
    operationLayout->setContentsMargins(2, 2, 2, 2);
    auto connectBtn = new QPushButton("连接");
    auto disconnectBtn = new QPushButton("断开");
    connectBtn->setMaximumWidth(60);
    disconnectBtn->setMaximumWidth(60);
    operationLayout->addWidget(connectBtn);
    operationLayout->addWidget(disconnectBtn);
    deviceTable->setCellWidget(0, 7, operationWidget);
    
    deviceListLayout->addLayout(toolBar);
    deviceListLayout->addWidget(deviceTable);
    
    // 设备详情组
    auto deviceDetailsGroup = new QGroupBox("设备详情");
    auto detailsLayout = new QGridLayout(deviceDetailsGroup);
    
    int row = 0;
    detailsLayout->addWidget(new QLabel("设备名称:"), row, 0);
    detailsLayout->addWidget(new QLabel("点胶控制器"), row++, 1);
    
    detailsLayout->addWidget(new QLabel("设备类型:"), row, 0);
    detailsLayout->addWidget(new QLabel("串口设备"), row++, 1);
    
    detailsLayout->addWidget(new QLabel("通信协议:"), row, 0);
    detailsLayout->addWidget(new QLabel("自定义协议"), row++, 1);
    
    detailsLayout->addWidget(new QLabel("连接状态:"), row, 0);
    auto connectionStatusLabel = new QLabel("已连接");
    connectionStatusLabel->setStyleSheet("QLabel { color: green; font-weight: bold; }");
    detailsLayout->addWidget(connectionStatusLabel, row++, 1);
    
    detailsLayout->addWidget(new QLabel("最后通信:"), row, 0);
    detailsLayout->addWidget(new QLabel("2024-01-01 12:00:00"), row++, 1);
    
    detailsLayout->addWidget(new QLabel("通信统计:"), row, 0);
    auto statsWidget = new QWidget();
    auto statsLayout = new QVBoxLayout(statsWidget);
    statsLayout->addWidget(new QLabel("发送: 1234 帧"));
    statsLayout->addWidget(new QLabel("接收: 1230 帧"));
    statsLayout->addWidget(new QLabel("错误: 4 帧"));
    detailsLayout->addWidget(statsWidget, row++, 1);
    
    // 通信监控组
    auto monitorGroup = new QGroupBox("通信监控");
    auto monitorLayout = new QVBoxLayout(monitorGroup);
    
    // 监控控制
    auto monitorControlLayout = new QHBoxLayout();
    auto startMonitorBtn = new QPushButton("开始监控");
    auto stopMonitorBtn = new QPushButton("停止监控");
    auto clearLogBtn = new QPushButton("清除日志");
    auto saveLogBtn = new QPushButton("保存日志");
    
    startMonitorBtn->setProperty("class", "success");
    stopMonitorBtn->setProperty("class", "danger");
    
    monitorControlLayout->addWidget(startMonitorBtn);
    monitorControlLayout->addWidget(stopMonitorBtn);
    monitorControlLayout->addWidget(clearLogBtn);
    monitorControlLayout->addWidget(saveLogBtn);
    monitorControlLayout->addStretch();
    
    // 监控日志
    auto logTextEdit = new QTextEdit();
    logTextEdit->setMaximumHeight(150);
    logTextEdit->setReadOnly(true);
    logTextEdit->setFont(QFont("Consolas", 9));
    
    // 添加示例日志
    logTextEdit->append("[12:00:01] TX: AA 55 01 04 01 02 03 04 0F 0D");
    logTextEdit->append("[12:00:01] RX: AA 55 80 04 01 02 03 04 94 0D");
    logTextEdit->append("[12:00:02] TX: AA 55 02 00 02 0D");
    logTextEdit->append("[12:00:02] RX: AA 55 80 01 00 81 0D");
    
    monitorLayout->addLayout(monitorControlLayout);
    monitorLayout->addWidget(logTextEdit);
    
    // 布局组装
    layout->addWidget(deviceListGroup);
    layout->addWidget(deviceDetailsGroup);
    layout->addWidget(monitorGroup);
    
    return deviceTab;
}

QWidget* CommunicationWidget::createStatisticsTab()
{
    auto statsTab = new QWidget();
    auto layout = new QVBoxLayout(statsTab);
    
    // 总体统计组
    auto overallStatsGroup = new QGroupBox("总体统计");
    auto overallLayout = new QGridLayout(overallStatsGroup);
    
    // 统计数据
    auto createStatCard = [](const QString& title, const QString& value, const QString& color) {
        auto card = new QFrame();
        card->setFrameStyle(QFrame::Box);
        card->setStyleSheet(QString("QFrame { border: 2px solid %1; border-radius: 5px; padding: 10px; }").arg(color));
        
        auto cardLayout = new QVBoxLayout(card);
        auto titleLabel = new QLabel(title);
        titleLabel->setAlignment(Qt::AlignCenter);
        titleLabel->setStyleSheet("font-weight: bold; color: gray;");
        
        auto valueLabel = new QLabel(value);
        valueLabel->setAlignment(Qt::AlignCenter);
        valueLabel->setStyleSheet(QString("font-size: 24px; font-weight: bold; color: %1;").arg(color));
        
        cardLayout->addWidget(titleLabel);
        cardLayout->addWidget(valueLabel);
        
        return card;
    };
    
    overallLayout->addWidget(createStatCard("总连接数", "5", "#2196F3"), 0, 0);
    overallLayout->addWidget(createStatCard("活跃连接", "3", "#4CAF50"), 0, 1);
    overallLayout->addWidget(createStatCard("总消息数", "12,345", "#FF9800"), 0, 2);
    overallLayout->addWidget(createStatCard("错误率", "0.3%", "#F44336"), 0, 3);
    
    // 详细统计表
    auto detailStatsGroup = new QGroupBox("详细统计");
    auto detailLayout = new QVBoxLayout(detailStatsGroup);
    
    auto statsTable = new QTableWidget(0, 7);
    QStringList headers = {"连接名称", "消息发送", "消息接收", "错误次数", "成功率", "平均延迟", "状态"};
    statsTable->setHorizontalHeaderLabels(headers);
    statsTable->setAlternatingRowColors(true);
    statsTable->horizontalHeader()->setStretchLastSection(true);
    
    // 添加示例数据
    QStringList connections = {"串口连接1", "TCP连接1", "CAN连接1", "Modbus连接1"};
    QStringList sendCounts = {"1234", "5678", "9012", "3456"};
    QStringList receiveCounts = {"1230", "5675", "9010", "3454"};
    QStringList errorCounts = {"4", "3", "2", "2"};
    QStringList successRates = {"99.7%", "99.9%", "99.8%", "99.9%"};
    QStringList avgDelays = {"12ms", "8ms", "5ms", "15ms"};
    QStringList statuses = {"正常", "正常", "正常", "正常"};
    
    for (int i = 0; i < connections.size(); ++i) {
        statsTable->insertRow(i);
        statsTable->setItem(i, 0, new QTableWidgetItem(connections[i]));
        statsTable->setItem(i, 1, new QTableWidgetItem(sendCounts[i]));
        statsTable->setItem(i, 2, new QTableWidgetItem(receiveCounts[i]));
        statsTable->setItem(i, 3, new QTableWidgetItem(errorCounts[i]));
        statsTable->setItem(i, 4, new QTableWidgetItem(successRates[i]));
        statsTable->setItem(i, 5, new QTableWidgetItem(avgDelays[i]));
        statsTable->setItem(i, 6, new QTableWidgetItem(statuses[i]));
    }
    
    detailLayout->addWidget(statsTable);
    
    // 操作按钮
    auto buttonLayout = new QHBoxLayout();
    auto refreshStatsBtn = new QPushButton("刷新统计");
    auto resetStatsBtn = new QPushButton("重置统计");
    auto exportStatsBtn = new QPushButton("导出报告");
    
    refreshStatsBtn->setProperty("class", "primary");
    resetStatsBtn->setProperty("class", "warning");
    exportStatsBtn->setProperty("class", "success");
    
    buttonLayout->addWidget(refreshStatsBtn);
    buttonLayout->addWidget(resetStatsBtn);
    buttonLayout->addWidget(exportStatsBtn);
    buttonLayout->addStretch();
    
    // 布局组装
    layout->addWidget(overallStatsGroup);
    layout->addWidget(detailStatsGroup);
    layout->addLayout(buttonLayout);
    layout->addStretch();
    
    return statsTab;
}

void CommunicationWidget::setupConnections()
{
    // 串口相关连接
    connect(m_serialRefreshBtn, &QPushButton::clicked, this, &CommunicationWidget::refreshSerialPorts);
    connect(m_serialConnectBtn, &QPushButton::clicked, this, &CommunicationWidget::connectSerial);
    connect(m_serialDisconnectBtn, &QPushButton::clicked, this, &CommunicationWidget::disconnectSerial);
    
    // TCP相关连接
    connect(m_tcpConnectBtn, &QPushButton::clicked, this, &CommunicationWidget::connectTcp);
    connect(m_tcpDisconnectBtn, &QPushButton::clicked, this, &CommunicationWidget::disconnectTcp);
    
    // CAN相关连接
    connect(m_canConnectBtn, &QPushButton::clicked, this, &CommunicationWidget::connectCan);
    connect(m_canDisconnectBtn, &QPushButton::clicked, this, &CommunicationWidget::disconnectCan);
    
    // Modbus相关连接
    connect(m_modbusConnectBtn, &QPushButton::clicked, this, &CommunicationWidget::connectModbus);
    connect(m_modbusDisconnectBtn, &QPushButton::clicked, this, &CommunicationWidget::disconnectModbus);
    
    // 设备管理相关连接 - 暂时注释掉，因为这些按钮不存在
    // connect(m_addDeviceBtn, &QPushButton::clicked, this, &CommunicationWidget::addDevice);
    // connect(m_removeDeviceBtn, &QPushButton::clicked, this, &CommunicationWidget::removeDevice);
    // connect(m_refreshDeviceBtn, &QPushButton::clicked, this, &CommunicationWidget::refreshDevices);
    
    // 统计相关连接 - 暂时注释掉，因为这些按钮不存在
    // connect(m_resetStatsBtn, &QPushButton::clicked, this, &CommunicationWidget::resetStatistics);
    // connect(m_exportStatsBtn, &QPushButton::clicked, this, &CommunicationWidget::exportStatistics);
    
    // 初始化时刷新串口列表
    refreshSerialPorts();
}

void CommunicationWidget::refreshSerialPorts()
{
    m_serialPortCombo->clear();
    // 这里应该添加实际的串口枚举代码
    m_serialPortCombo->addItems({"COM1", "COM2", "COM3", "/dev/ttyUSB0", "/dev/ttyUSB1"});
}

void CommunicationWidget::connectSerial()
{
    // 实现串口连接逻辑
    m_serialConnectBtn->setEnabled(false);
    m_serialDisconnectBtn->setEnabled(true);
    m_serialStatusLabel->setText("状态: 已连接");
    
    emit serialConnected();
}

void CommunicationWidget::disconnectSerial()
{
    // 实现串口断开逻辑
    m_serialConnectBtn->setEnabled(true);
    m_serialDisconnectBtn->setEnabled(false);
    m_serialStatusLabel->setText("状态: 未连接");
    
    emit serialDisconnected();
}

void CommunicationWidget::connectTcp()
{
    // 实现TCP连接逻辑
    m_tcpConnectBtn->setEnabled(false);
    m_tcpDisconnectBtn->setEnabled(true);
    m_tcpStatusLabel->setText("状态: 已连接");
    
    emit tcpConnected();
}

void CommunicationWidget::disconnectTcp()
{
    // 实现TCP断开逻辑
    m_tcpConnectBtn->setEnabled(true);
    m_tcpDisconnectBtn->setEnabled(false);
    m_tcpStatusLabel->setText("状态: 未连接");
    
    emit tcpDisconnected();
}

void CommunicationWidget::connectCan()
{
    // 实现CAN连接逻辑
    m_canConnectBtn->setEnabled(false);
    m_canDisconnectBtn->setEnabled(true);
    m_canStatusLabel->setText("状态: 已连接");
    
    emit canConnected();
}

void CommunicationWidget::disconnectCan()
{
    // 实现CAN断开逻辑
    m_canConnectBtn->setEnabled(true);
    m_canDisconnectBtn->setEnabled(false);
    m_canStatusLabel->setText("状态: 未连接");
    
    emit canDisconnected();
}

void CommunicationWidget::connectModbus()
{
    // 实现Modbus连接逻辑
    m_modbusConnectBtn->setEnabled(false);
    m_modbusDisconnectBtn->setEnabled(true);
    m_modbusStatusLabel->setText("状态: 已连接");
    
    emit modbusConnected();
}

void CommunicationWidget::disconnectModbus()
{
    // 实现Modbus断开逻辑
    m_modbusConnectBtn->setEnabled(true);
    m_modbusDisconnectBtn->setEnabled(false);
    m_modbusStatusLabel->setText("状态: 未连接");
    
    emit modbusDisconnected();
}

void CommunicationWidget::addDevice()
{
    // 暂时留空，因为相关UI组件不存在
}

void CommunicationWidget::removeDevice()
{
    // 暂时留空，因为相关UI组件不存在
}

void CommunicationWidget::refreshDevices()
{
    // 暂时留空，因为相关UI组件不存在
}

void CommunicationWidget::resetStatistics()
{
    // 暂时留空，因为相关UI组件不存在
}

void CommunicationWidget::exportStatistics()
{
    // 暂时留空，因为相关UI组件不存在
}

void CommunicationWidget::loadConfiguration()
{
    // 加载配置文件
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(configPath);
    
    QFile configFile(configPath + "/communication_config.json");
    if (configFile.open(QIODevice::ReadOnly)) {
        QJsonDocument doc = QJsonDocument::fromJson(configFile.readAll());
        QJsonObject config = doc.object();
        
        // 加载串口配置 - 添加空指针检查
        if (config.contains("serial")) {
            QJsonObject serial = config["serial"].toObject();
            if (m_serialBaudRateCombo) m_serialBaudRateCombo->setCurrentText(serial["baudrate"].toString("115200"));
            if (m_serialDataBitsCombo) m_serialDataBitsCombo->setCurrentText(serial["dataBits"].toString("8"));
            if (m_serialParityCombo) m_serialParityCombo->setCurrentText(serial["parity"].toString("None"));
            if (m_serialStopBitsCombo) m_serialStopBitsCombo->setCurrentText(serial["stopBits"].toString("1"));
            if (m_serialFlowControlCombo) m_serialFlowControlCombo->setCurrentText(serial["flowControl"].toString("None"));
        }
        
        // 加载TCP配置 - 添加空指针检查
        if (config.contains("tcp")) {
            QJsonObject tcp = config["tcp"].toObject();
            if (m_tcpHostEdit) m_tcpHostEdit->setText(tcp["host"].toString("192.168.1.100"));
            if (m_tcpPortSpin) m_tcpPortSpin->setValue(tcp["port"].toInt(8080));
            if (m_tcpTimeoutSpin) m_tcpTimeoutSpin->setValue(tcp["timeout"].toInt(5000));
            if (m_tcpAutoReconnectCheck) m_tcpAutoReconnectCheck->setChecked(tcp["autoReconnect"].toBool(true));
        }
        
        // 加载CAN配置 - 添加空指针检查
        if (config.contains("can")) {
            QJsonObject can = config["can"].toObject();
            if (m_canPluginCombo) m_canPluginCombo->setCurrentText(can["plugin"].toString("socketcan"));
            if (m_canInterfaceEdit) m_canInterfaceEdit->setText(can["interface"].toString("can0"));
            if (m_canBitrateSpin) m_canBitrateSpin->setValue(can["bitrate"].toInt(250000));
            if (m_canSamplePointSpin) m_canSamplePointSpin->setValue(can["samplePoint"].toDouble(0.75));
        }
        
        // 暂时跳过Modbus配置加载，因为相关UI控件可能未正确初始化
        // TODO: 修复Modbus UI控件的成员变量初始化问题
        /*
        if (config.contains("modbus")) {
            QJsonObject modbus = config["modbus"].toObject();
            if (m_modbusModeCombo) m_modbusModeCombo->setCurrentText(modbus["mode"].toString("RTU"));
            if (m_modbusSlaveIdSpin) m_modbusSlaveIdSpin->setValue(modbus["slaveId"].toInt(1));
            if (m_modbusTimeoutSpin) m_modbusTimeoutSpin->setValue(modbus["timeout"].toInt(1000));
            if (m_modbusRetriesSpin) m_modbusRetriesSpin->setValue(modbus["retries"].toInt(3));
        }
        */
    }
}

void CommunicationWidget::saveConfiguration()
{
    QString configPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(configPath);
    
    QJsonObject config;
    
    // 保存串口配置 - 添加空指针检查
    if (m_serialBaudRateCombo && m_serialDataBitsCombo && m_serialParityCombo && 
        m_serialStopBitsCombo && m_serialFlowControlCombo) {
        QJsonObject serial;
        serial["baudrate"] = m_serialBaudRateCombo->currentText();
        serial["dataBits"] = m_serialDataBitsCombo->currentText();
        serial["parity"] = m_serialParityCombo->currentText();
        serial["stopBits"] = m_serialStopBitsCombo->currentText();
        serial["flowControl"] = m_serialFlowControlCombo->currentText();
        config["serial"] = serial;
    }
    
    // 保存TCP配置 - 添加空指针检查
    if (m_tcpHostEdit && m_tcpPortSpin && m_tcpTimeoutSpin && m_tcpAutoReconnectCheck) {
        QJsonObject tcp;
        tcp["host"] = m_tcpHostEdit->text();
        tcp["port"] = m_tcpPortSpin->value();
        tcp["timeout"] = m_tcpTimeoutSpin->value();
        tcp["autoReconnect"] = m_tcpAutoReconnectCheck->isChecked();
        config["tcp"] = tcp;
    }
    
    // 保存CAN配置 - 添加空指针检查
    if (m_canPluginCombo && m_canInterfaceEdit && m_canBitrateSpin && m_canSamplePointSpin) {
        QJsonObject can;
        can["plugin"] = m_canPluginCombo->currentText();
        can["interface"] = m_canInterfaceEdit->text();
        can["bitrate"] = m_canBitrateSpin->value();
        can["samplePoint"] = m_canSamplePointSpin->value();
        config["can"] = can;
    }
    
    // 暂时跳过Modbus配置保存，因为相关UI控件可能未正确初始化
    // TODO: 修复Modbus UI控件的成员变量初始化问题
    /*
    if (m_modbusModeCombo && m_modbusSlaveIdSpin && m_modbusTimeoutSpin && m_modbusRetriesSpin) {
        QJsonObject modbus;
        modbus["mode"] = m_modbusModeCombo->currentText();
        modbus["slaveId"] = m_modbusSlaveIdSpin->value();
        modbus["timeout"] = m_modbusTimeoutSpin->value();
        modbus["retries"] = m_modbusRetriesSpin->value();
        config["modbus"] = modbus;
    }
    */
    
    QJsonDocument doc(config);
    QFile configFile(configPath + "/communication_config.json");
    if (configFile.open(QIODevice::WriteOnly)) {
        configFile.write(doc.toJson());
    }
} 