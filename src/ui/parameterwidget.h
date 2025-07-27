#pragma once

#include <QWidget>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTextEdit>
#include <QTabWidget>
#include <QSplitter>
#include <QFileDialog>
#include <QProgressBar>
#include <QTimer>
#include <QJsonObject>
#include <QJsonDocument>
#include <QJsonArray>

// 点胶程序结构
struct GlueProgram {
    QString name;               // 程序名称
    QString description;        // 程序描述
    QString version;            // 版本号
    QDateTime createTime;       // 创建时间
    QDateTime modifyTime;       // 修改时间
    
    // 程序参数
    struct ProgramParams {
        double volume;          // 胶量
        double speed;           // 速度
        double pressure;        // 压力
        double temperature;     // 温度
        int dwellTime;          // 停留时间
        int riseTime;           // 上升时间
        int fallTime;           // 下降时间
        
        ProgramParams() : volume(1.0), speed(10.0), pressure(2.0), temperature(25.0)
                        , dwellTime(100), riseTime(50), fallTime(50) {}
    } params;
    
    // 轨迹点列表
    struct TrajectoryPoint {
        double x, y, z;         // 位置坐标
        double speed;           // 该点速度
        double volume;          // 该点胶量
        int dwellTime;          // 该点停留时间
        bool isGluePoint;       // 是否为点胶点
        
        TrajectoryPoint() : x(0), y(0), z(0), speed(10.0), volume(1.0)
                          , dwellTime(100), isGluePoint(true) {}
    };
    
    QList<TrajectoryPoint> trajectory;
    
    GlueProgram() : name("新程序"), description(""), version("1.0")
                  , createTime(QDateTime::currentDateTime())
                  , modifyTime(QDateTime::currentDateTime()) {}
};

// 参数模板结构
struct ParameterTemplate {
    QString name;               // 模板名称
    QString category;           // 分类
    QString description;        // 描述
    QJsonObject parameters;     // 参数内容
    
    ParameterTemplate() : name("默认模板"), category("通用"), description("") {}
};

class ParameterWidget : public QWidget
{
    Q_OBJECT

public:
    explicit ParameterWidget(QWidget* parent = nullptr);
    ~ParameterWidget();
    
    // 程序管理
    void loadProgram(const QString& filePath);
    void saveProgram(const QString& filePath);
    void newProgram();
    void deleteProgram();
    
    // 参数管理
    void loadParameters(const QJsonObject& params);
    QJsonObject saveParameters() const;
    void resetParameters();
    
    // 模板管理
    void loadTemplate(const QString& templateName);
    void saveTemplate(const QString& templateName);
    void deleteTemplate(const QString& templateName);
    
    // 轨迹管理
    void addTrajectoryPoint(const GlueProgram::TrajectoryPoint& point);
    void removeTrajectoryPoint(int index);
    void updateTrajectoryPoint(int index, const GlueProgram::TrajectoryPoint& point);
    void clearTrajectory();
    
    // 当前程序
    GlueProgram getCurrentProgram() const;
    void setCurrentProgram(const GlueProgram& program);

public slots:
    void onImportProgram();
    void onExportProgram();
    void onNewProgram();
    void onDeleteProgram();
    void onDuplicateProgram();
    
    void onLoadTemplate();
    void onSaveTemplate();
    void onDeleteTemplate();
    
    void onAddTrajectoryPoint();
    void onRemoveTrajectoryPoint();
    void onEditTrajectoryPoint();
    void onClearTrajectory();
    
    void onParameterChanged();
    void onProgramSelectionChanged();
    void onTrajectorySelectionChanged();
    
    void onValidateParameters();
    void onOptimizeParameters();

signals:
    void programChanged(const GlueProgram& program);
    void parametersChanged();
    void trajectoryChanged();
    void templateChanged(const QString& templateName);

private slots:
    void onProgramItemChanged(QTreeWidgetItem* item, int column);
    void onTrajectoryItemChanged(QTableWidgetItem* item);
    void onParameterItemChanged();

private:
    void setupUI();
    void setupProgramPanel();
    void setupParameterPanel();
    void setupTrajectoryPanel();
    void setupTemplatePanel();
    void setupConnections();
    
    void updateProgramList();
    void updateParameterDisplay();
    void updateTrajectoryDisplay();
    void updateTemplateList();
    
    void loadProgramList();
    void saveProgramList();
    void loadTemplateList();
    void saveTemplateList();
    
    bool validateProgram(const GlueProgram& program, QString& error);
    void optimizeTrajectory();
    void calculateTrajectoryTime();
    
    QString formatTime(double seconds) const;
    QString formatDistance(double distance) const;
    void addDefaultTemplates();
    void optimizeByDistance();
    void optimizeByTime();
    void smoothSpeed();
    void removeDuplicatePoints();
    double calculateTotalDistance() const;
    double calculateTotalTime() const;
    
    // UI组件
    QTabWidget* tabWidget;
    QSplitter* mainSplitter;
    
    // 程序管理面板
    QGroupBox* programGroup;
    QTreeWidget* programTreeWidget;
    QPushButton* importProgramButton;
    QPushButton* exportProgramButton;
    QPushButton* newProgramButton;
    QPushButton* deleteProgramButton;
    QPushButton* duplicateProgramButton;
    
    // 程序信息面板
    QGroupBox* programInfoGroup;
    QLineEdit* programNameEdit;
    QLineEdit* programVersionEdit;
    QTextEdit* programDescriptionEdit;
    QLabel* createTimeLabel;
    QLabel* modifyTimeLabel;
    
    // 参数设置面板
    QGroupBox* parameterGroup;
    QTableWidget* parameterTableWidget;
    QPushButton* validateParametersButton;
    QPushButton* optimizeParametersButton;
    QPushButton* resetParametersButton;
    
    // 轨迹编辑面板
    QGroupBox* trajectoryGroup;
    QTableWidget* trajectoryTableWidget;
    QPushButton* addPointButton;
    QPushButton* removePointButton;
    QPushButton* editPointButton;
    QPushButton* clearTrajectoryButton;
    QPushButton* optimizeTrajectoryButton;
    
    // 轨迹统计面板
    QGroupBox* trajectoryStatsGroup;
    QLabel* totalPointsLabel;
    QLabel* totalDistanceLabel;
    QLabel* totalTimeLabel;
    QLabel* totalVolumeLabel;
    QProgressBar* trajectoryProgressBar;
    
    // 模板管理面板
    QGroupBox* templateGroup;
    QTreeWidget* templateTreeWidget;
    QPushButton* loadTemplateButton;
    QPushButton* saveTemplateButton;
    QPushButton* deleteTemplateButton;
    
    // 数据成员
    GlueProgram currentProgram;
    QList<GlueProgram> programList;
    QList<ParameterTemplate> templateList;
    
    QString currentProgramPath;
    QString programsDirectory;
    QString templatesDirectory;
    
    bool isModified;
    QTimer* autoSaveTimer;
    
    // 参数验证规则
    struct ValidationRule {
        QString parameter;
        double minValue;
        double maxValue;
        QString unit;
        QString description;
    };
    QList<ValidationRule> validationRules;
    
    // 辅助方法
    void initializeParameterTable();
    void initializeValidationRules();
    void autoSave();
}; 