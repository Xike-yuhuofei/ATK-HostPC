#pragma once

#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDateTimeEdit>
#include <QProgressBar>
#include <QSlider>
#include <QTimer>
#include <QSplitter>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QListWidget>
#include <QListWidgetItem>
#include <QStandardItemModel>
#include <QSortFilterProxyModel>
#include <QHeaderView>
#include <QMessageBox>
#include <QFileDialog>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>
#include <QDateTime>
#include <QMutex>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QApplication>
#include <QStyle>
#include <QSoundEffect>
#include <QSystemTrayIcon>

// 报警级别枚举
enum class AlarmLevel {
    Info = 0,       // 信息
    Warning = 1,    // 警告
    Error = 2,      // 错误
    Critical = 3,   // 严重
    Emergency = 4   // 紧急
};

// 报警类型枚举
enum class AlarmType {
    System = 0,         // 系统报警
    Device = 1,         // 设备报警
    Process = 2,        // 工艺报警
    Quality = 3,        // 质量报警
    Safety = 4,         // 安全报警
    Communication = 5,  // 通讯报警
    Temperature = 6,    // 温度报警
    Pressure = 7,       // 压力报警
    Position = 8,       // 位置报警
    Speed = 9          // 速度报警
};

// 报警状态枚举
enum class AlarmStatus {
    Active = 0,         // 激活
    Acknowledged = 1,   // 已确认
    Resolved = 2,       // 已解决
    Suppressed = 3      // 已抑制
};

#include "data/datamodels.h"

// 报警配置结构
struct AlarmConfig {
    bool enableAudibleAlarms;       // 启用声音报警
    bool enableVisualAlarms;        // 启用视觉报警
    bool enableEmailNotification;  // 启用邮件通知
    bool enableSMSNotification;     // 启用短信通知
    bool enableSystemTray;          // 启用系统托盘通知
    int maxActiveAlarms;            // 最大激活报警数
    int autoAcknowledgeTime;        // 自动确认时间(秒)
    int alarmSoundDuration;         // 报警声音持续时间(秒)
    QString alarmSoundFile;         // 报警声音文件
    QStringList emailRecipients;   // 邮件接收者
    QStringList smsRecipients;      // 短信接收者
    bool enableAlarmHistory;        // 启用报警历史
    int maxHistoryRecords;          // 最大历史记录数
    bool enableAlarmStatistics;     // 启用报警统计
    int statisticsUpdateInterval;   // 统计更新间隔(秒)
    
    AlarmConfig() 
        : enableAudibleAlarms(true), enableVisualAlarms(true)
        , enableEmailNotification(false), enableSMSNotification(false)
        , enableSystemTray(true), maxActiveAlarms(100), autoAcknowledgeTime(0)
        , alarmSoundDuration(5), enableAlarmHistory(true), maxHistoryRecords(10000)
        , enableAlarmStatistics(true), statisticsUpdateInterval(60) {}
};

// 报警阈值结构
struct AlarmThreshold {
    QString parameterName;          // 参数名称
    AlarmType type;                 // 报警类型
    AlarmLevel level;               // 报警级别
    double highHigh;                // 高高限
    double high;                    // 高限
    double low;                     // 低限
    double lowLow;                  // 低低限
    bool enableHighHigh;            // 启用高高限
    bool enableHigh;                // 启用高限
    bool enableLow;                 // 启用低限
    bool enableLowLow;              // 启用低低限
    int delayTime;                  // 延时时间(秒)
    int deadband;                   // 死区
    bool isEnabled;                 // 是否启用
    
    AlarmThreshold() 
        : type(AlarmType::Process), level(AlarmLevel::Warning)
        , highHigh(100), high(80), low(20), lowLow(0)
        , enableHighHigh(false), enableHigh(true), enableLow(true), enableLowLow(false)
        , delayTime(0), deadband(0), isEnabled(true) {}
};

// 报警统计结构
struct AlarmStatistics {
    int totalAlarms;                // 总报警数
    int activeAlarms;               // 激活报警数
    int acknowledgedAlarms;         // 已确认报警数
    int resolvedAlarms;             // 已解决报警数
    QMap<AlarmType, int> alarmsByType;      // 按类型统计
    QMap<AlarmLevel, int> alarmsByLevel;    // 按级别统计
    QMap<QString, int> alarmsByDevice;      // 按设备统计
    QMap<QString, int> alarmsByOperator;    // 按操作员统计
    double averageResponseTime;     // 平均响应时间(秒)
    double averageResolveTime;      // 平均解决时间(秒)
    QDateTime lastUpdateTime;       // 最后更新时间
    
    AlarmStatistics() 
        : totalAlarms(0), activeAlarms(0), acknowledgedAlarms(0), resolvedAlarms(0)
        , averageResponseTime(0), averageResolveTime(0)
        , lastUpdateTime(QDateTime::currentDateTime()) {}
};

class AlarmWidget : public QWidget
{
    Q_OBJECT

public:
    explicit AlarmWidget(QWidget* parent = nullptr);
    ~AlarmWidget();
    
    // 报警管理接口
    void triggerAlarm(const AlarmRecord& alarm);
    void acknowledgeAlarm(int alarmId, const QString& user);
    void resolveAlarm(int alarmId, const QString& user, const QString& solution);
    void suppressAlarm(int alarmId, const QString& reason);
    void clearAlarm(int alarmId);
    void clearAllAlarms();
    
    // 报警查询接口
    AlarmRecord getAlarmRecord(int alarmId);
    QList<AlarmRecord> getActiveAlarms();
    QList<AlarmRecord> getAlarmHistory(const QDateTime& startTime, const QDateTime& endTime);
    QList<AlarmRecord> getAlarmsByType(AlarmType type);
    QList<AlarmRecord> getAlarmsByLevel(AlarmLevel level);
    QList<AlarmRecord> getAlarmsByDevice(const QString& deviceName);
    
    // 报警配置接口
    void setAlarmConfig(const AlarmConfig& config);
    AlarmConfig getAlarmConfig() const;
    void addAlarmThreshold(const AlarmThreshold& threshold);
    void removeAlarmThreshold(const QString& parameterName);
    void updateAlarmThreshold(const AlarmThreshold& threshold);
    QList<AlarmThreshold> getAlarmThresholds() const;
    
    // 报警统计接口
    AlarmStatistics getAlarmStatistics() const;
    void updateStatistics();
    void resetStatistics();
    
    // 报警检查接口
    void checkParameter(const QString& parameterName, double value);
    void checkAllParameters(const QJsonObject& parameters);
    bool isAlarmActive(const QString& alarmCode);
    int getActiveAlarmCount() const;
    int getUnacknowledgedAlarmCount() const;

public slots:
    void onParameterValueChanged(const QString& parameter, double value);
    void onDeviceStatusChanged(const QString& device, int status);
    void onCommunicationError(const QString& connection, const QString& error);
    void onSystemError(const QString& error);
    void onQualityAlert(const QString& parameter, double value, double threshold);
    void onSafetyAlert(const QString& message);

private slots:
    void onAcknowledgeSelected();
    void onResolveSelected();
    void onSuppressSelected();
    void onClearSelected();
    void onClearAll();
    void onRefreshAlarms();
    void onExportAlarms();
    void onConfigureAlarms();
    void onShowStatistics();
    void onShowHistory();
    void onFilterChanged();
    void onSortChanged();
    void onAlarmSelectionChanged();
    void onThresholdChanged();
    void onConfigChanged();
    void onPlayAlarmSound();
    void onStopAlarmSound();
    void onUpdateTimer();
    void onStatisticsTimer();
    void onAutoAcknowledgeTimer();

signals:
    void alarmTriggered(const AlarmRecord& alarm);
    void alarmAcknowledged(int alarmId, const QString& user);
    void alarmResolved(int alarmId, const QString& user);
    void alarmCleared(int alarmId);
    void alarmConfigChanged(const AlarmConfig& config);
    void thresholdChanged(const AlarmThreshold& threshold);
    void statisticsUpdated(const AlarmStatistics& stats);
    void criticalAlarmTriggered(const AlarmRecord& alarm);
    void emergencyAlarmTriggered(const AlarmRecord& alarm);

private:
    void setupUI();
    void setupDatabase();
    void setupConnections();
    void setupActiveAlarmsTab();
    void setupHistoryTab();
    void setupThresholdsTab();
    void setupStatisticsTab();
    void setupConfigTab();
    
    void loadActiveAlarms();
    void loadAlarmHistory();
    void loadAlarmThresholds();
    void loadAlarmConfig();
    void updateActiveAlarmsTable();
    void updateHistoryTable();
    void updateThresholdsTable();
    void updateStatisticsDisplay();
    void updateAlarmSummary();
    
    bool initializeDatabase();
    bool createTables();
    bool insertAlarmRecord(const AlarmRecord& alarm);
    bool updateAlarmRecord(const AlarmRecord& alarm);
    bool deleteAlarmRecord(int alarmId);
    bool insertAlarmThreshold(const AlarmThreshold& threshold);
    bool updateAlarmThresholdRecord(const AlarmThreshold& threshold);
    bool deleteAlarmThreshold(const QString& parameterName);
    
    void processAlarm(const AlarmRecord& alarm);
    void playAlarmSound(AlarmLevel level);
    void showVisualAlarm(const AlarmRecord& alarm);
    void sendNotification(const AlarmRecord& alarm);
    void sendEmailNotification(const AlarmRecord& alarm);
    void sendSMSNotification(const AlarmRecord& alarm);
    void showSystemTrayNotification(const AlarmRecord& alarm);
    
    void applyAlarmFilters();
    void sortAlarms(int column, Qt::SortOrder order);
    void exportAlarmsToCSV(const QString& filename);
    void exportAlarmsToExcel(const QString& filename);
    void printAlarmReport();
    
    void showAlarmDetailsDialog(const AlarmRecord& alarm);
    void showThresholdDialog(const AlarmThreshold& threshold = AlarmThreshold());
    void showConfigDialog();
    void showStatisticsDialog();
    
    QString formatAlarmLevel(AlarmLevel level) const;
    QString formatAlarmType(AlarmType type) const;
    QString formatAlarmStatus(AlarmStatus status) const;
    QString formatDateTime(const QDateTime& dateTime) const;
    QString formatDuration(qint64 seconds) const;
    QColor getAlarmLevelColor(AlarmLevel level) const;
    QIcon getAlarmLevelIcon(AlarmLevel level) const;
    QIcon getAlarmTypeIcon(AlarmType type) const;
    
    bool validateThreshold(const AlarmThreshold& threshold) const;
    bool checkThresholdViolation(const AlarmThreshold& threshold, double value) const;
    AlarmLevel determineAlarmLevel(const AlarmThreshold& threshold, double value) const;
    
    void calculateStatistics();
    void updateAlarmCounts();
    void cleanupOldAlarms();
    
    // 缺少的函数声明
    void saveAlarmConfig();
    void resetAlarmConfig();
    void exportThresholdsToJson(const QString& filename);
    void importThresholdsFromJson(const QString& filename);
    void exportStatisticsToCSV(const QString& filename);

private:
    // UI组件
    QTabWidget* m_tabWidget;
    QSplitter* m_mainSplitter;
    
    // 激活报警页面
    QWidget* m_activeAlarmsTab;
    QTableWidget* m_activeAlarmsTable;
    QComboBox* m_alarmTypeFilter;
    QComboBox* m_alarmLevelFilter;
    QComboBox* m_alarmStatusFilter;
    QLineEdit* m_alarmSearchEdit;
    QPushButton* m_acknowledgeBtn;
    QPushButton* m_resolveBtn;
    QPushButton* m_suppressBtn;
    QPushButton* m_clearBtn;
    QPushButton* m_clearAllBtn;
    QPushButton* m_refreshBtn;
    QPushButton* m_exportBtn;
    QPushButton* m_configBtn;
    QLabel* m_totalAlarmsLabel;
    QLabel* m_activeAlarmsLabel;
    QLabel* m_unacknowledgedLabel;
    QLabel* m_criticalAlarmsLabel;
    
    // 历史记录页面
    QWidget* m_historyTab;
    QTableWidget* m_historyTable;
    QDateTimeEdit* m_historyStartDate;
    QDateTimeEdit* m_historyEndDate;
    QComboBox* m_historyTypeFilter;
    QComboBox* m_historyLevelFilter;
    QPushButton* m_historySearchBtn;
    QPushButton* m_historyExportBtn;
    QPushButton* m_historyClearBtn;
    QLabel* m_historyCountLabel;
    
    // 阈值配置页面
    QWidget* m_thresholdsTab;
    QTableWidget* m_thresholdsTable;
    QPushButton* m_addThresholdBtn;
    QPushButton* m_editThresholdBtn;
    QPushButton* m_deleteThresholdBtn;
    QPushButton* m_importThresholdsBtn;
    QPushButton* m_exportThresholdsBtn;
    QCheckBox* m_enableThresholdsCheckBox;
    
    // 统计分析页面
    QWidget* m_statisticsTab;
    QLabel* m_statsOverviewLabel;
    QTableWidget* m_statsTable;
    QPushButton* m_updateStatsBtn;
    QPushButton* m_resetStatsBtn;
    QPushButton* m_exportStatsBtn;
    QProgressBar* m_statsProgress;
    
    // 配置页面
    QWidget* m_configTab;
    QCheckBox* m_enableAudibleCheckBox;
    QCheckBox* m_enableVisualCheckBox;
    QCheckBox* m_enableEmailCheckBox;
    QCheckBox* m_enableSMSCheckBox;
    QCheckBox* m_enableTrayCheckBox;
    QSpinBox* m_maxActiveAlarmsSpinBox;
    QSpinBox* m_autoAckTimeSpinBox;
    QSpinBox* m_soundDurationSpinBox;
    QLineEdit* m_soundFileEdit;
    QPushButton* m_browseSoundBtn;
    QPushButton* m_testSoundBtn;
    QTextEdit* m_emailRecipientsEdit;
    QTextEdit* m_smsRecipientsEdit;
    QPushButton* m_saveConfigBtn;
    QPushButton* m_resetConfigBtn;
    QCheckBox* m_enableAlarmHistoryCheckBox;
    QSpinBox* m_maxHistoryRecordsSpinBox;
    QCheckBox* m_enableStatisticsCheckBox;
    QSpinBox* m_statisticsUpdateIntervalSpinBox;
    
    // 数据模型
    QStandardItemModel* m_activeAlarmsModel;
    QStandardItemModel* m_historyModel;
    QStandardItemModel* m_thresholdsModel;
    QStandardItemModel* m_statisticsModel;
    QSortFilterProxyModel* m_activeAlarmsProxy;
    QSortFilterProxyModel* m_historyProxy;
    QSortFilterProxyModel* m_thresholdsProxy;
    
    // 数据存储
    QList<AlarmRecord> m_activeAlarms;
    QList<AlarmRecord> m_alarmHistory;
    QList<AlarmThreshold> m_alarmThresholds;
    AlarmConfig m_alarmConfig;
    AlarmStatistics m_alarmStatistics;
    
    // 数据库
    QSqlDatabase m_database;
    QString m_databasePath;
    
    // 定时器
    QTimer* m_updateTimer;
    QTimer* m_statisticsTimer;
    QTimer* m_autoAcknowledgeTimer;
    QTimer* m_cleanupTimer;
    
    // 声音和通知
    QSoundEffect* m_alarmSound;
    QSystemTrayIcon* m_systemTray;
    
    // 状态变量
    bool m_isInitialized;
    bool m_isSoundPlaying;
    int m_nextAlarmId;
    QDateTime m_lastUpdateTime;
    QMutex m_alarmMutex;
    
    // 配置
    QString m_configDirectory;
    QString m_soundDirectory;
    QString m_exportDirectory;
    
    // 常量
    static const int UPDATE_INTERVAL = 1000;           // 更新间隔(ms)
    static const int STATISTICS_INTERVAL = 60000;      // 统计间隔(ms)
    static const int CLEANUP_INTERVAL = 3600000;       // 清理间隔(ms)
    static const int MAX_DISPLAY_ALARMS = 1000;        // 最大显示报警数
    static const QStringList ALARM_TYPES;
    static const QStringList ALARM_LEVELS;
    static const QStringList ALARM_STATUSES;
}; 