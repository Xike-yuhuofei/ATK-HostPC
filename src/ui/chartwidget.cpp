#include "chartwidget.h"
#include "logger/logmanager.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QStandardPaths>
#include <QDir>
#include <QApplication>
#include <QStyle>
#include <QHeaderView>
#include <QSplitter>
#include <QPrinter>
#include <QPrintDialog>
#include <QPixmap>
#include <QBuffer>
#include <QJsonDocument>
#include <cmath>
#include <algorithm>

// 静态常量定义
const QStringList ChartWidget::CHART_THEMES = {"Light", "Dark", "Blue", "Brown", "Qt"};
const QStringList ChartWidget::TIME_RANGES = {"最近1小时", "最近6小时", "最近24小时", "最近7天", "最近30天", "自定义"};

ChartWidget::ChartWidget(QWidget* parent) 
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_mainSplitter(nullptr)
    , m_updateTimer(nullptr)
    , m_refreshTimer(nullptr)
    , m_isRealTimeMonitoring(false)
    , m_isPaused(false)
    , m_analysisThread(nullptr)
    , m_maxDataPoints(DEFAULT_MAX_POINTS)
    , m_updateInterval(DEFAULT_UPDATE_INTERVAL)
    , m_enableTrendPrediction(false)
    , m_predictionPeriod(24)
{
    // 设置导出目录
    m_exportDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/GlueDispenser/Charts";
    QDir().mkpath(m_exportDirectory);
    
    setupUI();
    setupConnections();
    
    // 创建定时器
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(m_updateInterval);
    connect(m_updateTimer, &QTimer::timeout, this, &ChartWidget::onUpdateTimer);
    
    m_refreshTimer = new QTimer(this);
    m_refreshTimer->setInterval(5000); // 5秒刷新一次
    connect(m_refreshTimer, &QTimer::timeout, this, &ChartWidget::onRefreshData);
    
    // 初始化图表配置
    initializeChartConfigs();
    
    // 创建默认图表
    createDefaultCharts();
    
    LogManager::getInstance()->info("图表组件已创建", "Chart");
}

ChartWidget::~ChartWidget()
{
    if (m_isRealTimeMonitoring) {
        stopRealTimeMonitoring();
    }
    
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    if (m_refreshTimer) {
        m_refreshTimer->stop();
    }
    
    // 清理图表
    clearAllCharts();
    
    LogManager::getInstance()->info("图表组件已销毁", "Chart");
}

void ChartWidget::setupUI()
{
    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 创建分割器
    m_mainSplitter = new QSplitter(Qt::Vertical);
    
    // 创建标签页
    m_tabWidget = new QTabWidget;
    m_mainSplitter->addWidget(m_tabWidget);
    
    // 创建控制面板
    setupControlPanel();
    m_mainSplitter->addWidget(m_controlPanel);
    
    // 创建统计面板
    setupStatisticsPanel();
    m_mainSplitter->addWidget(m_statisticsPanel);
    
    // 设置分割器比例
    m_mainSplitter->setSizes({600, 150, 150});
    
    mainLayout->addWidget(m_mainSplitter);
    setLayout(mainLayout);
}

void ChartWidget::setupControlPanel()
{
    m_controlPanel = new QGroupBox("图表控制");
    QGridLayout* layout = new QGridLayout(m_controlPanel);
    
    // 图表类型选择
    layout->addWidget(new QLabel("图表类型:"), 0, 0);
    m_chartTypeCombo = new QComboBox;
    m_chartTypeCombo->addItems({"实时监控", "历史趋势", "质量分析", "生产统计", 
                               "报警分析", "性能监控", "工艺控制", "对比分析"});
    layout->addWidget(m_chartTypeCombo, 0, 1);
    
    // 时间范围选择
    layout->addWidget(new QLabel("时间范围:"), 0, 2);
    m_timeRangeCombo = new QComboBox;
    m_timeRangeCombo->addItems(TIME_RANGES);
    layout->addWidget(m_timeRangeCombo, 0, 3);
    
    // 数据系列选择
    layout->addWidget(new QLabel("数据系列:"), 1, 0);
    m_seriesCombo = new QComboBox;
    layout->addWidget(m_seriesCombo, 1, 1);
    
    // 时间选择
    layout->addWidget(new QLabel("开始时间:"), 1, 2);
    m_startTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-1));
    m_startTimeEdit->setDisplayFormat("yyyy-MM-dd hh:mm");
    layout->addWidget(m_startTimeEdit, 1, 3);
    
    layout->addWidget(new QLabel("结束时间:"), 2, 0);
    m_endTimeEdit = new QDateTimeEdit(QDateTime::currentDateTime());
    m_endTimeEdit->setDisplayFormat("yyyy-MM-dd hh:mm");
    layout->addWidget(m_endTimeEdit, 2, 1);
    
    // 控制按钮
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    m_refreshButton = new QPushButton("刷新数据");
    m_refreshButton->setIcon(style()->standardIcon(QStyle::SP_BrowserReload));
    buttonLayout->addWidget(m_refreshButton);
    
    m_exportButton = new QPushButton("导出图表");
    m_exportButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    buttonLayout->addWidget(m_exportButton);
    
    m_printButton = new QPushButton("打印图表");
    m_printButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    buttonLayout->addWidget(m_printButton);
    
    m_zoomInButton = new QPushButton("放大");
    m_zoomInButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    buttonLayout->addWidget(m_zoomInButton);
    
    m_zoomOutButton = new QPushButton("缩小");
    m_zoomOutButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogListView));
    buttonLayout->addWidget(m_zoomOutButton);
    
    m_resetZoomButton = new QPushButton("重置缩放");
    m_resetZoomButton->setIcon(style()->standardIcon(QStyle::SP_DialogResetButton));
    buttonLayout->addWidget(m_resetZoomButton);
    
    buttonLayout->addStretch();
    layout->addLayout(buttonLayout, 2, 2, 1, 2);
    
    // 显示选项
    QHBoxLayout* optionLayout = new QHBoxLayout;
    
    m_autoScaleCheckBox = new QCheckBox("自动缩放");
    m_autoScaleCheckBox->setChecked(true);
    optionLayout->addWidget(m_autoScaleCheckBox);
    
    m_animationCheckBox = new QCheckBox("启用动画");
    m_animationCheckBox->setChecked(true);
    optionLayout->addWidget(m_animationCheckBox);
    
    m_legendCheckBox = new QCheckBox("显示图例");
    m_legendCheckBox->setChecked(true);
    optionLayout->addWidget(m_legendCheckBox);
    
    m_gridCheckBox = new QCheckBox("显示网格");
    m_gridCheckBox->setChecked(true);
    optionLayout->addWidget(m_gridCheckBox);
    
    optionLayout->addStretch();
    layout->addLayout(optionLayout, 3, 0, 1, 4);
}

void ChartWidget::setupStatisticsPanel()
{
    m_statisticsPanel = new QGroupBox("统计信息");
    QHBoxLayout* layout = new QHBoxLayout(m_statisticsPanel);
    
    // 统计数据显示
    QGridLayout* statsLayout = new QGridLayout;
    
    statsLayout->addWidget(new QLabel("平均值:"), 0, 0);
    m_averageLabel = new QLabel("0.00");
    m_averageLabel->setStyleSheet("QLabel { font-weight: bold; color: blue; }");
    statsLayout->addWidget(m_averageLabel, 0, 1);
    
    statsLayout->addWidget(new QLabel("最大值:"), 0, 2);
    m_maximumLabel = new QLabel("0.00");
    m_maximumLabel->setStyleSheet("QLabel { font-weight: bold; color: red; }");
    statsLayout->addWidget(m_maximumLabel, 0, 3);
    
    statsLayout->addWidget(new QLabel("最小值:"), 0, 4);
    m_minimumLabel = new QLabel("0.00");
    m_minimumLabel->setStyleSheet("QLabel { font-weight: bold; color: green; }");
    statsLayout->addWidget(m_minimumLabel, 0, 5);
    
    statsLayout->addWidget(new QLabel("标准偏差:"), 1, 0);
    m_stdDeviationLabel = new QLabel("0.00");
    statsLayout->addWidget(m_stdDeviationLabel, 1, 1);
    
    statsLayout->addWidget(new QLabel("数据点数:"), 1, 2);
    m_countLabel = new QLabel("0");
    statsLayout->addWidget(m_countLabel, 1, 3);
    
    statsLayout->addWidget(new QLabel("数据范围:"), 1, 4);
    m_rangeLabel = new QLabel("0.00");
    statsLayout->addWidget(m_rangeLabel, 1, 5);
    
    layout->addLayout(statsLayout);
    
    // 分析按钮
    QVBoxLayout* buttonLayout = new QVBoxLayout;
    
    m_showStatsButton = new QPushButton("详细统计");
    buttonLayout->addWidget(m_showStatsButton);
    
    m_showTrendButton = new QPushButton("趋势分析");
    buttonLayout->addWidget(m_showTrendButton);
    
    m_showComparisonButton = new QPushButton("对比分析");
    buttonLayout->addWidget(m_showComparisonButton);
    
    layout->addLayout(buttonLayout);
    
    // 分析进度和结果
    QVBoxLayout* analysisLayout = new QVBoxLayout;
    
    m_analysisProgress = new QProgressBar;
    m_analysisProgress->setVisible(false);
    analysisLayout->addWidget(m_analysisProgress);
    
    m_analysisResults = new QTextEdit;
    m_analysisResults->setMaximumHeight(80);
    m_analysisResults->setReadOnly(true);
    analysisLayout->addWidget(m_analysisResults);
    
    layout->addLayout(analysisLayout);
}

void ChartWidget::setupConnections()
{
    // 控制面板连接
    connect(m_chartTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChartWidget::onChartTypeChanged);
    connect(m_timeRangeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChartWidget::onTimeRangeChanged);
    connect(m_seriesCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ChartWidget::onSeriesSelectionChanged);
    connect(m_startTimeEdit, &QDateTimeEdit::dateTimeChanged,
            this, &ChartWidget::onTimeRangeChanged);
    connect(m_endTimeEdit, &QDateTimeEdit::dateTimeChanged,
            this, &ChartWidget::onTimeRangeChanged);
    
    // 按钮连接
    connect(m_refreshButton, &QPushButton::clicked, this, &ChartWidget::onRefreshData);
    connect(m_exportButton, &QPushButton::clicked, this, &ChartWidget::onExportChart);
    connect(m_printButton, &QPushButton::clicked, this, &ChartWidget::onPrintChart);
    connect(m_zoomInButton, &QPushButton::clicked, this, &ChartWidget::onZoomIn);
    connect(m_zoomOutButton, &QPushButton::clicked, this, &ChartWidget::onZoomOut);
    connect(m_resetZoomButton, &QPushButton::clicked, this, &ChartWidget::onResetZoom);
    
    // 选项连接
    connect(m_autoScaleCheckBox, &QCheckBox::toggled, this, &ChartWidget::onAutoScaleToggled);
    connect(m_animationCheckBox, &QCheckBox::toggled, this, &ChartWidget::onAnimationToggled);
    connect(m_legendCheckBox, &QCheckBox::toggled, this, &ChartWidget::onLegendToggled);
    connect(m_gridCheckBox, &QCheckBox::toggled, this, &ChartWidget::onGridToggled);
    
    // 统计面板连接
    connect(m_showStatsButton, &QPushButton::clicked, this, &ChartWidget::onShowStatistics);
    connect(m_showTrendButton, &QPushButton::clicked, this, &ChartWidget::onShowTrendAnalysis);
    connect(m_showComparisonButton, &QPushButton::clicked, this, &ChartWidget::onShowComparison);
}

void ChartWidget::initializeChartConfigs()
{
    // 实时监控图表配置
    ChartConfig realTimeConfig;
    realTimeConfig.type = ChartType::RealTimeMonitor;
    realTimeConfig.title = "实时数据监控";
    realTimeConfig.xAxisTitle = "时间";
    realTimeConfig.yAxisTitle = "数值";
    realTimeConfig.maxDataPoints = 500;
    realTimeConfig.updateInterval = 1000;
    realTimeConfig.autoScale = true;
    realTimeConfig.showLegend = true;
    realTimeConfig.showGrid = true;
    realTimeConfig.enableAnimation = false; // 实时图表不使用动画
    m_chartConfigs[ChartType::RealTimeMonitor] = realTimeConfig;
    
    // 历史趋势图表配置
    ChartConfig historyConfig;
    historyConfig.type = ChartType::HistoryTrend;
    historyConfig.title = "历史趋势分析";
    historyConfig.xAxisTitle = "时间";
    historyConfig.yAxisTitle = "数值";
    historyConfig.maxDataPoints = 2000;
    historyConfig.updateInterval = 5000;
    historyConfig.autoScale = true;
    historyConfig.showLegend = true;
    historyConfig.showGrid = true;
    historyConfig.enableAnimation = true;
    m_chartConfigs[ChartType::HistoryTrend] = historyConfig;
    
    // 质量分析图表配置
    ChartConfig qualityConfig;
    qualityConfig.type = ChartType::QualityAnalysis;
    qualityConfig.title = "质量分析统计";
    qualityConfig.xAxisTitle = "时间";
    qualityConfig.yAxisTitle = "质量指标";
    qualityConfig.maxDataPoints = 1000;
    qualityConfig.updateInterval = 10000;
    qualityConfig.autoScale = true;
    qualityConfig.showLegend = true;
    qualityConfig.showGrid = true;
    qualityConfig.enableAnimation = true;
    m_chartConfigs[ChartType::QualityAnalysis] = qualityConfig;
    
    // 生产统计图表配置
    ChartConfig productionConfig;
    productionConfig.type = ChartType::ProductionStats;
    productionConfig.title = "生产统计分析";
    productionConfig.xAxisTitle = "时间";
    productionConfig.yAxisTitle = "产量";
    productionConfig.maxDataPoints = 1000;
    productionConfig.updateInterval = 30000;
    productionConfig.autoScale = true;
    productionConfig.showLegend = true;
    productionConfig.showGrid = true;
    productionConfig.enableAnimation = true;
    m_chartConfigs[ChartType::ProductionStats] = productionConfig;
    
    // 报警分析图表配置
    ChartConfig alarmConfig;
    alarmConfig.type = ChartType::AlarmAnalysis;
    alarmConfig.title = "报警分析统计";
    alarmConfig.xAxisTitle = "时间";
    alarmConfig.yAxisTitle = "报警次数";
    alarmConfig.maxDataPoints = 1000;
    alarmConfig.updateInterval = 60000;
    alarmConfig.autoScale = true;
    alarmConfig.showLegend = true;
    alarmConfig.showGrid = true;
    alarmConfig.enableAnimation = true;
    m_chartConfigs[ChartType::AlarmAnalysis] = alarmConfig;
    
    // 性能监控图表配置
    ChartConfig performanceConfig;
    performanceConfig.type = ChartType::PerformanceMonitor;
    performanceConfig.title = "性能监控分析";
    performanceConfig.xAxisTitle = "时间";
    performanceConfig.yAxisTitle = "性能指标";
    performanceConfig.maxDataPoints = 1000;
    performanceConfig.updateInterval = 5000;
    performanceConfig.autoScale = true;
    performanceConfig.showLegend = true;
    performanceConfig.showGrid = true;
    performanceConfig.enableAnimation = false;
    m_chartConfigs[ChartType::PerformanceMonitor] = performanceConfig;
    
    // 工艺控制图表配置
    ChartConfig processConfig;
    processConfig.type = ChartType::ProcessControl;
    processConfig.title = "工艺控制监控";
    processConfig.xAxisTitle = "时间";
    processConfig.yAxisTitle = "工艺参数";
    processConfig.maxDataPoints = 1000;
    processConfig.updateInterval = 2000;
    processConfig.autoScale = true;
    processConfig.showLegend = true;
    processConfig.showGrid = true;
    processConfig.enableAnimation = false;
    m_chartConfigs[ChartType::ProcessControl] = processConfig;
    
    // 对比分析图表配置
    ChartConfig comparisonConfig;
    comparisonConfig.type = ChartType::ComparisonAnalysis;
    comparisonConfig.title = "对比分析";
    comparisonConfig.xAxisTitle = "时间";
    comparisonConfig.yAxisTitle = "对比数值";
    comparisonConfig.maxDataPoints = 1000;
    comparisonConfig.updateInterval = 10000;
    comparisonConfig.autoScale = true;
    comparisonConfig.showLegend = true;
    comparisonConfig.showGrid = true;
    comparisonConfig.enableAnimation = true;
    m_chartConfigs[ChartType::ComparisonAnalysis] = comparisonConfig;
}

void ChartWidget::createDefaultCharts()
{
    // 创建所有类型的图表
    createRealTimeChart();
    createHistoryChart();
    createQualityChart();
    createProductionChart();
    createAlarmChart();
    createPerformanceChart();
    createProcessChart();
    createComparisonChart();
}

void ChartWidget::createRealTimeChart()
{
    ChartType type = ChartType::RealTimeMonitor;
    const ChartConfig& config = m_chartConfigs[type];
    
    // 创建图表
    QChart* chart = new QChart();
    chart->setTitle(config.title);
    chart->setAnimationOptions(config.enableAnimation ? QChart::SeriesAnimations : QChart::NoAnimation);
    
    // 创建图表视图
    QChartView* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    
    // 添加到标签页
    m_tabWidget->addTab(chartView, "实时监控");
    
    // 存储引用
    m_charts[type] = chart;
    m_chartViews[type] = chartView;
    
    // 配置图表
    configureChart(chart, config);
    
    LogManager::getInstance()->info("创建实时监控图表", "Chart");
}

void ChartWidget::createHistoryChart()
{
    ChartType type = ChartType::HistoryTrend;
    const ChartConfig& config = m_chartConfigs[type];
    
    QChart* chart = new QChart();
    chart->setTitle(config.title);
    chart->setAnimationOptions(config.enableAnimation ? QChart::SeriesAnimations : QChart::NoAnimation);
    
    QChartView* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    
    m_tabWidget->addTab(chartView, "历史趋势");
    
    m_charts[type] = chart;
    m_chartViews[type] = chartView;
    
    configureChart(chart, config);
    
    LogManager::getInstance()->info("创建历史趋势图表", "Chart");
}

void ChartWidget::createQualityChart()
{
    ChartType type = ChartType::QualityAnalysis;
    const ChartConfig& config = m_chartConfigs[type];
    
    QChart* chart = new QChart();
    chart->setTitle(config.title);
    chart->setAnimationOptions(config.enableAnimation ? QChart::SeriesAnimations : QChart::NoAnimation);
    
    QChartView* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    
    m_tabWidget->addTab(chartView, "质量分析");
    
    m_charts[type] = chart;
    m_chartViews[type] = chartView;
    
    configureChart(chart, config);
    
    LogManager::getInstance()->info("创建质量分析图表", "Chart");
}

void ChartWidget::createProductionChart()
{
    ChartType type = ChartType::ProductionStats;
    const ChartConfig& config = m_chartConfigs[type];
    
    QChart* chart = new QChart();
    chart->setTitle(config.title);
    chart->setAnimationOptions(config.enableAnimation ? QChart::SeriesAnimations : QChart::NoAnimation);
    
    QChartView* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    
    m_tabWidget->addTab(chartView, "生产统计");
    
    m_charts[type] = chart;
    m_chartViews[type] = chartView;
    
    configureChart(chart, config);
    
    LogManager::getInstance()->info("创建生产统计图表", "Chart");
}

void ChartWidget::createAlarmChart()
{
    ChartType type = ChartType::AlarmAnalysis;
    const ChartConfig& config = m_chartConfigs[type];
    
    QChart* chart = new QChart();
    chart->setTitle(config.title);
    chart->setAnimationOptions(config.enableAnimation ? QChart::SeriesAnimations : QChart::NoAnimation);
    
    QChartView* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    
    m_tabWidget->addTab(chartView, "报警分析");
    
    m_charts[type] = chart;
    m_chartViews[type] = chartView;
    
    configureChart(chart, config);
    
    LogManager::getInstance()->info("创建报警分析图表", "Chart");
}

void ChartWidget::createPerformanceChart()
{
    ChartType type = ChartType::PerformanceMonitor;
    const ChartConfig& config = m_chartConfigs[type];
    
    QChart* chart = new QChart();
    chart->setTitle(config.title);
    chart->setAnimationOptions(config.enableAnimation ? QChart::SeriesAnimations : QChart::NoAnimation);
    
    QChartView* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    
    m_tabWidget->addTab(chartView, "性能监控");
    
    m_charts[type] = chart;
    m_chartViews[type] = chartView;
    
    configureChart(chart, config);
    
    LogManager::getInstance()->info("创建性能监控图表", "Chart");
}

void ChartWidget::createProcessChart()
{
    ChartType type = ChartType::ProcessControl;
    const ChartConfig& config = m_chartConfigs[type];
    
    QChart* chart = new QChart();
    chart->setTitle(config.title);
    chart->setAnimationOptions(config.enableAnimation ? QChart::SeriesAnimations : QChart::NoAnimation);
    
    QChartView* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    
    m_tabWidget->addTab(chartView, "工艺控制");
    
    m_charts[type] = chart;
    m_chartViews[type] = chartView;
    
    configureChart(chart, config);
    
    LogManager::getInstance()->info("创建工艺控制图表", "Chart");
}

void ChartWidget::createComparisonChart()
{
    ChartType type = ChartType::ComparisonAnalysis;
    const ChartConfig& config = m_chartConfigs[type];
    
    QChart* chart = new QChart();
    chart->setTitle(config.title);
    chart->setAnimationOptions(config.enableAnimation ? QChart::SeriesAnimations : QChart::NoAnimation);
    
    QChartView* chartView = new QChartView(chart);
    chartView->setRenderHint(QPainter::Antialiasing);
    
    m_tabWidget->addTab(chartView, "对比分析");
    
    m_charts[type] = chart;
    m_chartViews[type] = chartView;
    
    configureChart(chart, config);
    
    LogManager::getInstance()->info("创建对比分析图表", "Chart");
}

void ChartWidget::configureChart(QChart* chart, const ChartConfig& config)
{
    if (!chart) return;
    
    // 设置背景色
    chart->setBackgroundBrush(QBrush(config.backgroundColor));
    
    // 设置图例
    chart->legend()->setVisible(config.showLegend);
    chart->legend()->setAlignment(Qt::AlignBottom);
    
    // 应用主题
    applyChartTheme(chart);
}

void ChartWidget::applyChartTheme(QChart* chart)
{
    if (!chart) return;
    
    // 应用默认主题
    chart->setTheme(QChart::ChartThemeLight);
    
    // 自定义样式
    chart->setTitleFont(QFont("Arial", 12, QFont::Bold));
    chart->setBackgroundRoundness(0);
    chart->setMargins(QMargins(10, 10, 10, 10));
}

// 数据管理功能
void ChartWidget::addDataPoint(ChartType type, const ChartData& data)
{
    QMutexLocker locker(&m_dataMutex);
    
    if (!m_chartData.contains(type)) {
        m_chartData[type] = QMap<QString, QList<ChartData>>();
    }
    
    if (!m_chartData[type].contains(data.name)) {
        m_chartData[type][data.name] = QList<ChartData>();
    }
    
    // 添加数据点
    m_chartData[type][data.name].append(data);
    
    // 限制数据点数量
    const ChartConfig& config = m_chartConfigs[type];
    while (m_chartData[type][data.name].size() > config.maxDataPoints) {
        m_chartData[type][data.name].removeFirst();
    }
    
    // 更新图表
    updateChart(type);
    
    emit chartDataChanged(type, data.name);
}

void ChartWidget::addDataSeries(ChartType type, const QString& seriesName, const QList<ChartData>& data)
{
    QMutexLocker locker(&m_dataMutex);
    
    if (!m_chartData.contains(type)) {
        m_chartData[type] = QMap<QString, QList<ChartData>>();
    }
    
    m_chartData[type][seriesName] = data;
    
    // 限制数据点数量
    const ChartConfig& config = m_chartConfigs[type];
    while (m_chartData[type][seriesName].size() > config.maxDataPoints) {
        m_chartData[type][seriesName].removeFirst();
    }
    
    // 更新图表
    updateChart(type);
    
    emit chartDataChanged(type, seriesName);
}

void ChartWidget::updateChart(ChartType type)
{
    if (!m_charts.contains(type)) return;
    
    QChart* chart = m_charts[type];
    if (!chart) return;
    
    // 根据图表类型更新
    switch (type) {
        case ChartType::RealTimeMonitor:
            updateRealTimeChart();
            break;
        case ChartType::HistoryTrend:
            updateHistoryChart();
            break;
        case ChartType::QualityAnalysis:
            updateQualityChart();
            break;
        case ChartType::ProductionStats:
            updateProductionChart();
            break;
        case ChartType::AlarmAnalysis:
            updateAlarmChart();
            break;
        case ChartType::PerformanceMonitor:
            updatePerformanceChart();
            break;
        case ChartType::ProcessControl:
            updateProcessChart();
            break;
        case ChartType::ComparisonAnalysis:
            updateComparisonChart();
            break;
    }
}

void ChartWidget::updateRealTimeChart()
{
    ChartType type = ChartType::RealTimeMonitor;
    QChart* chart = m_charts[type];
    if (!chart) return;
    
    // 清除现有系列
    chart->removeAllSeries();
    
    // 添加数据系列
    if (m_chartData.contains(type)) {
        int seriesIndex = 0;
        for (auto it = m_chartData[type].begin(); it != m_chartData[type].end(); ++it) {
            const QString& seriesName = it.key();
            const QList<ChartData>& data = it.value();
            
            if (data.isEmpty()) continue;
            
            QLineSeries* series = new QLineSeries();
            series->setName(seriesName);
            series->setPen(QPen(generateSeriesColor(seriesIndex), 2));
            
            for (const ChartData& point : data) {
                series->append(point.timestamp.toMSecsSinceEpoch(), point.value);
            }
            
            chart->addSeries(series);
            seriesIndex++;
        }
    }
    
    // 更新坐标轴
    updateChartAxes(chart, m_chartConfigs[type]);
}

void ChartWidget::updateHistoryChart()
{
    ChartType type = ChartType::HistoryTrend;
    QChart* chart = m_charts[type];
    if (!chart) return;
    
    // 清除现有系列
    chart->removeAllSeries();
    
    // 添加数据系列
    if (m_chartData.contains(type)) {
        int seriesIndex = 0;
        for (auto it = m_chartData[type].begin(); it != m_chartData[type].end(); ++it) {
            const QString& seriesName = it.key();
            const QList<ChartData>& data = it.value();
            
            if (data.isEmpty()) continue;
            
            QSplineSeries* series = new QSplineSeries();
            series->setName(seriesName);
            series->setPen(QPen(generateSeriesColor(seriesIndex), 2));
            
            for (const ChartData& point : data) {
                series->append(point.timestamp.toMSecsSinceEpoch(), point.value);
            }
            
            chart->addSeries(series);
            seriesIndex++;
        }
    }
    
    // 更新坐标轴
    updateChartAxes(chart, m_chartConfigs[type]);
}

void ChartWidget::updateQualityChart()
{
    ChartType type = ChartType::QualityAnalysis;
    QChart* chart = m_charts[type];
    if (!chart) return;
    
    // 清除现有系列
    chart->removeAllSeries();
    
    // 添加数据系列
    if (m_chartData.contains(type)) {
        int seriesIndex = 0;
        for (auto it = m_chartData[type].begin(); it != m_chartData[type].end(); ++it) {
            const QString& seriesName = it.key();
            const QList<ChartData>& data = it.value();
            
            if (data.isEmpty()) continue;
            
            QScatterSeries* series = new QScatterSeries();
            series->setName(seriesName);
            series->setColor(generateSeriesColor(seriesIndex));
            series->setMarkerSize(8);
            
            for (const ChartData& point : data) {
                series->append(point.timestamp.toMSecsSinceEpoch(), point.value);
            }
            
            chart->addSeries(series);
            seriesIndex++;
        }
    }
    
    // 更新坐标轴
    updateChartAxes(chart, m_chartConfigs[type]);
}

void ChartWidget::updateProductionChart()
{
    ChartType type = ChartType::ProductionStats;
    QChart* chart = m_charts[type];
    if (!chart) return;
    
    // 清除现有系列
    chart->removeAllSeries();
    
    // 创建柱状图系列
    QBarSeries* barSeries = new QBarSeries();
    
    if (m_chartData.contains(type)) {
        for (auto it = m_chartData[type].begin(); it != m_chartData[type].end(); ++it) {
            const QString& seriesName = it.key();
            const QList<ChartData>& data = it.value();
            
            if (data.isEmpty()) continue;
            
            QBarSet* barSet = new QBarSet(seriesName);
            for (const ChartData& point : data) {
                *barSet << point.value;
            }
            
            barSeries->append(barSet);
        }
    }
    
    chart->addSeries(barSeries);
    
    // 更新坐标轴
    updateChartAxes(chart, m_chartConfigs[type]);
}

void ChartWidget::updateAlarmChart()
{
    ChartType type = ChartType::AlarmAnalysis;
    QChart* chart = m_charts[type];
    if (!chart) return;
    
    // 清除现有系列
    chart->removeAllSeries();
    
    // 添加数据系列
    if (m_chartData.contains(type)) {
        int seriesIndex = 0;
        for (auto it = m_chartData[type].begin(); it != m_chartData[type].end(); ++it) {
            const QString& seriesName = it.key();
            const QList<ChartData>& data = it.value();
            
            if (data.isEmpty()) continue;
            
            QLineSeries* series = new QLineSeries();
            series->setName(seriesName);
            series->setPen(QPen(generateSeriesColor(seriesIndex), 2));
            
            for (const ChartData& point : data) {
                series->append(point.timestamp.toMSecsSinceEpoch(), point.value);
            }
            
            chart->addSeries(series);
            seriesIndex++;
        }
    }
    
    // 更新坐标轴
    updateChartAxes(chart, m_chartConfigs[type]);
}

void ChartWidget::updatePerformanceChart()
{
    ChartType type = ChartType::PerformanceMonitor;
    QChart* chart = m_charts[type];
    if (!chart) return;
    
    // 清除现有系列
    chart->removeAllSeries();
    
    // 添加数据系列
    if (m_chartData.contains(type)) {
        int seriesIndex = 0;
        for (auto it = m_chartData[type].begin(); it != m_chartData[type].end(); ++it) {
            const QString& seriesName = it.key();
            const QList<ChartData>& data = it.value();
            
            if (data.isEmpty()) continue;
            
            QAreaSeries* series = new QAreaSeries();
            series->setName(seriesName);
            
            QLineSeries* upperSeries = new QLineSeries();
            QLineSeries* lowerSeries = new QLineSeries();
            
            for (const ChartData& point : data) {
                upperSeries->append(point.timestamp.toMSecsSinceEpoch(), point.value);
                lowerSeries->append(point.timestamp.toMSecsSinceEpoch(), 0);
            }
            
            series->setUpperSeries(upperSeries);
            series->setLowerSeries(lowerSeries);
            series->setColor(generateSeriesColor(seriesIndex));
            
            chart->addSeries(series);
            seriesIndex++;
        }
    }
    
    // 更新坐标轴
    updateChartAxes(chart, m_chartConfigs[type]);
}

void ChartWidget::updateProcessChart()
{
    ChartType type = ChartType::ProcessControl;
    QChart* chart = m_charts[type];
    if (!chart) return;
    
    // 清除现有系列
    chart->removeAllSeries();
    
    // 添加数据系列
    if (m_chartData.contains(type)) {
        int seriesIndex = 0;
        for (auto it = m_chartData[type].begin(); it != m_chartData[type].end(); ++it) {
            const QString& seriesName = it.key();
            const QList<ChartData>& data = it.value();
            
            if (data.isEmpty()) continue;
            
            QLineSeries* series = new QLineSeries();
            series->setName(seriesName);
            series->setPen(QPen(generateSeriesColor(seriesIndex), 2));
            
            for (const ChartData& point : data) {
                series->append(point.timestamp.toMSecsSinceEpoch(), point.value);
            }
            
            chart->addSeries(series);
            seriesIndex++;
        }
    }
    
    // 更新坐标轴
    updateChartAxes(chart, m_chartConfigs[type]);
}

void ChartWidget::updateComparisonChart()
{
    ChartType type = ChartType::ComparisonAnalysis;
    QChart* chart = m_charts[type];
    if (!chart) return;
    
    // 清除现有系列
    chart->removeAllSeries();
    
    // 添加数据系列
    if (m_chartData.contains(type)) {
        int seriesIndex = 0;
        for (auto it = m_chartData[type].begin(); it != m_chartData[type].end(); ++it) {
            const QString& seriesName = it.key();
            const QList<ChartData>& data = it.value();
            
            if (data.isEmpty()) continue;
            
            QSplineSeries* series = new QSplineSeries();
            series->setName(seriesName);
            series->setPen(QPen(generateSeriesColor(seriesIndex), 2));
            
            for (const ChartData& point : data) {
                series->append(point.timestamp.toMSecsSinceEpoch(), point.value);
            }
            
            chart->addSeries(series);
            seriesIndex++;
        }
    }
    
    // 更新坐标轴
    updateChartAxes(chart, m_chartConfigs[type]);
}

void ChartWidget::updateChartAxes(QChart* chart, const ChartConfig& config)
{
    if (!chart) return;
    
    // 移除现有坐标轴
    for (QAbstractAxis* axis : chart->axes()) {
        chart->removeAxis(axis);
        delete axis;
    }
    
    // 创建X轴（时间轴）
    QDateTimeAxis* xAxis = new QDateTimeAxis;
    xAxis->setTitleText(config.xAxisTitle);
    xAxis->setFormat("hh:mm:ss");
    xAxis->setTickCount(10);
    chart->addAxis(xAxis, Qt::AlignBottom);
    
    // 创建Y轴（数值轴）
    QValueAxis* yAxis = new QValueAxis;
    yAxis->setTitleText(config.yAxisTitle);
    yAxis->setLabelFormat("%.2f");
    if (config.autoScale) {
        yAxis->applyNiceNumbers();
    } else {
        yAxis->setRange(config.minValue, config.maxValue);
    }
    chart->addAxis(yAxis, Qt::AlignLeft);
    
    // 将系列附加到坐标轴
    for (QAbstractSeries* series : chart->series()) {
        series->attachAxis(xAxis);
        series->attachAxis(yAxis);
    }
    
    // 设置网格
    if (config.showGrid) {
        xAxis->setGridLineVisible(true);
        yAxis->setGridLineVisible(true);
        xAxis->setGridLineColor(config.gridColor);
        yAxis->setGridLineColor(config.gridColor);
    } else {
        xAxis->setGridLineVisible(false);
        yAxis->setGridLineVisible(false);
    }
}

QColor ChartWidget::generateSeriesColor(int index) const
{
    // 预定义颜色列表
    static const QList<QColor> colors = {
        QColor(31, 119, 180),   // 蓝色
        QColor(255, 127, 14),   // 橙色
        QColor(44, 160, 44),    // 绿色
        QColor(214, 39, 40),    // 红色
        QColor(148, 103, 189),  // 紫色
        QColor(140, 86, 75),    // 棕色
        QColor(227, 119, 194),  // 粉色
        QColor(127, 127, 127),  // 灰色
        QColor(188, 189, 34),   // 黄绿色
        QColor(23, 190, 207)    // 青色
    };
    
    return colors[index % colors.size()];
}

QString ChartWidget::formatValue(double value, const QString& unit) const
{
    if (unit.isEmpty()) {
        return QString::number(value, 'f', 2);
    }
    return QString("%1 %2").arg(value, 0, 'f', 2).arg(unit);
}

QString ChartWidget::formatTime(const QDateTime& time) const
{
    return time.toString("yyyy-MM-dd hh:mm:ss");
}

// 实时监控功能
void ChartWidget::startRealTimeMonitoring()
{
    if (m_isRealTimeMonitoring) return;
    
    m_isRealTimeMonitoring = true;
    m_isPaused = false;
    
    m_updateTimer->start();
    m_refreshTimer->start();
    
    LogManager::getInstance()->info("开始实时监控", "Chart");
}

void ChartWidget::stopRealTimeMonitoring()
{
    if (!m_isRealTimeMonitoring) return;
    
    m_isRealTimeMonitoring = false;
    m_isPaused = false;
    
    m_updateTimer->stop();
    m_refreshTimer->stop();
    
    LogManager::getInstance()->info("停止实时监控", "Chart");
}

void ChartWidget::pauseRealTimeMonitoring()
{
    if (!m_isRealTimeMonitoring || m_isPaused) return;
    
    m_isPaused = true;
    m_updateTimer->stop();
    
    LogManager::getInstance()->info("暂停实时监控", "Chart");
}

void ChartWidget::resumeRealTimeMonitoring()
{
    if (!m_isRealTimeMonitoring || !m_isPaused) return;
    
    m_isPaused = false;
    m_updateTimer->start();
    
    LogManager::getInstance()->info("恢复实时监控", "Chart");
}

bool ChartWidget::isRealTimeMonitoring() const
{
    return m_isRealTimeMonitoring;
}

// 统计分析功能
StatisticsData ChartWidget::calculateStatistics(ChartType type, const QString& seriesName)
{
    StatisticsData stats;
    
    if (!m_chartData.contains(type) || !m_chartData[type].contains(seriesName)) {
        return stats;
    }
    
    const QList<ChartData>& data = m_chartData[type][seriesName];
    calculateSeriesStatistics(data, stats);
    
    return stats;
}

void ChartWidget::calculateSeriesStatistics(const QList<ChartData>& data, StatisticsData& stats)
{
    if (data.isEmpty()) return;
    
    stats.count = data.size();
    stats.sum = 0;
    stats.minimum = data.first().value;
    stats.maximum = data.first().value;
    stats.startTime = data.first().timestamp;
    stats.endTime = data.last().timestamp;
    
    // 计算基本统计量
    for (const ChartData& point : data) {
        stats.sum += point.value;
        stats.minimum = std::min(stats.minimum, point.value);
        stats.maximum = std::max(stats.maximum, point.value);
    }
    
    stats.average = stats.sum / stats.count;
    stats.range = stats.maximum - stats.minimum;
    
    // 计算方差和标准偏差
    double variance = 0;
    for (const ChartData& point : data) {
        double diff = point.value - stats.average;
        variance += diff * diff;
    }
    stats.variance = variance / stats.count;
    stats.stdDeviation = std::sqrt(stats.variance);
}

// 槽函数实现
void ChartWidget::onChartTypeChanged()
{
    int index = m_chartTypeCombo->currentIndex();
    ChartType type = static_cast<ChartType>(index);
    
    // 切换到对应的标签页
    m_tabWidget->setCurrentIndex(index);
    
    // 更新数据系列下拉框
    m_seriesCombo->clear();
    if (m_chartData.contains(type)) {
        m_seriesCombo->addItems(m_chartData[type].keys());
    }
    
    // 更新统计信息
    updateStatisticsDisplay(type);
}

void ChartWidget::onTimeRangeChanged()
{
    int rangeIndex = m_timeRangeCombo->currentIndex();
    QDateTime endTime = QDateTime::currentDateTime();
    QDateTime startTime;
    
    switch (rangeIndex) {
        case 0: // 最近1小时
            startTime = endTime.addSecs(-3600);
            break;
        case 1: // 最近6小时
            startTime = endTime.addSecs(-21600);
            break;
        case 2: // 最近24小时
            startTime = endTime.addDays(-1);
            break;
        case 3: // 最近7天
            startTime = endTime.addDays(-7);
            break;
        case 4: // 最近30天
            startTime = endTime.addDays(-30);
            break;
        case 5: // 自定义
            startTime = m_startTimeEdit->dateTime();
            endTime = m_endTimeEdit->dateTime();
            break;
        default:
            startTime = endTime.addDays(-1);
            break;
    }
    
    // 更新时间选择控件
    if (rangeIndex != 5) { // 非自定义模式
        m_startTimeEdit->setDateTime(startTime);
        m_endTimeEdit->setDateTime(endTime);
    }
    
    // 启用/禁用时间选择控件
    bool customMode = (rangeIndex == 5);
    m_startTimeEdit->setEnabled(customMode);
    m_endTimeEdit->setEnabled(customMode);
    
    // 刷新当前图表
    onRefreshData();
}

void ChartWidget::onSeriesSelectionChanged()
{
    QString seriesName = m_seriesCombo->currentText();
    if (seriesName.isEmpty()) return;
    
    // 更新统计信息
    int chartIndex = m_chartTypeCombo->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    StatisticsData stats = calculateStatistics(type, seriesName);
    updateStatisticsLabels(stats);
}

void ChartWidget::onRefreshData()
{
    // 刷新当前显示的图表
    int chartIndex = m_tabWidget->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    refreshChart(type);
    
    LogManager::getInstance()->info("刷新图表数据", "Chart");
}

void ChartWidget::onUpdateTimer()
{
    if (!m_isRealTimeMonitoring || m_isPaused) return;
    
    // 更新实时监控图表
    updateChart(ChartType::RealTimeMonitor);
    
    m_lastUpdateTime = QDateTime::currentDateTime();
}

void ChartWidget::onExportChart()
{
    int chartIndex = m_tabWidget->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    QString fileName = QFileDialog::getSaveFileName(this,
        "导出图表", m_exportDirectory + "/chart.png",
        "PNG图片 (*.png);;JPEG图片 (*.jpg);;PDF文档 (*.pdf)");
    
    if (!fileName.isEmpty()) {
        exportChart(type, fileName);
    }
}

void ChartWidget::onPrintChart()
{
    int chartIndex = m_tabWidget->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    printChart(type);
}

void ChartWidget::onZoomIn()
{
    int chartIndex = m_tabWidget->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    zoomChart(type, 1.2);
}

void ChartWidget::onZoomOut()
{
    int chartIndex = m_tabWidget->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    zoomChart(type, 0.8);
}

void ChartWidget::onResetZoom()
{
    int chartIndex = m_tabWidget->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    resetZoom(type);
}

void ChartWidget::onAutoScaleToggled(bool enabled)
{
    int chartIndex = m_tabWidget->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    m_chartConfigs[type].autoScale = enabled;
    updateChart(type);
}

void ChartWidget::onAnimationToggled(bool enabled)
{
    int chartIndex = m_tabWidget->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    m_chartConfigs[type].enableAnimation = enabled;
    
    if (m_charts.contains(type)) {
        QChart* chart = m_charts[type];
        chart->setAnimationOptions(enabled ? QChart::SeriesAnimations : QChart::NoAnimation);
    }
}

void ChartWidget::onLegendToggled(bool enabled)
{
    int chartIndex = m_tabWidget->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    m_chartConfigs[type].showLegend = enabled;
    
    if (m_charts.contains(type)) {
        QChart* chart = m_charts[type];
        chart->legend()->setVisible(enabled);
    }
}

void ChartWidget::onGridToggled(bool enabled)
{
    int chartIndex = m_tabWidget->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    m_chartConfigs[type].showGrid = enabled;
    updateChart(type);
}

void ChartWidget::onShowStatistics()
{
    QString seriesName = m_seriesCombo->currentText();
    if (seriesName.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择数据系列");
        return;
    }
    
    int chartIndex = m_chartTypeCombo->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    StatisticsData stats = calculateStatistics(type, seriesName);
    
    // 显示详细统计信息对话框
    showStatisticsDialog(stats, seriesName);
}

void ChartWidget::onShowTrendAnalysis()
{
    QString seriesName = m_seriesCombo->currentText();
    if (seriesName.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择数据系列");
        return;
    }
    
    int chartIndex = m_chartTypeCombo->currentIndex();
    ChartType type = static_cast<ChartType>(chartIndex);
    
    // 执行趋势分析
    performTrendAnalysis(type, seriesName);
}

void ChartWidget::onShowComparison()
{
    // 显示对比分析对话框
    showComparisonDialog();
}

// 图表操作功能
void ChartWidget::refreshChart(ChartType type)
{
    updateChart(type);
    
    // 更新统计信息
    updateStatisticsDisplay(type);
}

void ChartWidget::refreshAllCharts()
{
    for (auto it = m_charts.begin(); it != m_charts.end(); ++it) {
        updateChart(it.key());
    }
}

void ChartWidget::exportChart(ChartType type, const QString& filePath)
{
    if (!m_chartViews.contains(type)) return;
    
    QChartView* chartView = m_chartViews[type];
    if (!chartView) return;
    
    // 获取文件扩展名
    QString extension = QFileInfo(filePath).suffix().toLower();
    
    if (extension == "pdf") {
        // 导出为PDF
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(filePath);
        printer.setPageSize(QPageSize::A4);
        
        QPainter painter(&printer);
        chartView->render(&painter);
    } else {
        // 导出为图片
        QPixmap pixmap = chartView->grab();
        pixmap.save(filePath);
    }
    
    emit exportCompleted(filePath);
    LogManager::getInstance()->info("导出图表: " + filePath, "Chart");
}

void ChartWidget::printChart(ChartType type)
{
    if (!m_chartViews.contains(type)) return;
    
    QChartView* chartView = m_chartViews[type];
    if (!chartView) return;
    
    QPrinter printer;
    QPrintDialog printDialog(&printer, this);
    
    if (printDialog.exec() == QDialog::Accepted) {
        QPainter painter(&printer);
        chartView->render(&painter);
        
        LogManager::getInstance()->info("打印图表", "Chart");
    }
}

void ChartWidget::zoomChart(ChartType type, double factor)
{
    if (!m_chartViews.contains(type)) return;
    
    QChartView* chartView = m_chartViews[type];
    if (!chartView) return;
    
    QChart* chart = chartView->chart();
    if (!chart) return;
    
    chart->zoom(factor);
}

void ChartWidget::resetZoom(ChartType type)
{
    if (!m_chartViews.contains(type)) return;
    
    QChartView* chartView = m_chartViews[type];
    if (!chartView) return;
    
    QChart* chart = chartView->chart();
    if (!chart) return;
    
    chart->zoomReset();
}

// 辅助功能
void ChartWidget::updateStatisticsDisplay(ChartType type)
{
    QString seriesName = m_seriesCombo->currentText();
    if (seriesName.isEmpty()) return;
    
    StatisticsData stats = calculateStatistics(type, seriesName);
    updateStatisticsLabels(stats);
}

void ChartWidget::updateStatisticsLabels(const StatisticsData& stats)
{
    m_averageLabel->setText(QString::number(stats.average, 'f', 2));
    m_maximumLabel->setText(QString::number(stats.maximum, 'f', 2));
    m_minimumLabel->setText(QString::number(stats.minimum, 'f', 2));
    m_stdDeviationLabel->setText(QString::number(stats.stdDeviation, 'f', 2));
    m_countLabel->setText(QString::number(stats.count));
    m_rangeLabel->setText(QString::number(stats.range, 'f', 2));
}

void ChartWidget::showStatisticsDialog(const StatisticsData& stats, const QString& seriesName)
{
    QDialog dialog(this);
    dialog.setWindowTitle("统计信息 - " + seriesName);
    dialog.setModal(true);
    dialog.resize(400, 300);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QTextEdit* textEdit = new QTextEdit;
    textEdit->setReadOnly(true);
    
    QString statsText = QString(
        "数据系列: %1\n\n"
        "数据点数: %2\n"
        "平均值: %3\n"
        "最大值: %4\n"
        "最小值: %5\n"
        "数据范围: %6\n"
        "标准偏差: %7\n"
        "方差: %8\n"
        "总和: %9\n\n"
        "时间范围:\n"
        "开始时间: %10\n"
        "结束时间: %11"
    ).arg(seriesName)
     .arg(stats.count)
     .arg(stats.average, 0, 'f', 4)
     .arg(stats.maximum, 0, 'f', 4)
     .arg(stats.minimum, 0, 'f', 4)
     .arg(stats.range, 0, 'f', 4)
     .arg(stats.stdDeviation, 0, 'f', 4)
     .arg(stats.variance, 0, 'f', 4)
     .arg(stats.sum, 0, 'f', 4)
     .arg(formatTime(stats.startTime))
     .arg(formatTime(stats.endTime));
    
    textEdit->setText(statsText);
    layout->addWidget(textEdit);
    
    QPushButton* closeButton = new QPushButton("关闭");
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeButton);
    
    dialog.exec();
}

void ChartWidget::performTrendAnalysis(ChartType type, const QString& seriesName)
{
    if (!m_chartData.contains(type) || !m_chartData[type].contains(seriesName)) {
        return;
    }
    
    const QList<ChartData>& data = m_chartData[type][seriesName];
    if (data.size() < 3) {
        QMessageBox::information(this, "提示", "数据点数量不足，无法进行趋势分析");
        return;
    }
    
    // 显示分析进度
    m_analysisProgress->setVisible(true);
    m_analysisProgress->setRange(0, 100);
    m_analysisProgress->setValue(0);
    
    // 执行线性回归分析
    QList<ChartData> trendData;
    performRegression(data, trendData);
    
    m_analysisProgress->setValue(50);
    
    // 检测异常值
    QList<int> anomalies;
    detectAnomalies(data, anomalies);
    
    m_analysisProgress->setValue(100);
    
    // 显示分析结果
    QString analysisText = QString(
        "趋势分析结果 - %1\n\n"
        "数据点数: %2\n"
        "趋势方向: %3\n"
        "异常点数: %4\n"
        "数据质量: %5\n"
    ).arg(seriesName)
     .arg(data.size())
     .arg(trendData.size() > 1 && trendData.last().value > trendData.first().value ? "上升" : "下降")
     .arg(anomalies.size())
     .arg(anomalies.size() < data.size() * 0.05 ? "良好" : "需要关注");
    
    m_analysisResults->setText(analysisText);
    
    // 隐藏进度条
    m_analysisProgress->setVisible(false);
    
    LogManager::getInstance()->info("完成趋势分析: " + seriesName, "Chart");
}

void ChartWidget::performRegression(const QList<ChartData>& data, QList<ChartData>& regression)
{
    if (data.size() < 2) return;
    
    // 简单线性回归
    int n = data.size();
    double sumX = 0, sumY = 0, sumXY = 0, sumX2 = 0;
    
    for (int i = 0; i < n; ++i) {
        double x = i; // 使用索引作为X值
        double y = data[i].value;
        
        sumX += x;
        sumY += y;
        sumXY += x * y;
        sumX2 += x * x;
    }
    
    double slope = (n * sumXY - sumX * sumY) / (n * sumX2 - sumX * sumX);
    double intercept = (sumY - slope * sumX) / n;
    
    // 生成回归线数据
    regression.clear();
    for (int i = 0; i < n; ++i) {
        ChartData point;
        point.timestamp = data[i].timestamp;
        point.value = slope * i + intercept;
        point.name = "趋势线";
        regression.append(point);
    }
}

void ChartWidget::detectAnomalies(const QList<ChartData>& data, QList<int>& anomalies)
{
    if (data.size() < 3) return;
    
    // 使用3σ准则检测异常值
    StatisticsData stats;
    calculateSeriesStatistics(data, stats);
    
    double threshold = 3 * stats.stdDeviation;
    
    anomalies.clear();
    for (int i = 0; i < data.size(); ++i) {
        if (std::abs(data[i].value - stats.average) > threshold) {
            anomalies.append(i);
        }
    }
}

void ChartWidget::showComparisonDialog()
{
    QDialog dialog(this);
    dialog.setWindowTitle("对比分析");
    dialog.setModal(true);
    dialog.resize(500, 400);
    
    QVBoxLayout* layout = new QVBoxLayout(&dialog);
    
    QLabel* label = new QLabel("对比分析功能正在开发中...");
    layout->addWidget(label);
    
    QPushButton* closeButton = new QPushButton("关闭");
    connect(closeButton, &QPushButton::clicked, &dialog, &QDialog::accept);
    layout->addWidget(closeButton);
    
    dialog.exec();
}

// 数据接收槽函数
void ChartWidget::onDataReceived(const QJsonObject& data)
{
    // 解析接收到的数据并添加到相应图表
    if (data.contains("type") && data.contains("value") && data.contains("timestamp")) {
        QString dataType = data["type"].toString();
        double value = data["value"].toDouble();
        QDateTime timestamp = QDateTime::fromString(data["timestamp"].toString(), Qt::ISODate);
        
        ChartData chartData(dataType, timestamp, value);
        
        // 根据数据类型添加到相应图表
        if (dataType.contains("温度") || dataType.contains("压力") || dataType.contains("速度")) {
            addDataPoint(ChartType::RealTimeMonitor, chartData);
        }
        if (dataType.contains("质量")) {
            addDataPoint(ChartType::QualityAnalysis, chartData);
        }
        if (dataType.contains("产量")) {
            addDataPoint(ChartType::ProductionStats, chartData);
        }
    }
}

void ChartWidget::onParameterChanged(const QString& parameter, double value)
{
    ChartData data(parameter, QDateTime::currentDateTime(), value);
    addDataPoint(ChartType::ProcessControl, data);
}

void ChartWidget::onAlarmTriggered(const QString& alarmType, const QString& message)
{
    Q_UNUSED(message)
    ChartData data(alarmType, QDateTime::currentDateTime(), 1.0); // 报警计数
    addDataPoint(ChartType::AlarmAnalysis, data);
}

void ChartWidget::onProductionDataUpdated(const QJsonObject& data)
{
    if (data.contains("count")) {
        ChartData chartData("产量", QDateTime::currentDateTime(), data["count"].toDouble());
        addDataPoint(ChartType::ProductionStats, chartData);
    }
}

void ChartWidget::onQualityDataUpdated(const QJsonObject& data)
{
    if (data.contains("qualityRate")) {
        ChartData chartData("合格率", QDateTime::currentDateTime(), data["qualityRate"].toDouble());
        addDataPoint(ChartType::QualityAnalysis, chartData);
    }
}

void ChartWidget::onPerformanceDataUpdated(const QJsonObject& data)
{
    if (data.contains("efficiency")) {
        ChartData chartData("效率", QDateTime::currentDateTime(), data["efficiency"].toDouble());
        addDataPoint(ChartType::PerformanceMonitor, chartData);
    }
}

// 清理功能
void ChartWidget::clearAllCharts()
{
    // 清理所有图表数据
    m_chartData.clear();
    
    // 清理图表和视图
    for (auto it = m_charts.begin(); it != m_charts.end(); ++it) {
        QChart* chart = it.value();
        if (chart) {
            chart->removeAllSeries();
        }
    }
    
    LogManager::getInstance()->info("清理所有图表数据", "Chart");
}

void ChartWidget::clearChartData(ChartType type)
{
    if (m_chartData.contains(type)) {
        m_chartData[type].clear();
        updateChart(type);
    }
}

void ChartWidget::clearSeriesData(ChartType type, const QString& seriesName)
{
    if (m_chartData.contains(type) && m_chartData[type].contains(seriesName)) {
        m_chartData[type][seriesName].clear();
        updateChart(type);
    }
}

void ChartWidget::onConfigChanged()
{
    // 配置变化时重新加载图表设置
    for (auto it = m_chartConfigs.begin(); it != m_chartConfigs.end(); ++it) {
        ChartType type = it.key();
        updateChart(type);
    }
    
    LogManager::getInstance()->info("图表配置已更新", "Chart");
} 