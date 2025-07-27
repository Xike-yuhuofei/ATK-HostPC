#include "datarecordwidget.h"
#include <QApplication>
#include <QSplitter>
#include <QTextStream>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QSqlDriver>
#include <QSqlRecord>
#include <QDesktopServices>
#include <QUrl>
#include <QPrinter>
#include <QPrintDialog>
#include <QTextDocument>
#include <QTextCursor>
#include <QTextTable>
#include <QTextTableFormat>
#include <QTextCharFormat>
#include <QTextBlockFormat>
#include <QFont>
#include <QFontMetrics>
#include <QPixmap>
#include <QBuffer>
#include <QImageWriter>

DataRecordWidget::DataRecordWidget(QWidget *parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_productionTab(nullptr)
    , m_productionTable(nullptr)
    , m_productTypeFilter(nullptr)
    , m_startDateEdit(nullptr)
    , m_endDateEdit(nullptr)
    , m_refreshBtn(nullptr)
    , m_exportBtn(nullptr)
    , m_totalBatchesLabel(nullptr)
    , m_totalProductsLabel(nullptr)
    , m_qualityRateLabel(nullptr)
    , m_qualityTab(nullptr)
    , m_qualityTable(nullptr)
    , m_batchFilter(nullptr)
    , m_qualityFilter(nullptr)
    , m_qualityChartBtn(nullptr)
    , m_qualityChartView(nullptr)
    , m_qualityStatsLabel(nullptr)
    , m_alarmTab(nullptr)
    , m_alarmTable(nullptr)
    , m_alarmTypeFilter(nullptr)
    , m_alarmLevelFilter(nullptr)
    , m_acknowledgeBtn(nullptr)
    , m_clearAlarmsBtn(nullptr)
    , m_alarmCountLabel(nullptr)
    , m_unacknowledgedLabel(nullptr)
    , m_statisticsTab(nullptr)
    , m_trendChartView(nullptr)
    , m_defectChartView(nullptr)
    , m_efficiencyChartView(nullptr)
    , m_statisticsTable(nullptr)
    , m_statisticsPeriod(nullptr)
    , m_updateStatsBtn(nullptr)
    , m_reportTab(nullptr)
    , m_reportType(nullptr)
    , m_reportStartDate(nullptr)
    , m_reportEndDate(nullptr)
    , m_reportPreview(nullptr)
    , m_generateReportBtn(nullptr)
    , m_printReportBtn(nullptr)
    , m_saveReportBtn(nullptr)
    , m_exportTab(nullptr)
    , m_exportDataType(nullptr)
    , m_exportFormat(nullptr)
    , m_exportPath(nullptr)
    , m_browseBtn(nullptr)
    , m_exportDataBtn(nullptr)
    , m_backupBtn(nullptr)
    , m_restoreBtn(nullptr)
    , m_exportProgress(nullptr)
    , m_productionModel(nullptr)
    , m_qualityModel(nullptr)
    , m_alarmModel(nullptr)
    , m_statisticsModel(nullptr)
    , m_productionProxy(nullptr)
    , m_qualityProxy(nullptr)
    , m_alarmProxy(nullptr)
    , m_updateTimer(nullptr)
    , m_backupTimer(nullptr)
    , m_maxRecords(10000)
    , m_backupInterval(3600000) // 1小时
    , m_autoBackup(true)
    , m_realTimeUpdate(true)
    , m_isRecording(false)
    , m_currentBatchId(0)
{
    setupUI();
    setupDatabase();
    setupConnections();
    
    // 初始化定时器
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(5000); // 5秒更新一次
    connect(m_updateTimer, &QTimer::timeout, this, &DataRecordWidget::onUpdateStatistics);
    
    m_backupTimer = new QTimer(this);
    m_backupTimer->setInterval(m_backupInterval);
    connect(m_backupTimer, &QTimer::timeout, this, &DataRecordWidget::onBackupData);
    
    // 设置导出目录
    m_exportDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/GlueDispenser/Exports";
    QDir().mkpath(m_exportDirectory);
    
    // 加载初始数据
    loadProductionData();
    loadQualityData();
    loadAlarmData();
    loadStatisticsData();
    
    // 启动定时器
    if (m_realTimeUpdate) {
        m_updateTimer->start();
    }
    if (m_autoBackup) {
        m_backupTimer->start();
    }
}

DataRecordWidget::~DataRecordWidget()
{
    if (m_updateTimer) {
        m_updateTimer->stop();
    }
    if (m_backupTimer) {
        m_backupTimer->stop();
    }
    
    if (m_database.isOpen()) {
        m_database.close();
    }
}

void DataRecordWidget::setupUI()
{
    auto mainLayout = new QVBoxLayout(this);
    
    // 创建标签页控件
    m_tabWidget = new QTabWidget(this);
    
    // 设置标签页
    setupProductionTab();
    setupQualityTab();
    setupAlarmTab();
    setupStatisticsTab();
    setupReportTab();
    setupExportTab();
    
    mainLayout->addWidget(m_tabWidget);
    
    // 设置样式
    setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #c0c0c0;
            background-color: white;
        }
        QTabWidget::tab-bar {
            alignment: left;
        }
        QTabBar::tab {
            background-color: #f0f0f0;
            border: 1px solid #c0c0c0;
            padding: 8px 16px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: white;
            border-bottom: 1px solid white;
        }
        QTabBar::tab:hover {
            background-color: #e0e0e0;
        }
        QTableWidget {
            gridline-color: #d0d0d0;
            background-color: white;
            alternate-background-color: #f8f8f8;
        }
        QTableWidget::item {
            padding: 4px;
            border: none;
        }
        QTableWidget::item:selected {
            background-color: #3daee9;
            color: white;
        }
        QHeaderView::section {
            background-color: #f0f0f0;
            border: 1px solid #c0c0c0;
            padding: 4px;
            font-weight: bold;
        }
        QPushButton {
            background-color: #4CAF50;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 4px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #45a049;
        }
        QPushButton:pressed {
            background-color: #3d8b40;
        }
        QPushButton:disabled {
            background-color: #cccccc;
            color: #666666;
        }
        QComboBox {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            padding: 4px;
            background-color: white;
        }
        QComboBox:focus {
            border: 2px solid #3daee9;
        }
        QDateTimeEdit {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            padding: 4px;
            background-color: white;
        }
        QDateTimeEdit:focus {
            border: 2px solid #3daee9;
        }
        QLineEdit {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            padding: 4px;
            background-color: white;
        }
        QLineEdit:focus {
            border: 2px solid #3daee9;
        }
        QTextEdit {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            background-color: white;
        }
        QTextEdit:focus {
            border: 2px solid #3daee9;
        }
        QProgressBar {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            text-align: center;
            background-color: #f0f0f0;
        }
        QProgressBar::chunk {
            background-color: #4CAF50;
            border-radius: 3px;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #c0c0c0;
            border-radius: 4px;
            margin-top: 10px;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
        QLabel {
            color: #333333;
        }
    )");
}

void DataRecordWidget::setupProductionTab()
{
    m_productionTab = new QWidget();
    m_tabWidget->addTab(m_productionTab, "生产数据");
    
    auto layout = new QVBoxLayout(m_productionTab);
    
    // 创建控制面板
    auto controlPanel = new QGroupBox("控制面板", m_productionTab);
    auto controlLayout = new QHBoxLayout(controlPanel);
    
    // 产品类型过滤
    controlLayout->addWidget(new QLabel("产品类型:"));
    m_productTypeFilter = new QComboBox();
    m_productTypeFilter->addItems({"全部", "标准型", "精密型", "特殊型"});
    controlLayout->addWidget(m_productTypeFilter);
    
    // 时间范围选择
    controlLayout->addWidget(new QLabel("开始时间:"));
    m_startDateEdit = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-7));
    m_startDateEdit->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
    controlLayout->addWidget(m_startDateEdit);
    
    controlLayout->addWidget(new QLabel("结束时间:"));
    m_endDateEdit = new QDateTimeEdit(QDateTime::currentDateTime());
    m_endDateEdit->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
    controlLayout->addWidget(m_endDateEdit);
    
    // 操作按钮
    m_refreshBtn = new QPushButton("刷新数据");
    m_exportBtn = new QPushButton("导出数据");
    controlLayout->addWidget(m_refreshBtn);
    controlLayout->addWidget(m_exportBtn);
    
    controlLayout->addStretch();
    layout->addWidget(controlPanel);
    
    // 创建统计信息面板
    auto statsPanel = new QGroupBox("统计信息", m_productionTab);
    auto statsLayout = new QHBoxLayout(statsPanel);
    
    m_totalBatchesLabel = new QLabel("总批次: 0");
    m_totalProductsLabel = new QLabel("总产品: 0");
    m_qualityRateLabel = new QLabel("合格率: 0%");
    
    statsLayout->addWidget(m_totalBatchesLabel);
    statsLayout->addWidget(m_totalProductsLabel);
    statsLayout->addWidget(m_qualityRateLabel);
    statsLayout->addStretch();
    
    layout->addWidget(statsPanel);
    
    // 创建数据表格
    m_productionTable = new QTableWidget(0, 13, m_productionTab);
    QStringList headers = {"批次ID", "批次名称", "产品类型", "开始时间", "结束时间", 
                          "总数量", "合格数量", "不良数量", "合格率", "操作员", 
                          "程序名称", "状态", "备注"};
    m_productionTable->setHorizontalHeaderLabels(headers);
    m_productionTable->setAlternatingRowColors(true);
    m_productionTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_productionTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_productionTable->setSortingEnabled(true);
    m_productionTable->horizontalHeader()->setStretchLastSection(true);
    m_productionTable->verticalHeader()->setVisible(false);
    
    layout->addWidget(m_productionTable);
    
    // 创建数据模型
    m_productionModel = new QStandardItemModel(this);
    m_productionModel->setHorizontalHeaderLabels(headers);
    
    m_productionProxy = new QSortFilterProxyModel(this);
    m_productionProxy->setSourceModel(m_productionModel);
    m_productionProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void DataRecordWidget::setupQualityTab()
{
    m_qualityTab = new QWidget();
    m_tabWidget->addTab(m_qualityTab, "质量数据");
    
    auto layout = new QVBoxLayout(m_qualityTab);
    
    // 创建控制面板
    auto controlPanel = new QGroupBox("控制面板", m_qualityTab);
    auto controlLayout = new QHBoxLayout(controlPanel);
    
    // 批次过滤
    controlLayout->addWidget(new QLabel("批次:"));
    m_batchFilter = new QComboBox();
    m_batchFilter->addItem("全部");
    controlLayout->addWidget(m_batchFilter);
    
    // 质量等级过滤
    controlLayout->addWidget(new QLabel("质量等级:"));
    m_qualityFilter = new QComboBox();
    m_qualityFilter->addItems({"全部", "A级", "B级", "C级", "D级", "不合格"});
    controlLayout->addWidget(m_qualityFilter);
    
    // 图表按钮
    m_qualityChartBtn = new QPushButton("显示图表");
    controlLayout->addWidget(m_qualityChartBtn);
    
    controlLayout->addStretch();
    layout->addWidget(controlPanel);
    
    // 创建统计信息面板
    auto statsPanel = new QGroupBox("质量统计", m_qualityTab);
    auto statsLayout = new QHBoxLayout(statsPanel);
    
    m_qualityStatsLabel = new QLabel("质量统计信息");
    statsLayout->addWidget(m_qualityStatsLabel);
    statsLayout->addStretch();
    
    layout->addWidget(statsPanel);
    
    // 创建分割器
    auto splitter = new QSplitter(Qt::Horizontal, m_qualityTab);
    
    // 创建数据表格
    m_qualityTable = new QTableWidget(0, 14, splitter);
    QStringList headers = {"记录ID", "批次ID", "时间戳", "X坐标", "Y坐标", "Z坐标", 
                          "胶量", "压力", "温度", "速度", "质量等级", "合格", 
                          "缺陷类型", "检测员"};
    m_qualityTable->setHorizontalHeaderLabels(headers);
    m_qualityTable->setAlternatingRowColors(true);
    m_qualityTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_qualityTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_qualityTable->setSortingEnabled(true);
    m_qualityTable->verticalHeader()->setVisible(false);
    
    // 创建图表视图
    m_qualityChartView = new QChartView(splitter);
    m_qualityChartView->setRenderHint(QPainter::Antialiasing);
    
    splitter->addWidget(m_qualityTable);
    splitter->addWidget(m_qualityChartView);
    splitter->setStretchFactor(0, 2);
    splitter->setStretchFactor(1, 1);
    
    layout->addWidget(splitter);
    
    // 创建数据模型
    m_qualityModel = new QStandardItemModel(this);
    m_qualityModel->setHorizontalHeaderLabels(headers);
    
    m_qualityProxy = new QSortFilterProxyModel(this);
    m_qualityProxy->setSourceModel(m_qualityModel);
    m_qualityProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void DataRecordWidget::setupAlarmTab()
{
    m_alarmTab = new QWidget();
    m_tabWidget->addTab(m_alarmTab, "报警记录");
    
    auto layout = new QVBoxLayout(m_alarmTab);
    
    // 创建控制面板
    auto controlPanel = new QGroupBox("控制面板", m_alarmTab);
    auto controlLayout = new QHBoxLayout(controlPanel);
    
    // 报警类型过滤
    controlLayout->addWidget(new QLabel("报警类型:"));
    m_alarmTypeFilter = new QComboBox();
    m_alarmTypeFilter->addItems({"全部", "设备故障", "工艺异常", "质量报警", "安全报警", "系统错误"});
    controlLayout->addWidget(m_alarmTypeFilter);
    
    // 报警等级过滤
    controlLayout->addWidget(new QLabel("报警等级:"));
    m_alarmLevelFilter = new QComboBox();
    m_alarmLevelFilter->addItems({"全部", "低", "中", "高", "紧急"});
    controlLayout->addWidget(m_alarmLevelFilter);
    
    // 操作按钮
    m_acknowledgeBtn = new QPushButton("确认报警");
    m_clearAlarmsBtn = new QPushButton("清除历史");
    controlLayout->addWidget(m_acknowledgeBtn);
    controlLayout->addWidget(m_clearAlarmsBtn);
    
    controlLayout->addStretch();
    layout->addWidget(controlPanel);
    
    // 创建统计信息面板
    auto statsPanel = new QGroupBox("报警统计", m_alarmTab);
    auto statsLayout = new QHBoxLayout(statsPanel);
    
    m_alarmCountLabel = new QLabel("总报警: 0");
    m_unacknowledgedLabel = new QLabel("未确认: 0");
    
    statsLayout->addWidget(m_alarmCountLabel);
    statsLayout->addWidget(m_unacknowledgedLabel);
    statsLayout->addStretch();
    
    layout->addWidget(statsPanel);
    
    // 创建数据表格
    m_alarmTable = new QTableWidget(0, 13, m_alarmTab);
    QStringList headers = {"报警ID", "时间戳", "报警类型", "报警等级", "报警代码", 
                          "报警信息", "设备名称", "操作员", "已确认", "确认时间", 
                          "确认用户", "解决方案", "备注"};
    m_alarmTable->setHorizontalHeaderLabels(headers);
    m_alarmTable->setAlternatingRowColors(true);
    m_alarmTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_alarmTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_alarmTable->setSortingEnabled(true);
    m_alarmTable->horizontalHeader()->setStretchLastSection(true);
    m_alarmTable->verticalHeader()->setVisible(false);
    
    layout->addWidget(m_alarmTable);
    
    // 创建数据模型
    m_alarmModel = new QStandardItemModel(this);
    m_alarmModel->setHorizontalHeaderLabels(headers);
    
    m_alarmProxy = new QSortFilterProxyModel(this);
    m_alarmProxy->setSourceModel(m_alarmModel);
    m_alarmProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void DataRecordWidget::setupStatisticsTab()
{
    m_statisticsTab = new QWidget();
    m_tabWidget->addTab(m_statisticsTab, "统计分析");
    
    auto layout = new QVBoxLayout(m_statisticsTab);
    
    // 创建控制面板
    auto controlPanel = new QGroupBox("控制面板", m_statisticsTab);
    auto controlLayout = new QHBoxLayout(controlPanel);
    
    // 统计周期选择
    controlLayout->addWidget(new QLabel("统计周期:"));
    m_statisticsPeriod = new QComboBox();
    m_statisticsPeriod->addItems({"日", "周", "月", "季度", "年"});
    controlLayout->addWidget(m_statisticsPeriod);
    
    // 更新按钮
    m_updateStatsBtn = new QPushButton("更新统计");
    controlLayout->addWidget(m_updateStatsBtn);
    
    controlLayout->addStretch();
    layout->addWidget(controlPanel);
    
    // 创建图表区域
    auto chartWidget = new QWidget();
    auto chartLayout = new QGridLayout(chartWidget);
    
    // 趋势图表
    m_trendChartView = new QChartView();
    m_trendChartView->setRenderHint(QPainter::Antialiasing);
    chartLayout->addWidget(m_trendChartView, 0, 0);
    
    // 缺陷分布图表
    m_defectChartView = new QChartView();
    m_defectChartView->setRenderHint(QPainter::Antialiasing);
    chartLayout->addWidget(m_defectChartView, 0, 1);
    
    // 效率图表
    m_efficiencyChartView = new QChartView();
    m_efficiencyChartView->setRenderHint(QPainter::Antialiasing);
    chartLayout->addWidget(m_efficiencyChartView, 1, 0);
    
    // 统计表格
    m_statisticsTable = new QTableWidget(0, 14, chartWidget);
    QStringList headers = {"日期", "总批次", "总产品", "合格产品", "不良产品", 
                          "合格率", "生产效率", "运行时间", "停机时间", "报警次数", 
                          "主要缺陷", "平均胶量", "平均压力", "平均温度"};
    m_statisticsTable->setHorizontalHeaderLabels(headers);
    m_statisticsTable->setAlternatingRowColors(true);
    m_statisticsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_statisticsTable->setSortingEnabled(true);
    m_statisticsTable->verticalHeader()->setVisible(false);
    chartLayout->addWidget(m_statisticsTable, 1, 1);
    
    layout->addWidget(chartWidget);
    
    // 创建数据模型
    m_statisticsModel = new QStandardItemModel(this);
    m_statisticsModel->setHorizontalHeaderLabels(headers);
}

void DataRecordWidget::setupReportTab()
{
    m_reportTab = new QWidget();
    m_tabWidget->addTab(m_reportTab, "报表生成");
    
    auto layout = new QVBoxLayout(m_reportTab);
    
    // 创建控制面板
    auto controlPanel = new QGroupBox("报表设置", m_reportTab);
    auto controlLayout = new QGridLayout(controlPanel);
    
    // 报表类型
    controlLayout->addWidget(new QLabel("报表类型:"), 0, 0);
    m_reportType = new QComboBox();
    m_reportType->addItems({"生产日报", "质量周报", "设备月报", "年度总结", "自定义报表"});
    controlLayout->addWidget(m_reportType, 0, 1);
    
    // 时间范围
    controlLayout->addWidget(new QLabel("开始时间:"), 1, 0);
    m_reportStartDate = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-7));
    m_reportStartDate->setDisplayFormat("yyyy-MM-dd");
    controlLayout->addWidget(m_reportStartDate, 1, 1);
    
    controlLayout->addWidget(new QLabel("结束时间:"), 2, 0);
    m_reportEndDate = new QDateTimeEdit(QDateTime::currentDateTime());
    m_reportEndDate->setDisplayFormat("yyyy-MM-dd");
    controlLayout->addWidget(m_reportEndDate, 2, 1);
    
    // 操作按钮
    auto buttonLayout = new QHBoxLayout();
    m_generateReportBtn = new QPushButton("生成报表");
    m_printReportBtn = new QPushButton("打印报表");
    m_saveReportBtn = new QPushButton("保存报表");
    buttonLayout->addWidget(m_generateReportBtn);
    buttonLayout->addWidget(m_printReportBtn);
    buttonLayout->addWidget(m_saveReportBtn);
    buttonLayout->addStretch();
    controlLayout->addLayout(buttonLayout, 3, 0, 1, 2);
    
    layout->addWidget(controlPanel);
    
    // 创建报表预览
    auto previewPanel = new QGroupBox("报表预览", m_reportTab);
    auto previewLayout = new QVBoxLayout(previewPanel);
    
    m_reportPreview = new QTextEdit();
    m_reportPreview->setReadOnly(true);
    previewLayout->addWidget(m_reportPreview);
    
    layout->addWidget(previewPanel);
}

void DataRecordWidget::setupExportTab()
{
    m_exportTab = new QWidget();
    m_tabWidget->addTab(m_exportTab, "数据导出");
    
    auto layout = new QVBoxLayout(m_exportTab);
    
    // 创建导出设置面板
    auto exportPanel = new QGroupBox("导出设置", m_exportTab);
    auto exportLayout = new QGridLayout(exportPanel);
    
    // 数据类型
    exportLayout->addWidget(new QLabel("数据类型:"), 0, 0);
    m_exportDataType = new QComboBox();
    m_exportDataType->addItems({"生产数据", "质量数据", "报警记录", "统计数据", "全部数据"});
    exportLayout->addWidget(m_exportDataType, 0, 1);
    
    // 导出格式
    exportLayout->addWidget(new QLabel("导出格式:"), 1, 0);
    m_exportFormat = new QComboBox();
    m_exportFormat->addItems({"CSV", "Excel", "JSON", "XML", "PDF"});
    exportLayout->addWidget(m_exportFormat, 1, 1);
    
    // 导出路径
    exportLayout->addWidget(new QLabel("导出路径:"), 2, 0);
    auto pathLayout = new QHBoxLayout();
    m_exportPath = new QLineEdit();
    m_exportPath->setText(m_exportDirectory);
    m_browseBtn = new QPushButton("浏览");
    pathLayout->addWidget(m_exportPath);
    pathLayout->addWidget(m_browseBtn);
    exportLayout->addLayout(pathLayout, 2, 1);
    
    // 操作按钮
    auto buttonLayout = new QHBoxLayout();
    m_exportDataBtn = new QPushButton("导出数据");
    buttonLayout->addWidget(m_exportDataBtn);
    buttonLayout->addStretch();
    exportLayout->addLayout(buttonLayout, 3, 0, 1, 2);
    
    // 进度条
    m_exportProgress = new QProgressBar();
    m_exportProgress->setVisible(false);
    exportLayout->addWidget(m_exportProgress, 4, 0, 1, 2);
    
    layout->addWidget(exportPanel);
    
    // 创建备份恢复面板
    auto backupPanel = new QGroupBox("数据备份", m_exportTab);
    auto backupLayout = new QHBoxLayout(backupPanel);
    
    m_backupBtn = new QPushButton("备份数据");
    m_restoreBtn = new QPushButton("恢复数据");
    backupLayout->addWidget(m_backupBtn);
    backupLayout->addWidget(m_restoreBtn);
    backupLayout->addStretch();
    
    layout->addWidget(backupPanel);
    
    layout->addStretch();
}

void DataRecordWidget::setupDatabase()
{
    // 设置数据库路径
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir);
    m_databasePath = dataDir + "/production_data.db";
    
    // 创建数据库连接
    m_database = QSqlDatabase::addDatabase("QSQLITE", "ProductionDB");
    m_database.setDatabaseName(m_databasePath);
    
    if (!m_database.open()) {
        emit databaseError("无法打开数据库: " + m_database.lastError().text());
        return;
    }
    
    // 创建表格
    if (!createTables()) {
        emit databaseError("无法创建数据表");
        return;
    }
    
    // 优化数据库性能
    QSqlQuery query(m_database);
    query.exec("PRAGMA synchronous = NORMAL");
    query.exec("PRAGMA cache_size = 10000");
    query.exec("PRAGMA temp_store = MEMORY");
    query.exec("PRAGMA journal_mode = WAL");
}

bool DataRecordWidget::createTables()
{
    QSqlQuery query(m_database);
    
    // 创建生产批次表
    QString createProductionTable = R"(
        CREATE TABLE IF NOT EXISTS production_batches (
            batch_id INTEGER PRIMARY KEY AUTOINCREMENT,
            batch_name TEXT NOT NULL,
            product_type TEXT NOT NULL,
            start_time DATETIME NOT NULL,
            end_time DATETIME,
            total_count INTEGER DEFAULT 0,
            qualified_count INTEGER DEFAULT 0,
            defect_count INTEGER DEFAULT 0,
            quality_rate REAL DEFAULT 0.0,
            operator_name TEXT,
            program_name TEXT,
            notes TEXT,
            parameters TEXT,
            quality_data TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createProductionTable)) {
        return false;
    }
    
    // 创建质量数据表
    QString createQualityTable = R"(
        CREATE TABLE IF NOT EXISTS quality_data (
            record_id INTEGER PRIMARY KEY AUTOINCREMENT,
            batch_id INTEGER NOT NULL,
            timestamp DATETIME NOT NULL,
            position_x REAL NOT NULL,
            position_y REAL NOT NULL,
            position_z REAL NOT NULL,
            glue_volume REAL NOT NULL,
            pressure REAL NOT NULL,
            temperature REAL NOT NULL,
            speed REAL NOT NULL,
            quality_level TEXT NOT NULL,
            is_qualified BOOLEAN NOT NULL,
            defect_type TEXT,
            inspector TEXT,
            notes TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            FOREIGN KEY (batch_id) REFERENCES production_batches(batch_id)
        )
    )";
    
    if (!query.exec(createQualityTable)) {
        return false;
    }
    
    // 创建报警记录表
    QString createAlarmTable = R"(
        CREATE TABLE IF NOT EXISTS alarm_records (
            alarm_id INTEGER PRIMARY KEY AUTOINCREMENT,
            timestamp DATETIME NOT NULL,
            alarm_type TEXT NOT NULL,
            alarm_level TEXT NOT NULL,
            alarm_code TEXT NOT NULL,
            alarm_message TEXT NOT NULL,
            device_name TEXT NOT NULL,
            operator_name TEXT,
            is_acknowledged BOOLEAN DEFAULT FALSE,
            acknowledge_time DATETIME,
            acknowledge_user TEXT,
            solution TEXT,
            notes TEXT,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createAlarmTable)) {
        return false;
    }
    
    // 创建统计数据表
    QString createStatisticsTable = R"(
        CREATE TABLE IF NOT EXISTS statistics_data (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            date DATE NOT NULL UNIQUE,
            total_batches INTEGER DEFAULT 0,
            total_products INTEGER DEFAULT 0,
            qualified_products INTEGER DEFAULT 0,
            defect_products INTEGER DEFAULT 0,
            quality_rate REAL DEFAULT 0.0,
            efficiency REAL DEFAULT 0.0,
            uptime REAL DEFAULT 0.0,
            downtime REAL DEFAULT 0.0,
            alarm_count INTEGER DEFAULT 0,
            top_defect_type TEXT,
            average_glue_volume REAL DEFAULT 0.0,
            average_pressure REAL DEFAULT 0.0,
            average_temperature REAL DEFAULT 0.0,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createStatisticsTable)) {
        return false;
    }
    
    // 创建索引
    query.exec("CREATE INDEX IF NOT EXISTS idx_production_start_time ON production_batches(start_time)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_production_product_type ON production_batches(product_type)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_quality_batch_id ON quality_data(batch_id)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_quality_timestamp ON quality_data(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_alarm_timestamp ON alarm_records(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_alarm_type ON alarm_records(alarm_type)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_statistics_date ON statistics_data(date)");
    
    return true;
}

void DataRecordWidget::setupConnections()
{
    // 生产数据页面连接
    connect(m_refreshBtn, &QPushButton::clicked, this, &DataRecordWidget::onRefreshData);
    connect(m_exportBtn, &QPushButton::clicked, this, &DataRecordWidget::onExportData);
    connect(m_productTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &DataRecordWidget::onFilterChanged);
    connect(m_startDateEdit, &QDateTimeEdit::dateTimeChanged, this, &DataRecordWidget::onDateRangeChanged);
    connect(m_endDateEdit, &QDateTimeEdit::dateTimeChanged, this, &DataRecordWidget::onDateRangeChanged);
    connect(m_productionTable, &QTableWidget::itemSelectionChanged, 
            this, &DataRecordWidget::onBatchSelectionChanged);
    
    // 质量数据页面连接
    connect(m_batchFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &DataRecordWidget::onFilterChanged);
    connect(m_qualityFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &DataRecordWidget::onFilterChanged);
    connect(m_qualityChartBtn, &QPushButton::clicked, this, &DataRecordWidget::onShowChart);
    connect(m_qualityTable, &QTableWidget::itemSelectionChanged, 
            this, &DataRecordWidget::onQualityDataSelectionChanged);
    
    // 报警记录页面连接
    connect(m_alarmTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &DataRecordWidget::onFilterChanged);
    connect(m_alarmLevelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &DataRecordWidget::onFilterChanged);
    connect(m_acknowledgeBtn, &QPushButton::clicked, this, &DataRecordWidget::onAcknowledgeAlarm);
    connect(m_clearAlarmsBtn, &QPushButton::clicked, this, &DataRecordWidget::onClearOldData);
    connect(m_alarmTable, &QTableWidget::itemSelectionChanged, 
            this, &DataRecordWidget::onAlarmSelectionChanged);
    
    // 统计分析页面连接
    connect(m_statisticsPeriod, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &DataRecordWidget::onFilterChanged);
    connect(m_updateStatsBtn, &QPushButton::clicked, this, &DataRecordWidget::onUpdateStatistics);
    
    // 报表生成页面连接
    connect(m_reportType, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &DataRecordWidget::onFilterChanged);
    connect(m_generateReportBtn, &QPushButton::clicked, this, &DataRecordWidget::onGenerateReport);
    connect(m_printReportBtn, &QPushButton::clicked, this, &DataRecordWidget::onPrintReport);
    connect(m_saveReportBtn, &QPushButton::clicked, this, &DataRecordWidget::onGenerateReport);
    
    // 数据导出页面连接
    connect(m_browseBtn, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, "选择导出目录", m_exportDirectory);
        if (!dir.isEmpty()) {
            m_exportPath->setText(dir);
            m_exportDirectory = dir;
        }
    });
    connect(m_exportDataBtn, &QPushButton::clicked, this, &DataRecordWidget::onExportData);
    connect(m_backupBtn, &QPushButton::clicked, this, &DataRecordWidget::onBackupData);
    connect(m_restoreBtn, &QPushButton::clicked, this, &DataRecordWidget::onRestoreData);
}

// ... existing code ... 