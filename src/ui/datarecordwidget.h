#ifndef DATARECORDWIDGET_H
#define DATARECORDWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QDateTimeEdit>
#include <QLineEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QProgressBar>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QCheckBox>
#include <QTimer>
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlTableModel>
#include <QChart>
#include <QChartView>
#include <QLineSeries>
#include <QBarSeries>
#include <QBarSet>
#include <QValueAxis>
#include <QDateTimeAxis>
#include <QPieSeries>
#include <QPieSlice>
#include <QScatterSeries>

#include <QtCharts/QChartView>
#include <QtCharts/QChart>
#include <QtCharts/QBarSeries>
#include <QtCharts/QLineSeries>
#include <QtCharts/QPieSeries>

// Use QtCharts namespace

// 生产批次数据结构
struct ProductionBatch {
    int batchId;                    // 批次ID
    QString batchName;              // 批次名称
    QString productType;            // 产品类型
    QDateTime startTime;            // 开始时间
    QDateTime endTime;              // 结束时间
    int totalCount;                 // 总数量
    int qualifiedCount;             // 合格数量
    int defectCount;                // 不良数量
    double qualityRate;             // 合格率
    QString operatorName;           // 操作员
    QString programName;            // 使用程序
    QString notes;                  // 备注
    QJsonObject parameters;         // 工艺参数
    QJsonArray qualityData;         // 质量数据
};

// 质量检测数据结构
struct QualityData {
    int recordId;                   // 记录ID
    int batchId;                    // 批次ID
    QDateTime timestamp;            // 时间戳
    double positionX;               // X坐标
    double positionY;               // Y坐标
    double positionZ;               // Z坐标
    double glueVolume;              // 胶量
    double pressure;                // 压力
    double temperature;             // 温度
    double speed;                   // 速度
    QString qualityLevel;           // 质量等级(A/B/C/D)
    bool isQualified;               // 是否合格
    QString defectType;             // 缺陷类型
    QString inspector;              // 检测员
    QString notes;                  // 备注
};

// 报警记录数据结构
struct DataRecordAlarm {
    int alarmId;                    // 报警ID
    QDateTime timestamp;            // 时间戳
    QString alarmType;              // 报警类型
    QString alarmLevel;             // 报警等级(低/中/高/紧急)
    QString alarmCode;              // 报警代码
    QString alarmMessage;           // 报警信息
    QString deviceName;             // 设备名称
    QString operatorName;           // 操作员
    bool isAcknowledged;            // 是否确认
    QDateTime acknowledgeTime;      // 确认时间
    QString acknowledgeUser;        // 确认用户
    QString solution;               // 解决方案
    QString notes;                  // 备注
};

// 统计数据结构
struct DataRecordStatistics {
    QDateTime date;                 // 日期
    int totalBatches;               // 总批次数
    int totalProducts;              // 总产品数
    int qualifiedProducts;          // 合格产品数
    int defectProducts;             // 不良产品数
    double qualityRate;             // 合格率
    double efficiency;              // 生产效率
    double uptime;                  // 设备运行时间
    double downtime;                // 设备停机时间
    int alarmCount;                 // 报警次数
    QString topDefectType;          // 主要缺陷类型
    double averageGlueVolume;       // 平均胶量
    double averagePressure;         // 平均压力
    double averageTemperature;      // 平均温度
};

class DataRecordWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DataRecordWidget(QWidget *parent = nullptr);
    ~DataRecordWidget();

    // 数据记录接口
    void addProductionBatch(const ProductionBatch& batch);
    void addQualityData(const QualityData& data);
    void addAlarmRecord(const DataRecordAlarm& alarm);
    void updateBatchStatus(int batchId, const QString& status);
    
    // 数据查询接口
    QList<ProductionBatch> getProductionBatches(const QDateTime& startTime, const QDateTime& endTime);
    QList<QualityData> getQualityData(int batchId);
    QList<DataRecordAlarm> getAlarmRecords(const QDateTime& startTime, const QDateTime& endTime);
    DataRecordStatistics getStatistics(const QDateTime& date);
    
    // 数据导出接口
    bool exportToCSV(const QString& filename, const QString& dataType);
    bool exportToExcel(const QString& filename, const QString& dataType);
    bool exportReport(const QString& filename, const QDateTime& startTime, const QDateTime& endTime);

public slots:
    void onDataReceived(const QJsonObject& data);
    void onAlarmTriggered(const QString& alarmType, const QString& message);
    void onBatchStarted(const QString& batchName, const QString& productType);
    void onBatchCompleted(int batchId);

private slots:
    void onRefreshData();
    void onExportData();
    void onFilterChanged();
    void onDateRangeChanged();
    void onBatchSelectionChanged();
    void onQualityDataSelectionChanged();
    void onAlarmSelectionChanged();
    void onGenerateReport();
    void onClearOldData();
    void onBackupData();
    void onRestoreData();
    void onUpdateStatistics();
    void onShowChart();
    void onPrintReport();
    void onAcknowledgeAlarm();
    void onSearchData();

private:
    void setupUI();
    void setupDatabase();
    void setupConnections();
    void setupProductionTab();
    void setupQualityTab();
    void setupAlarmTab();
    void setupStatisticsTab();
    void setupReportTab();
    void setupExportTab();
    
    void loadProductionData();
    void loadQualityData();
    void loadAlarmData();
    void loadStatisticsData();
    void updateProductionTable();
    void updateQualityTable();
    void updateAlarmTable();
    void updateStatisticsCharts();
    void updateSummaryInfo();
    
    void createQualityChart();
    void createTrendChart();
    void createDefectChart();
    void createEfficiencyChart();
    
    bool initializeDatabase();
    bool createTables();
    bool insertProductionBatch(const ProductionBatch& batch);
    bool insertQualityData(const QualityData& data);
    bool insertAlarmRecord(const DataRecordAlarm& alarm);
    bool updateDatabase();
    
    void applyFilters();
    void resetFilters();
    void validateData();
    void optimizeDatabase();
    
    QString formatDateTime(const QDateTime& dateTime);
    QString formatDuration(qint64 seconds);
    QString formatFileSize(qint64 bytes);
    QColor getQualityColor(const QString& level);
    QColor getAlarmColor(const QString& level);

private:
    // UI组件
    QTabWidget* m_tabWidget;
    
    // 生产数据页面
    QWidget* m_productionTab;
    QTableWidget* m_productionTable;
    QComboBox* m_productTypeFilter;
    QDateTimeEdit* m_startDateEdit;
    QDateTimeEdit* m_endDateEdit;
    QPushButton* m_refreshBtn;
    QPushButton* m_exportBtn;
    QLabel* m_totalBatchesLabel;
    QLabel* m_totalProductsLabel;
    QLabel* m_qualityRateLabel;
    
    // 质量数据页面
    QWidget* m_qualityTab;
    QTableWidget* m_qualityTable;
    QComboBox* m_batchFilter;
    QComboBox* m_qualityFilter;
    QPushButton* m_qualityChartBtn;
    QChartView* m_qualityChartView;
    QLabel* m_qualityStatsLabel;
    
    // 报警记录页面
    QWidget* m_alarmTab;
    QTableWidget* m_alarmTable;
    QComboBox* m_alarmTypeFilter;
    QComboBox* m_alarmLevelFilter;
    QPushButton* m_acknowledgeBtn;
    QPushButton* m_clearAlarmsBtn;
    QLabel* m_alarmCountLabel;
    QLabel* m_unacknowledgedLabel;
    
    // 统计分析页面
    QWidget* m_statisticsTab;
    QChartView* m_trendChartView;
    QChartView* m_defectChartView;
    QChartView* m_efficiencyChartView;
    QTableWidget* m_statisticsTable;
    QComboBox* m_statisticsPeriod;
    QPushButton* m_updateStatsBtn;
    
    // 报表生成页面
    QWidget* m_reportTab;
    QComboBox* m_reportType;
    QDateTimeEdit* m_reportStartDate;
    QDateTimeEdit* m_reportEndDate;
    QTextEdit* m_reportPreview;
    QPushButton* m_generateReportBtn;
    QPushButton* m_printReportBtn;
    QPushButton* m_saveReportBtn;
    
    // 数据导出页面
    QWidget* m_exportTab;
    QComboBox* m_exportDataType;
    QComboBox* m_exportFormat;
    QLineEdit* m_exportPath;
    QPushButton* m_browseBtn;
    QPushButton* m_exportDataBtn;
    QPushButton* m_backupBtn;
    QPushButton* m_restoreBtn;
    QProgressBar* m_exportProgress;
    
    // 数据模型
    QStandardItemModel* m_productionModel;
    QStandardItemModel* m_qualityModel;
    QStandardItemModel* m_alarmModel;
    QStandardItemModel* m_statisticsModel;
    QSortFilterProxyModel* m_productionProxy;
    QSortFilterProxyModel* m_qualityProxy;
    QSortFilterProxyModel* m_alarmProxy;
    
    // 数据库
    QSqlDatabase m_database;
    QString m_databasePath;
    
    // 数据缓存
    QList<ProductionBatch> m_productionBatches;
    QList<QualityData> m_qualityDataList;
    QList<DataRecordAlarm> m_alarmRecords;
    QList<DataRecordStatistics> m_statisticsDataList;
    
    // 定时器
    QTimer* m_updateTimer;
    QTimer* m_backupTimer;
    
    // 配置参数
    int m_maxRecords;
    int m_backupInterval;
    QString m_exportDirectory;
    QString m_reportTemplate;
    bool m_autoBackup;
    bool m_realTimeUpdate;
    
    // 状态变量
    bool m_isRecording;
    int m_currentBatchId;
    QDateTime m_lastUpdateTime;
    QString m_currentOperator;
    
signals:
    void batchAdded(const ProductionBatch& batch);
    void qualityDataAdded(const QualityData& data);
    void alarmAdded(const DataRecordAlarm& alarm);
    void statisticsUpdated(const DataRecordStatistics& stats);
    void dataExported(const QString& filename);
    void reportGenerated(const QString& filename);
    void databaseError(const QString& error);
    void backupCompleted(const QString& filename);
    void dataCleared(int recordCount);
};

#endif // DATARECORDWIDGET_H 