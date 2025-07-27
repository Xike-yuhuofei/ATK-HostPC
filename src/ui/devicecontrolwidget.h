#pragma once

#include <QWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPushButton>
#include <QLabel>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QSlider>
#include <QProgressBar>
#include <QTextEdit>
#include <QTimer>
#include <QButtonGroup>
#include <QCheckBox>
#include <QTabWidget>
#include <QFrame>
#include <QSplitter>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonDocument>
#include <QMessageBox>
#include "communication/protocolparser.h"

// 点胶设备状态枚举
enum class DeviceState {
    Stopped,        // 停止
    Running,        // 运行中
    Paused,         // 暂停
    Homing,         // 回原点
    Starting,       // 启动中
    Stopping,       // 停止中
    Error,          // 错误
    EmergencyStop   // 紧急停止
};

// 点胶参数结构
struct GlueParameters {
    double volume;          // 胶量 (μL)
    double speed;           // 点胶速度 (mm/s)
    double pressure;        // 压力 (Bar)
    double temperature;     // 温度 (°C)
    int dwellTime;          // 停留时间 (ms)
    int riseTime;           // 上升时间 (ms)
    int fallTime;           // 下降时间 (ms)
    
    GlueParameters() 
        : volume(1.0), speed(10.0), pressure(2.0), temperature(25.0)
        , dwellTime(100), riseTime(50), fallTime(50) {}
};

// 运动参数结构
struct MotionParameters {
    double x, y, z;         // 位置坐标 (mm)
    double velocityX, velocityY, velocityZ;  // 速度 (mm/s)
    double accelerationX, accelerationY, accelerationZ;  // 加速度 (mm/s²)
    
    MotionParameters() 
        : x(0), y(0), z(0)
        , velocityX(100), velocityY(100), velocityZ(50)
        , accelerationX(1000), accelerationY(1000), accelerationZ(500) {}
};

class SerialWorker;

class DeviceControlWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceControlWidget(QWidget* parent = nullptr);
    ~DeviceControlWidget();
    
    // 设备状态
    void setDeviceState(DeviceState state);
    DeviceState getDeviceState() const;
    
    // 参数设置
    void setGlueParameters(const GlueParameters& params);
    GlueParameters getGlueParameters() const;
    
    void setMotionParameters(const MotionParameters& params);
    MotionParameters getMotionParameters() const;
    
    // 串口通讯
    void setSerialWorker(SerialWorker* worker);

public slots:
    // 设备控制
    void startDevice();
    void stopDevice();
    void pauseDevice();
    void resumeDevice();
    void homeDevice();
    void emergencyStop();
    
    // 运动控制
    void moveToPosition(double x, double y, double z);
    void jogMove(const QString& axis, double distance);
    void setOrigin();
    
    // 参数更新
    void updateGlueParameters();
    void updateMotionParameters();
    
    // 状态更新
    void updateDeviceStatus();
    void updatePositionDisplay();

signals:
    void deviceStateChanged(DeviceState state);
    void parametersChanged();
    void positionChanged(double x, double y, double z);
    void emergencyStopTriggered();

private slots:
    void onStartButtonClicked();
    void onStopButtonClicked();
    void onPauseButtonClicked();
    void onHomeButtonClicked();
    void onEmergencyStopButtonClicked();
    
    void onMoveButtonClicked();
    void onJogButtonClicked();
    void onSetOriginButtonClicked();
    
    void onGlueParameterChanged();
    void onMotionParameterChanged();
    
    void onUpdateTimer();
    void onFrameReceived(const ProtocolFrame& frame);
    
    // 新增的槽函数
    void onConnectionStatusChanged(bool connected);
    void onDataReceived(const QByteArray& data);
    void onErrorOccurred(const QString& error);
    void updateProgress();
    void updateHomingProgress();

private:
    void setupUI();
    void setupDeviceControlPanel();
    void setupMotionControlPanel();
    void setupParameterPanel();
    void setupStatusPanel();
    void setupConnections();
    
    void sendCommand(ProtocolCommand command, const QByteArray& data = QByteArray());
    void updateControlButtons();
    void updateStatusDisplay();
    void logMessage(const QString& message);
    
    // 新增的私有方法
    bool validateParameters();
    bool validatePosition(double x, double y, double z);
    void startProgressMonitoring();
    void stopProgressMonitoring();
    void startHomingMonitoring();
    void connectToDevice();
    void disconnectFromDevice();
    void parseReceivedData(const QByteArray& data);
    void updateDeviceStateFromString(const QString& stateStr);
    
    // UI组件
    QTabWidget* tabWidget;
    
    // 设备控制面板
    QGroupBox* deviceControlGroup;
    QPushButton* startButton;
    QPushButton* stopButton;
    QPushButton* pauseButton;
    QPushButton* homeButton;
    QPushButton* emergencyButton;
    QLabel* deviceStateLabel;
    QProgressBar* progressBar;
    
    // 运动控制面板
    QGroupBox* motionControlGroup;
    QDoubleSpinBox* xPositionSpinBox;
    QDoubleSpinBox* yPositionSpinBox;
    QDoubleSpinBox* zPositionSpinBox;
    QPushButton* moveButton;
    QPushButton* setOriginButton;
    
    // 点动控制
    QGroupBox* jogControlGroup;
    QPushButton* jogXPlusButton;
    QPushButton* jogXMinusButton;
    QPushButton* jogYPlusButton;
    QPushButton* jogYMinusButton;
    QPushButton* jogZPlusButton;
    QPushButton* jogZMinusButton;
    QDoubleSpinBox* jogStepSpinBox;
    
    // 参数设置面板
    QGroupBox* glueParameterGroup;
    QDoubleSpinBox* glueVolumeSpinBox;
    QDoubleSpinBox* glueSpeedSpinBox;
    QDoubleSpinBox* gluePressureSpinBox;
    QDoubleSpinBox* glueTemperatureSpinBox;
    QSpinBox* dwellTimeSpinBox;
    QSpinBox* riseTimeSpinBox;
    QSpinBox* fallTimeSpinBox;
    
    QGroupBox* motionParameterGroup;
    QDoubleSpinBox* motionSpeedSpinBox;
    QDoubleSpinBox* motionAccelerationSpinBox;
    QDoubleSpinBox* velocityXSpinBox;
    QDoubleSpinBox* velocityYSpinBox;
    QDoubleSpinBox* velocityZSpinBox;
    QDoubleSpinBox* accelerationXSpinBox;
    QDoubleSpinBox* accelerationYSpinBox;
    QDoubleSpinBox* accelerationZSpinBox;
    
    // 状态显示面板
    QGroupBox* statusGroup;
    QLabel* currentXLabel;
    QLabel* currentYLabel;
    QLabel* currentZLabel;
    QLabel* currentVolumeLabel;
    QLabel* currentPressureLabel;
    QLabel* currentTemperatureLabel;
    QLabel* alarmStatusLabel;
    QTextEdit* logTextEdit;
    
    // 数据成员
    DeviceState deviceState;
    GlueParameters glueParams;
    MotionParameters motionParams;
    SerialWorker* serialWorker;
    QTimer* updateTimer;
    
    // 新增的定时器和状态变量
    QTimer* progressTimer;
    QTimer* homingTimer;
    QDateTime homingStartTime;
    int progressValue;
    
    // 当前状态
    double currentX, currentY, currentZ;
    double currentVolume, currentPressure, currentTemperature;
    bool isConnected;
    QString lastError;
}; 