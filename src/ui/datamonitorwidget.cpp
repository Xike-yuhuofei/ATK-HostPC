#include "datamonitorwidget.h"
#include "communication/serialworker.h"
#include "communication/protocolparser.h"
#include "logger/logmanager.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QApplication>
#include <QStyle>
#include <QTextStream>
#include <QDataStream>
#include <QSplitter>

DataMonitorWidget::DataMonitorWidget(QWidget* parent) 
    : QWidget(parent)
    , tabWidget(nullptr)
    , serialWorker(nullptr)
    , isMonitoring(false)
    , isPaused(false)
{
    setupUI();
    setupConnections();
    
    // 创建更新定时器
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &DataMonitorWidget::onUpdateTimer);
    
    // 初始化图表
    initializeCharts();
    
    // 设置默认配置
    config = MonitorConfig();
    updateTimer->setInterval(config.updateInterval);
    
    LogManager::getInstance()->info("数据监控界面已创建", "DataMonitor");
}

DataMonitorWidget::~DataMonitorWidget()
{
    if (isMonitoring) {
        stopMonitoring();
    }
    LogManager::getInstance()->info("数据监控界面已销毁", "DataMonitor");
}

void DataMonitorWidget::setupUI()
{
    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 创建标签页
    tabWidget = new QTabWidget;
    
    // 实时监控页面
    QWidget* realTimePage = new QWidget;
    QVBoxLayout* realTimeLayout = new QVBoxLayout(realTimePage);
    
    setupRealTimePanel();
    setupControlPanel();
    
    QSplitter* realTimeSplitter = new QSplitter(Qt::Vertical);
    realTimeSplitter->addWidget(realTimeGroup);
    realTimeSplitter->addWidget(controlGroup);
    realTimeSplitter->setSizes({400, 100});
    
    realTimeLayout->addWidget(realTimeSplitter);
    
    // 图表监控页面
    QWidget* chartPage = new QWidget;
    QVBoxLayout* chartLayout = new QVBoxLayout(chartPage);
    
    setupChartPanel();
    chartLayout->addWidget(chartGroup);
    
    // 数据表格页面
    QWidget* dataTablePage = new QWidget;
    QVBoxLayout* dataTableLayout = new QVBoxLayout(dataTablePage);
    
    setupDataTablePanel();
    dataTableLayout->addWidget(dataTableGroup);
    
    // 添加到标签页
    tabWidget->addTab(realTimePage, "实时监控");
    tabWidget->addTab(chartPage, "曲线图表");
    tabWidget->addTab(dataTablePage, "历史数据");
    
    mainLayout->addWidget(tabWidget);
    setLayout(mainLayout);
}

void DataMonitorWidget::setupRealTimePanel()
{
    realTimeGroup = new QGroupBox("实时数据");
    QGridLayout* layout = new QGridLayout(realTimeGroup);
    
    // 位置显示
    QGroupBox* positionGroup = new QGroupBox("当前位置");
    QGridLayout* positionLayout = new QGridLayout(positionGroup);
    
    positionLayout->addWidget(new QLabel("X轴:"), 0, 0);
    positionXLabel = new QLabel("0.000 mm");
    positionXLabel->setStyleSheet("QLabel { font-weight: bold; color: blue; font-size: 14px; }");
    positionLayout->addWidget(positionXLabel, 0, 1);
    
    positionLayout->addWidget(new QLabel("Y轴:"), 1, 0);
    positionYLabel = new QLabel("0.000 mm");
    positionYLabel->setStyleSheet("QLabel { font-weight: bold; color: blue; font-size: 14px; }");
    positionLayout->addWidget(positionYLabel, 1, 1);
    
    positionLayout->addWidget(new QLabel("Z轴:"), 2, 0);
    positionZLabel = new QLabel("0.000 mm");
    positionZLabel->setStyleSheet("QLabel { font-weight: bold; color: blue; font-size: 14px; }");
    positionLayout->addWidget(positionZLabel, 2, 1);
    
    // 运动参数显示
    QGroupBox* motionGroup = new QGroupBox("运动参数");
    QGridLayout* motionLayout = new QGridLayout(motionGroup);
    
    motionLayout->addWidget(new QLabel("速度:"), 0, 0);
    velocityLCD = new QLCDNumber(6);
    velocityLCD->setStyleSheet("QLCDNumber { background-color: black; color: green; }");
    velocityLCD->setSegmentStyle(QLCDNumber::Flat);
    motionLayout->addWidget(velocityLCD, 0, 1);
    motionLayout->addWidget(new QLabel("mm/s"), 0, 2);
    
    // 工艺参数显示
    QGroupBox* processGroup = new QGroupBox("工艺参数");
    QGridLayout* processLayout = new QGridLayout(processGroup);
    
    processLayout->addWidget(new QLabel("压力:"), 0, 0);
    pressureLCD = new QLCDNumber(5);
    pressureLCD->setStyleSheet("QLCDNumber { background-color: black; color: orange; }");
    pressureLCD->setSegmentStyle(QLCDNumber::Flat);
    processLayout->addWidget(pressureLCD, 0, 1);
    processLayout->addWidget(new QLabel("Bar"), 0, 2);
    
    processLayout->addWidget(new QLabel("温度:"), 1, 0);
    temperatureLCD = new QLCDNumber(5);
    temperatureLCD->setStyleSheet("QLCDNumber { background-color: black; color: red; }");
    temperatureLCD->setSegmentStyle(QLCDNumber::Flat);
    processLayout->addWidget(temperatureLCD, 1, 1);
    processLayout->addWidget(new QLabel("°C"), 1, 2);
    
    processLayout->addWidget(new QLabel("胶量:"), 2, 0);
    volumeLCD = new QLCDNumber(6);
    volumeLCD->setStyleSheet("QLCDNumber { background-color: black; color: cyan; }");
    volumeLCD->setSegmentStyle(QLCDNumber::Flat);
    processLayout->addWidget(volumeLCD, 2, 1);
    processLayout->addWidget(new QLabel("μL"), 2, 2);
    
    // 状态显示
    QGroupBox* statusGroup = new QGroupBox("设备状态");
    QVBoxLayout* statusLayout = new QVBoxLayout(statusGroup);
    
    statusLabel = new QLabel("停止");
    statusLabel->setStyleSheet("QLabel { font-weight: bold; color: red; font-size: 16px; "
                              "border: 2px solid gray; padding: 5px; border-radius: 5px; }");
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLayout->addWidget(statusLabel);
    
    progressBar = new QProgressBar;
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    progressBar->setFormat("进度: %p%");
    statusLayout->addWidget(progressBar);
    
    // 布局设置
    layout->addWidget(positionGroup, 0, 0);
    layout->addWidget(motionGroup, 0, 1);
    layout->addWidget(processGroup, 1, 0);
    layout->addWidget(statusGroup, 1, 1);
}

void DataMonitorWidget::setupChartPanel()
{
    chartGroup = new QGroupBox("实时曲线");
    QGridLayout* layout = new QGridLayout(chartGroup);
    
    // 创建图表视图
    positionChartView = new QChartView;
    velocityChartView = new QChartView;
    pressureChartView = new QChartView;
    temperatureChartView = new QChartView;
    
    // 设置图表视图属性
    positionChartView->setRenderHint(QPainter::Antialiasing);
    velocityChartView->setRenderHint(QPainter::Antialiasing);
    pressureChartView->setRenderHint(QPainter::Antialiasing);
    temperatureChartView->setRenderHint(QPainter::Antialiasing);
    
    // 布局设置
    layout->addWidget(positionChartView, 0, 0);
    layout->addWidget(velocityChartView, 0, 1);
    layout->addWidget(pressureChartView, 1, 0);
    layout->addWidget(temperatureChartView, 1, 1);
}

void DataMonitorWidget::setupDataTablePanel()
{
    dataTableGroup = new QGroupBox("历史数据");
    QVBoxLayout* layout = new QVBoxLayout(dataTableGroup);
    
    // 数据表格
    dataTableWidget = new QTableWidget;
    dataTableWidget->setColumnCount(9);
    dataTableWidget->setHorizontalHeaderLabels({
        "时间", "X位置", "Y位置", "Z位置", "速度", "压力", "温度", "胶量", "状态"
    });
    dataTableWidget->setAlternatingRowColors(true);
    dataTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    dataTableWidget->horizontalHeader()->setStretchLastSection(true);
    
    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    exportButton = new QPushButton("导出数据");
    exportButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    clearButton = new QPushButton("清空历史");
    clearButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    
    dataCountLabel = new QLabel("数据条数: 0");
    
    buttonLayout->addWidget(exportButton);
    buttonLayout->addWidget(clearButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(dataCountLabel);
    
    layout->addWidget(dataTableWidget);
    layout->addLayout(buttonLayout);
}

void DataMonitorWidget::setupControlPanel()
{
    controlGroup = new QGroupBox("监控控制");
    QHBoxLayout* layout = new QHBoxLayout(controlGroup);
    
    // 控制按钮
    startButton = new QPushButton("开始监控");
    startButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    startButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    
    stopButton = new QPushButton("停止监控");
    stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    stopButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    stopButton->setEnabled(false);
    
    pauseButton = new QPushButton("暂停监控");
    pauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    pauseButton->setStyleSheet("QPushButton { background-color: #FF9800; color: white; font-weight: bold; }");
    pauseButton->setEnabled(false);
    
    configButton = new QPushButton("监控设置");
    configButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    
    // 状态标签
    monitoringStatusLabel = new QLabel("监控状态: 已停止");
    monitoringStatusLabel->setStyleSheet("QLabel { font-weight: bold; }");
    
    layout->addWidget(startButton);
    layout->addWidget(stopButton);
    layout->addWidget(pauseButton);
    layout->addWidget(configButton);
    layout->addStretch();
    layout->addWidget(monitoringStatusLabel);
}

void DataMonitorWidget::setupConnections()
{
    // 控制按钮信号
    connect(startButton, &QPushButton::clicked, this, &DataMonitorWidget::startMonitoring);
    connect(stopButton, &QPushButton::clicked, this, &DataMonitorWidget::stopMonitoring);
    connect(pauseButton, &QPushButton::clicked, [this]() {
        if (isPaused) {
            resumeMonitoring();
        } else {
            pauseMonitoring();
        }
    });
    connect(configButton, &QPushButton::clicked, this, &DataMonitorWidget::onConfigSettings);
    
    // 数据表格按钮信号
    connect(exportButton, &QPushButton::clicked, this, &DataMonitorWidget::onExportData);
    connect(clearButton, &QPushButton::clicked, this, &DataMonitorWidget::onClearHistory);
}

void DataMonitorWidget::initializeCharts()
{
    // 位置图表
    positionChart = new QChart();
    positionChart->setTitle("位置监控");
    positionChart->setAnimationOptions(QChart::NoAnimation);
    
    positionXSeries = new QLineSeries();
    positionXSeries->setName("X轴");
    positionXSeries->setPen(QPen(Qt::red, 2));
    
    positionYSeries = new QLineSeries();
    positionYSeries->setName("Y轴");
    positionYSeries->setPen(QPen(Qt::green, 2));
    
    positionZSeries = new QLineSeries();
    positionZSeries->setName("Z轴");
    positionZSeries->setPen(QPen(Qt::blue, 2));
    
    positionChart->addSeries(positionXSeries);
    positionChart->addSeries(positionYSeries);
    positionChart->addSeries(positionZSeries);
    positionChart->createDefaultAxes();
    positionChartView->setChart(positionChart);
    
    // 速度图表
    velocityChart = new QChart();
    velocityChart->setTitle("速度监控");
    velocityChart->setAnimationOptions(QChart::NoAnimation);
    
    velocitySeries = new QLineSeries();
    velocitySeries->setName("速度");
    velocitySeries->setPen(QPen(Qt::darkGreen, 2));
    
    velocityChart->addSeries(velocitySeries);
    velocityChart->createDefaultAxes();
    velocityChartView->setChart(velocityChart);
    
    // 压力图表
    pressureChart = new QChart();
    pressureChart->setTitle("压力监控");
    pressureChart->setAnimationOptions(QChart::NoAnimation);
    
    pressureSeries = new QLineSeries();
    pressureSeries->setName("压力");
    pressureSeries->setPen(QPen(Qt::darkMagenta, 2));
    
    pressureChart->addSeries(pressureSeries);
    pressureChart->createDefaultAxes();
    pressureChartView->setChart(pressureChart);
    
    // 温度图表
    temperatureChart = new QChart();
    temperatureChart->setTitle("温度监控");
    temperatureChart->setAnimationOptions(QChart::NoAnimation);
    
    temperatureSeries = new QLineSeries();
    temperatureSeries->setName("温度");
    temperatureSeries->setPen(QPen(Qt::darkRed, 2));
    
    temperatureChart->addSeries(temperatureSeries);
    temperatureChart->createDefaultAxes();
    temperatureChartView->setChart(temperatureChart);
}

// 监控控制
void DataMonitorWidget::startMonitoring()
{
    if (isMonitoring) return;
    
    isMonitoring = true;
    isPaused = false;
    startTime = QDateTime::currentDateTime();
    
    updateTimer->start();
    
    // 更新按钮状态
    startButton->setEnabled(false);
    stopButton->setEnabled(true);
    pauseButton->setEnabled(true);
    pauseButton->setText("暂停监控");
    
    monitoringStatusLabel->setText("监控状态: 运行中");
    monitoringStatusLabel->setStyleSheet("QLabel { font-weight: bold; color: green; }");
    
    emit monitoringStateChanged(true);
    LogManager::getInstance()->info("开始数据监控", "DataMonitor");
}

void DataMonitorWidget::stopMonitoring()
{
    if (!isMonitoring) return;
    
    isMonitoring = false;
    isPaused = false;
    
    updateTimer->stop();
    
    // 更新按钮状态
    startButton->setEnabled(true);
    stopButton->setEnabled(false);
    pauseButton->setEnabled(false);
    pauseButton->setText("暂停监控");
    
    monitoringStatusLabel->setText("监控状态: 已停止");
    monitoringStatusLabel->setStyleSheet("QLabel { font-weight: bold; color: red; }");
    
    emit monitoringStateChanged(false);
    LogManager::getInstance()->info("停止数据监控", "DataMonitor");
}

void DataMonitorWidget::pauseMonitoring()
{
    if (!isMonitoring || isPaused) return;
    
    isPaused = true;
    updateTimer->stop();
    
    pauseButton->setText("恢复监控");
    monitoringStatusLabel->setText("监控状态: 已暂停");
    monitoringStatusLabel->setStyleSheet("QLabel { font-weight: bold; color: orange; }");
    
    LogManager::getInstance()->info("暂停数据监控", "DataMonitor");
}

void DataMonitorWidget::resumeMonitoring()
{
    if (!isMonitoring || !isPaused) return;
    
    isPaused = false;
    updateTimer->start();
    
    pauseButton->setText("暂停监控");
    monitoringStatusLabel->setText("监控状态: 运行中");
    monitoringStatusLabel->setStyleSheet("QLabel { font-weight: bold; color: green; }");
    
    LogManager::getInstance()->info("恢复数据监控", "DataMonitor");
}

void DataMonitorWidget::updateRealTimeData(const RealTimeData& data)
{
    QMutexLocker locker(&dataMutex);
    currentData = data;
    
    // 添加到历史数据
    addDataPoint(data);
    
    // 更新显示
    updateRealTimeDisplay();
    updateCharts();
    
    // 检查报警
    if (config.enableAlerts) {
        checkAlerts(data);
    }
    
    emit dataUpdated(data);
}

void DataMonitorWidget::addDataPoint(const RealTimeData& data)
{
    historyData.append(data);
    
    // 限制历史数据大小
    while (historyData.size() > config.historySize) {
        historyData.removeFirst();
    }
    
    // 更新图表数据
    addChartData(data);
    
    // 定期更新数据表格
    static int updateCounter = 0;
    if (++updateCounter >= TABLE_UPDATE_INTERVAL) {
        updateCounter = 0;
        updateDataTable();
    }
}

void DataMonitorWidget::updateRealTimeDisplay()
{
    // 更新位置显示
    positionXLabel->setText(formatValue(currentData.positionX, "mm", 3));
    positionYLabel->setText(formatValue(currentData.positionY, "mm", 3));
    positionZLabel->setText(formatValue(currentData.positionZ, "mm", 3));
    
    // 更新LCD显示
    velocityLCD->display(currentData.velocity);
    pressureLCD->display(currentData.pressure);
    temperatureLCD->display(currentData.temperature);
    volumeLCD->display(currentData.glueVolume);
    
    // 更新状态显示
    QString statusText;
    QColor statusColor = getStatusColor(currentData.deviceStatus);
    
    switch (currentData.deviceStatus) {
        case 0: statusText = "停止"; break;
        case 1: statusText = "运行中"; break;
        case 2: statusText = "暂停"; break;
        case 3: statusText = "回原点"; break;
        case 4: statusText = "错误"; break;
        case 5: statusText = "紧急停止"; break;
        default: statusText = "未知"; break;
    }
    
    statusLabel->setText(statusText);
    statusLabel->setStyleSheet(QString("QLabel { font-weight: bold; color: %1; font-size: 16px; "
                                      "border: 2px solid gray; padding: 5px; border-radius: 5px; }")
                              .arg(statusColor.name()));
}

void DataMonitorWidget::addChartData(const RealTimeData& data)
{
    double timeValue = data.timestamp.toMSecsSinceEpoch();
    
    // 添加数据点
    positionXSeries->append(timeValue, data.positionX);
    positionYSeries->append(timeValue, data.positionY);
    positionZSeries->append(timeValue, data.positionZ);
    velocitySeries->append(timeValue, data.velocity);
    pressureSeries->append(timeValue, data.pressure);
    temperatureSeries->append(timeValue, data.temperature);
    
    // 限制数据点数量
    if (positionXSeries->count() > MAX_CHART_POINTS) {
        positionXSeries->removePoints(0, positionXSeries->count() - MAX_CHART_POINTS);
        positionYSeries->removePoints(0, positionYSeries->count() - MAX_CHART_POINTS);
        positionZSeries->removePoints(0, positionZSeries->count() - MAX_CHART_POINTS);
        velocitySeries->removePoints(0, velocitySeries->count() - MAX_CHART_POINTS);
        pressureSeries->removePoints(0, pressureSeries->count() - MAX_CHART_POINTS);
        temperatureSeries->removePoints(0, temperatureSeries->count() - MAX_CHART_POINTS);
    }
    
    // 更新图表范围
    updateChartRange();
}

void DataMonitorWidget::updateChartRange()
{
    // 获取最近的时间范围
    if (positionXSeries->count() > 1) {
        double minTime = positionXSeries->at(0).x();
        double maxTime = positionXSeries->at(positionXSeries->count() - 1).x();
        
        // 更新X轴范围（时间轴）
        positionChart->axes(Qt::Horizontal).first()->setRange(QDateTime::fromMSecsSinceEpoch(minTime), 
                                                             QDateTime::fromMSecsSinceEpoch(maxTime));
        velocityChart->axes(Qt::Horizontal).first()->setRange(QDateTime::fromMSecsSinceEpoch(minTime), 
                                                             QDateTime::fromMSecsSinceEpoch(maxTime));
        pressureChart->axes(Qt::Horizontal).first()->setRange(QDateTime::fromMSecsSinceEpoch(minTime), 
                                                             QDateTime::fromMSecsSinceEpoch(maxTime));
        temperatureChart->axes(Qt::Horizontal).first()->setRange(QDateTime::fromMSecsSinceEpoch(minTime), 
                                                                QDateTime::fromMSecsSinceEpoch(maxTime));
    }
}

void DataMonitorWidget::updateDataTable()
{
    dataTableWidget->setRowCount(historyData.size());
    
    for (int i = 0; i < historyData.size(); ++i) {
        const RealTimeData& data = historyData[i];
        
        dataTableWidget->setItem(i, 0, new QTableWidgetItem(formatTime(data.timestamp)));
        dataTableWidget->setItem(i, 1, new QTableWidgetItem(formatValue(data.positionX, "", 3)));
        dataTableWidget->setItem(i, 2, new QTableWidgetItem(formatValue(data.positionY, "", 3)));
        dataTableWidget->setItem(i, 3, new QTableWidgetItem(formatValue(data.positionZ, "", 3)));
        dataTableWidget->setItem(i, 4, new QTableWidgetItem(formatValue(data.velocity, "", 2)));
        dataTableWidget->setItem(i, 5, new QTableWidgetItem(formatValue(data.pressure, "", 2)));
        dataTableWidget->setItem(i, 6, new QTableWidgetItem(formatValue(data.temperature, "", 1)));
        dataTableWidget->setItem(i, 7, new QTableWidgetItem(formatValue(data.glueVolume, "", 3)));
        
        QString statusText;
        switch (data.deviceStatus) {
            case 0: statusText = "停止"; break;
            case 1: statusText = "运行中"; break;
            case 2: statusText = "暂停"; break;
            case 3: statusText = "回原点"; break;
            case 4: statusText = "错误"; break;
            case 5: statusText = "紧急停止"; break;
            default: statusText = "未知"; break;
        }
        dataTableWidget->setItem(i, 8, new QTableWidgetItem(statusText));
    }
    
    // 滚动到底部
    dataTableWidget->scrollToBottom();
    
    // 更新数据计数
    dataCountLabel->setText(QString("数据条数: %1").arg(historyData.size()));
}

void DataMonitorWidget::checkAlerts(const RealTimeData& data)
{
    QStringList alerts;
    
    if (data.temperature > config.alertThresholds.maxTemperature) {
        alerts << QString("温度过高: %1°C").arg(data.temperature, 0, 'f', 1);
    }
    if (data.temperature < config.alertThresholds.minTemperature) {
        alerts << QString("温度过低: %1°C").arg(data.temperature, 0, 'f', 1);
    }
    if (data.pressure > config.alertThresholds.maxPressure) {
        alerts << QString("压力过高: %1Bar").arg(data.pressure, 0, 'f', 2);
    }
    if (data.pressure < config.alertThresholds.minPressure) {
        alerts << QString("压力过低: %1Bar").arg(data.pressure, 0, 'f', 2);
    }
    if (data.velocity > config.alertThresholds.maxVelocity) {
        alerts << QString("速度过快: %1mm/s").arg(data.velocity, 0, 'f', 2);
    }
    
    for (const QString& alert : alerts) {
        emit alertTriggered(alert);
        LogManager::getInstance()->warning("监控报警: " + alert, "DataMonitor");
    }
}

// 槽函数实现
void DataMonitorWidget::onUpdateTimer()
{
    if (!isMonitoring || isPaused) return;
    
    // 模拟数据更新（实际应该从串口接收）
    onDataReceived();
}

void DataMonitorWidget::onDataReceived()
{
    // 这里应该处理从串口接收到的数据
    // 现在使用模拟数据进行演示
    static double time = 0;
    time += 0.1;
    
    RealTimeData data;
    data.timestamp = QDateTime::currentDateTime();
    data.positionX = 10 * sin(time * 0.1);
    data.positionY = 10 * cos(time * 0.1);
    data.positionZ = 2 * sin(time * 0.2);
    data.velocity = 50 + 20 * sin(time * 0.3);
    data.pressure = 2.0 + 0.5 * sin(time * 0.15);
    data.temperature = 25.0 + 5 * sin(time * 0.05);
    data.glueVolume = 1.0 + 0.2 * sin(time * 0.25);
    data.deviceStatus = 1; // 运行中
    
    updateRealTimeData(data);
}

void DataMonitorWidget::onFrameReceived(const ProtocolFrame& frame)
{
    // 解析协议帧并更新数据
    if (frame.command == ProtocolCommand::ReadSensorData) {
        if (frame.data.size() >= 32) { // 假设数据包大小
            QDataStream stream(frame.data);
            stream.setByteOrder(QDataStream::LittleEndian);
            
            RealTimeData data;
            data.timestamp = QDateTime::currentDateTime();
            
            float x, y, z, vel, press, temp, vol;
            quint8 status;
            
            stream >> x >> y >> z >> vel >> press >> temp >> vol >> status;
            
            data.positionX = x;
            data.positionY = y;
            data.positionZ = z;
            data.velocity = vel;
            data.pressure = press;
            data.temperature = temp;
            data.glueVolume = vol;
            data.deviceStatus = status;
            
            updateRealTimeData(data);
        }
    }
}

void DataMonitorWidget::onExportData()
{
    QString fileName = QFileDialog::getSaveFileName(this, 
        "导出监控数据", "monitor_data.csv", "CSV文件 (*.csv)");
    
    if (!fileName.isEmpty()) {
        exportData(fileName);
    }
}

void DataMonitorWidget::onClearHistory()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认清空");
    msgBox.setText("确定要清空所有历史数据吗？此操作不可恢复。");
    msgBox.setIcon(QMessageBox::Question);
    
    QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
    QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.setDefaultButton(noButton);
    
    msgBox.exec();
    
    if (msgBox.clickedButton() == yesButton) {
        clearHistory();
    }
}

void DataMonitorWidget::onConfigSettings()
{
    // 配置对话框待实现
    QMessageBox::information(this, "提示", "监控配置功能待实现");
}

// 工具函数
QString DataMonitorWidget::formatValue(double value, const QString& unit, int precision) const
{
    return QString("%1 %2").arg(value, 0, 'f', precision).arg(unit);
}

QString DataMonitorWidget::formatTime(const QDateTime& time) const
{
    return time.toString("yyyy-MM-dd hh:mm:ss.zzz");
}

QColor DataMonitorWidget::getStatusColor(int status) const
{
    switch (status) {
        case 0: return Qt::red;        // 停止
        case 1: return Qt::green;      // 运行中
        case 2: return Qt::yellow;     // 暂停
        case 3: return Qt::blue;       // 回原点
        case 4: return Qt::red;        // 错误
        case 5: return Qt::darkRed;    // 紧急停止
        default: return Qt::gray;      // 未知
    }
}

void DataMonitorWidget::setSerialWorker(SerialWorker* worker)
{
    serialWorker = worker;
    if (serialWorker) {
        connect(serialWorker, &SerialWorker::frameReceived, 
                this, &DataMonitorWidget::onFrameReceived);
    }
}

void DataMonitorWidget::clearHistory()
{
    QMutexLocker locker(&dataMutex);
    historyData.clear();
    
    // 清空图表
    positionXSeries->clear();
    positionYSeries->clear();
    positionZSeries->clear();
    velocitySeries->clear();
    pressureSeries->clear();
    temperatureSeries->clear();
    
    // 清空表格
    dataTableWidget->setRowCount(0);
    dataCountLabel->setText("数据条数: 0");
    
    LogManager::getInstance()->info("清空历史数据", "DataMonitor");
}

void DataMonitorWidget::exportData(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "错误", "无法创建文件: " + filePath);
        return;
    }
    
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);
    
    // 写入表头
    stream << "时间,X位置,Y位置,Z位置,速度,压力,温度,胶量,状态\n";
    
    // 写入数据
    for (const RealTimeData& data : historyData) {
        stream << formatTime(data.timestamp) << ","
               << data.positionX << ","
               << data.positionY << ","
               << data.positionZ << ","
               << data.velocity << ","
               << data.pressure << ","
               << data.temperature << ","
               << data.glueVolume << ","
               << data.deviceStatus << "\n";
    }
    
    QMessageBox::information(this, "导出完成", 
                            QString("成功导出 %1 条数据到文件：\n%2")
                            .arg(historyData.size()).arg(filePath));
    
    LogManager::getInstance()->info("导出监控数据: " + filePath, "DataMonitor");
}

MonitorConfig DataMonitorWidget::getMonitorConfig() const
{
    return config;
}

void DataMonitorWidget::setMonitorConfig(const MonitorConfig& newConfig)
{
    config = newConfig;
    updateTimer->setInterval(config.updateInterval);
    
    // 限制历史数据大小
    while (historyData.size() > config.historySize) {
        historyData.removeFirst();
    }
    
    emit onConfigChanged();
}

QList<RealTimeData> DataMonitorWidget::getHistoryData() const
{
    return historyData;
} 