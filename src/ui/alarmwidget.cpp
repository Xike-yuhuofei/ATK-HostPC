#include "alarmwidget.h"
#include "../logger/logmanager.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <QHeaderView>
#include <QSplitter>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QClipboard>
#include <QMimeData>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QThread>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTextStream>
#include <QMutexLocker>

AlarmWidget::AlarmWidget(QWidget* parent) 
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_alarmSound(nullptr)
    , m_systemTray(nullptr)
    , m_isInitialized(false)
    , m_isSoundPlaying(false)
    , m_nextAlarmId(1)
{
    // 初始化配置目录
    m_configDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/config";
    m_soundDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/sounds";
    m_exportDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    
    // 创建目录
    QDir().mkpath(m_configDirectory);
    QDir().mkpath(m_soundDirectory);
    
    // 初始化默认配置
    m_alarmConfig.enableAudibleAlarms = true;
    m_alarmConfig.enableVisualAlarms = true;
    m_alarmConfig.enableSystemTray = true;
    m_alarmConfig.maxActiveAlarms = 100;
    m_alarmConfig.autoAcknowledgeTime = 0;
    m_alarmConfig.alarmSoundDuration = 5;
    m_alarmConfig.alarmSoundFile = ":/sounds/alarm.wav";
    m_alarmConfig.enableAlarmHistory = true;
    m_alarmConfig.maxHistoryRecords = 10000;
    m_alarmConfig.enableAlarmStatistics = true;
    m_alarmConfig.statisticsUpdateInterval = 60;
    
    setupUI();
    // 暂时跳过数据库初始化，避免崩溃
    // setupDatabase();
    setupConnections();
    
    // 初始化定时器
    m_updateTimer = new QTimer(this);
    m_updateTimer->setInterval(UPDATE_INTERVAL);
    connect(m_updateTimer, &QTimer::timeout, this, &AlarmWidget::onUpdateTimer);
    
    m_statisticsTimer = new QTimer(this);
    m_statisticsTimer->setInterval(STATISTICS_INTERVAL);
    connect(m_statisticsTimer, &QTimer::timeout, this, &AlarmWidget::onStatisticsTimer);
    
    m_autoAcknowledgeTimer = new QTimer(this);
    connect(m_autoAcknowledgeTimer, &QTimer::timeout, this, &AlarmWidget::onAutoAcknowledgeTimer);
    
    m_cleanupTimer = new QTimer(this);
    m_cleanupTimer->setInterval(CLEANUP_INTERVAL);
    connect(m_cleanupTimer, &QTimer::timeout, this, &AlarmWidget::cleanupOldAlarms);
    
    // 初始化系统托盘
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        m_systemTray = new QSystemTrayIcon(this);
        
        // 尝试加载图标，如果失败则使用默认图标
        QIcon trayIcon(":/icons/alarm.png");
        if (trayIcon.isNull()) {
            // 如果alarm.png不存在，使用settings.png作为备用
            trayIcon = QIcon(":/icons/settings.png");
            if (trayIcon.isNull()) {
                // 如果都不存在，创建一个简单的默认图标
                QPixmap pixmap(16, 16);
                pixmap.fill(Qt::red);
                trayIcon = QIcon(pixmap);
            }
        }
        
        m_systemTray->setIcon(trayIcon);
        m_systemTray->setToolTip("报警系统");
        
        // 只有在图标设置成功后才显示
        if (!m_systemTray->icon().isNull()) {
            m_systemTray->show();
        }
    }
    
    // 暂时跳过配置和数据加载，避免崩溃
    // loadAlarmConfig();
    // loadAlarmThresholds();
    // loadActiveAlarms();
    
    // 暂时不启动定时器
    // m_updateTimer->start();
    // m_statisticsTimer->start();
    // m_cleanupTimer->start();
    
    m_isInitialized = true;
    
    LogManager::getInstance()->info("报警系统初始化完成", "AlarmWidget");
}

AlarmWidget::~AlarmWidget()
{
    // 停止定时器
    if (m_updateTimer) m_updateTimer->stop();
    if (m_statisticsTimer) m_statisticsTimer->stop();
    if (m_autoAcknowledgeTimer) m_autoAcknowledgeTimer->stop();
    if (m_cleanupTimer) m_cleanupTimer->stop();
    
    // 清理资源
    if (m_alarmSound) {
        m_alarmSound->deleteLater();
        m_alarmSound = nullptr;
    }
    
    // 关闭数据库
    if (m_database.isOpen()) {
        m_database.close();
    }
    
    LogManager::getInstance()->info("报警系统已关闭", "AlarmWidget");
}

void AlarmWidget::setupUI()
{
    auto layout = new QVBoxLayout(this);
    
    // 创建主分割器
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    
    // 创建标签页
    m_tabWidget = new QTabWidget(this);
    
    setupActiveAlarmsTab();
    setupHistoryTab();
    setupThresholdsTab();
    setupStatisticsTab();
    setupConfigTab();
    
    layout->addWidget(m_tabWidget);
    
    // 设置样式
    setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #C0C0C0;
            background-color: white;
        }
        QTabWidget::tab-bar {
            alignment: left;
        }
        QTabBar::tab {
            background-color: #E0E0E0;
            border: 1px solid #C0C0C0;
            padding: 8px 16px;
            margin-right: 2px;
        }
        QTabBar::tab:selected {
            background-color: white;
            border-bottom: 1px solid white;
        }
        QTabBar::tab:hover {
            background-color: #F0F0F0;
        }
        QTableWidget {
            gridline-color: #E0E0E0;
            selection-background-color: #3399FF;
            alternate-background-color: #F8F8F8;
        }
        QTableWidget::item {
            padding: 4px;
        }
        QGroupBox {
            font-weight: bold;
            border: 2px solid #C0C0C0;
            border-radius: 5px;
            margin-top: 10px;
            padding-top: 10px;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 10px;
            padding: 0 5px 0 5px;
        }
        QPushButton {
            background-color: #E0E0E0;
            border: 1px solid #C0C0C0;
            border-radius: 3px;
            padding: 6px 12px;
            min-width: 80px;
        }
        QPushButton:hover {
            background-color: #D0D0D0;
        }
        QPushButton:pressed {
            background-color: #C0C0C0;
        }
        QPushButton:disabled {
            background-color: #F0F0F0;
            color: #808080;
        }
    )");
}

void AlarmWidget::setupActiveAlarmsTab()
{
    m_activeAlarmsTab = new QWidget();
    m_tabWidget->addTab(m_activeAlarmsTab, "激活报警");
    
    auto layout = new QVBoxLayout(m_activeAlarmsTab);
    
    // 创建控制面板
    auto controlPanel = new QGroupBox("控制面板", m_activeAlarmsTab);
    auto controlLayout = new QHBoxLayout(controlPanel);
    
    // 过滤器
    controlLayout->addWidget(new QLabel("类型:"));
    m_alarmTypeFilter = new QComboBox();
    m_alarmTypeFilter->addItems({"全部", "系统", "设备", "工艺", "质量", "安全", "通讯", "温度", "压力", "位置", "速度"});
    controlLayout->addWidget(m_alarmTypeFilter);
    
    controlLayout->addWidget(new QLabel("级别:"));
    m_alarmLevelFilter = new QComboBox();
    m_alarmLevelFilter->addItems({"全部", "信息", "警告", "错误", "严重", "紧急"});
    controlLayout->addWidget(m_alarmLevelFilter);
    
    controlLayout->addWidget(new QLabel("状态:"));
    m_alarmStatusFilter = new QComboBox();
    m_alarmStatusFilter->addItems({"全部", "激活", "已确认", "已解决", "已抑制"});
    controlLayout->addWidget(m_alarmStatusFilter);
    
    // 搜索框
    controlLayout->addWidget(new QLabel("搜索:"));
    m_alarmSearchEdit = new QLineEdit();
    m_alarmSearchEdit->setPlaceholderText("搜索报警信息...");
    controlLayout->addWidget(m_alarmSearchEdit);
    
    controlLayout->addStretch();
    layout->addWidget(controlPanel);
    
    // 创建操作按钮面板
    auto buttonPanel = new QGroupBox("操作", m_activeAlarmsTab);
    auto buttonLayout = new QHBoxLayout(buttonPanel);
    
    m_acknowledgeBtn = new QPushButton("确认");
    m_acknowledgeBtn->setIcon(QIcon(":/icons/check.png"));
    m_resolveBtn = new QPushButton("解决");
    m_resolveBtn->setIcon(QIcon(":/icons/resolve.png"));
    m_suppressBtn = new QPushButton("抑制");
    m_suppressBtn->setIcon(QIcon(":/icons/suppress.png"));
    m_clearBtn = new QPushButton("清除");
    m_clearBtn->setIcon(QIcon(":/icons/clear.png"));
    m_clearAllBtn = new QPushButton("全部清除");
    m_clearAllBtn->setIcon(QIcon(":/icons/clear_all.png"));
    m_refreshBtn = new QPushButton("刷新");
    m_refreshBtn->setIcon(QIcon(":/icons/refresh.png"));
    m_exportBtn = new QPushButton("导出");
    m_exportBtn->setIcon(QIcon(":/icons/export.png"));
    m_configBtn = new QPushButton("配置");
    m_configBtn->setIcon(QIcon(":/icons/config.png"));
    
    buttonLayout->addWidget(m_acknowledgeBtn);
    buttonLayout->addWidget(m_resolveBtn);
    buttonLayout->addWidget(m_suppressBtn);
    buttonLayout->addWidget(m_clearBtn);
    buttonLayout->addWidget(m_clearAllBtn);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_refreshBtn);
    buttonLayout->addWidget(m_exportBtn);
    buttonLayout->addWidget(m_configBtn);
    buttonLayout->addStretch();
    
    layout->addWidget(buttonPanel);
    
    // 创建统计面板
    auto statsPanel = new QGroupBox("统计信息", m_activeAlarmsTab);
    auto statsLayout = new QHBoxLayout(statsPanel);
    
    m_totalAlarmsLabel = new QLabel("总报警: 0");
    m_activeAlarmsLabel = new QLabel("激活: 0");
    m_unacknowledgedLabel = new QLabel("未确认: 0");
    m_criticalAlarmsLabel = new QLabel("严重: 0");
    
    statsLayout->addWidget(m_totalAlarmsLabel);
    statsLayout->addWidget(m_activeAlarmsLabel);
    statsLayout->addWidget(m_unacknowledgedLabel);
    statsLayout->addWidget(m_criticalAlarmsLabel);
    statsLayout->addStretch();
    
    layout->addWidget(statsPanel);
    
    // 创建报警表格
    m_activeAlarmsTable = new QTableWidget(0, 15, m_activeAlarmsTab);
    QStringList headers = {"ID", "类型", "级别", "状态", "代码", "信息", "设备", "参数", "当前值", 
                          "阈值", "发生时间", "确认时间", "操作员", "次数", "备注"};
    m_activeAlarmsTable->setHorizontalHeaderLabels(headers);
    m_activeAlarmsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_activeAlarmsTable->setAlternatingRowColors(true);
    m_activeAlarmsTable->setSortingEnabled(true);
    m_activeAlarmsTable->verticalHeader()->setVisible(false);
    m_activeAlarmsTable->horizontalHeader()->setStretchLastSection(true);
    
    layout->addWidget(m_activeAlarmsTable);
    
    // 创建数据模型
    m_activeAlarmsModel = new QStandardItemModel(this);
    m_activeAlarmsModel->setHorizontalHeaderLabels(headers);
    
    m_activeAlarmsProxy = new QSortFilterProxyModel(this);
    m_activeAlarmsProxy->setSourceModel(m_activeAlarmsModel);
    m_activeAlarmsProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_activeAlarmsProxy->setFilterKeyColumn(-1); // 搜索所有列
}

void AlarmWidget::setupHistoryTab()
{
    m_historyTab = new QWidget();
    m_tabWidget->addTab(m_historyTab, "历史记录");
    
    auto layout = new QVBoxLayout(m_historyTab);
    
    // 创建查询面板
    auto queryPanel = new QGroupBox("查询条件", m_historyTab);
    auto queryLayout = new QHBoxLayout(queryPanel);
    
    queryLayout->addWidget(new QLabel("开始时间:"));
    m_historyStartDate = new QDateTimeEdit(QDateTime::currentDateTime().addDays(-7));
    m_historyStartDate->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
    queryLayout->addWidget(m_historyStartDate);
    
    queryLayout->addWidget(new QLabel("结束时间:"));
    m_historyEndDate = new QDateTimeEdit(QDateTime::currentDateTime());
    m_historyEndDate->setDisplayFormat("yyyy-MM-dd hh:mm:ss");
    queryLayout->addWidget(m_historyEndDate);
    
    queryLayout->addWidget(new QLabel("类型:"));
    m_historyTypeFilter = new QComboBox();
    m_historyTypeFilter->addItems({"全部", "系统", "设备", "工艺", "质量", "安全", "通讯", "温度", "压力", "位置", "速度"});
    queryLayout->addWidget(m_historyTypeFilter);
    
    queryLayout->addWidget(new QLabel("级别:"));
    m_historyLevelFilter = new QComboBox();
    m_historyLevelFilter->addItems({"全部", "信息", "警告", "错误", "严重", "紧急"});
    queryLayout->addWidget(m_historyLevelFilter);
    
    m_historySearchBtn = new QPushButton("查询");
    m_historySearchBtn->setIcon(QIcon(":/icons/search.png"));
    queryLayout->addWidget(m_historySearchBtn);
    
    queryLayout->addStretch();
    layout->addWidget(queryPanel);
    
    // 创建操作面板
    auto historyButtonPanel = new QGroupBox("操作", m_historyTab);
    auto historyButtonLayout = new QHBoxLayout(historyButtonPanel);
    
    m_historyExportBtn = new QPushButton("导出");
    m_historyExportBtn->setIcon(QIcon(":/icons/export.png"));
    m_historyClearBtn = new QPushButton("清理历史");
    m_historyClearBtn->setIcon(QIcon(":/icons/clear.png"));
    
    historyButtonLayout->addWidget(m_historyExportBtn);
    historyButtonLayout->addWidget(m_historyClearBtn);
    
    m_historyCountLabel = new QLabel("记录数: 0");
    historyButtonLayout->addWidget(m_historyCountLabel);
    historyButtonLayout->addStretch();
    
    layout->addWidget(historyButtonPanel);
    
    // 创建历史记录表格
    m_historyTable = new QTableWidget(0, 16, m_historyTab);
    QStringList historyHeaders = {"ID", "类型", "级别", "状态", "代码", "信息", "设备", "参数", "当前值", 
                                 "阈值", "发生时间", "确认时间", "解决时间", "操作员", "解决方案", "备注"};
    m_historyTable->setHorizontalHeaderLabels(historyHeaders);
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setAlternatingRowColors(true);
    m_historyTable->setSortingEnabled(true);
    m_historyTable->verticalHeader()->setVisible(false);
    m_historyTable->horizontalHeader()->setStretchLastSection(true);
    
    layout->addWidget(m_historyTable);
    
    // 创建数据模型
    m_historyModel = new QStandardItemModel(this);
    m_historyModel->setHorizontalHeaderLabels(historyHeaders);
    
    m_historyProxy = new QSortFilterProxyModel(this);
    m_historyProxy->setSourceModel(m_historyModel);
    m_historyProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
}

void AlarmWidget::setupThresholdsTab()
{
    m_thresholdsTab = new QWidget();
    m_tabWidget->addTab(m_thresholdsTab, "阈值配置");
    
    auto layout = new QVBoxLayout(m_thresholdsTab);
    
    // 创建控制面板
    auto thresholdControlPanel = new QGroupBox("控制", m_thresholdsTab);
    auto thresholdControlLayout = new QHBoxLayout(thresholdControlPanel);
    
    m_enableThresholdsCheckBox = new QCheckBox("启用阈值检查");
    m_enableThresholdsCheckBox->setChecked(true);
    thresholdControlLayout->addWidget(m_enableThresholdsCheckBox);
    
    thresholdControlLayout->addStretch();
    
    m_addThresholdBtn = new QPushButton("添加");
    m_addThresholdBtn->setIcon(QIcon(":/icons/add.png"));
    m_editThresholdBtn = new QPushButton("编辑");
    m_editThresholdBtn->setIcon(QIcon(":/icons/edit.png"));
    m_deleteThresholdBtn = new QPushButton("删除");
    m_deleteThresholdBtn->setIcon(QIcon(":/icons/delete.png"));
    m_importThresholdsBtn = new QPushButton("导入");
    m_importThresholdsBtn->setIcon(QIcon(":/icons/import.png"));
    m_exportThresholdsBtn = new QPushButton("导出");
    m_exportThresholdsBtn->setIcon(QIcon(":/icons/export.png"));
    
    thresholdControlLayout->addWidget(m_addThresholdBtn);
    thresholdControlLayout->addWidget(m_editThresholdBtn);
    thresholdControlLayout->addWidget(m_deleteThresholdBtn);
    thresholdControlLayout->addStretch();
    thresholdControlLayout->addWidget(m_importThresholdsBtn);
    thresholdControlLayout->addWidget(m_exportThresholdsBtn);
    
    layout->addWidget(thresholdControlPanel);
    
    // 创建阈值表格
    m_thresholdsTable = new QTableWidget(0, 12, m_thresholdsTab);
    QStringList thresholdHeaders = {"参数名称", "类型", "级别", "高高限", "高限", "低限", "低低限", 
                                   "延时(s)", "死区", "启用", "状态", "备注"};
    m_thresholdsTable->setHorizontalHeaderLabels(thresholdHeaders);
    m_thresholdsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_thresholdsTable->setAlternatingRowColors(true);
    m_thresholdsTable->setSortingEnabled(true);
    m_thresholdsTable->verticalHeader()->setVisible(false);
    m_thresholdsTable->horizontalHeader()->setStretchLastSection(true);
    
    layout->addWidget(m_thresholdsTable);
    
    // 创建数据模型
    m_thresholdsModel = new QStandardItemModel(this);
    m_thresholdsModel->setHorizontalHeaderLabels(thresholdHeaders);
    
    m_thresholdsProxy = new QSortFilterProxyModel(this);
    m_thresholdsProxy->setSourceModel(m_thresholdsModel);
}

void AlarmWidget::setupStatisticsTab()
{
    m_statisticsTab = new QWidget();
    m_tabWidget->addTab(m_statisticsTab, "统计分析");
    
    auto layout = new QVBoxLayout(m_statisticsTab);
    
    // 创建概览面板
    auto overviewPanel = new QGroupBox("概览", m_statisticsTab);
    auto overviewLayout = new QVBoxLayout(overviewPanel);
    
    m_statsOverviewLabel = new QLabel();
    m_statsOverviewLabel->setWordWrap(true);
    m_statsOverviewLabel->setStyleSheet("QLabel { font-size: 12px; padding: 10px; }");
    overviewLayout->addWidget(m_statsOverviewLabel);
    
    layout->addWidget(overviewPanel);
    
    // 创建控制面板
    auto statsControlPanel = new QGroupBox("控制", m_statisticsTab);
    auto statsControlLayout = new QHBoxLayout(statsControlPanel);
    
    m_updateStatsBtn = new QPushButton("更新统计");
    m_updateStatsBtn->setIcon(QIcon(":/icons/refresh.png"));
    m_resetStatsBtn = new QPushButton("重置统计");
    m_resetStatsBtn->setIcon(QIcon(":/icons/reset.png"));
    m_exportStatsBtn = new QPushButton("导出统计");
    m_exportStatsBtn->setIcon(QIcon(":/icons/export.png"));
    
    statsControlLayout->addWidget(m_updateStatsBtn);
    statsControlLayout->addWidget(m_resetStatsBtn);
    statsControlLayout->addWidget(m_exportStatsBtn);
    
    m_statsProgress = new QProgressBar();
    m_statsProgress->setVisible(false);
    statsControlLayout->addWidget(m_statsProgress);
    
    statsControlLayout->addStretch();
    
    layout->addWidget(statsControlPanel);
    
    // 创建统计表格
    m_statsTable = new QTableWidget(0, 6, m_statisticsTab);
    QStringList statsHeaders = {"统计项", "总数", "今日", "本周", "本月", "说明"};
    m_statsTable->setHorizontalHeaderLabels(statsHeaders);
    m_statsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_statsTable->setAlternatingRowColors(true);
    m_statsTable->verticalHeader()->setVisible(false);
    m_statsTable->horizontalHeader()->setStretchLastSection(true);
    
    layout->addWidget(m_statsTable);
    
    // 创建数据模型
    m_statisticsModel = new QStandardItemModel(this);
    m_statisticsModel->setHorizontalHeaderLabels(statsHeaders);
}

void AlarmWidget::setupConfigTab()
{
    m_configTab = new QWidget();
    m_tabWidget->addTab(m_configTab, "配置");
    
    auto layout = new QVBoxLayout(m_configTab);
    
    // 创建报警设置面板
    auto alarmSettingsPanel = new QGroupBox("报警设置", m_configTab);
    auto alarmSettingsLayout = new QGridLayout(alarmSettingsPanel);
    
    m_enableAudibleCheckBox = new QCheckBox("启用声音报警");
    m_enableVisualCheckBox = new QCheckBox("启用视觉报警");
    m_enableEmailCheckBox = new QCheckBox("启用邮件通知");
    m_enableSMSCheckBox = new QCheckBox("启用短信通知");
    m_enableTrayCheckBox = new QCheckBox("启用系统托盘通知");
    
    alarmSettingsLayout->addWidget(m_enableAudibleCheckBox, 0, 0);
    alarmSettingsLayout->addWidget(m_enableVisualCheckBox, 0, 1);
    alarmSettingsLayout->addWidget(m_enableEmailCheckBox, 1, 0);
    alarmSettingsLayout->addWidget(m_enableSMSCheckBox, 1, 1);
    alarmSettingsLayout->addWidget(m_enableTrayCheckBox, 2, 0);
    
    layout->addWidget(alarmSettingsPanel);
    
    // 创建参数设置面板
    auto parameterPanel = new QGroupBox("参数设置", m_configTab);
    auto parameterLayout = new QGridLayout(parameterPanel);
    
    parameterLayout->addWidget(new QLabel("最大激活报警数:"), 0, 0);
    m_maxActiveAlarmsSpinBox = new QSpinBox();
    m_maxActiveAlarmsSpinBox->setRange(1, 10000);
    m_maxActiveAlarmsSpinBox->setValue(100);
    parameterLayout->addWidget(m_maxActiveAlarmsSpinBox, 0, 1);
    
    parameterLayout->addWidget(new QLabel("自动确认时间(秒):"), 1, 0);
    m_autoAckTimeSpinBox = new QSpinBox();
    m_autoAckTimeSpinBox->setRange(0, 3600);
    m_autoAckTimeSpinBox->setValue(0);
    parameterLayout->addWidget(m_autoAckTimeSpinBox, 1, 1);
    
    parameterLayout->addWidget(new QLabel("声音持续时间(秒):"), 2, 0);
    m_soundDurationSpinBox = new QSpinBox();
    m_soundDurationSpinBox->setRange(1, 60);
    m_soundDurationSpinBox->setValue(5);
    parameterLayout->addWidget(m_soundDurationSpinBox, 2, 1);
    
    layout->addWidget(parameterPanel);
    
    // 创建声音设置面板
    auto soundPanel = new QGroupBox("声音设置", m_configTab);
    auto soundLayout = new QHBoxLayout(soundPanel);
    
    soundLayout->addWidget(new QLabel("声音文件:"));
    m_soundFileEdit = new QLineEdit();
    m_soundFileEdit->setReadOnly(true);
    soundLayout->addWidget(m_soundFileEdit);
    
    m_browseSoundBtn = new QPushButton("浏览");
    m_browseSoundBtn->setIcon(QIcon(":/icons/open.png"));
    soundLayout->addWidget(m_browseSoundBtn);
    
    m_testSoundBtn = new QPushButton("测试");
    m_testSoundBtn->setIcon(QIcon(":/icons/play.png"));
    soundLayout->addWidget(m_testSoundBtn);
    
    layout->addWidget(soundPanel);
    
    // 创建通知设置面板
    auto notificationPanel = new QGroupBox("通知设置", m_configTab);
    auto notificationLayout = new QVBoxLayout(notificationPanel);
    
    notificationLayout->addWidget(new QLabel("邮件接收者(每行一个):"));
    m_emailRecipientsEdit = new QTextEdit();
    m_emailRecipientsEdit->setMaximumHeight(80);
    notificationLayout->addWidget(m_emailRecipientsEdit);
    
    notificationLayout->addWidget(new QLabel("短信接收者(每行一个):"));
    m_smsRecipientsEdit = new QTextEdit();
    m_smsRecipientsEdit->setMaximumHeight(80);
    notificationLayout->addWidget(m_smsRecipientsEdit);
    
    layout->addWidget(notificationPanel);
    
    // 创建操作按钮面板
    auto configButtonPanel = new QGroupBox("操作", m_configTab);
    auto configButtonLayout = new QHBoxLayout(configButtonPanel);
    
    m_saveConfigBtn = new QPushButton("保存配置");
    m_saveConfigBtn->setIcon(QIcon(":/icons/save.png"));
    m_resetConfigBtn = new QPushButton("重置配置");
    m_resetConfigBtn->setIcon(QIcon(":/icons/reset.png"));
    
    configButtonLayout->addWidget(m_saveConfigBtn);
    configButtonLayout->addWidget(m_resetConfigBtn);
    configButtonLayout->addStretch();
    
    layout->addWidget(configButtonPanel);
    
    layout->addStretch();
}

void AlarmWidget::setupDatabase()
{
    // 设置数据库路径
    m_databasePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/alarms.db";
    
    // 确保目录存在
    QDir dir(QFileInfo(m_databasePath).absolutePath());
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    
    // 初始化数据库
    if (!initializeDatabase()) {
        LogManager::getInstance()->error("报警数据库初始化失败", "AlarmWidget");
        return;
    }
    
    LogManager::getInstance()->info("报警数据库初始化成功: " + m_databasePath, "AlarmWidget");
}

bool AlarmWidget::initializeDatabase()
{
    // 创建数据库连接
    m_database = QSqlDatabase::addDatabase("QSQLITE", "alarm_db");
    m_database.setDatabaseName(m_databasePath);
    
    if (!m_database.open()) {
        LogManager::getInstance()->error("无法打开报警数据库: " + m_database.lastError().text(), "AlarmWidget");
        return false;
    }
    
    // 创建表格
    return createTables();
}

bool AlarmWidget::createTables()
{
    QSqlQuery query(m_database);
    
    // 创建报警记录表
    QString createAlarmTable = R"(
        CREATE TABLE IF NOT EXISTS alarm_records (
            alarm_id INTEGER PRIMARY KEY AUTOINCREMENT,
            alarm_type INTEGER NOT NULL,
            alarm_level INTEGER NOT NULL,
            alarm_status INTEGER NOT NULL,
            alarm_code TEXT NOT NULL,
            alarm_message TEXT NOT NULL,
            device_name TEXT NOT NULL,
            parameter_name TEXT,
            parameter_value REAL DEFAULT 0,
            threshold_value REAL DEFAULT 0,
            timestamp DATETIME NOT NULL,
            acknowledge_time DATETIME,
            resolve_time DATETIME,
            operator_name TEXT,
            acknowledge_user TEXT,
            resolve_user TEXT,
            solution TEXT,
            notes TEXT,
            count INTEGER DEFAULT 1,
            is_audible BOOLEAN DEFAULT TRUE,
            is_visible BOOLEAN DEFAULT TRUE,
            category TEXT,
            priority INTEGER DEFAULT 1,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createAlarmTable)) {
        LogManager::getInstance()->error("创建报警记录表失败: " + query.lastError().text(), "AlarmWidget");
        return false;
    }
    
    // 创建报警阈值表
    QString createThresholdTable = R"(
        CREATE TABLE IF NOT EXISTS alarm_thresholds (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            parameter_name TEXT NOT NULL UNIQUE,
            alarm_type INTEGER NOT NULL,
            alarm_level INTEGER NOT NULL,
            high_high REAL DEFAULT 0,
            high_value REAL DEFAULT 0,
            low_value REAL DEFAULT 0,
            low_low REAL DEFAULT 0,
            enable_high_high BOOLEAN DEFAULT FALSE,
            enable_high BOOLEAN DEFAULT TRUE,
            enable_low BOOLEAN DEFAULT TRUE,
            enable_low_low BOOLEAN DEFAULT FALSE,
            delay_time INTEGER DEFAULT 0,
            deadband INTEGER DEFAULT 0,
            is_enabled BOOLEAN DEFAULT TRUE,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createThresholdTable)) {
        LogManager::getInstance()->error("创建报警阈值表失败: " + query.lastError().text(), "AlarmWidget");
        return false;
    }
    
    // 创建报警配置表
    QString createConfigTable = R"(
        CREATE TABLE IF NOT EXISTS alarm_config (
            id INTEGER PRIMARY KEY,
            enable_audible BOOLEAN DEFAULT TRUE,
            enable_visual BOOLEAN DEFAULT TRUE,
            enable_email BOOLEAN DEFAULT FALSE,
            enable_sms BOOLEAN DEFAULT FALSE,
            enable_tray BOOLEAN DEFAULT TRUE,
            max_active_alarms INTEGER DEFAULT 100,
            auto_acknowledge_time INTEGER DEFAULT 0,
            sound_duration INTEGER DEFAULT 5,
            sound_file TEXT,
            email_recipients TEXT,
            sms_recipients TEXT,
            enable_history BOOLEAN DEFAULT TRUE,
            max_history_records INTEGER DEFAULT 10000,
            enable_statistics BOOLEAN DEFAULT TRUE,
            statistics_update_interval INTEGER DEFAULT 60,
            created_at DATETIME DEFAULT CURRENT_TIMESTAMP,
            updated_at DATETIME DEFAULT CURRENT_TIMESTAMP
        )
    )";
    
    if (!query.exec(createConfigTable)) {
        LogManager::getInstance()->error("创建报警配置表失败: " + query.lastError().text(), "AlarmWidget");
        return false;
    }
    
    // 创建索引
    query.exec("CREATE INDEX IF NOT EXISTS idx_alarm_timestamp ON alarm_records(timestamp)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_alarm_type ON alarm_records(alarm_type)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_alarm_level ON alarm_records(alarm_level)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_alarm_status ON alarm_records(alarm_status)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_alarm_device ON alarm_records(device_name)");
    query.exec("CREATE INDEX IF NOT EXISTS idx_threshold_parameter ON alarm_thresholds(parameter_name)");
    
    return true;
}

void AlarmWidget::showVisualAlarm(const AlarmRecord& alarm)
{
    // 简化实现：在状态栏显示报警信息
    QString message = QString("【%1】%2").arg(formatAlarmLevel(static_cast<AlarmLevel>(alarm.alarmLevel))).arg(alarm.alarmMessage);
    
    // 可以在这里添加更复杂的视觉效果，如闪烁、颜色变化等
    // 暂时只记录日志
    LogManager::getInstance()->info("视觉报警: " + message, "AlarmWidget");
}

void AlarmWidget::sendEmailNotification(const AlarmRecord& alarm)
{
    // 简化实现：记录邮件发送请求
    QString message = QString("邮件通知: 【%1】%2 - 设备: %3")
                     .arg(formatAlarmLevel(static_cast<AlarmLevel>(alarm.alarmLevel)))
                     .arg(alarm.alarmMessage)
                     .arg(alarm.deviceName);
    
    LogManager::getInstance()->info(message, "AlarmWidget");
    
    // 在实际项目中，这里应该调用邮件发送服务
}

void AlarmWidget::sendSMSNotification(const AlarmRecord& alarm)
{
    // 简化实现：记录短信发送请求
    QString message = QString("短信通知: 【%1】%2 - 设备: %3")
                     .arg(formatAlarmLevel(static_cast<AlarmLevel>(alarm.alarmLevel)))
                     .arg(alarm.alarmMessage)
                     .arg(alarm.deviceName);
    
    LogManager::getInstance()->info(message, "AlarmWidget");
    
    // 在实际项目中，这里应该调用短信发送服务
}

void AlarmWidget::showSystemTrayNotification(const AlarmRecord& alarm)
{
    if (m_systemTray && m_systemTray->isVisible()) {
        QString title = QString("报警通知 - %1").arg(formatAlarmLevel(static_cast<AlarmLevel>(alarm.alarmLevel)));
        QString message = QString("设备: %1\n%2").arg(alarm.deviceName).arg(alarm.alarmMessage);
        
        QSystemTrayIcon::MessageIcon icon;
        switch (static_cast<AlarmLevel>(alarm.alarmLevel)) {
            case AlarmLevel::Info:
                icon = QSystemTrayIcon::Information;
                break;
            case AlarmLevel::Warning:
                icon = QSystemTrayIcon::Warning;
                break;
            case AlarmLevel::Error:
            case AlarmLevel::Critical:
            case AlarmLevel::Emergency:
                icon = QSystemTrayIcon::Critical;
                break;
            default:
                icon = QSystemTrayIcon::Information;
                break;
        }
        
        m_systemTray->showMessage(title, message, icon, 5000);
        LogManager::getInstance()->info("系统托盘通知: " + title, "AlarmWidget");
    }
}

QString AlarmWidget::formatAlarmLevel(AlarmLevel level) const
{
    switch (level) {
        case AlarmLevel::Info:
            return "信息";
        case AlarmLevel::Warning:
            return "警告";
        case AlarmLevel::Error:
            return "错误";
        case AlarmLevel::Critical:
            return "严重";
        case AlarmLevel::Emergency:
            return "紧急";
        default:
            return "未知";
    }
}

QString AlarmWidget::formatAlarmType(AlarmType type) const
{
    switch (type) {
        case AlarmType::System:
            return "系统报警";
        case AlarmType::Device:
            return "设备报警";
        case AlarmType::Process:
            return "工艺报警";
        case AlarmType::Quality:
            return "质量报警";
        case AlarmType::Safety:
            return "安全报警";
        case AlarmType::Communication:
            return "通信报警";
        case AlarmType::Temperature:
            return "温度报警";
        case AlarmType::Pressure:
            return "压力报警";
        case AlarmType::Position:
            return "位置报警";
        case AlarmType::Speed:
            return "速度报警";
        default:
            return "未知类型";
    }
}

QString AlarmWidget::formatAlarmStatus(AlarmStatus status) const
{
    switch (status) {
        case AlarmStatus::Active:
            return "激活";
        case AlarmStatus::Acknowledged:
            return "已确认";
        case AlarmStatus::Resolved:
            return "已解决";
        case AlarmStatus::Suppressed:
            return "已抑制";
        default:
            return "未知状态";
    }
}

QString AlarmWidget::formatDuration(qint64 milliseconds) const
{
    qint64 seconds = milliseconds / 1000;
    qint64 minutes = seconds / 60;
    qint64 hours = minutes / 60;
    qint64 days = hours / 24;
    
    if (days > 0) {
        return QString("%1天%2时%3分").arg(days).arg(hours % 24).arg(minutes % 60);
    } else if (hours > 0) {
        return QString("%1时%2分%3秒").arg(hours).arg(minutes % 60).arg(seconds % 60);
    } else if (minutes > 0) {
        return QString("%1分%2秒").arg(minutes).arg(seconds % 60);
    } else {
        return QString("%1秒").arg(seconds);
    }
}

QString AlarmWidget::formatDateTime(const QDateTime& dateTime) const
{
    return dateTime.toString("yyyy-MM-dd hh:mm:ss");
}

void AlarmWidget::updateAlarmThreshold(const AlarmThreshold& threshold)
{
    QSqlQuery query(m_database);
    query.prepare(R"(
        UPDATE alarm_thresholds SET
            type = ?, level = ?, high_high = ?, high = ?, low = ?, low_low = ?,
            enable_high_high = ?, enable_high = ?, enable_low = ?, enable_low_low = ?,
            delay_time = ?, deadband = ?, is_enabled = ?, update_time = ?
        WHERE parameter_name = ?
    )");
    
    query.addBindValue(static_cast<int>(threshold.type));
    query.addBindValue(static_cast<int>(threshold.level));
    query.addBindValue(threshold.highHigh);
    query.addBindValue(threshold.high);
    query.addBindValue(threshold.low);
    query.addBindValue(threshold.lowLow);
    query.addBindValue(threshold.enableHighHigh);
    query.addBindValue(threshold.enableHigh);
    query.addBindValue(threshold.enableLow);
    query.addBindValue(threshold.enableLowLow);
    query.addBindValue(threshold.delayTime);
    query.addBindValue(threshold.deadband);
    query.addBindValue(threshold.isEnabled);
    query.addBindValue(QDateTime::currentDateTime());
    query.addBindValue(threshold.parameterName);
    
    if (query.exec()) {
        // 更新内存中的阈值
        for (int i = 0; i < m_alarmThresholds.size(); ++i) {
            if (m_alarmThresholds[i].parameterName == threshold.parameterName) {
                m_alarmThresholds[i] = threshold;
                break;
            }
        }
        
        updateThresholdsTable();
        LogManager::getInstance()->info("更新报警阈值: " + threshold.parameterName, "AlarmWidget");
    } else {
        LogManager::getInstance()->error("更新报警阈值失败: " + query.lastError().text(), "AlarmWidget");
    }
}

void AlarmWidget::calculateStatistics()
{
    QSqlQuery query(m_database);
    
    // 重置统计数据
    m_alarmStatistics = AlarmStatistics();
    m_alarmStatistics.lastUpdateTime = QDateTime::currentDateTime();
    
    // 计算总报警数
    query.exec("SELECT COUNT(*) FROM alarm_records");
    if (query.next()) {
        m_alarmStatistics.totalAlarms = query.value(0).toInt();
    }
    
    // 计算激活报警数
    query.exec("SELECT COUNT(*) FROM alarm_records WHERE status = 0");
    if (query.next()) {
        m_alarmStatistics.activeAlarms = query.value(0).toInt();
    }
    
    // 计算已确认报警数
    query.exec("SELECT COUNT(*) FROM alarm_records WHERE status = 1");
    if (query.next()) {
        m_alarmStatistics.acknowledgedAlarms = query.value(0).toInt();
    }
    
    // 计算已解决报警数
    query.exec("SELECT COUNT(*) FROM alarm_records WHERE status = 2");
    if (query.next()) {
        m_alarmStatistics.resolvedAlarms = query.value(0).toInt();
    }
    
    // 计算按类型统计
    query.exec("SELECT type, COUNT(*) FROM alarm_records GROUP BY type");
    while (query.next()) {
        AlarmType type = static_cast<AlarmType>(query.value(0).toInt());
        int count = query.value(1).toInt();
        m_alarmStatistics.alarmsByType[type] = count;
    }
    
    // 计算按级别统计
    query.exec("SELECT level, COUNT(*) FROM alarm_records GROUP BY level");
    while (query.next()) {
        AlarmLevel level = static_cast<AlarmLevel>(query.value(0).toInt());
        int count = query.value(1).toInt();
        m_alarmStatistics.alarmsByLevel[level] = count;
    }
    
    // 计算平均响应时间
    query.exec("SELECT AVG(acknowledge_time - trigger_time) FROM alarm_records WHERE acknowledge_time IS NOT NULL");
    if (query.next()) {
        m_alarmStatistics.averageResponseTime = query.value(0).toDouble();
    }
    
    // 计算平均解决时间
    query.exec("SELECT AVG(resolve_time - trigger_time) FROM alarm_records WHERE resolve_time IS NOT NULL");
    if (query.next()) {
        m_alarmStatistics.averageResolveTime = query.value(0).toDouble();
    }
    
    LogManager::getInstance()->info("报警统计已更新", "AlarmWidget");
}

void AlarmWidget::removeAlarmThreshold(const QString& parameterName)
{
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM alarm_thresholds WHERE parameter_name = ?");
    query.addBindValue(parameterName);
    
    if (query.exec()) {
        // 从内存中移除
        for (int i = 0; i < m_alarmThresholds.size(); ++i) {
            if (m_alarmThresholds[i].parameterName == parameterName) {
                m_alarmThresholds.removeAt(i);
                break;
            }
        }
        
        updateThresholdsTable();
        LogManager::getInstance()->info("删除报警阈值: " + parameterName, "AlarmWidget");
    } else {
        LogManager::getInstance()->error("删除报警阈值失败: " + query.lastError().text(), "AlarmWidget");
    }
}

void AlarmWidget::processAlarm(const AlarmRecord& alarm)
{
    // 处理报警记录
    insertAlarmRecord(alarm);
    
    // 播放报警声音
    if (m_alarmConfig.enableAudibleAlarms) {
        playAlarmSound(static_cast<AlarmLevel>(alarm.alarmLevel));
    }
    
    // 视觉报警
    if (m_alarmConfig.enableVisualAlarms) {
        showVisualAlarm(alarm);
    }
    
    // 邮件通知
    if (m_alarmConfig.enableEmailNotification) {
        sendEmailNotification(alarm);
    }
    
    // 短信通知
    if (m_alarmConfig.enableSMSNotification) {
        sendSMSNotification(alarm);
    }
    
    // 系统托盘通知
    if (m_alarmConfig.enableSystemTray) {
        showSystemTrayNotification(alarm);
    }
    
    // 更新界面
    updateActiveAlarmsTable();
    updateStatisticsDisplay();
    
    LogManager::getInstance()->info("处理报警: " + alarm.alarmMessage, "AlarmWidget");
}

void AlarmWidget::onSortChanged()
{
    // 重新排序表格
    updateActiveAlarmsTable();
    updateHistoryTable();
}

void AlarmWidget::checkParameter(const QString& parameterName, double value)
{
    // 检查参数是否触发报警阈值
    for (const AlarmThreshold& threshold : m_alarmThresholds) {
        if (threshold.parameterName == parameterName && threshold.isEnabled) {
            bool triggered = false;
            AlarmLevel level = threshold.level;
            QString message;
            
            if (threshold.enableHighHigh && value > threshold.highHigh) {
                triggered = true;
                message = QString("参数 %1 超过高高限 (%.2f > %.2f)").arg(parameterName).arg(value).arg(threshold.highHigh);
            } else if (threshold.enableHigh && value > threshold.high) {
                triggered = true;
                message = QString("参数 %1 超过高限 (%.2f > %.2f)").arg(parameterName).arg(value).arg(threshold.high);
            } else if (threshold.enableLow && value < threshold.low) {
                triggered = true;
                message = QString("参数 %1 低于低限 (%.2f < %.2f)").arg(parameterName).arg(value).arg(threshold.low);
            } else if (threshold.enableLowLow && value < threshold.lowLow) {
                triggered = true;
                message = QString("参数 %1 低于低低限 (%.2f < %.2f)").arg(parameterName).arg(value).arg(threshold.lowLow);
            }
            
            if (triggered) {
                AlarmRecord alarm;
                alarm.alarmType = static_cast<int>(threshold.type);
                alarm.alarmLevel = static_cast<int>(level);
                alarm.deviceName = parameterName;
                alarm.alarmMessage = message;
                alarm.timestamp = QDateTime::currentDateTime();
                alarm.alarmStatus = static_cast<int>(AlarmStatus::Active);
                alarm.parameterValue = value;
                
                triggerAlarm(alarm);
            }
        }
    }
}

void AlarmWidget::resetStatistics()
{
    m_alarmStatistics = AlarmStatistics();
    m_alarmStatistics.lastUpdateTime = QDateTime::currentDateTime();
    updateStatisticsDisplay();
    LogManager::getInstance()->info("报警统计已重置", "AlarmWidget");
}

void AlarmWidget::addAlarmThreshold(const AlarmThreshold& threshold)
{
    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO alarm_thresholds (
            parameter_name, type, level, high_high, high, low, low_low,
            enable_high_high, enable_high, enable_low, enable_low_low,
            delay_time, deadband, is_enabled, create_time, update_time
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    
    query.addBindValue(threshold.parameterName);
    query.addBindValue(static_cast<int>(threshold.type));
    query.addBindValue(static_cast<int>(threshold.level));
    query.addBindValue(threshold.highHigh);
    query.addBindValue(threshold.high);
    query.addBindValue(threshold.low);
    query.addBindValue(threshold.lowLow);
    query.addBindValue(threshold.enableHighHigh);
    query.addBindValue(threshold.enableHigh);
    query.addBindValue(threshold.enableLow);
    query.addBindValue(threshold.enableLowLow);
    query.addBindValue(threshold.delayTime);
    query.addBindValue(threshold.deadband);
    query.addBindValue(threshold.isEnabled);
    query.addBindValue(QDateTime::currentDateTime());
    query.addBindValue(QDateTime::currentDateTime());
    
    if (query.exec()) {
        m_alarmThresholds.append(threshold);
        updateThresholdsTable();
        LogManager::getInstance()->info("添加报警阈值: " + threshold.parameterName, "AlarmWidget");
    } else {
        LogManager::getInstance()->error("添加报警阈值失败: " + query.lastError().text(), "AlarmWidget");
    }
}

bool AlarmWidget::insertAlarmRecord(const AlarmRecord& alarm)
{
    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT INTO alarm_records (
            type, level, source, message, trigger_time, status, value,
            acknowledge_time, acknowledge_user, resolve_time, resolve_user,
            solution, suppress_reason, is_suppressed
        ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    
    query.addBindValue(static_cast<int>(alarm.alarmType));
    query.addBindValue(static_cast<int>(alarm.alarmLevel));
    query.addBindValue(alarm.deviceName);
    query.addBindValue(alarm.alarmMessage);
    query.addBindValue(alarm.timestamp);
    query.addBindValue(static_cast<int>(alarm.alarmStatus));
    query.addBindValue(alarm.parameterValue);
    query.addBindValue(alarm.acknowledgeTime);
    query.addBindValue(alarm.acknowledgeUser);
    query.addBindValue(alarm.resolveTime);
    query.addBindValue(alarm.resolveUser);
    query.addBindValue(alarm.solution);
    query.addBindValue(alarm.notes);
    query.addBindValue(false); // isSuppressed 字段不存在，用false代替
    
    if (!query.exec()) {
        LogManager::getInstance()->error("插入报警记录失败: " + query.lastError().text(), "AlarmWidget");
        return false;
    }
    return true;
}

bool AlarmWidget::updateAlarmRecord(const AlarmRecord& alarm)
{
    QSqlQuery query(m_database);
    query.prepare(R"(
        UPDATE alarm_records SET
            type = ?, level = ?, source = ?, message = ?, trigger_time = ?,
            status = ?, value = ?, acknowledge_time = ?, acknowledge_user = ?,
            resolve_time = ?, resolve_user = ?, solution = ?, suppress_reason = ?,
            is_suppressed = ?
        WHERE id = ?
    )");
    
    query.addBindValue(static_cast<int>(alarm.alarmType));
    query.addBindValue(static_cast<int>(alarm.alarmLevel));
    query.addBindValue(alarm.deviceName);
    query.addBindValue(alarm.alarmMessage);
    query.addBindValue(alarm.timestamp);
    query.addBindValue(static_cast<int>(alarm.alarmStatus));
    query.addBindValue(alarm.parameterValue);
    query.addBindValue(alarm.acknowledgeTime);
    query.addBindValue(alarm.acknowledgeUser);
    query.addBindValue(alarm.resolveTime);
    query.addBindValue(alarm.resolveUser);
    query.addBindValue(alarm.solution);
    query.addBindValue(alarm.notes);
    query.addBindValue(false); // isSuppressed 字段不存在，用false代替
    query.addBindValue(alarm.alarmId);
    
    if (!query.exec()) {
        LogManager::getInstance()->error("更新报警记录失败: " + query.lastError().text(), "AlarmWidget");
        return false;
    }
    return true;
}

void AlarmWidget::setupConnections()
{
    // 激活报警页面连接
    connect(m_alarmTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlarmWidget::onFilterChanged);
    connect(m_alarmLevelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlarmWidget::onFilterChanged);
    connect(m_alarmStatusFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlarmWidget::onFilterChanged);
    connect(m_alarmSearchEdit, &QLineEdit::textChanged, this, &AlarmWidget::onFilterChanged);
    
    connect(m_acknowledgeBtn, &QPushButton::clicked, this, &AlarmWidget::onAcknowledgeSelected);
    connect(m_resolveBtn, &QPushButton::clicked, this, &AlarmWidget::onResolveSelected);
    connect(m_suppressBtn, &QPushButton::clicked, this, &AlarmWidget::onSuppressSelected);
    connect(m_clearBtn, &QPushButton::clicked, this, &AlarmWidget::onClearSelected);
    connect(m_clearAllBtn, &QPushButton::clicked, this, &AlarmWidget::onClearAll);
    connect(m_refreshBtn, &QPushButton::clicked, this, &AlarmWidget::onRefreshAlarms);
    connect(m_exportBtn, &QPushButton::clicked, this, &AlarmWidget::onExportAlarms);
    connect(m_configBtn, &QPushButton::clicked, this, &AlarmWidget::onConfigureAlarms);
    
    connect(m_activeAlarmsTable, &QTableWidget::itemSelectionChanged, 
            this, &AlarmWidget::onAlarmSelectionChanged);
    connect(m_activeAlarmsTable, &QTableWidget::itemDoubleClicked, 
            this, [this](QTableWidgetItem* item) {
                if (item && item->row() >= 0 && item->row() < m_activeAlarms.size()) {
                    showAlarmDetailsDialog(m_activeAlarms[item->row()]);
                }
            });
    
    // 历史记录页面连接
    connect(m_historySearchBtn, &QPushButton::clicked, this, &AlarmWidget::onShowHistory);
    connect(m_historyExportBtn, &QPushButton::clicked, this, &AlarmWidget::onExportAlarms);
    connect(m_historyClearBtn, &QPushButton::clicked, this, &AlarmWidget::cleanupOldAlarms);
    connect(m_historyStartDate, &QDateTimeEdit::dateTimeChanged, this, &AlarmWidget::onShowHistory);
    connect(m_historyEndDate, &QDateTimeEdit::dateTimeChanged, this, &AlarmWidget::onShowHistory);
    connect(m_historyTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlarmWidget::onShowHistory);
    connect(m_historyLevelFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &AlarmWidget::onShowHistory);
    
    // 阈值配置页面连接
    connect(m_enableThresholdsCheckBox, &QCheckBox::toggled, this, &AlarmWidget::onThresholdChanged);
    connect(m_addThresholdBtn, &QPushButton::clicked, this, [this]() {
        showThresholdDialog();
    });
    connect(m_editThresholdBtn, &QPushButton::clicked, this, [this]() {
        int row = m_thresholdsTable->currentRow();
        if (row >= 0 && row < m_alarmThresholds.size()) {
            showThresholdDialog(m_alarmThresholds[row]);
        }
    });
    connect(m_deleteThresholdBtn, &QPushButton::clicked, this, [this]() {
        int row = m_thresholdsTable->currentRow();
        if (row >= 0 && row < m_alarmThresholds.size()) {
            const QString& parameterName = m_alarmThresholds[row].parameterName;
            QMessageBox msgBox(this);
            msgBox.setWindowTitle("确认删除");
            msgBox.setText(QString("确定要删除参数 '%1' 的阈值配置吗？").arg(parameterName));
            msgBox.setIcon(QMessageBox::Question);
            
            QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
            QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
            msgBox.setDefaultButton(noButton);
            
            msgBox.exec();
            
            if (msgBox.clickedButton() == yesButton) {
                removeAlarmThreshold(parameterName);
            }
        }
    });
    connect(m_exportThresholdsBtn, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getSaveFileName(this, "导出阈值配置", 
            m_exportDirectory + "/thresholds.json", "JSON Files (*.json)");
        if (!fileName.isEmpty()) {
            // 导出阈值配置到JSON文件
            exportThresholdsToJson(fileName);
        }
    });
    connect(m_importThresholdsBtn, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "导入阈值配置", 
            m_exportDirectory, "JSON Files (*.json)");
        if (!fileName.isEmpty()) {
            // 从JSON文件导入阈值配置
            importThresholdsFromJson(fileName);
        }
    });
    
    // 统计分析页面连接
    connect(m_updateStatsBtn, &QPushButton::clicked, this, &AlarmWidget::updateStatistics);
    connect(m_resetStatsBtn, &QPushButton::clicked, this, &AlarmWidget::resetStatistics);
    connect(m_exportStatsBtn, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getSaveFileName(this, "导出统计数据", 
            m_exportDirectory + "/alarm_statistics.csv", "CSV Files (*.csv)");
        if (!fileName.isEmpty()) {
            exportStatisticsToCSV(fileName);
        }
    });
    
    // 配置页面连接
    connect(m_enableAudibleCheckBox, &QCheckBox::toggled, this, &AlarmWidget::onConfigChanged);
    connect(m_enableVisualCheckBox, &QCheckBox::toggled, this, &AlarmWidget::onConfigChanged);
    connect(m_enableEmailCheckBox, &QCheckBox::toggled, this, &AlarmWidget::onConfigChanged);
    connect(m_enableSMSCheckBox, &QCheckBox::toggled, this, &AlarmWidget::onConfigChanged);
    connect(m_enableTrayCheckBox, &QCheckBox::toggled, this, &AlarmWidget::onConfigChanged);
    connect(m_maxActiveAlarmsSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &AlarmWidget::onConfigChanged);
    connect(m_autoAckTimeSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &AlarmWidget::onConfigChanged);
    connect(m_soundDurationSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), 
            this, &AlarmWidget::onConfigChanged);
    
    connect(m_browseSoundBtn, &QPushButton::clicked, this, [this]() {
        QString fileName = QFileDialog::getOpenFileName(this, "选择声音文件", 
            m_soundDirectory, "Audio Files (*.wav *.mp3 *.ogg)");
        if (!fileName.isEmpty()) {
            m_soundFileEdit->setText(fileName);
            m_alarmConfig.alarmSoundFile = fileName;
        }
    });
    connect(m_testSoundBtn, &QPushButton::clicked, this, &AlarmWidget::onPlayAlarmSound);
    
    connect(m_saveConfigBtn, &QPushButton::clicked, this, [this]() {
        saveAlarmConfig();
        QMessageBox::information(this, "保存成功", "报警配置已保存");
    });
    connect(m_resetConfigBtn, &QPushButton::clicked, this, [this]() {
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("确认重置");
        msgBox.setText("确定要重置所有配置到默认值吗？");
        msgBox.setIcon(QMessageBox::Question);
        
        QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
        QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
        msgBox.setDefaultButton(noButton);
        
        msgBox.exec();
        
        if (msgBox.clickedButton() == yesButton) {
            resetAlarmConfig();
            loadAlarmConfig();
        }
    });
}

// 槽函数实现
void AlarmWidget::onUpdateTimer()
{
    // 定期更新报警状态
    updateAlarmSummary();
    
    // 检查自动确认
    if (m_alarmConfig.autoAcknowledgeTime > 0) {
        QDateTime currentTime = QDateTime::currentDateTime();
        for (auto& alarm : m_activeAlarms) {
            if (alarm.alarmStatus == static_cast<int>(AlarmStatus::Active)) {
                qint64 elapsedSeconds = alarm.timestamp.secsTo(currentTime);
                if (elapsedSeconds >= m_alarmConfig.autoAcknowledgeTime) {
                    acknowledgeAlarm(alarm.alarmId, "系统自动确认");
                }
            }
        }
    }
}

void AlarmWidget::onStatisticsTimer()
{
    updateStatistics();
}

void AlarmWidget::onAutoAcknowledgeTimer()
{
    // 自动确认超时报警
    for (auto& alarm : m_activeAlarms) {
        if (alarm.alarmStatus == static_cast<int>(AlarmStatus::Active)) {
            acknowledgeAlarm(alarm.alarmId, "自动确认");
        }
    }
    m_autoAcknowledgeTimer->stop();
}

void AlarmWidget::onFilterChanged()
{
    applyAlarmFilters();
}

void AlarmWidget::onAlarmSelectionChanged()
{
    int selectedCount = m_activeAlarmsTable->selectionModel()->selectedRows().count();
    
    m_acknowledgeBtn->setEnabled(selectedCount > 0);
    m_resolveBtn->setEnabled(selectedCount > 0);
    m_suppressBtn->setEnabled(selectedCount > 0);
    m_clearBtn->setEnabled(selectedCount > 0);
}

void AlarmWidget::onAcknowledgeSelected()
{
    QModelIndexList selectedRows = m_activeAlarmsTable->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) return;
    
    QString user = "操作员"; // 这里应该从用户管理系统获取当前用户
    
    for (const QModelIndex& index : selectedRows) {
        int row = index.row();
        if (row >= 0 && row < m_activeAlarms.size()) {
            int alarmId = m_activeAlarms[row].alarmId;
            acknowledgeAlarm(alarmId, user);
        }
    }
}

void AlarmWidget::onResolveSelected()
{
    QModelIndexList selectedRows = m_activeAlarmsTable->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) return;
    
    QString user = "操作员"; // 这里应该从用户管理系统获取当前用户
    QString solution = "手动解决"; // 这里可以弹出对话框让用户输入解决方案
    
    for (const QModelIndex& index : selectedRows) {
        int row = index.row();
        if (row >= 0 && row < m_activeAlarms.size()) {
            int alarmId = m_activeAlarms[row].alarmId;
            resolveAlarm(alarmId, user, solution);
        }
    }
}

void AlarmWidget::onSuppressSelected()
{
    QModelIndexList selectedRows = m_activeAlarmsTable->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) return;
    
    QString reason = "手动抑制"; // 这里可以弹出对话框让用户输入抑制原因
    
    for (const QModelIndex& index : selectedRows) {
        int row = index.row();
        if (row >= 0 && row < m_activeAlarms.size()) {
            int alarmId = m_activeAlarms[row].alarmId;
            suppressAlarm(alarmId, reason);
        }
    }
}

void AlarmWidget::onClearSelected()
{
    QModelIndexList selectedRows = m_activeAlarmsTable->selectionModel()->selectedRows();
    if (selectedRows.isEmpty()) return;
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认清除");
    msgBox.setText(QString("确定要清除选中的 %1 个报警吗？").arg(selectedRows.size()));
    msgBox.setIcon(QMessageBox::Question);
    
    QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
    QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.setDefaultButton(noButton);
    
    msgBox.exec();
    
    if (msgBox.clickedButton() == yesButton) {
        
        QList<int> alarmIds;
        for (const QModelIndex& index : selectedRows) {
            int row = index.row();
            if (row >= 0 && row < m_activeAlarms.size()) {
                alarmIds.append(m_activeAlarms[row].alarmId);
            }
        }
        
        for (int alarmId : alarmIds) {
            clearAlarm(alarmId);
        }
    }
}

void AlarmWidget::onClearAll()
{
    if (m_activeAlarms.isEmpty()) return;
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认清除");
    msgBox.setText(QString("确定要清除所有 %1 个激活报警吗？").arg(m_activeAlarms.size()));
    msgBox.setIcon(QMessageBox::Question);
    
    QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
    QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.setDefaultButton(noButton);
    
    msgBox.exec();
    
    if (msgBox.clickedButton() == yesButton) {
        clearAllAlarms();
    }
}

void AlarmWidget::onRefreshAlarms()
{
    loadActiveAlarms();
    updateActiveAlarmsTable();
    updateAlarmSummary();
}

void AlarmWidget::onExportAlarms()
{
    QString fileName = QFileDialog::getSaveFileName(this, "导出报警数据", 
        m_exportDirectory + "/alarms.csv", "CSV Files (*.csv)");
    
    if (!fileName.isEmpty()) {
        exportAlarmsToCSV(fileName);
    }
}

void AlarmWidget::onConfigureAlarms()
{
    showConfigDialog();
}

void AlarmWidget::onShowHistory()
{
    loadAlarmHistory();
    updateHistoryTable();
}

void AlarmWidget::onShowStatistics()
{
    showStatisticsDialog();
}

void AlarmWidget::updateStatistics()
{
    m_statsProgress->setVisible(true);
    m_statsProgress->setRange(0, 0);
    
    // 在单独线程中计算统计数据
    QThread* thread = QThread::create([this]() {
        calculateStatistics();
    });
    
    connect(thread, &QThread::finished, this, [this, thread]() {
        m_statsProgress->setVisible(false);
        updateStatisticsDisplay();
        thread->deleteLater();
    });
    
    thread->start();
}

void AlarmWidget::onThresholdChanged()
{
    // 阈值配置改变时的处理
    bool enabled = m_enableThresholdsCheckBox->isChecked();
    
    m_addThresholdBtn->setEnabled(enabled);
    m_editThresholdBtn->setEnabled(enabled);
    m_deleteThresholdBtn->setEnabled(enabled);
    m_importThresholdsBtn->setEnabled(enabled);
    m_exportThresholdsBtn->setEnabled(enabled);
    m_thresholdsTable->setEnabled(enabled);
    
    // 更新所有阈值的启用状态
    for (auto& threshold : m_alarmThresholds) {
        threshold.isEnabled = enabled;
        updateAlarmThreshold(threshold);
    }
}

void AlarmWidget::onConfigChanged()
{
    // 配置改变时更新内部配置，添加空指针检查
    if (m_enableAudibleCheckBox) {
        m_alarmConfig.enableAudibleAlarms = m_enableAudibleCheckBox->isChecked();
    }
    if (m_enableVisualCheckBox) {
        m_alarmConfig.enableVisualAlarms = m_enableVisualCheckBox->isChecked();
    }
    if (m_enableEmailCheckBox) {
        m_alarmConfig.enableEmailNotification = m_enableEmailCheckBox->isChecked();
    }
    if (m_enableSMSCheckBox) {
        m_alarmConfig.enableSMSNotification = m_enableSMSCheckBox->isChecked();
    }
    if (m_enableTrayCheckBox) {
        m_alarmConfig.enableSystemTray = m_enableTrayCheckBox->isChecked();
    }
    if (m_maxActiveAlarmsSpinBox) {
        m_alarmConfig.maxActiveAlarms = m_maxActiveAlarmsSpinBox->value();
    }
    if (m_autoAckTimeSpinBox) {
        m_alarmConfig.autoAcknowledgeTime = m_autoAckTimeSpinBox->value();
    }
    if (m_soundDurationSpinBox) {
        m_alarmConfig.alarmSoundDuration = m_soundDurationSpinBox->value();
    }
    if (m_soundFileEdit) {
        m_alarmConfig.alarmSoundFile = m_soundFileEdit->text();
    }
    if (m_emailRecipientsEdit) {
        m_alarmConfig.emailRecipients = m_emailRecipientsEdit->toPlainText().split('\n');
    }
    if (m_smsRecipientsEdit) {
        m_alarmConfig.smsRecipients = m_smsRecipientsEdit->toPlainText().split('\n');
    }
    
    // 跳过未定义的组件，避免崩溃
    // m_alarmConfig.enableAlarmHistory = m_enableAlarmHistoryCheckBox->isChecked();
    // m_alarmConfig.maxHistoryRecords = m_maxHistoryRecordsSpinBox->value();
    // m_alarmConfig.enableAlarmStatistics = m_enableStatisticsCheckBox->isChecked();
    // m_alarmConfig.statisticsUpdateInterval = m_statisticsUpdateIntervalSpinBox->value();
    
    emit alarmConfigChanged(m_alarmConfig);
}

void AlarmWidget::onPlayAlarmSound()
{
    playAlarmSound(AlarmLevel::Warning);
}

void AlarmWidget::onStopAlarmSound()
{
    if (m_alarmSound) {
        m_alarmSound->stop();
        m_isSoundPlaying = false;
    }
}

void AlarmWidget::playAlarmSound(AlarmLevel level)
{
    if (!m_alarmConfig.enableAudibleAlarms || m_isSoundPlaying) {
        return;
    }
    
    if (!m_alarmSound) {
        m_alarmSound = new QSoundEffect(this);
    }
    
    QString soundFile = m_alarmConfig.alarmSoundFile;
    if (soundFile.isEmpty() || !QFile::exists(soundFile)) {
        // 使用默认系统声音
        QApplication::beep();
        return;
    }
    
    m_alarmSound->setSource(QUrl::fromLocalFile(soundFile));
    m_alarmSound->setVolume(0.7);
    
    // 根据报警级别调整音量和循环次数
    switch (level) {
        case AlarmLevel::Emergency:
            m_alarmSound->setVolume(1.0);
            m_alarmSound->setLoopCount(QSoundEffect::Infinite);
            break;
        case AlarmLevel::Critical:
            m_alarmSound->setVolume(0.9);
            m_alarmSound->setLoopCount(5);
            break;
        case AlarmLevel::Error:
            m_alarmSound->setVolume(0.8);
            m_alarmSound->setLoopCount(3);
            break;
        case AlarmLevel::Warning:
            m_alarmSound->setVolume(0.7);
            m_alarmSound->setLoopCount(2);
            break;
        case AlarmLevel::Info:
            m_alarmSound->setVolume(0.5);
            m_alarmSound->setLoopCount(1);
            break;
    }
    
    m_alarmSound->play();
    m_isSoundPlaying = true;
    
    // 设置停止定时器
    if (m_alarmConfig.alarmSoundDuration > 0) {
        QTimer::singleShot(m_alarmConfig.alarmSoundDuration * 1000, this, [this]() {
            if (m_alarmSound) {
                m_alarmSound->stop();
                m_isSoundPlaying = false;
            }
        });
    }
}

// UI更新函数实现
void AlarmWidget::updateActiveAlarmsTable()
{
    m_activeAlarmsTable->setRowCount(m_activeAlarms.size());
    
    for (int i = 0; i < m_activeAlarms.size(); ++i) {
        const AlarmRecord& alarm = m_activeAlarms[i];
        
        m_activeAlarmsTable->setItem(i, 0, new QTableWidgetItem(QString::number(alarm.alarmId)));
        m_activeAlarmsTable->setItem(i, 1, new QTableWidgetItem(formatAlarmType(static_cast<AlarmType>(alarm.alarmType))));
        m_activeAlarmsTable->setItem(i, 2, new QTableWidgetItem(formatAlarmLevel(static_cast<AlarmLevel>(alarm.alarmLevel))));
        m_activeAlarmsTable->setItem(i, 3, new QTableWidgetItem(formatAlarmStatus(static_cast<AlarmStatus>(alarm.alarmStatus))));
        m_activeAlarmsTable->setItem(i, 4, new QTableWidgetItem(alarm.alarmCode));
        m_activeAlarmsTable->setItem(i, 5, new QTableWidgetItem(alarm.alarmMessage));
        m_activeAlarmsTable->setItem(i, 6, new QTableWidgetItem(alarm.deviceName));
        m_activeAlarmsTable->setItem(i, 7, new QTableWidgetItem(alarm.parameterName));
        m_activeAlarmsTable->setItem(i, 8, new QTableWidgetItem(QString::number(alarm.parameterValue, 'f', 2)));
        m_activeAlarmsTable->setItem(i, 9, new QTableWidgetItem(QString::number(alarm.thresholdValue, 'f', 2)));
        m_activeAlarmsTable->setItem(i, 10, new QTableWidgetItem(formatDateTime(alarm.timestamp)));
        m_activeAlarmsTable->setItem(i, 11, new QTableWidgetItem(formatDateTime(alarm.acknowledgeTime)));
        m_activeAlarmsTable->setItem(i, 12, new QTableWidgetItem(alarm.operatorName));
        m_activeAlarmsTable->setItem(i, 13, new QTableWidgetItem(QString::number(1)));
        m_activeAlarmsTable->setItem(i, 14, new QTableWidgetItem(alarm.notes));
        
        // 设置行颜色
        QColor color = getAlarmLevelColor(static_cast<AlarmLevel>(alarm.alarmLevel));
        for (int j = 0; j < m_activeAlarmsTable->columnCount(); ++j) {
            QTableWidgetItem* item = m_activeAlarmsTable->item(i, j);
            if (item) {
                item->setBackground(color.lighter(180));
                item->setIcon(getAlarmLevelIcon(static_cast<AlarmLevel>(alarm.alarmLevel)));
            }
        }
    }
    
    m_activeAlarmsTable->resizeColumnsToContents();
}

void AlarmWidget::updateHistoryTable()
{
    // 获取历史记录
    QDateTime startTime = m_historyStartDate->dateTime();
    QDateTime endTime = m_historyEndDate->dateTime();
    QList<AlarmRecord> history = getAlarmHistory(startTime, endTime);
    
    m_historyTable->setRowCount(history.size());
    
    for (int i = 0; i < history.size(); ++i) {
        const AlarmRecord& alarm = history[i];
        
        m_historyTable->setItem(i, 0, new QTableWidgetItem(QString::number(alarm.alarmId)));
        m_historyTable->setItem(i, 1, new QTableWidgetItem(formatAlarmType(static_cast<AlarmType>(alarm.alarmType))));
        m_historyTable->setItem(i, 2, new QTableWidgetItem(formatAlarmLevel(static_cast<AlarmLevel>(alarm.alarmLevel))));
        m_historyTable->setItem(i, 3, new QTableWidgetItem(formatAlarmStatus(static_cast<AlarmStatus>(alarm.alarmStatus))));
        m_historyTable->setItem(i, 4, new QTableWidgetItem(alarm.alarmCode));
        m_historyTable->setItem(i, 5, new QTableWidgetItem(alarm.alarmMessage));
        m_historyTable->setItem(i, 6, new QTableWidgetItem(alarm.deviceName));
        m_historyTable->setItem(i, 7, new QTableWidgetItem(alarm.parameterName));
        m_historyTable->setItem(i, 8, new QTableWidgetItem(QString::number(alarm.parameterValue, 'f', 2)));
        m_historyTable->setItem(i, 9, new QTableWidgetItem(QString::number(alarm.thresholdValue, 'f', 2)));
        m_historyTable->setItem(i, 10, new QTableWidgetItem(formatDateTime(alarm.timestamp)));
        m_historyTable->setItem(i, 11, new QTableWidgetItem(formatDateTime(alarm.acknowledgeTime)));
        m_historyTable->setItem(i, 12, new QTableWidgetItem(formatDateTime(alarm.resolveTime)));
        m_historyTable->setItem(i, 13, new QTableWidgetItem(alarm.operatorName));
        m_historyTable->setItem(i, 14, new QTableWidgetItem(alarm.solution));
        m_historyTable->setItem(i, 15, new QTableWidgetItem(alarm.notes));
    }
    
    m_historyCountLabel->setText(QString("记录数: %1").arg(history.size()));
    m_historyTable->resizeColumnsToContents();
}

void AlarmWidget::updateThresholdsTable()
{
    m_thresholdsTable->setRowCount(m_alarmThresholds.size());
    
    for (int i = 0; i < m_alarmThresholds.size(); ++i) {
        const AlarmThreshold& threshold = m_alarmThresholds[i];
        
        m_thresholdsTable->setItem(i, 0, new QTableWidgetItem(threshold.parameterName));
        m_thresholdsTable->setItem(i, 1, new QTableWidgetItem(formatAlarmType(threshold.type)));
        m_thresholdsTable->setItem(i, 2, new QTableWidgetItem(formatAlarmLevel(threshold.level)));
        m_thresholdsTable->setItem(i, 3, new QTableWidgetItem(QString::number(threshold.highHigh, 'f', 2)));
        m_thresholdsTable->setItem(i, 4, new QTableWidgetItem(QString::number(threshold.high, 'f', 2)));
        m_thresholdsTable->setItem(i, 5, new QTableWidgetItem(QString::number(threshold.low, 'f', 2)));
        m_thresholdsTable->setItem(i, 6, new QTableWidgetItem(QString::number(threshold.lowLow, 'f', 2)));
        m_thresholdsTable->setItem(i, 7, new QTableWidgetItem(QString::number(threshold.delayTime)));
        m_thresholdsTable->setItem(i, 8, new QTableWidgetItem(QString::number(threshold.deadband)));
        
        QTableWidgetItem* enabledItem = new QTableWidgetItem(threshold.isEnabled ? "是" : "否");
        enabledItem->setCheckState(threshold.isEnabled ? Qt::Checked : Qt::Unchecked);
        m_thresholdsTable->setItem(i, 9, enabledItem);
        
        m_thresholdsTable->setItem(i, 10, new QTableWidgetItem("正常"));
        m_thresholdsTable->setItem(i, 11, new QTableWidgetItem(""));
    }
    
    m_thresholdsTable->resizeColumnsToContents();
}

void AlarmWidget::updateStatisticsDisplay()
{
    // 更新概览信息
    QString overview = QString(
        "报警系统统计概览 (更新时间: %1)\n\n"
        "总报警数: %2\n"
        "激活报警: %3\n"
        "已确认报警: %4\n"
        "已解决报警: %5\n"
        "平均响应时间: %6\n"
        "平均解决时间: %7\n"
    ).arg(formatDateTime(m_alarmStatistics.lastUpdateTime))
     .arg(m_alarmStatistics.totalAlarms)
     .arg(m_alarmStatistics.activeAlarms)
     .arg(m_alarmStatistics.acknowledgedAlarms)
     .arg(m_alarmStatistics.resolvedAlarms)
     .arg(formatDuration(m_alarmStatistics.averageResponseTime))
     .arg(formatDuration(m_alarmStatistics.averageResolveTime));
    
    m_statsOverviewLabel->setText(overview);
    
    // 更新统计表格
    m_statsTable->setRowCount(0);
    
    // 按类型统计
    for (auto it = m_alarmStatistics.alarmsByType.begin(); 
         it != m_alarmStatistics.alarmsByType.end(); ++it) {
        int row = m_statsTable->rowCount();
        m_statsTable->insertRow(row);
        m_statsTable->setItem(row, 0, new QTableWidgetItem(formatAlarmType(it.key())));
        m_statsTable->setItem(row, 1, new QTableWidgetItem(QString::number(it.value())));
        m_statsTable->setItem(row, 2, new QTableWidgetItem("-"));
        m_statsTable->setItem(row, 3, new QTableWidgetItem("-"));
        m_statsTable->setItem(row, 4, new QTableWidgetItem("-"));
        m_statsTable->setItem(row, 5, new QTableWidgetItem("按类型统计"));
    }
    
    // 按级别统计
    for (auto it = m_alarmStatistics.alarmsByLevel.begin(); 
         it != m_alarmStatistics.alarmsByLevel.end(); ++it) {
        int row = m_statsTable->rowCount();
        m_statsTable->insertRow(row);
        m_statsTable->setItem(row, 0, new QTableWidgetItem(formatAlarmLevel(it.key())));
        m_statsTable->setItem(row, 1, new QTableWidgetItem(QString::number(it.value())));
        m_statsTable->setItem(row, 2, new QTableWidgetItem("-"));
        m_statsTable->setItem(row, 3, new QTableWidgetItem("-"));
        m_statsTable->setItem(row, 4, new QTableWidgetItem("-"));
        m_statsTable->setItem(row, 5, new QTableWidgetItem("按级别统计"));
    }
    
    m_statsTable->resizeColumnsToContents();
}

void AlarmWidget::updateAlarmSummary()
{
    int totalAlarms = m_activeAlarms.size();
    int unacknowledgedAlarms = 0;
    int criticalAlarms = 0;
    
    for (const auto& alarm : m_activeAlarms) {
        if (alarm.alarmStatus == static_cast<int>(AlarmStatus::Active)) {
            unacknowledgedAlarms++;
        }
        if (alarm.alarmLevel == static_cast<int>(AlarmLevel::Critical) || alarm.alarmLevel == static_cast<int>(AlarmLevel::Emergency)) {
            criticalAlarms++;
        }
    }
    
    m_totalAlarmsLabel->setText(QString("总报警: %1").arg(totalAlarms));
    m_activeAlarmsLabel->setText(QString("激活: %1").arg(totalAlarms));
    m_unacknowledgedLabel->setText(QString("未确认: %1").arg(unacknowledgedAlarms));
    m_criticalAlarmsLabel->setText(QString("严重: %1").arg(criticalAlarms));
    
    // 设置标签颜色
    if (criticalAlarms > 0) {
        m_criticalAlarmsLabel->setStyleSheet("QLabel { color: red; font-weight: bold; }");
    } else {
        m_criticalAlarmsLabel->setStyleSheet("QLabel { color: green; }");
    }
    
    if (unacknowledgedAlarms > 0) {
        m_unacknowledgedLabel->setStyleSheet("QLabel { color: orange; font-weight: bold; }");
    } else {
        m_unacknowledgedLabel->setStyleSheet("QLabel { color: green; }");
    }
}

// 数据加载函数实现
void AlarmWidget::loadActiveAlarms()
{
    m_activeAlarms.clear();
    
    QSqlQuery query(m_database);
    query.exec("SELECT * FROM alarm_records WHERE alarm_status IN (0, 1, 3) ORDER BY timestamp DESC");
    
    while (query.next()) {
        AlarmRecord alarm;
        alarm.alarmId = query.value("alarm_id").toInt();
        alarm.alarmType = static_cast<int>(query.value("alarm_type").toInt());
        alarm.alarmLevel = static_cast<int>(query.value("alarm_level").toInt());
        alarm.alarmStatus = static_cast<int>(query.value("alarm_status").toInt());
        alarm.alarmCode = query.value("alarm_code").toString();
        alarm.alarmMessage = query.value("alarm_message").toString();
        alarm.deviceName = query.value("device_name").toString();
        alarm.parameterName = query.value("parameter_name").toString();
        alarm.parameterValue = query.value("parameter_value").toDouble();
        alarm.thresholdValue = query.value("threshold_value").toDouble();
        alarm.timestamp = query.value("timestamp").toDateTime();
        alarm.acknowledgeTime = query.value("acknowledge_time").toDateTime();
        alarm.resolveTime = query.value("resolve_time").toDateTime();
        alarm.operatorName = query.value("operator_name").toString();
        alarm.acknowledgeUser = query.value("acknowledge_user").toString();
        alarm.resolveUser = query.value("resolve_user").toString();
        alarm.solution = query.value("solution").toString();
        alarm.notes = query.value("notes").toString();
        // count field not used
        // alarm.isAudible = query.value("is_audible").toBool();
        // alarm.isVisible = query.value("is_visible").toBool();
        // alarm.category = query.value("category").toString();
        // alarm.priority = query.value("priority").toInt();
        
        m_activeAlarms.append(alarm);
    }
    
    LogManager::getInstance()->info(QString("加载激活报警: %1 个").arg(m_activeAlarms.size()), "AlarmWidget");
}

void AlarmWidget::loadAlarmHistory()
{
    m_alarmHistory.clear();
    
    QDateTime startTime = m_historyStartDate->dateTime();
    QDateTime endTime = m_historyEndDate->dateTime();
    
    QSqlQuery query(m_database);
    query.prepare("SELECT * FROM alarm_records WHERE timestamp BETWEEN ? AND ? ORDER BY timestamp DESC");
    query.addBindValue(startTime);
    query.addBindValue(endTime);
    query.exec();
    
    while (query.next()) {
        AlarmRecord alarm;
        alarm.alarmId = query.value("alarm_id").toInt();
        alarm.alarmType = static_cast<int>(query.value("alarm_type").toInt());
        alarm.alarmLevel = static_cast<int>(query.value("alarm_level").toInt());
        alarm.alarmStatus = static_cast<int>(query.value("alarm_status").toInt());
        alarm.alarmCode = query.value("alarm_code").toString();
        alarm.alarmMessage = query.value("alarm_message").toString();
        alarm.deviceName = query.value("device_name").toString();
        alarm.parameterName = query.value("parameter_name").toString();
        alarm.parameterValue = query.value("parameter_value").toDouble();
        alarm.thresholdValue = query.value("threshold_value").toDouble();
        alarm.timestamp = query.value("timestamp").toDateTime();
        alarm.acknowledgeTime = query.value("acknowledge_time").toDateTime();
        alarm.resolveTime = query.value("resolve_time").toDateTime();
        alarm.operatorName = query.value("operator_name").toString();
        alarm.acknowledgeUser = query.value("acknowledge_user").toString();
        alarm.resolveUser = query.value("resolve_user").toString();
        alarm.solution = query.value("solution").toString();
        alarm.notes = query.value("notes").toString();
        // count field not used
        // alarm.isAudible = query.value("is_audible").toBool();
        // alarm.isVisible = query.value("is_visible").toBool();
        // alarm.category = query.value("category").toString();
        // alarm.priority = query.value("priority").toInt();
        
        m_alarmHistory.append(alarm);
    }
    
    LogManager::getInstance()->info(QString("加载历史报警: %1 个").arg(m_alarmHistory.size()), "AlarmWidget");
}

void AlarmWidget::loadAlarmThresholds()
{
    m_alarmThresholds.clear();
    
    QSqlQuery query(m_database);
    query.exec("SELECT * FROM alarm_thresholds ORDER BY parameter_name");
    
    while (query.next()) {
        AlarmThreshold threshold;
        threshold.parameterName = query.value("parameter_name").toString();
        threshold.type = static_cast<AlarmType>(query.value("alarm_type").toInt());
        threshold.level = static_cast<AlarmLevel>(query.value("alarm_level").toInt());
        threshold.highHigh = query.value("high_high").toDouble();
        threshold.high = query.value("high_value").toDouble();
        threshold.low = query.value("low_value").toDouble();
        threshold.lowLow = query.value("low_low").toDouble();
        threshold.enableHighHigh = query.value("enable_high_high").toBool();
        threshold.enableHigh = query.value("enable_high").toBool();
        threshold.enableLow = query.value("enable_low").toBool();
        threshold.enableLowLow = query.value("enable_low_low").toBool();
        threshold.delayTime = query.value("delay_time").toInt();
        threshold.deadband = query.value("deadband").toInt();
        threshold.isEnabled = query.value("is_enabled").toBool();
        
        m_alarmThresholds.append(threshold);
    }
    
    LogManager::getInstance()->info(QString("加载报警阈值: %1 个").arg(m_alarmThresholds.size()), "AlarmWidget");
}

void AlarmWidget::loadAlarmConfig()
{
    QSqlQuery query(m_database);
    query.exec("SELECT * FROM alarm_config WHERE id = 1");
    
    if (query.next()) {
        m_alarmConfig.enableAudibleAlarms = query.value("enable_audible").toBool();
        m_alarmConfig.enableVisualAlarms = query.value("enable_visual").toBool();
        m_alarmConfig.enableEmailNotification = query.value("enable_email").toBool();
        m_alarmConfig.enableSMSNotification = query.value("enable_sms").toBool();
        m_alarmConfig.enableSystemTray = query.value("enable_tray").toBool();
        m_alarmConfig.maxActiveAlarms = query.value("max_active_alarms").toInt();
        m_alarmConfig.autoAcknowledgeTime = query.value("auto_acknowledge_time").toInt();
        m_alarmConfig.alarmSoundDuration = query.value("sound_duration").toInt();
        m_alarmConfig.alarmSoundFile = query.value("sound_file").toString();
        m_alarmConfig.emailRecipients = query.value("email_recipients").toString().split('\n');
        m_alarmConfig.smsRecipients = query.value("sms_recipients").toString().split('\n');
        m_alarmConfig.enableAlarmHistory = query.value("enable_history").toBool();
        m_alarmConfig.maxHistoryRecords = query.value("max_history_records").toInt();
        m_alarmConfig.enableAlarmStatistics = query.value("enable_statistics").toBool();
        m_alarmConfig.statisticsUpdateInterval = query.value("statistics_update_interval").toInt();
    }
    
    // 更新UI控件
    m_enableAudibleCheckBox->setChecked(m_alarmConfig.enableAudibleAlarms);
    m_enableVisualCheckBox->setChecked(m_alarmConfig.enableVisualAlarms);
    m_enableEmailCheckBox->setChecked(m_alarmConfig.enableEmailNotification);
    m_enableSMSCheckBox->setChecked(m_alarmConfig.enableSMSNotification);
    m_enableTrayCheckBox->setChecked(m_alarmConfig.enableSystemTray);
    m_maxActiveAlarmsSpinBox->setValue(m_alarmConfig.maxActiveAlarms);
    m_autoAckTimeSpinBox->setValue(m_alarmConfig.autoAcknowledgeTime);
    m_soundDurationSpinBox->setValue(m_alarmConfig.alarmSoundDuration);
    m_soundFileEdit->setText(m_alarmConfig.alarmSoundFile);
    m_emailRecipientsEdit->setPlainText(m_alarmConfig.emailRecipients.join('\n'));
    m_smsRecipientsEdit->setPlainText(m_alarmConfig.smsRecipients.join('\n'));
    
    LogManager::getInstance()->info("加载报警配置完成", "AlarmWidget");
}

// 外部接口实现
void AlarmWidget::onParameterValueChanged(const QString& parameter, double value)
{
    checkParameter(parameter, value);
}

void AlarmWidget::onDeviceStatusChanged(const QString& device, int status)
{
    if (status == 0) { // 设备故障
        AlarmRecord alarm;
        alarm.alarmType = static_cast<int>(AlarmType::Device);
        alarm.alarmLevel = static_cast<int>(AlarmLevel::Error);
        alarm.alarmCode = "DEV_FAULT";
        alarm.alarmMessage = "设备故障";
        alarm.deviceName = device;
        alarm.operatorName = "系统";
        // alarm.category = "设备报警";
        // alarm.priority = 3;
        
        triggerAlarm(alarm);
    }
}

void AlarmWidget::onCommunicationError(const QString& connection, const QString& error)
{
    AlarmRecord alarm;
    alarm.alarmType = static_cast<int>(AlarmType::Communication);
    alarm.alarmLevel = static_cast<int>(AlarmLevel::Warning);
    alarm.alarmCode = "COMM_ERROR";
    alarm.alarmMessage = QString("通讯错误: %1").arg(error);
    alarm.deviceName = connection;
    alarm.operatorName = "系统";
    // alarm.category = "通讯报警";
    // alarm.priority = 2;
    
    triggerAlarm(alarm);
}

void AlarmWidget::onSystemError(const QString& error)
{
    AlarmRecord alarm;
    alarm.alarmType = static_cast<int>(AlarmType::System);
    alarm.alarmLevel = static_cast<int>(AlarmLevel::Error);
    alarm.alarmCode = "SYS_ERROR";
    alarm.alarmMessage = QString("系统错误: %1").arg(error);
    alarm.deviceName = "系统";
    alarm.operatorName = "系统";
    // alarm.category = "系统报警";
    // alarm.priority = 3;
    
    triggerAlarm(alarm);
}

void AlarmWidget::onQualityAlert(const QString& parameter, double value, double threshold)
{
    AlarmRecord alarm;
    alarm.alarmType = static_cast<int>(AlarmType::Quality);
    alarm.alarmLevel = static_cast<int>(AlarmLevel::Warning);
    alarm.alarmCode = "QUALITY_ALERT";
    alarm.alarmMessage = QString("质量报警: %1 超出阈值").arg(parameter);
    alarm.deviceName = "质量检测";
    alarm.parameterName = parameter;
    alarm.parameterValue = value;
    alarm.thresholdValue = threshold;
    alarm.operatorName = "系统";
    // alarm.category = "质量报警";
    // alarm.priority = 2;
    
    triggerAlarm(alarm);
}

void AlarmWidget::onSafetyAlert(const QString& message)
{
    AlarmRecord alarm;
    alarm.alarmType = static_cast<int>(AlarmType::Safety);
    alarm.alarmLevel = static_cast<int>(AlarmLevel::Critical);
    alarm.alarmCode = "SAFETY_ALERT";
    alarm.alarmMessage = QString("安全报警: %1").arg(message);
    alarm.deviceName = "安全系统";
    alarm.operatorName = "系统";
    // alarm.category = "安全报警";
    // alarm.priority = 4;
    
    triggerAlarm(alarm);
}

// 清理函数实现
void AlarmWidget::cleanupOldAlarms()
{
    if (!m_alarmConfig.enableAlarmHistory) return;
    
    QDateTime cutoffTime = QDateTime::currentDateTime().addDays(-30); // 保留30天
    
    QSqlQuery query(m_database);
    query.prepare("DELETE FROM alarm_records WHERE timestamp < ? AND alarm_status = 2");
    query.addBindValue(cutoffTime);
    
    if (query.exec()) {
        int deletedCount = query.numRowsAffected();
        LogManager::getInstance()->info(QString("清理旧报警记录: %1 条").arg(deletedCount), "AlarmWidget");
    }
}

// 报警管理接口实现
void AlarmWidget::triggerAlarm(const AlarmRecord& alarm)
{
    QMutexLocker locker(&m_alarmMutex);
    
    // 检查是否已存在相同的报警
    for (auto& existingAlarm : m_activeAlarms) {
        if (existingAlarm.alarmCode == alarm.alarmCode && 
            existingAlarm.deviceName == alarm.deviceName) {
            // 更新现有报警
            // existingAlarm.count++;
            existingAlarm.timestamp = alarm.timestamp;
            existingAlarm.parameterValue = alarm.parameterValue;
            updateAlarmRecord(existingAlarm);
            updateActiveAlarmsTable();
            return;
        }
    }
    
    // 创建新报警
    AlarmRecord newAlarm = alarm;
    newAlarm.alarmId = m_nextAlarmId++;
    newAlarm.alarmStatus = static_cast<int>(AlarmStatus::Active);
    newAlarm.timestamp = QDateTime::currentDateTime();
    
    // 插入数据库
    if (insertAlarmRecord(newAlarm)) {
        m_activeAlarms.append(newAlarm);
        
        // 处理报警
        processAlarm(newAlarm);
        
        // 更新显示
        updateActiveAlarmsTable();
        updateAlarmSummary();
        
        // 发出信号
        emit alarmTriggered(newAlarm);
        
        if (newAlarm.alarmLevel == static_cast<int>(AlarmLevel::Critical)) {
            emit criticalAlarmTriggered(newAlarm);
        } else if (newAlarm.alarmLevel == static_cast<int>(AlarmLevel::Emergency)) {
            emit emergencyAlarmTriggered(newAlarm);
        }
        
        LogManager::getInstance()->warning(
            QString("报警触发: %1 - %2").arg(newAlarm.alarmCode, newAlarm.alarmMessage), 
            "AlarmWidget");
    }
}

void AlarmWidget::acknowledgeAlarm(int alarmId, const QString& user)
{
    QMutexLocker locker(&m_alarmMutex);
    
    for (auto& alarm : m_activeAlarms) {
        if (alarm.alarmId == alarmId) {
            alarm.alarmStatus = static_cast<int>(AlarmStatus::Acknowledged);
            alarm.acknowledgeTime = QDateTime::currentDateTime();
            alarm.acknowledgeUser = user;
            
            updateAlarmRecord(alarm);
            updateActiveAlarmsTable();
            updateAlarmSummary();
            
            emit alarmAcknowledged(alarmId, user);
            
            LogManager::getInstance()->info(
                QString("报警已确认: %1 by %2").arg(alarm.alarmCode, user), 
                "AlarmWidget");
            break;
        }
    }
}

void AlarmWidget::resolveAlarm(int alarmId, const QString& user, const QString& solution)
{
    QMutexLocker locker(&m_alarmMutex);
    
    for (int i = 0; i < m_activeAlarms.size(); ++i) {
        if (m_activeAlarms[i].alarmId == alarmId) {
            AlarmRecord& alarm = m_activeAlarms[i];
            alarm.alarmStatus = static_cast<int>(AlarmStatus::Resolved);
            alarm.resolveTime = QDateTime::currentDateTime();
            alarm.resolveUser = user;
            alarm.solution = solution;
            
            updateAlarmRecord(alarm);
            
            // 将已解决的报警从激活列表移除
            m_activeAlarms.removeAt(i);
            
            updateActiveAlarmsTable();
            updateAlarmSummary();
            
            emit alarmResolved(alarmId, user);
            
            LogManager::getInstance()->info(
                QString("报警已解决: %1 by %2").arg(alarm.alarmCode, user), 
                "AlarmWidget");
            break;
        }
    }
}

void AlarmWidget::suppressAlarm(int alarmId, const QString& reason)
{
    QMutexLocker locker(&m_alarmMutex);
    
    for (auto& alarm : m_activeAlarms) {
        if (alarm.alarmId == alarmId) {
            alarm.alarmStatus = static_cast<int>(AlarmStatus::Suppressed);
            alarm.notes = reason;
            
            updateAlarmRecord(alarm);
            updateActiveAlarmsTable();
            updateAlarmSummary();
            
            LogManager::getInstance()->info(
                QString("报警已抑制: %1 - %2").arg(alarm.alarmCode, reason), 
                "AlarmWidget");
            break;
        }
    }
}

void AlarmWidget::clearAlarm(int alarmId)
{
    QMutexLocker locker(&m_alarmMutex);
    
    for (int i = 0; i < m_activeAlarms.size(); ++i) {
        if (m_activeAlarms[i].alarmId == alarmId) {
            AlarmRecord alarm = m_activeAlarms[i];
            m_activeAlarms.removeAt(i);
            
            updateActiveAlarmsTable();
            updateAlarmSummary();
            
            emit alarmCleared(alarmId);
            
            LogManager::getInstance()->info(
                QString("报警已清除: %1").arg(alarm.alarmCode), 
                "AlarmWidget");
            break;
        }
    }
}

void AlarmWidget::clearAllAlarms()
{
    QMutexLocker locker(&m_alarmMutex);
    
    int count = m_activeAlarms.size();
    m_activeAlarms.clear();
    
    updateActiveAlarmsTable();
    updateAlarmSummary();
    
    LogManager::getInstance()->info(
        QString("已清除所有报警，共 %1 个").arg(count), 
        "AlarmWidget");
}

// 查询接口实现
AlarmRecord AlarmWidget::getAlarmRecord(int alarmId)
{
    for (const auto& alarm : m_activeAlarms) {
        if (alarm.alarmId == alarmId) {
            return alarm;
        }
    }
    
    // 如果在激活报警中没找到，查询数据库
    QSqlQuery query(m_database);
    query.prepare("SELECT * FROM alarm_records WHERE alarm_id = ?");
    query.addBindValue(alarmId);
    query.exec();
    
    if (query.next()) {
        AlarmRecord alarm;
        // 解析数据库记录...
        return alarm;
    }
    
    return AlarmRecord(); // 返回空记录
}

QList<AlarmRecord> AlarmWidget::getActiveAlarms()
{
    return m_activeAlarms;
}

QList<AlarmRecord> AlarmWidget::getAlarmHistory(const QDateTime& startTime, const QDateTime& endTime)
{
    QList<AlarmRecord> history;
    
    QSqlQuery query(m_database);
    query.prepare("SELECT * FROM alarm_records WHERE timestamp BETWEEN ? AND ? ORDER BY timestamp DESC");
    query.addBindValue(startTime);
    query.addBindValue(endTime);
    query.exec();
    
    while (query.next()) {
        AlarmRecord alarm;
        alarm.alarmId = query.value("alarm_id").toInt();
        alarm.alarmType = static_cast<int>(query.value("alarm_type").toInt());
        alarm.alarmLevel = static_cast<int>(query.value("alarm_level").toInt());
        alarm.alarmStatus = static_cast<int>(query.value("alarm_status").toInt());
        alarm.alarmCode = query.value("alarm_code").toString();
        alarm.alarmMessage = query.value("alarm_message").toString();
        alarm.deviceName = query.value("device_name").toString();
        alarm.parameterName = query.value("parameter_name").toString();
        alarm.parameterValue = query.value("parameter_value").toDouble();
        alarm.thresholdValue = query.value("threshold_value").toDouble();
        alarm.timestamp = query.value("timestamp").toDateTime();
        alarm.acknowledgeTime = query.value("acknowledge_time").toDateTime();
        alarm.resolveTime = query.value("resolve_time").toDateTime();
        alarm.operatorName = query.value("operator_name").toString();
        alarm.acknowledgeUser = query.value("acknowledge_user").toString();
        alarm.resolveUser = query.value("resolve_user").toString();
        alarm.solution = query.value("solution").toString();
        alarm.notes = query.value("notes").toString();
        // count field not used
        // alarm.isAudible = query.value("is_audible").toBool();
        // alarm.isVisible = query.value("is_visible").toBool();
        // alarm.category = query.value("category").toString();
        // alarm.priority = query.value("priority").toInt();
        
        history.append(alarm);
    }
    
    return history;
}

QList<AlarmRecord> AlarmWidget::getAlarmsByType(AlarmType type)
{
    QList<AlarmRecord> result;
    for (const auto& alarm : m_activeAlarms) {
        if (alarm.alarmType == static_cast<int>(type)) {
            result.append(alarm);
        }
    }
    return result;
}

QList<AlarmRecord> AlarmWidget::getAlarmsByLevel(AlarmLevel level)
{
    QList<AlarmRecord> result;
    for (const auto& alarm : m_activeAlarms) {
        if (alarm.alarmLevel == static_cast<int>(level)) {
            result.append(alarm);
        }
    }
    return result;
}

QList<AlarmRecord> AlarmWidget::getAlarmsByDevice(const QString& deviceName)
{
    QList<AlarmRecord> result;
    for (const auto& alarm : m_activeAlarms) {
        if (alarm.deviceName == deviceName) {
            result.append(alarm);
        }
    }
    return result;
}

// 配置接口实现
void AlarmWidget::setAlarmConfig(const AlarmConfig& config)
{
    m_alarmConfig = config;
    saveAlarmConfig();
    loadAlarmConfig(); // 重新加载以更新UI
    emit alarmConfigChanged(config);
}

AlarmConfig AlarmWidget::getAlarmConfig() const
{
    return m_alarmConfig;
}

QList<AlarmThreshold> AlarmWidget::getAlarmThresholds() const
{
    return m_alarmThresholds;
}

// 统计接口实现
AlarmStatistics AlarmWidget::getAlarmStatistics() const
{
    return m_alarmStatistics;
}

// 检查接口实现
bool AlarmWidget::isAlarmActive(const QString& alarmCode)
{
    for (const auto& alarm : m_activeAlarms) {
        if (alarm.alarmCode == alarmCode && alarm.alarmStatus == static_cast<int>(AlarmStatus::Active)) {
            return true;
        }
    }
    return false;
}

int AlarmWidget::getActiveAlarmCount() const
{
    return m_activeAlarms.size();
}

int AlarmWidget::getUnacknowledgedAlarmCount() const
{
    int count = 0;
    for (const auto& alarm : m_activeAlarms) {
        if (alarm.alarmStatus == static_cast<int>(AlarmStatus::Active)) {
            count++;
        }
    }
    return count;
}

// 辅助函数实现
void AlarmWidget::applyAlarmFilters()
{
    QString typeFilter = m_alarmTypeFilter->currentText();
    QString levelFilter = m_alarmLevelFilter->currentText();
    QString statusFilter = m_alarmStatusFilter->currentText();
    QString searchText = m_alarmSearchEdit->text();
    
    for (int i = 0; i < m_activeAlarmsTable->rowCount(); ++i) {
        bool visible = true;
        
        // 类型过滤
        if (typeFilter != "全部") {
            QString itemType = m_activeAlarmsTable->item(i, 1)->text();
            if (itemType != typeFilter) {
                visible = false;
            }
        }
        
        // 级别过滤
        if (visible && levelFilter != "全部") {
            QString itemLevel = m_activeAlarmsTable->item(i, 2)->text();
            if (itemLevel != levelFilter) {
                visible = false;
            }
        }
        
        // 状态过滤
        if (visible && statusFilter != "全部") {
            QString itemStatus = m_activeAlarmsTable->item(i, 3)->text();
            if (itemStatus != statusFilter) {
                visible = false;
            }
        }
        
        // 搜索过滤
        if (visible && !searchText.isEmpty()) {
            bool found = false;
            for (int j = 0; j < m_activeAlarmsTable->columnCount(); ++j) {
                QTableWidgetItem* item = m_activeAlarmsTable->item(i, j);
                if (item && item->text().contains(searchText, Qt::CaseInsensitive)) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                visible = false;
            }
        }
        
        m_activeAlarmsTable->setRowHidden(i, !visible);
    }
}

void AlarmWidget::exportAlarmsToCSV(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", "无法创建文件: " + filename);
        return;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // 写入CSV头部
    QStringList headers = {"报警ID", "类型", "级别", "状态", "代码", "信息", "设备", "参数", 
                          "当前值", "阈值", "发生时间", "确认时间", "操作员", "次数", "备注"};
    out << headers.join(",") << "\n";
    
    // 写入数据
    for (const auto& alarm : m_activeAlarms) {
        QStringList row;
        row << QString::number(alarm.alarmId);
        row << formatAlarmType(static_cast<AlarmType>(alarm.alarmType));
        row << formatAlarmLevel(static_cast<AlarmLevel>(alarm.alarmLevel));
        row << formatAlarmStatus(static_cast<AlarmStatus>(alarm.alarmStatus));
        row << alarm.alarmCode;
        row << alarm.alarmMessage;
        row << alarm.deviceName;
        row << alarm.parameterName;
        row << QString::number(alarm.parameterValue, 'f', 2);
        row << QString::number(alarm.thresholdValue, 'f', 2);
        row << formatDateTime(alarm.timestamp);
        row << formatDateTime(alarm.acknowledgeTime);
        row << alarm.operatorName;
        row << QString::number(1);
        row << alarm.notes;
        
        out << row.join(",") << "\n";
    }
    
    file.close();
    
    QMessageBox::information(this, "导出成功", 
        QString("已成功导出 %1 条报警记录到文件:\n%2").arg(m_activeAlarms.size()).arg(filename));
    
    LogManager::getInstance()->info("导出报警数据到: " + filename, "AlarmWidget");
}

void AlarmWidget::showAlarmDetailsDialog(const AlarmRecord& alarm)
{
    QDialog dialog(this);
    dialog.setWindowTitle("报警详情");
    dialog.setModal(true);
    dialog.resize(500, 400);
    
    auto layout = new QVBoxLayout(&dialog);
    
    // 创建详情表格
    auto detailsTable = new QTableWidget(16, 2, &dialog);
    detailsTable->setHorizontalHeaderLabels({"属性", "值"});
    detailsTable->verticalHeader()->setVisible(false);
    detailsTable->setAlternatingRowColors(true);
    
    // 填充详情
    QStringList properties = {"报警ID", "报警类型", "报警级别", "报警状态", "报警代码", 
                             "报警信息", "设备名称", "参数名称", "当前值", "阈值", 
                             "发生时间", "确认时间", "解决时间", "操作员", "解决方案", "备注"};
    
    QStringList values = {
        QString::number(alarm.alarmId),
        formatAlarmType(static_cast<AlarmType>(alarm.alarmType)),
        formatAlarmLevel(static_cast<AlarmLevel>(alarm.alarmLevel)),
        formatAlarmStatus(static_cast<AlarmStatus>(alarm.alarmStatus)),
        alarm.alarmCode,
        alarm.alarmMessage,
        alarm.deviceName,
        alarm.parameterName,
        QString::number(alarm.parameterValue, 'f', 2),
        QString::number(alarm.thresholdValue, 'f', 2),
        formatDateTime(alarm.timestamp),
        formatDateTime(alarm.acknowledgeTime),
        formatDateTime(alarm.resolveTime),
        alarm.operatorName,
        alarm.solution,
        alarm.notes
    };
    
    for (int i = 0; i < properties.size(); ++i) {
        detailsTable->setItem(i, 0, new QTableWidgetItem(properties[i]));
        detailsTable->setItem(i, 1, new QTableWidgetItem(values[i]));
    }
    
    detailsTable->resizeColumnsToContents();
    layout->addWidget(detailsTable);
    
    // 添加按钮
    auto buttonLayout = new QHBoxLayout();
    auto closeBtn = new QPushButton("关闭");
    connect(closeBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeBtn);
    layout->addLayout(buttonLayout);
    
    dialog.exec();
}

void AlarmWidget::showThresholdDialog(const AlarmThreshold& threshold)
{
    QDialog dialog(this);
    dialog.setWindowTitle(threshold.parameterName.isEmpty() ? "添加阈值" : "编辑阈值");
    dialog.setModal(true);
    dialog.resize(400, 300);
    
    auto layout = new QVBoxLayout(&dialog);
    
    // 参数名称
    auto nameLayout = new QHBoxLayout();
    nameLayout->addWidget(new QLabel("参数名称:"));
    auto nameEdit = new QLineEdit(threshold.parameterName);
    nameLayout->addWidget(nameEdit);
    layout->addLayout(nameLayout);
    
    // 报警类型
    auto typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel("报警类型:"));
    auto typeCombo = new QComboBox();
    typeCombo->addItems({"系统", "设备", "工艺", "质量", "安全", "通讯", "温度", "压力", "位置", "速度"});
    typeCombo->setCurrentIndex(static_cast<int>(threshold.type));
    typeLayout->addWidget(typeCombo);
    layout->addLayout(typeLayout);
    
    // 报警级别
    auto levelLayout = new QHBoxLayout();
    levelLayout->addWidget(new QLabel("报警级别:"));
    auto levelCombo = new QComboBox();
    levelCombo->addItems({"信息", "警告", "错误", "严重", "紧急"});
    levelCombo->setCurrentIndex(static_cast<int>(threshold.level));
    levelLayout->addWidget(levelCombo);
    layout->addLayout(levelLayout);
    
    // 阈值设置
    auto thresholdGroup = new QGroupBox("阈值设置");
    auto thresholdLayout = new QGridLayout(thresholdGroup);
    
    auto highHighSpin = new QDoubleSpinBox();
    highHighSpin->setRange(-999999, 999999);
    highHighSpin->setValue(threshold.highHigh);
    auto highHighCheck = new QCheckBox("启用高高限");
    highHighCheck->setChecked(threshold.enableHighHigh);
    thresholdLayout->addWidget(new QLabel("高高限:"), 0, 0);
    thresholdLayout->addWidget(highHighSpin, 0, 1);
    thresholdLayout->addWidget(highHighCheck, 0, 2);
    
    auto highSpin = new QDoubleSpinBox();
    highSpin->setRange(-999999, 999999);
    highSpin->setValue(threshold.high);
    auto highCheck = new QCheckBox("启用高限");
    highCheck->setChecked(threshold.enableHigh);
    thresholdLayout->addWidget(new QLabel("高限:"), 1, 0);
    thresholdLayout->addWidget(highSpin, 1, 1);
    thresholdLayout->addWidget(highCheck, 1, 2);
    
    auto lowSpin = new QDoubleSpinBox();
    lowSpin->setRange(-999999, 999999);
    lowSpin->setValue(threshold.low);
    auto lowCheck = new QCheckBox("启用低限");
    lowCheck->setChecked(threshold.enableLow);
    thresholdLayout->addWidget(new QLabel("低限:"), 2, 0);
    thresholdLayout->addWidget(lowSpin, 2, 1);
    thresholdLayout->addWidget(lowCheck, 2, 2);
    
    auto lowLowSpin = new QDoubleSpinBox();
    lowLowSpin->setRange(-999999, 999999);
    lowLowSpin->setValue(threshold.lowLow);
    auto lowLowCheck = new QCheckBox("启用低低限");
    lowLowCheck->setChecked(threshold.enableLowLow);
    thresholdLayout->addWidget(new QLabel("低低限:"), 3, 0);
    thresholdLayout->addWidget(lowLowSpin, 3, 1);
    thresholdLayout->addWidget(lowLowCheck, 3, 2);
    
    layout->addWidget(thresholdGroup);
    
    // 其他设置
    auto otherGroup = new QGroupBox("其他设置");
    auto otherLayout = new QGridLayout(otherGroup);
    
    auto delayTimeSpin = new QSpinBox();
    delayTimeSpin->setRange(0, 3600);
    delayTimeSpin->setValue(threshold.delayTime);
    delayTimeSpin->setSuffix(" 秒");
    otherLayout->addWidget(new QLabel("延时时间:"), 0, 0);
    otherLayout->addWidget(delayTimeSpin, 0, 1);
    
    auto deadbandSpin = new QSpinBox();
    deadbandSpin->setRange(0, 100);
    deadbandSpin->setValue(threshold.deadband);
    deadbandSpin->setSuffix(" %");
    otherLayout->addWidget(new QLabel("死区:"), 1, 0);
    otherLayout->addWidget(deadbandSpin, 1, 1);
    
    auto enabledCheck = new QCheckBox("启用阈值");
    enabledCheck->setChecked(threshold.isEnabled);
    otherLayout->addWidget(enabledCheck, 2, 0, 1, 2);
    
    layout->addWidget(otherGroup);
    
    // 按钮
    auto buttonLayout = new QHBoxLayout();
    auto okBtn = new QPushButton("确定");
    auto cancelBtn = new QPushButton("取消");
    
    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    
    buttonLayout->addStretch();
    buttonLayout->addWidget(okBtn);
    buttonLayout->addWidget(cancelBtn);
    layout->addLayout(buttonLayout);
    
    if (dialog.exec() == QDialog::Accepted) {
        AlarmThreshold newThreshold;
        newThreshold.parameterName = nameEdit->text();
        newThreshold.type = static_cast<AlarmType>(typeCombo->currentIndex());
        newThreshold.level = static_cast<AlarmLevel>(levelCombo->currentIndex());
        newThreshold.highHigh = highHighSpin->value();
        newThreshold.high = highSpin->value();
        newThreshold.low = lowSpin->value();
        newThreshold.lowLow = lowLowSpin->value();
        newThreshold.enableHighHigh = highHighCheck->isChecked();
        newThreshold.enableHigh = highCheck->isChecked();
        newThreshold.enableLow = lowCheck->isChecked();
        newThreshold.enableLowLow = lowLowCheck->isChecked();
        newThreshold.delayTime = delayTimeSpin->value();
        newThreshold.deadband = deadbandSpin->value();
        newThreshold.isEnabled = enabledCheck->isChecked();
        
        if (validateThreshold(newThreshold)) {
            if (threshold.parameterName.isEmpty()) {
                addAlarmThreshold(newThreshold);
            } else {
                updateAlarmThreshold(newThreshold);
            }
        } else {
            QMessageBox::warning(this, "错误", "阈值配置无效，请检查设置！");
        }
    }
}

void AlarmWidget::showConfigDialog()
{
    // 配置对话框已在配置页面中实现，这里可以切换到配置页面
    m_tabWidget->setCurrentIndex(4); // 配置页面索引
}

void AlarmWidget::showStatisticsDialog()
{
    // 统计对话框已在统计页面中实现，这里可以切换到统计页面
    m_tabWidget->setCurrentIndex(3); // 统计页面索引
}

// 配置保存和加载
void AlarmWidget::saveAlarmConfig()
{
    QSqlQuery query(m_database);
    query.prepare(R"(
        INSERT OR REPLACE INTO alarm_config (
            id, enable_audible, enable_visual, enable_email, enable_sms, enable_tray,
            max_active_alarms, auto_acknowledge_time, sound_duration, sound_file,
            email_recipients, sms_recipients, enable_history, max_history_records,
            enable_statistics, statistics_update_interval
        ) VALUES (1, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
    )");
    
    query.addBindValue(m_alarmConfig.enableAudibleAlarms);
    query.addBindValue(m_alarmConfig.enableVisualAlarms);
    query.addBindValue(m_alarmConfig.enableEmailNotification);
    query.addBindValue(m_alarmConfig.enableSMSNotification);
    query.addBindValue(m_alarmConfig.enableSystemTray);
    query.addBindValue(m_alarmConfig.maxActiveAlarms);
    query.addBindValue(m_alarmConfig.autoAcknowledgeTime);
    query.addBindValue(m_alarmConfig.alarmSoundDuration);
    query.addBindValue(m_alarmConfig.alarmSoundFile);
    query.addBindValue(m_alarmConfig.emailRecipients.join('\n'));
    query.addBindValue(m_alarmConfig.smsRecipients.join('\n'));
    query.addBindValue(m_alarmConfig.enableAlarmHistory);
    query.addBindValue(m_alarmConfig.maxHistoryRecords);
    query.addBindValue(m_alarmConfig.enableAlarmStatistics);
    query.addBindValue(m_alarmConfig.statisticsUpdateInterval);
    
    if (query.exec()) {
        LogManager::getInstance()->info("报警配置已保存", "AlarmWidget");
    } else {
        LogManager::getInstance()->error("保存报警配置失败: " + query.lastError().text(), "AlarmWidget");
    }
}

void AlarmWidget::resetAlarmConfig()
{
    // 重置为默认配置
    m_alarmConfig = AlarmConfig();
    saveAlarmConfig();
}

// 导出和导入功能
void AlarmWidget::exportThresholdsToJson(const QString& filename)
{
    QJsonArray thresholdsArray;
    
    for (const auto& threshold : m_alarmThresholds) {
        QJsonObject thresholdObj;
        thresholdObj["parameterName"] = threshold.parameterName;
        thresholdObj["type"] = static_cast<int>(threshold.type);
        thresholdObj["level"] = static_cast<int>(threshold.level);
        thresholdObj["highHigh"] = threshold.highHigh;
        thresholdObj["high"] = threshold.high;
        thresholdObj["low"] = threshold.low;
        thresholdObj["lowLow"] = threshold.lowLow;
        thresholdObj["enableHighHigh"] = threshold.enableHighHigh;
        thresholdObj["enableHigh"] = threshold.enableHigh;
        thresholdObj["enableLow"] = threshold.enableLow;
        thresholdObj["enableLowLow"] = threshold.enableLowLow;
        thresholdObj["delayTime"] = threshold.delayTime;
        thresholdObj["deadband"] = threshold.deadband;
        thresholdObj["isEnabled"] = threshold.isEnabled;
        
        thresholdsArray.append(thresholdObj);
    }
    
    QJsonDocument doc(thresholdsArray);
    
    QFile file(filename);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(doc.toJson());
        file.close();
        
        QMessageBox::information(this, "导出成功", 
            QString("已成功导出 %1 个阈值配置到文件:\n%2").arg(m_alarmThresholds.size()).arg(filename));
        
        LogManager::getInstance()->info("导出阈值配置到: " + filename, "AlarmWidget");
    } else {
        QMessageBox::warning(this, "导出失败", "无法创建文件: " + filename);
    }
}

void AlarmWidget::importThresholdsFromJson(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "导入失败", "无法打开文件: " + filename);
        return;
    }
    
    QByteArray data = file.readAll();
    file.close();
    
    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        QMessageBox::warning(this, "导入失败", "文件格式无效");
        return;
    }
    
    QJsonArray thresholdsArray = doc.array();
    int importedCount = 0;
    
    for (const auto& value : thresholdsArray) {
        QJsonObject thresholdObj = value.toObject();
        
        AlarmThreshold threshold;
        threshold.parameterName = thresholdObj["parameterName"].toString();
        threshold.type = static_cast<AlarmType>(thresholdObj["type"].toInt());
        threshold.level = static_cast<AlarmLevel>(thresholdObj["level"].toInt());
        threshold.highHigh = thresholdObj["highHigh"].toDouble();
        threshold.high = thresholdObj["high"].toDouble();
        threshold.low = thresholdObj["low"].toDouble();
        threshold.lowLow = thresholdObj["lowLow"].toDouble();
        threshold.enableHighHigh = thresholdObj["enableHighHigh"].toBool();
        threshold.enableHigh = thresholdObj["enableHigh"].toBool();
        threshold.enableLow = thresholdObj["enableLow"].toBool();
        threshold.enableLowLow = thresholdObj["enableLowLow"].toBool();
        threshold.delayTime = thresholdObj["delayTime"].toInt();
        threshold.deadband = thresholdObj["deadband"].toInt();
        threshold.isEnabled = thresholdObj["isEnabled"].toBool();
        
        if (validateThreshold(threshold)) {
            addAlarmThreshold(threshold);
            importedCount++;
        }
    }
    
    QMessageBox::information(this, "导入完成", 
        QString("已成功导入 %1 个阈值配置").arg(importedCount));
    
    LogManager::getInstance()->info(QString("从文件导入 %1 个阈值配置: %2").arg(importedCount).arg(filename), "AlarmWidget");
}

void AlarmWidget::exportStatisticsToCSV(const QString& filename)
{
    QFile file(filename);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "导出失败", "无法创建文件: " + filename);
        return;
    }
    
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    
    // 写入统计信息
    out << "报警系统统计报告\n";
    out << "生成时间," << QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss") << "\n";
    out << "统计时间," << formatDateTime(m_alarmStatistics.lastUpdateTime) << "\n\n";
    
    out << "总体统计\n";
    out << "总报警数," << m_alarmStatistics.totalAlarms << "\n";
    out << "激活报警," << m_alarmStatistics.activeAlarms << "\n";
    out << "已确认报警," << m_alarmStatistics.acknowledgedAlarms << "\n";
    out << "已解决报警," << m_alarmStatistics.resolvedAlarms << "\n";
    out << "平均响应时间(秒)," << m_alarmStatistics.averageResponseTime << "\n";
    out << "平均解决时间(秒)," << m_alarmStatistics.averageResolveTime << "\n\n";
    
    out << "按类型统计\n";
    out << "类型,数量\n";
    for (auto it = m_alarmStatistics.alarmsByType.begin(); 
         it != m_alarmStatistics.alarmsByType.end(); ++it) {
        out << formatAlarmType(it.key()) << "," << it.value() << "\n";
    }
    
    out << "\n按级别统计\n";
    out << "级别,数量\n";
    for (auto it = m_alarmStatistics.alarmsByLevel.begin(); 
         it != m_alarmStatistics.alarmsByLevel.end(); ++it) {
        out << formatAlarmLevel(it.key()) << "," << it.value() << "\n";
    }
    
    file.close();
    
    QMessageBox::information(this, "导出成功", "统计数据已导出到: " + filename);
    LogManager::getInstance()->info("导出统计数据到: " + filename, "AlarmWidget");
}

// 缺失函数的实现
QColor AlarmWidget::getAlarmLevelColor(AlarmLevel level) const
{
    switch (level) {
        case AlarmLevel::Info:
            return QColor(100, 149, 237); // 蓝色
        case AlarmLevel::Warning:
            return QColor(255, 165, 0);   // 橙色
        case AlarmLevel::Error:
            return QColor(255, 69, 0);    // 红橙色
        case AlarmLevel::Critical:
            return QColor(220, 20, 60);   // 深红色
        case AlarmLevel::Emergency:
            return QColor(139, 0, 0);     // 暗红色
        default:
            return QColor(128, 128, 128); // 灰色
    }
}

QIcon AlarmWidget::getAlarmLevelIcon(AlarmLevel level) const
{
    // 简化实现，返回空图标
    Q_UNUSED(level)
    return QIcon();
}

bool AlarmWidget::validateThreshold(const AlarmThreshold& threshold) const
{
    // 验证阈值配置
    if (threshold.parameterName.isEmpty()) {
        return false;
    }
    
    // 验证阈值逻辑关系
    if (threshold.enableHighHigh && threshold.enableHigh && 
        threshold.highHigh <= threshold.high) {
        return false;
    }
    
    if (threshold.enableLow && threshold.enableLowLow && 
        threshold.low <= threshold.lowLow) {
        return false;
    }
    
    if (threshold.enableHigh && threshold.enableLow && 
        threshold.high <= threshold.low) {
        return false;
    }
    
    return true;
} 