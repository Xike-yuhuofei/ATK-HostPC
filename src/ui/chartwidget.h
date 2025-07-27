#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QSlider>
#include <QDateTimeEdit>
#include <QTimer>
#include <QSplitter>
#include <QtCharts/QChart>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QSplineSeries>
#include <QtCharts/QScatterSeries>
#include <QtCharts/QAreaSeries>
#include <QtCharts/QBarSeries>
#include <QtCharts/QBarSet>
#include <QtCharts/QPieSeries>
#include <QtCharts/QPieSlice>
#include <QtCharts/QValueAxis>
#include <QtCharts/QDateTimeAxis>
#include <QtCharts/QCategoryAxis>
#include <QtCharts/QLogValueAxis>
#include <QDateTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QMutex>
#include <QThread>
#include <QProgressBar>
#include <QTextEdit>

// 图表类型枚举
enum class ChartType {
    RealTimeMonitor = 0,    // 实时监控图表
    HistoryTrend = 1,       // 历史趋势图表
    QualityAnalysis = 2,    // 质量分析图表
    ProductionStats = 3,    // 生产统计图表
    AlarmAnalysis = 4,      // 报警分析图表
    PerformanceMonitor = 5, // 性能监控图表
    ProcessControl = 6,     // 工艺控制图表
    ComparisonAnalysis = 7  // 对比分析图表
};

// 图表数据结构
struct ChartData {
    QString name;           // 数据系列名称
    QDateTime timestamp;    // 时间戳
    double value;          // 数值
    QString unit;          // 单位
    QString category;      // 分类
    QColor color;          // 颜色
    bool isValid;          // 是否有效
    
    ChartData() : value(0.0), isValid(true) {}
    ChartData(const QString& n, const QDateTime& t, double v, const QString& u = "")
        : name(n), timestamp(t), value(v), unit(u), isValid(true) {}
};

// 图表配置结构
struct ChartConfig {
    ChartType type;         // 图表类型
    QString title;          // 标题
    QString xAxisTitle;     // X轴标题
    QString yAxisTitle;     // Y轴标题
    int maxDataPoints;      // 最大数据点数
    int updateInterval;     // 更新间隔(ms)
    bool autoScale;         // 自动缩放
    double minValue;        // 最小值
    double maxValue;        // 最大值
    bool showLegend;        // 显示图例
    bool showGrid;          // 显示网格
    bool enableAnimation;   // 启用动画
    QColor backgroundColor; // 背景色
    QColor gridColor;       // 网格颜色
    
    ChartConfig() 
        : type(ChartType::RealTimeMonitor), maxDataPoints(1000), updateInterval(1000)
        , autoScale(true), minValue(0), maxValue(100), showLegend(true)
        , showGrid(true), enableAnimation(true), backgroundColor(Qt::white)
        , gridColor(Qt::lightGray) {}
};

// 统计数据结构
struct StatisticsData {
    double average;         // 平均值
    double maximum;         // 最大值
    double minimum;         // 最小值
    double stdDeviation;    // 标准偏差
    double variance;        // 方差
    int count;             // 数据点数
    double sum;            // 总和
    double range;          // 范围
    QDateTime startTime;   // 开始时间
    QDateTime endTime;     // 结束时间
    
    StatisticsData() : average(0), maximum(0), minimum(0), stdDeviation(0)
                     , variance(0), count(0), sum(0), range(0) {}
};

class ChartWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ChartWidget(QWidget* parent = nullptr);
    ~ChartWidget();
    
    // 图表管理
    void addChart(ChartType type, const QString& title);
    void removeChart(ChartType type);
    void clearAllCharts();
    void setChartConfig(ChartType type, const ChartConfig& config);
    ChartConfig getChartConfig(ChartType type) const;
    
    // 数据管理
    void addDataPoint(ChartType type, const ChartData& data);
    void addDataSeries(ChartType type, const QString& seriesName, const QList<ChartData>& data);
    void updateDataPoint(ChartType type, const QString& seriesName, const ChartData& data);
    void clearChartData(ChartType type);
    void clearSeriesData(ChartType type, const QString& seriesName);
    
    // 图表操作
    void refreshChart(ChartType type);
    void refreshAllCharts();
    void exportChart(ChartType type, const QString& filePath);
    void printChart(ChartType type);
    void zoomChart(ChartType type, double factor);
    void resetZoom(ChartType type);
    
    // 数据分析
    StatisticsData calculateStatistics(ChartType type, const QString& seriesName);
    QList<ChartData> getChartData(ChartType type, const QString& seriesName);
    QStringList getSeriesNames(ChartType type);
    
    // 实时监控
    void startRealTimeMonitoring();
    void stopRealTimeMonitoring();
    void pauseRealTimeMonitoring();
    void resumeRealTimeMonitoring();
    bool isRealTimeMonitoring() const;
    
    // 历史数据分析
    void loadHistoryData(const QDateTime& startTime, const QDateTime& endTime);
    void analyzeHistoryTrend(const QString& parameter, const QDateTime& startTime, const QDateTime& endTime);
    void compareHistoryData(const QStringList& parameters, const QDateTime& startTime, const QDateTime& endTime);
    
    // 预测分析
    void enableTrendPrediction(bool enable);
    void setPredictionPeriod(int hours);
    QList<ChartData> predictTrend(const QString& seriesName, int futurePeriod);

public slots:
    void onDataReceived(const QJsonObject& data);
    void onParameterChanged(const QString& parameter, double value);
    void onAlarmTriggered(const QString& alarmType, const QString& message);
    void onProductionDataUpdated(const QJsonObject& data);
    void onQualityDataUpdated(const QJsonObject& data);
    void onPerformanceDataUpdated(const QJsonObject& data);

private slots:
    void onChartTypeChanged();
    void onTimeRangeChanged();
    void onSeriesSelectionChanged();
    void onConfigChanged();
    void onExportChart();
    void onPrintChart();
    void onZoomIn();
    void onZoomOut();
    void onResetZoom();
    void onShowStatistics();
    void onShowTrendAnalysis();
    void onShowComparison();
    void onRefreshData();
    void onUpdateTimer();
    void onAutoScaleToggled(bool enabled);
    void onAnimationToggled(bool enabled);
    void onLegendToggled(bool enabled);
    void onGridToggled(bool enabled);

signals:
    void chartDataChanged(ChartType type, const QString& seriesName);
    void chartConfigChanged(ChartType type);
    void statisticsCalculated(ChartType type, const StatisticsData& stats);
    void trendPredicted(const QString& seriesName, const QList<ChartData>& prediction);
    void exportCompleted(const QString& filePath);
    void analysisCompleted(const QString& analysisType, const QJsonObject& results);

private:
    void setupUI();
    void setupChartTabs();
    void setupControlPanel();
    void setupStatisticsPanel();
    void setupConnections();
    
    void createRealTimeChart();
    void createHistoryChart();
    void createQualityChart();
    void createProductionChart();
    void createAlarmChart();
    void createPerformanceChart();
    void createProcessChart();
    void createComparisonChart();
    
    void updateRealTimeChart();
    void updateHistoryChart();
    void updateQualityChart();
    void updateProductionChart();
    void updateAlarmChart();
    void updatePerformanceChart();
    void updateProcessChart();
    void updateComparisonChart();
    
    void configureChart(QChart* chart, const ChartConfig& config);
    void addSeriesToChart(QChart* chart, const QString& seriesName, const QList<ChartData>& data);
    void updateChartAxes(QChart* chart, const ChartConfig& config);
    void applyChartTheme(QChart* chart);
    
    void calculateSeriesStatistics(const QList<ChartData>& data, StatisticsData& stats);
    void performTrendAnalysis(const QList<ChartData>& data, QList<ChartData>& trend);
    void performRegression(const QList<ChartData>& data, QList<ChartData>& regression);
    void detectAnomalies(const QList<ChartData>& data, QList<int>& anomalies);
    
    // 缺少的函数声明
    void initializeChartConfigs();
    void createDefaultCharts();
    void updateChart(ChartType type);
    void updateStatisticsDisplay(ChartType type);
    void updateStatisticsLabels(const StatisticsData& stats);
    void showStatisticsDialog(const StatisticsData& stats, const QString& seriesName);
    void showComparisonDialog();
    void performTrendAnalysis(ChartType type, const QString& seriesName);
    
    QString formatValue(double value, const QString& unit) const;
    QString formatTime(const QDateTime& time) const;
    QColor generateSeriesColor(int index) const;
    
private:
    // UI组件
    QTabWidget* m_tabWidget;
    QSplitter* m_mainSplitter;
    
    // 图表视图
    QMap<ChartType, QChartView*> m_chartViews;
    QMap<ChartType, QChart*> m_charts;
    QMap<ChartType, ChartConfig> m_chartConfigs;
    
    // 控制面板
    QGroupBox* m_controlPanel;
    QComboBox* m_chartTypeCombo;
    QComboBox* m_timeRangeCombo;
    QComboBox* m_seriesCombo;
    QDateTimeEdit* m_startTimeEdit;
    QDateTimeEdit* m_endTimeEdit;
    QPushButton* m_refreshButton;
    QPushButton* m_exportButton;
    QPushButton* m_printButton;
    QPushButton* m_zoomInButton;
    QPushButton* m_zoomOutButton;
    QPushButton* m_resetZoomButton;
    QCheckBox* m_autoScaleCheckBox;
    QCheckBox* m_animationCheckBox;
    QCheckBox* m_legendCheckBox;
    QCheckBox* m_gridCheckBox;
    
    // 统计面板
    QGroupBox* m_statisticsPanel;
    QLabel* m_averageLabel;
    QLabel* m_maximumLabel;
    QLabel* m_minimumLabel;
    QLabel* m_stdDeviationLabel;
    QLabel* m_countLabel;
    QLabel* m_rangeLabel;
    QPushButton* m_showStatsButton;
    QPushButton* m_showTrendButton;
    QPushButton* m_showComparisonButton;
    QProgressBar* m_analysisProgress;
    QTextEdit* m_analysisResults;
    
    // 数据存储
    QMap<ChartType, QMap<QString, QList<ChartData>>> m_chartData;
    QMap<ChartType, StatisticsData> m_statisticsData;
    
    // 定时器和状态
    QTimer* m_updateTimer;
    QTimer* m_refreshTimer;
    bool m_isRealTimeMonitoring;
    bool m_isPaused;
    QDateTime m_lastUpdateTime;
    
    // 线程和互斥锁
    QThread* m_analysisThread;
    QMutex m_dataMutex;
    
    // 配置
    int m_maxDataPoints;
    int m_updateInterval;
    bool m_enableTrendPrediction;
    int m_predictionPeriod;
    QString m_exportDirectory;
    
    // 常量
    static const int DEFAULT_MAX_POINTS = 1000;
    static const int DEFAULT_UPDATE_INTERVAL = 1000;
    static const int MAX_SERIES_COUNT = 10;
    static const QStringList CHART_THEMES;
    static const QStringList TIME_RANGES;
}; 