#include "devicecontrolwidget.h"
#include "communication/serialworker.h"
#include "logger/logmanager.h"
#include <QMessageBox>
#include <QDateTime>
#include <QApplication>
#include <QStyle>

DeviceControlWidget::DeviceControlWidget(QWidget* parent) 
    : QWidget(parent)
    , tabWidget(nullptr)
    , deviceState(DeviceState::Stopped)
    , serialWorker(nullptr)
    , currentX(0.0), currentY(0.0), currentZ(0.0)
    , currentVolume(0.0), currentPressure(0.0), currentTemperature(25.0)
    , isConnected(false)
{
    setupUI();
    setupConnections();
    
    // 创建更新定时器
    updateTimer = new QTimer(this);
    connect(updateTimer, &QTimer::timeout, this, &DeviceControlWidget::onUpdateTimer);
    updateTimer->start(100); // 100ms更新一次
    
    // 初始化界面状态
    updateControlButtons();
    updateStatusDisplay();
    
    LogManager::getInstance()->info("设备控制界面已创建", "DeviceControl");
}

DeviceControlWidget::~DeviceControlWidget()
{
    LogManager::getInstance()->info("设备控制界面已销毁", "DeviceControl");
}

void DeviceControlWidget::setupUI()
{
    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 创建标签页
    tabWidget = new QTabWidget;
    
    // 设备控制页面
    QWidget* deviceControlPage = new QWidget;
    QHBoxLayout* deviceControlLayout = new QHBoxLayout(deviceControlPage);
    
    setupDeviceControlPanel();
    setupMotionControlPanel();
    setupStatusPanel();
    
    deviceControlLayout->addWidget(deviceControlGroup);
    deviceControlLayout->addWidget(motionControlGroup);
    deviceControlLayout->addWidget(statusGroup);
    deviceControlLayout->setStretch(0, 1);
    deviceControlLayout->setStretch(1, 1);
    deviceControlLayout->setStretch(2, 1);
    
    // 参数设置页面
    QWidget* parameterPage = new QWidget;
    QHBoxLayout* parameterLayout = new QHBoxLayout(parameterPage);
    
    setupParameterPanel();
    
    parameterLayout->addWidget(glueParameterGroup);
    parameterLayout->addWidget(motionParameterGroup);
    parameterLayout->addStretch();
    
    // 添加到标签页
    tabWidget->addTab(deviceControlPage, "设备控制");
    tabWidget->addTab(parameterPage, "参数设置");
    
    mainLayout->addWidget(tabWidget);
    setLayout(mainLayout);
}

void DeviceControlWidget::setupDeviceControlPanel()
{
    deviceControlGroup = new QGroupBox("设备控制");
    QVBoxLayout* layout = new QVBoxLayout(deviceControlGroup);
    
    // 设备状态显示
    QHBoxLayout* statusLayout = new QHBoxLayout;
    statusLayout->addWidget(new QLabel("设备状态:"));
    deviceStateLabel = new QLabel("停止");
    deviceStateLabel->setStyleSheet("QLabel { font-weight: bold; color: red; }");
    statusLayout->addWidget(deviceStateLabel);
    statusLayout->addStretch();
    
    // 进度条
    progressBar = new QProgressBar;
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    
    // 控制按钮
    QGridLayout* buttonLayout = new QGridLayout;
    
    startButton = new QPushButton("启动");
    startButton->setIcon(style()->standardIcon(QStyle::SP_MediaPlay));
    startButton->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; }");
    
    stopButton = new QPushButton("停止");
    stopButton->setIcon(style()->standardIcon(QStyle::SP_MediaStop));
    stopButton->setStyleSheet("QPushButton { background-color: #f44336; color: white; font-weight: bold; }");
    
    pauseButton = new QPushButton("暂停");
    pauseButton->setIcon(style()->standardIcon(QStyle::SP_MediaPause));
    pauseButton->setStyleSheet("QPushButton { background-color: #FF9800; color: white; font-weight: bold; }");
    
    homeButton = new QPushButton("回原点");
    homeButton->setIcon(style()->standardIcon(QStyle::SP_ArrowUp));
    
    emergencyButton = new QPushButton("紧急停止");
    emergencyButton->setStyleSheet("QPushButton { background-color: #FF0000; color: white; font-weight: bold; font-size: 14px; }");
    
    buttonLayout->addWidget(startButton, 0, 0);
    buttonLayout->addWidget(stopButton, 0, 1);
    buttonLayout->addWidget(pauseButton, 1, 0);
    buttonLayout->addWidget(homeButton, 1, 1);
    buttonLayout->addWidget(emergencyButton, 2, 0, 1, 2);
    
    layout->addLayout(statusLayout);
    layout->addWidget(progressBar);
    layout->addLayout(buttonLayout);
    layout->addStretch();
}

void DeviceControlWidget::setupMotionControlPanel()
{
    motionControlGroup = new QGroupBox("运动控制");
    QVBoxLayout* layout = new QVBoxLayout(motionControlGroup);
    
    // 位置输入
    QGroupBox* positionGroup = new QGroupBox("目标位置");
    QGridLayout* positionLayout = new QGridLayout(positionGroup);
    
    positionLayout->addWidget(new QLabel("X轴 (mm):"), 0, 0);
    xPositionSpinBox = new QDoubleSpinBox;
    xPositionSpinBox->setRange(-1000, 1000);
    xPositionSpinBox->setDecimals(3);
    xPositionSpinBox->setSuffix(" mm");
    positionLayout->addWidget(xPositionSpinBox, 0, 1);
    
    positionLayout->addWidget(new QLabel("Y轴 (mm):"), 1, 0);
    yPositionSpinBox = new QDoubleSpinBox;
    yPositionSpinBox->setRange(-1000, 1000);
    yPositionSpinBox->setDecimals(3);
    yPositionSpinBox->setSuffix(" mm");
    positionLayout->addWidget(yPositionSpinBox, 1, 1);
    
    positionLayout->addWidget(new QLabel("Z轴 (mm):"), 2, 0);
    zPositionSpinBox = new QDoubleSpinBox;
    zPositionSpinBox->setRange(-100, 100);
    zPositionSpinBox->setDecimals(3);
    zPositionSpinBox->setSuffix(" mm");
    positionLayout->addWidget(zPositionSpinBox, 2, 1);
    
    QHBoxLayout* positionButtonLayout = new QHBoxLayout;
    moveButton = new QPushButton("移动到位置");
    moveButton->setIcon(style()->standardIcon(QStyle::SP_ArrowRight));
    setOriginButton = new QPushButton("设为原点");
    setOriginButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    
    positionButtonLayout->addWidget(moveButton);
    positionButtonLayout->addWidget(setOriginButton);
    
    // 点动控制
    jogControlGroup = new QGroupBox("点动控制");
    QGridLayout* jogLayout = new QGridLayout(jogControlGroup);
    
    jogLayout->addWidget(new QLabel("步长:"), 0, 0);
    jogStepSpinBox = new QDoubleSpinBox;
    jogStepSpinBox->setRange(0.001, 100);
    jogStepSpinBox->setValue(1.0);
    jogStepSpinBox->setDecimals(3);
    jogStepSpinBox->setSuffix(" mm");
    jogLayout->addWidget(jogStepSpinBox, 0, 1, 1, 2);
    
    // XY轴点动按钮布局
    jogYPlusButton = new QPushButton("Y+");
    jogLayout->addWidget(jogYPlusButton, 1, 1);
    
    jogXMinusButton = new QPushButton("X-");
    jogXPlusButton = new QPushButton("X+");
    jogLayout->addWidget(jogXMinusButton, 2, 0);
    jogLayout->addWidget(jogXPlusButton, 2, 2);
    
    jogYMinusButton = new QPushButton("Y-");
    jogLayout->addWidget(jogYMinusButton, 3, 1);
    
    // Z轴点动按钮
    jogZPlusButton = new QPushButton("Z+");
    jogZMinusButton = new QPushButton("Z-");
    jogLayout->addWidget(jogZPlusButton, 1, 3);
    jogLayout->addWidget(jogZMinusButton, 3, 3);
    
    layout->addWidget(positionGroup);
    layout->addLayout(positionButtonLayout);
    layout->addWidget(jogControlGroup);
    layout->addStretch();
}

void DeviceControlWidget::setupParameterPanel()
{
    // 点胶参数组
    glueParameterGroup = new QGroupBox("点胶参数");
    QGridLayout* glueLayout = new QGridLayout(glueParameterGroup);
    
    int row = 0;
    glueLayout->addWidget(new QLabel("胶量 (μL):"), row, 0);
    glueVolumeSpinBox = new QDoubleSpinBox;
    glueVolumeSpinBox->setRange(0.001, 1000);
    glueVolumeSpinBox->setValue(glueParams.volume);
    glueVolumeSpinBox->setDecimals(3);
    glueVolumeSpinBox->setSuffix(" μL");
    glueLayout->addWidget(glueVolumeSpinBox, row++, 1);
    
    glueLayout->addWidget(new QLabel("速度 (mm/s):"), row, 0);
    glueSpeedSpinBox = new QDoubleSpinBox;
    glueSpeedSpinBox->setRange(0.1, 1000);
    glueSpeedSpinBox->setValue(glueParams.speed);
    glueSpeedSpinBox->setDecimals(2);
    glueSpeedSpinBox->setSuffix(" mm/s");
    glueLayout->addWidget(glueSpeedSpinBox, row++, 1);
    
    glueLayout->addWidget(new QLabel("压力 (Bar):"), row, 0);
    gluePressureSpinBox = new QDoubleSpinBox;
    gluePressureSpinBox->setRange(0.1, 10.0);
    gluePressureSpinBox->setValue(glueParams.pressure);
    gluePressureSpinBox->setDecimals(2);
    gluePressureSpinBox->setSuffix(" Bar");
    glueLayout->addWidget(gluePressureSpinBox, row++, 1);
    
    glueLayout->addWidget(new QLabel("温度 (°C):"), row, 0);
    glueTemperatureSpinBox = new QDoubleSpinBox;
    glueTemperatureSpinBox->setRange(10.0, 80.0);
    glueTemperatureSpinBox->setValue(glueParams.temperature);
    glueTemperatureSpinBox->setDecimals(1);
    glueTemperatureSpinBox->setSuffix(" °C");
    glueLayout->addWidget(glueTemperatureSpinBox, row++, 1);
    
    glueLayout->addWidget(new QLabel("停留时间 (ms):"), row, 0);
    dwellTimeSpinBox = new QSpinBox;
    dwellTimeSpinBox->setRange(1, 10000);
    dwellTimeSpinBox->setValue(glueParams.dwellTime);
    dwellTimeSpinBox->setSuffix(" ms");
    glueLayout->addWidget(dwellTimeSpinBox, row++, 1);
    
    glueLayout->addWidget(new QLabel("上升时间 (ms):"), row, 0);
    riseTimeSpinBox = new QSpinBox;
    riseTimeSpinBox->setRange(1, 1000);
    riseTimeSpinBox->setValue(glueParams.riseTime);
    riseTimeSpinBox->setSuffix(" ms");
    glueLayout->addWidget(riseTimeSpinBox, row++, 1);
    
    glueLayout->addWidget(new QLabel("下降时间 (ms):"), row, 0);
    fallTimeSpinBox = new QSpinBox;
    fallTimeSpinBox->setRange(1, 1000);
    fallTimeSpinBox->setValue(glueParams.fallTime);
    fallTimeSpinBox->setSuffix(" ms");
    glueLayout->addWidget(fallTimeSpinBox, row++, 1);
    
    // 运动参数组
    motionParameterGroup = new QGroupBox("运动参数");
    QGridLayout* motionLayout = new QGridLayout(motionParameterGroup);
    
    row = 0;
    
    // 通用运动参数
    motionLayout->addWidget(new QLabel("运动速度 (mm/s):"), row, 0);
    motionSpeedSpinBox = new QDoubleSpinBox;
    motionSpeedSpinBox->setRange(1, 1000);
    motionSpeedSpinBox->setValue(100.0);
    motionSpeedSpinBox->setDecimals(1);
    motionSpeedSpinBox->setSuffix(" mm/s");
    motionLayout->addWidget(motionSpeedSpinBox, row++, 1);
    
    motionLayout->addWidget(new QLabel("运动加速度 (mm/s²):"), row, 0);
    motionAccelerationSpinBox = new QDoubleSpinBox;
    motionAccelerationSpinBox->setRange(100, 10000);
    motionAccelerationSpinBox->setValue(1000.0);
    motionAccelerationSpinBox->setDecimals(0);
    motionAccelerationSpinBox->setSuffix(" mm/s²");
    motionLayout->addWidget(motionAccelerationSpinBox, row++, 1);
    
    // 各轴单独参数
    motionLayout->addWidget(new QLabel("X轴速度 (mm/s):"), row, 0);
    velocityXSpinBox = new QDoubleSpinBox;
    velocityXSpinBox->setRange(1, 1000);
    velocityXSpinBox->setValue(motionParams.velocityX);
    velocityXSpinBox->setDecimals(1);
    velocityXSpinBox->setSuffix(" mm/s");
    motionLayout->addWidget(velocityXSpinBox, row++, 1);
    
    motionLayout->addWidget(new QLabel("Y轴速度 (mm/s):"), row, 0);
    velocityYSpinBox = new QDoubleSpinBox;
    velocityYSpinBox->setRange(1, 1000);
    velocityYSpinBox->setValue(motionParams.velocityY);
    velocityYSpinBox->setDecimals(1);
    velocityYSpinBox->setSuffix(" mm/s");
    motionLayout->addWidget(velocityYSpinBox, row++, 1);
    
    motionLayout->addWidget(new QLabel("Z轴速度 (mm/s):"), row, 0);
    velocityZSpinBox = new QDoubleSpinBox;
    velocityZSpinBox->setRange(1, 500);
    velocityZSpinBox->setValue(motionParams.velocityZ);
    velocityZSpinBox->setDecimals(1);
    velocityZSpinBox->setSuffix(" mm/s");
    motionLayout->addWidget(velocityZSpinBox, row++, 1);
    
    motionLayout->addWidget(new QLabel("X轴加速度 (mm/s²):"), row, 0);
    accelerationXSpinBox = new QDoubleSpinBox;
    accelerationXSpinBox->setRange(100, 10000);
    accelerationXSpinBox->setValue(motionParams.accelerationX);
    accelerationXSpinBox->setDecimals(0);
    accelerationXSpinBox->setSuffix(" mm/s²");
    motionLayout->addWidget(accelerationXSpinBox, row++, 1);
    
    motionLayout->addWidget(new QLabel("Y轴加速度 (mm/s²):"), row, 0);
    accelerationYSpinBox = new QDoubleSpinBox;
    accelerationYSpinBox->setRange(100, 10000);
    accelerationYSpinBox->setValue(motionParams.accelerationY);
    accelerationYSpinBox->setDecimals(0);
    accelerationYSpinBox->setSuffix(" mm/s²");
    motionLayout->addWidget(accelerationYSpinBox, row++, 1);
    
    motionLayout->addWidget(new QLabel("Z轴加速度 (mm/s²):"), row, 0);
    accelerationZSpinBox = new QDoubleSpinBox;
    accelerationZSpinBox->setRange(100, 5000);
    accelerationZSpinBox->setValue(motionParams.accelerationZ);
    accelerationZSpinBox->setDecimals(0);
    accelerationZSpinBox->setSuffix(" mm/s²");
    motionLayout->addWidget(accelerationZSpinBox, row++, 1);
}

void DeviceControlWidget::setupStatusPanel()
{
    statusGroup = new QGroupBox("状态监控");
    QVBoxLayout* layout = new QVBoxLayout(statusGroup);
    
    // 当前位置显示
    QGroupBox* positionGroup = new QGroupBox("当前位置");
    QGridLayout* positionLayout = new QGridLayout(positionGroup);
    
    positionLayout->addWidget(new QLabel("X轴:"), 0, 0);
    currentXLabel = new QLabel("0.000 mm");
    currentXLabel->setStyleSheet("QLabel { font-weight: bold; color: blue; }");
    positionLayout->addWidget(currentXLabel, 0, 1);
    
    positionLayout->addWidget(new QLabel("Y轴:"), 1, 0);
    currentYLabel = new QLabel("0.000 mm");
    currentYLabel->setStyleSheet("QLabel { font-weight: bold; color: blue; }");
    positionLayout->addWidget(currentYLabel, 1, 1);
    
    positionLayout->addWidget(new QLabel("Z轴:"), 2, 0);
    currentZLabel = new QLabel("0.000 mm");
    currentZLabel->setStyleSheet("QLabel { font-weight: bold; color: blue; }");
    positionLayout->addWidget(currentZLabel, 2, 1);
    
    // 点胶状态显示
    QGroupBox* glueStatusGroup = new QGroupBox("点胶状态");
    QGridLayout* glueStatusLayout = new QGridLayout(glueStatusGroup);
    
    glueStatusLayout->addWidget(new QLabel("胶量:"), 0, 0);
    currentVolumeLabel = new QLabel("0.000 μL");
    currentVolumeLabel->setStyleSheet("QLabel { font-weight: bold; color: green; }");
    glueStatusLayout->addWidget(currentVolumeLabel, 0, 1);
    
    glueStatusLayout->addWidget(new QLabel("压力:"), 1, 0);
    currentPressureLabel = new QLabel("0.00 Bar");
    currentPressureLabel->setStyleSheet("QLabel { font-weight: bold; color: green; }");
    glueStatusLayout->addWidget(currentPressureLabel, 1, 1);
    
    glueStatusLayout->addWidget(new QLabel("温度:"), 2, 0);
    currentTemperatureLabel = new QLabel("25.0 °C");
    currentTemperatureLabel->setStyleSheet("QLabel { font-weight: bold; color: green; }");
    glueStatusLayout->addWidget(currentTemperatureLabel, 2, 1);
    
    // 报警状态
    QGroupBox* alarmGroup = new QGroupBox("报警状态");
    QVBoxLayout* alarmLayout = new QVBoxLayout(alarmGroup);
    alarmStatusLabel = new QLabel("正常");
    alarmStatusLabel->setStyleSheet("QLabel { font-weight: bold; color: green; }");
    alarmLayout->addWidget(alarmStatusLabel);
    
    // 日志显示
    QGroupBox* logGroup = new QGroupBox("操作日志");
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    logTextEdit = new QTextEdit;
    logTextEdit->setMaximumHeight(100);
    logTextEdit->setReadOnly(true);
    logLayout->addWidget(logTextEdit);
    
    layout->addWidget(positionGroup);
    layout->addWidget(glueStatusGroup);
    layout->addWidget(alarmGroup);
    layout->addWidget(logGroup);
    layout->addStretch();
}

void DeviceControlWidget::setupConnections()
{
    // 设备控制按钮
    connect(startButton, &QPushButton::clicked, this, &DeviceControlWidget::onStartButtonClicked);
    connect(stopButton, &QPushButton::clicked, this, &DeviceControlWidget::onStopButtonClicked);
    connect(pauseButton, &QPushButton::clicked, this, &DeviceControlWidget::onPauseButtonClicked);
    connect(homeButton, &QPushButton::clicked, this, &DeviceControlWidget::onHomeButtonClicked);
    connect(emergencyButton, &QPushButton::clicked, this, &DeviceControlWidget::onEmergencyStopButtonClicked);
    
    // 运动控制按钮
    connect(moveButton, &QPushButton::clicked, this, &DeviceControlWidget::onMoveButtonClicked);
    connect(setOriginButton, &QPushButton::clicked, this, &DeviceControlWidget::onSetOriginButtonClicked);
    
    // 点动控制按钮
    connect(jogXPlusButton, &QPushButton::clicked, [this]() { onJogButtonClicked(); });
    connect(jogXMinusButton, &QPushButton::clicked, [this]() { onJogButtonClicked(); });
    connect(jogYPlusButton, &QPushButton::clicked, [this]() { onJogButtonClicked(); });
    connect(jogYMinusButton, &QPushButton::clicked, [this]() { onJogButtonClicked(); });
    connect(jogZPlusButton, &QPushButton::clicked, [this]() { onJogButtonClicked(); });
    connect(jogZMinusButton, &QPushButton::clicked, [this]() { onJogButtonClicked(); });
    
    // 参数变化信号
        connect(glueVolumeSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &DeviceControlWidget::onGlueParameterChanged);
    connect(glueSpeedSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &DeviceControlWidget::onGlueParameterChanged);
    connect(gluePressureSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &DeviceControlWidget::onGlueParameterChanged);
    connect(glueTemperatureSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &DeviceControlWidget::onGlueParameterChanged);
    connect(dwellTimeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &DeviceControlWidget::onGlueParameterChanged);
    connect(riseTimeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &DeviceControlWidget::onGlueParameterChanged);
    connect(fallTimeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &DeviceControlWidget::onGlueParameterChanged);
    
    connect(motionSpeedSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &DeviceControlWidget::onMotionParameterChanged);
    connect(motionAccelerationSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &DeviceControlWidget::onMotionParameterChanged);
    connect(velocityXSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &DeviceControlWidget::onMotionParameterChanged);
    connect(velocityYSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &DeviceControlWidget::onMotionParameterChanged);
    connect(velocityZSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &DeviceControlWidget::onMotionParameterChanged);
    connect(accelerationXSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &DeviceControlWidget::onMotionParameterChanged);
    connect(accelerationYSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &DeviceControlWidget::onMotionParameterChanged);
    connect(accelerationZSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), 
            this, &DeviceControlWidget::onMotionParameterChanged);
}

// 设备状态管理
void DeviceControlWidget::setDeviceState(DeviceState state)
{
    if (deviceState != state) {
        deviceState = state;
        updateControlButtons();
        updateStatusDisplay();
        emit deviceStateChanged(state);
        
        QString stateText;
        QString color;
        switch (state) {
            case DeviceState::Stopped:
                stateText = "停止";
                color = "red";
                break;
            case DeviceState::Running:
                stateText = "运行中";
                color = "green";
                break;
            case DeviceState::Paused:
                stateText = "暂停";
                color = "orange";
                break;
            case DeviceState::Homing:
                stateText = "回原点";
                color = "blue";
                break;
            case DeviceState::Starting:
                stateText = "启动中";
                color = "blue";
                break;
            case DeviceState::Stopping:
                stateText = "停止中";
                color = "orange";
                break;
            case DeviceState::Error:
                stateText = "错误";
                color = "red";
                break;
            case DeviceState::EmergencyStop:
                stateText = "紧急停止";
                color = "red";
                break;
        }
        
        deviceStateLabel->setText(stateText);
        deviceStateLabel->setStyleSheet(QString("QLabel { font-weight: bold; color: %1; }").arg(color));
        
        logMessage(QString("设备状态变更: %1").arg(stateText));
    }
}

DeviceState DeviceControlWidget::getDeviceState() const
{
    return deviceState;
}

// 参数设置
void DeviceControlWidget::setGlueParameters(const GlueParameters& params)
{
    glueParams = params;
    
    // 更新界面显示
    glueVolumeSpinBox->setValue(params.volume);
    glueSpeedSpinBox->setValue(params.speed);
    gluePressureSpinBox->setValue(params.pressure);
    glueTemperatureSpinBox->setValue(params.temperature);
    dwellTimeSpinBox->setValue(params.dwellTime);
    riseTimeSpinBox->setValue(params.riseTime);
    fallTimeSpinBox->setValue(params.fallTime);
}

GlueParameters DeviceControlWidget::getGlueParameters() const
{
    return glueParams;
}

void DeviceControlWidget::setMotionParameters(const MotionParameters& params)
{
    motionParams = params;
    
    // 更新界面显示
    velocityXSpinBox->setValue(params.velocityX);
    velocityYSpinBox->setValue(params.velocityY);
    velocityZSpinBox->setValue(params.velocityZ);
    accelerationXSpinBox->setValue(params.accelerationX);
    accelerationYSpinBox->setValue(params.accelerationY);
    accelerationZSpinBox->setValue(params.accelerationZ);
}

MotionParameters DeviceControlWidget::getMotionParameters() const
{
    return motionParams;
}

void DeviceControlWidget::setSerialWorker(SerialWorker* worker)
{
    serialWorker = worker;
    if (serialWorker) {
        connect(serialWorker, &SerialWorker::frameReceived, 
                this, &DeviceControlWidget::onFrameReceived);
        connect(serialWorker, &SerialWorker::connected, 
                [this]() { isConnected = true; updateControlButtons(); });
        connect(serialWorker, &SerialWorker::disconnected, 
                [this]() { isConnected = false; updateControlButtons(); });
    }
}

// 设备控制槽函数
void DeviceControlWidget::onStartButtonClicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接，请先连接设备！");
        return;
    }
    
    if (deviceState == DeviceState::Running) {
        QMessageBox::information(this, "提示", "设备已经在运行状态！");
        return;
    }
    
    // 检查参数有效性
    if (!validateParameters()) {
        QMessageBox::warning(this, "警告", "参数设置不正确，请检查参数设置！");
        return;
    }
    
    // 发送启动命令，添加空指针检查
    QByteArray command;
    command.append(static_cast<char>(ProtocolCommand::DeviceStart));
    
    // 添加参数数据，检查组件是否存在
    QJsonObject params;
    if (glueVolumeSpinBox) {
        params["volume"] = glueVolumeSpinBox->value();
    }
    if (gluePressureSpinBox) {
        params["pressure"] = gluePressureSpinBox->value();
    }
    if (glueTemperatureSpinBox) {
        params["temperature"] = glueTemperatureSpinBox->value();
    }
    if (motionSpeedSpinBox) {
        params["speed"] = motionSpeedSpinBox->value();
    }
    if (motionAccelerationSpinBox) {
        params["acceleration"] = motionAccelerationSpinBox->value();
    }
    
    QJsonDocument doc(params);
    command.append(doc.toJson(QJsonDocument::Compact));
    
    sendCommand(ProtocolCommand::DeviceStart, command);
    
    setDeviceState(DeviceState::Starting);
    if (progressBar) {
        progressBar->setValue(0);
    }
    
    LogManager::getInstance()->info("发送设备启动命令", "DeviceControl");
    
    // 启动进度监控
    startProgressMonitoring();
}

void DeviceControlWidget::onStopButtonClicked()
{
    if (deviceState == DeviceState::Stopped) {
        QMessageBox::information(this, "提示", "设备已经停止！");
        return;
    }
    
    // 发送停止命令
    sendCommand(ProtocolCommand::DeviceStop, QByteArray());
    
    setDeviceState(DeviceState::Stopping);
    if (progressBar) {
        progressBar->setValue(0);
    }
    
    LogManager::getInstance()->info("发送设备停止命令", "DeviceControl");
    
    // 停止进度监控
    stopProgressMonitoring();
}

void DeviceControlWidget::onPauseButtonClicked()
{
    if (deviceState != DeviceState::Running) {
        QMessageBox::warning(this, "警告", "设备未在运行状态，无法暂停！");
        return;
    }
    
    // 发送暂停命令
    sendCommand(ProtocolCommand::PauseDevice, QByteArray());
    
    setDeviceState(DeviceState::Paused);
    
    LogManager::getInstance()->info("发送设备暂停命令", "DeviceControl");
}

void DeviceControlWidget::onHomeButtonClicked()
{
    if (deviceState == DeviceState::Running) {
        QMessageBox::warning(this, "警告", "设备运行中，无法回原点！请先停止设备。");
        return;
    }
    
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接，请先连接设备！");
        return;
    }
    
    // 确认对话框
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认");
    msgBox.setText("确定要回原点吗？这将移动所有轴到原点位置。");
    msgBox.setIcon(QMessageBox::Question);
    
    QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
    QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.setDefaultButton(noButton);
    
    msgBox.exec();
    
    if (msgBox.clickedButton() == yesButton) {
        // 发送回原点命令
        sendCommand(ProtocolCommand::HomeDevice, QByteArray());
        
        setDeviceState(DeviceState::Homing);
        if (progressBar) {
            progressBar->setValue(0);
        }
        
        LogManager::getInstance()->info("发送设备回原点命令", "DeviceControl");
        
        // 启动回原点监控
        startHomingMonitoring();
    }
}

void DeviceControlWidget::onEmergencyStopButtonClicked()
{
    // 紧急停止不需要确认，立即执行
    sendCommand(ProtocolCommand::EmergencyStop, QByteArray());
    
    setDeviceState(DeviceState::EmergencyStop);
    if (progressBar) {
        progressBar->setValue(0);
    }
    
    // 重置所有状态
    currentX = currentY = currentZ = 0.0;
    currentVolume = currentPressure = 0.0;
    currentTemperature = 25.0;
    
    updateStatusDisplay();
    
    LogManager::getInstance()->error("紧急停止触发", "DeviceControl");
    
    // 发出紧急停止信号
    emit emergencyStopTriggered();
    
    // 显示紧急停止对话框
    QMessageBox::critical(this, "紧急停止", "设备已紧急停止！\n请检查设备状态后重新启动。");
}

void DeviceControlWidget::onMoveButtonClicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接，请先连接设备！");
        return;
    }
    
    if (deviceState == DeviceState::Running) {
        QMessageBox::warning(this, "警告", "设备运行中，无法移动！请先停止设备。");
        return;
    }
    
    // 添加空指针检查
    if (!xPositionSpinBox || !yPositionSpinBox || !zPositionSpinBox) {
        QMessageBox::warning(this, "警告", "位置输入控件未正确初始化！");
        return;
    }
    
    double targetX = xPositionSpinBox->value();
    double targetY = yPositionSpinBox->value();
    double targetZ = zPositionSpinBox->value();
    
    // 检查位置限制
    if (!validatePosition(targetX, targetY, targetZ)) {
        QMessageBox::warning(this, "警告", "目标位置超出设备限制范围！");
        return;
    }
    
    // 确认移动
    QString message = QString("确定要移动到位置 X:%1, Y:%2, Z:%3 吗？")
                     .arg(targetX, 0, 'f', 3)
                     .arg(targetY, 0, 'f', 3)
                     .arg(targetZ, 0, 'f', 3);
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认移动");
    msgBox.setText(message);
    msgBox.setIcon(QMessageBox::Question);
    
    QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
    QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.setDefaultButton(noButton);
    
    msgBox.exec();
    
    if (msgBox.clickedButton() == yesButton) {
        moveToPosition(targetX, targetY, targetZ);
    }
}

void DeviceControlWidget::onSetOriginButtonClicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接，请先连接设备！");
        return;
    }
    
    if (deviceState == DeviceState::Running) {
        QMessageBox::warning(this, "警告", "设备运行中，无法设置原点！请先停止设备。");
        return;
    }
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认");
    msgBox.setText("确定要将当前位置设为原点吗？");
    msgBox.setIcon(QMessageBox::Question);
    
    QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
    QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.setDefaultButton(noButton);
    
    msgBox.exec();
    
    if (msgBox.clickedButton() == yesButton) {
        // 发送设置原点命令
        sendCommand(ProtocolCommand::SetOrigin, QByteArray());
        
        // 更新当前位置为原点
        currentX = currentY = currentZ = 0.0;
        updateStatusDisplay();
        
        LogManager::getInstance()->info("设置当前位置为原点", "DeviceControl");
        
        QMessageBox::information(this, "成功", "原点设置成功！");
    }
}

void DeviceControlWidget::onJogButtonClicked()
{
    QPushButton* button = qobject_cast<QPushButton*>(sender());
    if (!button) return;
    
    double step = jogStepSpinBox->value();
    QString axis;
    double distance = 0;
    
    if (button == jogXPlusButton) {
        axis = "X";
        distance = step;
    } else if (button == jogXMinusButton) {
        axis = "X";
        distance = -step;
    } else if (button == jogYPlusButton) {
        axis = "Y";
        distance = step;
    } else if (button == jogYMinusButton) {
        axis = "Y";
        distance = -step;
    } else if (button == jogZPlusButton) {
        axis = "Z";
        distance = step;
    } else if (button == jogZMinusButton) {
        axis = "Z";
        distance = -step;
    }
    
    if (!axis.isEmpty()) {
        jogMove(axis, distance);
    }
}

void DeviceControlWidget::onGlueParameterChanged()
{
    // 更新参数结构
    glueParams.volume = glueVolumeSpinBox->value();
    glueParams.speed = glueSpeedSpinBox->value();
    glueParams.pressure = gluePressureSpinBox->value();
    glueParams.temperature = glueTemperatureSpinBox->value();
    glueParams.dwellTime = dwellTimeSpinBox->value();
    glueParams.riseTime = riseTimeSpinBox->value();
    glueParams.fallTime = fallTimeSpinBox->value();
    
    emit parametersChanged();
    updateGlueParameters();
}

void DeviceControlWidget::onMotionParameterChanged()
{
    // 更新参数结构
    motionParams.velocityX = velocityXSpinBox->value();
    motionParams.velocityY = velocityYSpinBox->value();
    motionParams.velocityZ = velocityZSpinBox->value();
    motionParams.accelerationX = accelerationXSpinBox->value();
    motionParams.accelerationY = accelerationYSpinBox->value();
    motionParams.accelerationZ = accelerationZSpinBox->value();
    
    emit parametersChanged();
    updateMotionParameters();
}

void DeviceControlWidget::onUpdateTimer()
{
    updatePositionDisplay();
    updateDeviceStatus();
}

void DeviceControlWidget::onFrameReceived(const ProtocolFrame& frame)
{
    // 处理接收到的协议帧
    switch (frame.command) {
        case ProtocolCommand::DeviceStatus:
            // 解析设备状态
            if (frame.data.size() >= 1) {
                quint8 status = frame.data[0];
                DeviceState newState = static_cast<DeviceState>(status);
                setDeviceState(newState);
            }
            break;
            
        case ProtocolCommand::ReadSensorData:
            // 解析传感器数据
            if (frame.data.size() >= 12) {
                // 假设数据格式：X(4字节) Y(4字节) Z(4字节)
                QDataStream stream(frame.data);
                stream.setByteOrder(QDataStream::LittleEndian);
                stream >> currentX >> currentY >> currentZ;
                emit positionChanged(currentX, currentY, currentZ);
            }
            break;
            
        default:
            break;
    }
}

// 设备控制实现
void DeviceControlWidget::startDevice()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接！");
        return;
    }
    
    if (deviceState == DeviceState::Running) {
        logMessage("设备已在运行中");
        return;
    }
    
    sendCommand(ProtocolCommand::DeviceStart);
    setDeviceState(DeviceState::Running);
    logMessage("发送启动命令");
}

void DeviceControlWidget::stopDevice()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接！");
        return;
    }
    
    sendCommand(ProtocolCommand::DeviceStop);
    setDeviceState(DeviceState::Stopped);
    logMessage("发送停止命令");
}

void DeviceControlWidget::pauseDevice()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接！");
        return;
    }
    
    // 使用自定义暂停命令
    QByteArray data;
    data.append(static_cast<char>(0x01)); // 暂停标志
    sendCommand(ProtocolCommand::WriteParameter, data);
    setDeviceState(DeviceState::Paused);
    logMessage("发送暂停命令");
}

void DeviceControlWidget::resumeDevice()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接！");
        return;
    }
    
    // 使用自定义恢复命令
    QByteArray data;
    data.append(static_cast<char>(0x02)); // 恢复标志
    sendCommand(ProtocolCommand::WriteParameter, data);
    setDeviceState(DeviceState::Running);
    logMessage("发送恢复命令");
}

void DeviceControlWidget::homeDevice()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接！");
        return;
    }
    
    sendCommand(ProtocolCommand::DeviceReset);
    setDeviceState(DeviceState::Homing);
    logMessage("发送回原点命令");
}

void DeviceControlWidget::emergencyStop()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接！");
        return;
    }
    
    // 紧急停止使用特殊命令
    QByteArray data;
    data.append(static_cast<char>(0xFF)); // 紧急停止标志
    sendCommand(ProtocolCommand::WriteParameter, data);
    setDeviceState(DeviceState::EmergencyStop);
    logMessage("触发紧急停止");
    emit emergencyStopTriggered();
}

void DeviceControlWidget::moveToPosition(double x, double y, double z)
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接！");
        return;
    }
    
    // 构建位置数据
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << static_cast<float>(x) << static_cast<float>(y) << static_cast<float>(z);
    
    sendCommand(ProtocolCommand::WriteParameter, data);
    logMessage(QString("移动到位置: X=%.3f, Y=%.3f, Z=%.3f").arg(x).arg(y).arg(z));
}

void DeviceControlWidget::jogMove(const QString& axis, double distance)
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接！");
        return;
    }
    
    // 构建点动数据
    QByteArray data;
    data.append(axis.toUtf8());
    data.append(static_cast<char>(0x00)); // 分隔符
    
    QDataStream stream(&data, QIODevice::WriteOnly | QIODevice::Append);
    stream.setByteOrder(QDataStream::LittleEndian);
    stream << static_cast<float>(distance);
    
    sendCommand(ProtocolCommand::WriteParameter, data);
    logMessage(QString("点动: %1轴 %2mm").arg(axis).arg(distance, 0, 'f', 3));
}

void DeviceControlWidget::setOrigin()
{
    if (!isConnected) {
        QMessageBox::warning(this, "警告", "设备未连接！");
        return;
    }
    
    QByteArray data;
    data.append(static_cast<char>(0x10)); // 设置原点标志
    sendCommand(ProtocolCommand::WriteParameter, data);
    
    // 更新当前位置显示
    currentX = currentY = currentZ = 0.0;
    emit positionChanged(currentX, currentY, currentZ);
    
    logMessage("设置当前位置为原点");
}

void DeviceControlWidget::updateGlueParameters()
{
    if (!isConnected) return;
    
    // 构建点胶参数数据
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    stream << static_cast<float>(glueParams.volume);
    stream << static_cast<float>(glueParams.speed);
    stream << static_cast<float>(glueParams.pressure);
    stream << static_cast<float>(glueParams.temperature);
    stream << static_cast<quint16>(glueParams.dwellTime);
    stream << static_cast<quint16>(glueParams.riseTime);
    stream << static_cast<quint16>(glueParams.fallTime);
    
    sendCommand(ProtocolCommand::WriteAllParameters, data);
    logMessage("更新点胶参数");
}

void DeviceControlWidget::updateMotionParameters()
{
    if (!isConnected) return;
    
    // 构建运动参数数据
    QByteArray data;
    QDataStream stream(&data, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::LittleEndian);
    
    stream << static_cast<float>(motionParams.velocityX);
    stream << static_cast<float>(motionParams.velocityY);
    stream << static_cast<float>(motionParams.velocityZ);
    stream << static_cast<float>(motionParams.accelerationX);
    stream << static_cast<float>(motionParams.accelerationY);
    stream << static_cast<float>(motionParams.accelerationZ);
    
    sendCommand(ProtocolCommand::WriteParameter, data);
    logMessage("更新运动参数");
}

void DeviceControlWidget::updateDeviceStatus()
{
    if (!isConnected) return;
    
    // 定期查询设备状态
    static int counter = 0;
    if (++counter >= 10) { // 每1秒查询一次
        counter = 0;
        sendCommand(ProtocolCommand::DeviceStatus);
        sendCommand(ProtocolCommand::ReadSensorData);
    }
}

void DeviceControlWidget::updatePositionDisplay()
{
    // 更新位置显示
    currentXLabel->setText(QString("%1 mm").arg(currentX, 0, 'f', 3));
    currentYLabel->setText(QString("%1 mm").arg(currentY, 0, 'f', 3));
    currentZLabel->setText(QString("%1 mm").arg(currentZ, 0, 'f', 3));
    
    // 更新点胶状态显示
    currentVolumeLabel->setText(QString("%1 μL").arg(currentVolume, 0, 'f', 3));
    currentPressureLabel->setText(QString("%1 Bar").arg(currentPressure, 0, 'f', 2));
    currentTemperatureLabel->setText(QString("%1 °C").arg(currentTemperature, 0, 'f', 1));
    
    // 更新目标位置显示
    xPositionSpinBox->setValue(currentX);
    yPositionSpinBox->setValue(currentY);
    zPositionSpinBox->setValue(currentZ);
}

void DeviceControlWidget::updateControlButtons()
{
    bool connected = isConnected;
    bool canControl = connected && (deviceState != DeviceState::Error);
    
    // 基本控制按钮 - 添加空指针检查
    if (startButton) {
        startButton->setEnabled(canControl && deviceState == DeviceState::Stopped);
    }
    if (stopButton) {
        stopButton->setEnabled(canControl && deviceState != DeviceState::Stopped);
    }
    if (homeButton) {
        homeButton->setEnabled(canControl && deviceState == DeviceState::Stopped);
    }
    if (emergencyButton) {
        emergencyButton->setEnabled(connected);
    }
    
    // 暂停/恢复按钮
    if (pauseButton) {
        if (deviceState == DeviceState::Running) {
            pauseButton->setText("暂停");
            pauseButton->setEnabled(canControl);
        } else if (deviceState == DeviceState::Paused) {
            pauseButton->setText("恢复");
            pauseButton->setEnabled(canControl);
        } else {
            pauseButton->setText("暂停");
            pauseButton->setEnabled(false);
        }
    }
    
    // 运动控制按钮
    bool canMove = canControl && (deviceState == DeviceState::Stopped || deviceState == DeviceState::Paused);
    if (moveButton) {
        moveButton->setEnabled(canMove);
    }
    if (setOriginButton) {
        setOriginButton->setEnabled(canMove);
    }
    
    // 点动按钮
    if (jogXPlusButton) jogXPlusButton->setEnabled(canMove);
    if (jogXMinusButton) jogXMinusButton->setEnabled(canMove);
    if (jogYPlusButton) jogYPlusButton->setEnabled(canMove);
    if (jogYMinusButton) jogYMinusButton->setEnabled(canMove);
    if (jogZPlusButton) jogZPlusButton->setEnabled(canMove);
    if (jogZMinusButton) jogZMinusButton->setEnabled(canMove);
}

void DeviceControlWidget::updateStatusDisplay()
{
    // 更新进度条 - 添加空指针检查
    if (progressBar) {
        if (deviceState == DeviceState::Running) {
            progressBar->setRange(0, 0); // 不确定进度
        } else {
            progressBar->setRange(0, 100);
            progressBar->setValue(0);
        }
    }
    
    // 更新报警状态
    if (alarmStatusLabel) {
        if (deviceState == DeviceState::Error || deviceState == DeviceState::EmergencyStop) {
            alarmStatusLabel->setText("报警");
            alarmStatusLabel->setStyleSheet("QLabel { font-weight: bold; color: red; }");
        } else {
            alarmStatusLabel->setText("正常");
            alarmStatusLabel->setStyleSheet("QLabel { font-weight: bold; color: green; }");
        }
    }
}

void DeviceControlWidget::sendCommand(ProtocolCommand command, const QByteArray& data)
{
    if (serialWorker && isConnected) {
        serialWorker->sendFrame(command, data);
    }
}

void DeviceControlWidget::logMessage(const QString& message)
{
    if (!logTextEdit) {
        return; // 如果日志控件未初始化，直接返回
    }
    
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    QString logEntry = QString("[%1] %2").arg(timestamp, message);
    
    logTextEdit->append(logEntry);
    
    // 限制日志行数
    if (logTextEdit->document()->blockCount() > 100) {
        QTextCursor cursor = logTextEdit->textCursor();
        cursor.movePosition(QTextCursor::Start);
        cursor.movePosition(QTextCursor::Down, QTextCursor::KeepAnchor, 10);
        cursor.removeSelectedText();
    }
    
    // 滚动到底部
    logTextEdit->moveCursor(QTextCursor::End);
    
    // 记录到系统日志
    LogManager::getInstance()->info(message, "DeviceControl");
}

bool DeviceControlWidget::validateParameters()
{
    // 检查点胶参数
    if (glueVolumeSpinBox->value() <= 0) {
        QMessageBox::warning(this, "参数错误", "点胶量必须大于0！");
        return false;
    }
    
    if (gluePressureSpinBox->value() <= 0) {
        QMessageBox::warning(this, "参数错误", "点胶压力必须大于0！");
        return false;
    }
    
    if (glueTemperatureSpinBox->value() < 10 || glueTemperatureSpinBox->value() > 80) {
        QMessageBox::warning(this, "参数错误", "点胶温度必须在10-80°C范围内！");
        return false;
    }
    
    // 检查运动参数
    if (motionSpeedSpinBox->value() <= 0) {
        QMessageBox::warning(this, "参数错误", "运动速度必须大于0！");
        return false;
    }
    
    if (motionAccelerationSpinBox->value() <= 0) {
        QMessageBox::warning(this, "参数错误", "运动加速度必须大于0！");
        return false;
    }
    
    return true;
}

bool DeviceControlWidget::validatePosition(double x, double y, double z)
{
    // 检查X轴限制
    if (x < -1000 || x > 1000) {
        return false;
    }
    
    // 检查Y轴限制
    if (y < -1000 || y > 1000) {
        return false;
    }
    
    // 检查Z轴限制
    if (z < -100 || z > 100) {
        return false;
    }
    
    return true;
}

void DeviceControlWidget::startProgressMonitoring()
{
    // 启动进度监控定时器
    if (!progressTimer) {
        progressTimer = new QTimer(this);
        connect(progressTimer, &QTimer::timeout, this, &DeviceControlWidget::updateProgress);
    }
    
    progressTimer->start(200); // 每200ms更新一次进度
    progressValue = 0;
}

void DeviceControlWidget::stopProgressMonitoring()
{
    if (progressTimer) {
        progressTimer->stop();
    }
    progressValue = 0;
    progressBar->setValue(0);
}

void DeviceControlWidget::startHomingMonitoring()
{
    // 启动回原点监控
    if (!homingTimer) {
        homingTimer = new QTimer(this);
        connect(homingTimer, &QTimer::timeout, this, &DeviceControlWidget::updateHomingProgress);
    }
    
    homingTimer->start(500); // 每500ms检查一次回原点状态
    homingStartTime = QDateTime::currentDateTime();
}

void DeviceControlWidget::updateProgress()
{
    if (deviceState == DeviceState::Starting || deviceState == DeviceState::Running) {
        progressValue += 2;
        if (progressValue >= 100) {
            progressValue = 100;
            if (deviceState == DeviceState::Starting) {
                setDeviceState(DeviceState::Running);
            }
        }
        progressBar->setValue(progressValue);
    }
}

void DeviceControlWidget::updateHomingProgress()
{
    QDateTime currentTime = QDateTime::currentDateTime();
    qint64 elapsedMs = homingStartTime.msecsTo(currentTime);
    
    // 模拟回原点进度（实际应该从设备获取）
    int progress = qMin(100, static_cast<int>(elapsedMs / 100)); // 10秒完成回原点
    progressBar->setValue(progress);
    
    if (progress >= 100) {
        // 回原点完成
        homingTimer->stop();
        setDeviceState(DeviceState::Stopped);
        currentX = currentY = currentZ = 0.0;
        updateStatusDisplay();
        
        LogManager::getInstance()->info("设备回原点完成", "DeviceControl");
        QMessageBox::information(this, "成功", "设备回原点完成！");
    }
}

void DeviceControlWidget::connectToDevice()
{
    if (isConnected) {
        QMessageBox::information(this, "提示", "设备已连接！");
        return;
    }
    
    // 创建串口工作器
    if (!serialWorker) {
        serialWorker = new SerialWorker(this);
        connect(serialWorker, &SerialWorker::dataReceived, this, &DeviceControlWidget::onDataReceived);
        connect(serialWorker, &SerialWorker::errorOccurred, this, &DeviceControlWidget::onErrorOccurred);
        connect(serialWorker, &SerialWorker::connectionStateChanged, this, [this](SerialConnectionState state) {
            onConnectionStatusChanged(state == SerialConnectionState::Connected);
        });
    }
    
    // 配置串口参数
    SerialConfig config;
    config.portName = "COM1"; // 默认端口，实际应该从配置读取
    config.baudRate = 115200;
    config.dataBits = QSerialPort::Data8;
    config.parity = QSerialPort::NoParity;
    config.stopBits = QSerialPort::OneStop;
    config.flowControl = QSerialPort::NoFlowControl;
    
    // 连接设备
    serialWorker->openPort(config);
    
    LogManager::getInstance()->info("尝试连接设备", "DeviceControl");
}

void DeviceControlWidget::disconnectFromDevice()
{
    if (!isConnected) {
        QMessageBox::information(this, "提示", "设备未连接！");
        return;
    }
    
    if (serialWorker) {
        serialWorker->closePort();
    }
    
    LogManager::getInstance()->info("断开设备连接", "DeviceControl");
}

void DeviceControlWidget::onConnectionStatusChanged(bool connected)
{
    isConnected = connected;
    updateControlButtons();
    
    if (connected) {
        LogManager::getInstance()->info("设备连接成功", "DeviceControl");
        QMessageBox::information(this, "成功", "设备连接成功！");
    } else {
        LogManager::getInstance()->info("设备连接断开", "DeviceControl");
        setDeviceState(DeviceState::Stopped);
    }
}

void DeviceControlWidget::onDataReceived(const QByteArray& data)
{
    // 解析接收到的数据
    parseReceivedData(data);
}

void DeviceControlWidget::onErrorOccurred(const QString& error)
{
    LogManager::getInstance()->error("设备通信错误: " + error, "DeviceControl");
    QMessageBox::warning(this, "通信错误", "设备通信错误:\n" + error);
}

void DeviceControlWidget::parseReceivedData(const QByteArray& data)
{
    // 解析设备返回的数据
    if (data.isEmpty()) return;
    
    // 简单的协议解析示例
    if (data.startsWith("POS:")) {
        // 位置数据: POS:X,Y,Z
        QString posStr = QString::fromUtf8(data.mid(4));
        QStringList posList = posStr.split(',');
        if (posList.size() >= 3) {
            currentX = posList[0].toDouble();
            currentY = posList[1].toDouble();
            currentZ = posList[2].toDouble();
            updateStatusDisplay();
            emit positionChanged(currentX, currentY, currentZ);
        }
    } else if (data.startsWith("STATUS:")) {
        // 状态数据: STATUS:state
        QString statusStr = QString::fromUtf8(data.mid(7));
        updateDeviceStateFromString(statusStr);
    } else if (data.startsWith("PARAM:")) {
        // 参数数据: PARAM:volume,pressure,temperature
        QString paramStr = QString::fromUtf8(data.mid(6));
        QStringList paramList = paramStr.split(',');
        if (paramList.size() >= 3) {
            currentVolume = paramList[0].toDouble();
            currentPressure = paramList[1].toDouble();
            currentTemperature = paramList[2].toDouble();
            updateStatusDisplay();
        }
    }
}

void DeviceControlWidget::updateDeviceStateFromString(const QString& stateStr)
{
    if (stateStr == "STOPPED") {
        setDeviceState(DeviceState::Stopped);
    } else if (stateStr == "RUNNING") {
        setDeviceState(DeviceState::Running);
    } else if (stateStr == "PAUSED") {
        setDeviceState(DeviceState::Paused);
    } else if (stateStr == "HOMING") {
        setDeviceState(DeviceState::Homing);
    } else if (stateStr == "ERROR") {
        setDeviceState(DeviceState::Error);
    }
} 