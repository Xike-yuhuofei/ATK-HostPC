#pragma once

#include <QWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QLCDNumber>
#include <QProgressBar>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTabWidget>
#include <QSplitter>
#include <QTimer>
#include <QDateTime>
#include <QQueue>
#include <QMutex>
#include <QPainter>
#include <QPushButton>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>

// 实时数据结构
struct RealTimeData {
    QDateTime timestamp;        // 时间戳
    double positionX;          // X轴位置
    double positionY;          // Y轴位置
    double positionZ;          // Z轴位置
    double velocity;           // 当前速度
    double pressure;           // 当前压力
    double temperature;        // 当前温度
    double glueVolume;         // 胶量
    int deviceStatus;          // 设备状态
    
    RealTimeData() 
        : timestamp(QDateTime::currentDateTime())
        , positionX(0), positionY(0), positionZ(0)
        , velocity(0), pressure(0), temperature(25.0)
        , glueVolume(0), deviceStatus(0) {}
};

// 监控配置
struct MonitorConfig {
    int updateInterval;        // 更新间隔(ms)
    int historySize;          // 历史数据大小
    bool enableLogging;       // 是否启用日志
    bool enableAlerts;        // 是否启用报警
    
    // 报警阈值
    struct AlertThresholds {
        double maxTemperature;    // 最高温度
        double minTemperature;    // 最低温度
        double maxPressure;       // 最高压力
        double minPressure;       // 最低压力
        double maxVelocity;       // 最高速度
        
        AlertThresholds() 
            : maxTemperature(60.0), minTemperature(15.0)
            , maxPressure(8.0), minPressure(0.5)
            , maxVelocity(800.0) {}
    } alertThresholds;
    
    MonitorConfig() 
        : updateInterval(100), historySize(1000)
        , enableLogging(true), enableAlerts(true) {}
};

class SerialWorker;
struct ProtocolFrame;

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>

// Use QtCharts namespace

class DataMonitorWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DataMonitorWidget(QWidget* parent = nullptr);
    ~DataMonitorWidget();
    
    // 数据更新
    void updateRealTimeData(const RealTimeData& data);
    void addDataPoint(const RealTimeData& data);
    
    // 配置管理
    void setMonitorConfig(const MonitorConfig& config);
    MonitorConfig getMonitorConfig() const;
    
    // 数据管理
    void clearHistory();
    void exportData(const QString& filePath);
    QList<RealTimeData> getHistoryData() const;
    
    // 串口通讯
    void setSerialWorker(SerialWorker* worker);

public slots:
    void startMonitoring();
    void stopMonitoring();
    void pauseMonitoring();
    void resumeMonitoring();
    
    void onDataReceived();
    void onUpdateTimer();
    void onConfigChanged();

signals:
    void dataUpdated(const RealTimeData& data);
    void alertTriggered(const QString& message);
    void monitoringStateChanged(bool isRunning);

private slots:
    void onFrameReceived(const ProtocolFrame& frame);
    void onExportData();
    void onClearHistory();
    void onConfigSettings();

private:
    void setupUI();
    void setupRealTimePanel();
    void setupChartPanel();
    void setupDataTablePanel();
    void setupControlPanel();
    void setupConnections();
    
    void updateRealTimeDisplay();
    void updateCharts();
    void updateDataTable();
    void checkAlerts(const RealTimeData& data);
    
    void initializeCharts();
    void addChartData(const RealTimeData& data);
    void updateChartRange();
    
    QString formatValue(double value, const QString& unit, int precision = 2) const;
    QString formatTime(const QDateTime& time) const;
    QColor getStatusColor(int status) const;
    
    // UI组件
    QTabWidget* tabWidget;
    QSplitter* mainSplitter;
    
    // 实时数据面板
    QGroupBox* realTimeGroup;
    QLabel* positionXLabel;
    QLabel* positionYLabel;
    QLabel* positionZLabel;
    QLCDNumber* velocityLCD;
    QLCDNumber* pressureLCD;
    QLCDNumber* temperatureLCD;
    QLCDNumber* volumeLCD;
    QLabel* statusLabel;
    QProgressBar* progressBar;
    
    // 图表面板
    QGroupBox* chartGroup;
    QChartView* positionChartView;
    QChartView* velocityChartView;
    QChartView* pressureChartView;
    QChartView* temperatureChartView;
    
    QChart* positionChart;
    QChart* velocityChart;
    QChart* pressureChart;
    QChart* temperatureChart;
    
    QLineSeries* positionXSeries;
    QLineSeries* positionYSeries;
    QLineSeries* positionZSeries;
    QLineSeries* velocitySeries;
    QLineSeries* pressureSeries;
    QLineSeries* temperatureSeries;
    
    // 数据表格面板
    QGroupBox* dataTableGroup;
    QTableWidget* dataTableWidget;
    QPushButton* exportButton;
    QPushButton* clearButton;
    QLabel* dataCountLabel;
    
    // 控制面板
    QGroupBox* controlGroup;
    QPushButton* startButton;
    QPushButton* stopButton;
    QPushButton* pauseButton;
    QPushButton* configButton;
    QLabel* monitoringStatusLabel;
    
    // 数据成员
    QList<RealTimeData> historyData;
    RealTimeData currentData;
    MonitorConfig config;
    SerialWorker* serialWorker;
    
    QTimer* updateTimer;
    QMutex dataMutex;
    
    bool isMonitoring;
    bool isPaused;
    QDateTime startTime;
    
    // 图表数据
    QQueue<double> timeData;
    QQueue<double> positionXData;
    QQueue<double> positionYData;
    QQueue<double> positionZData;
    QQueue<double> velocityData;
    QQueue<double> pressureData;
    QQueue<double> temperatureData;
    
    static constexpr int MAX_CHART_POINTS = 500;
    static constexpr int TABLE_UPDATE_INTERVAL = 10; // 每10个数据点更新一次表格
}; 