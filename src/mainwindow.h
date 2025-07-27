#pragma once

#include <QMainWindow>
#include <QCloseEvent>
#include <QShowEvent>
#include <QHideEvent>
#include <QTimer>
#include <QDateTime>

// 前置声明
class UIManager;
class BusinessLogicManager;
class SystemManager;
class EventCoordinator;
class ContinuousOptimizer;
class IntelligentAnalyzer;
class AdaptiveConfigManager;
class LoadBalancer;
class MLPerformancePredictor;

/**
 * @brief 重构后的MainWindow类
 * 
 * 采用管理器模式分离职责，MainWindow专注于：
 * - 作为各个管理器的容器和协调者
 * - 处理窗口生命周期事件
 * - 提供统一的应用程序入口点
 * 
 * 具体功能委托给专门的管理器：
 * - UIManager: UI界面管理
 * - BusinessLogicManager: 业务逻辑处理
 * - SystemManager: 系统管理
 * - EventCoordinator: 事件协调
 */
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    // 应用程序生命周期
    void initializeApplication();
    void shutdownApplication();
    
    // 管理器访问
    UIManager* getUIManager() const { return uiManager; }
    BusinessLogicManager* getBusinessLogicManager() const { return businessLogicManager; }
    SystemManager* getSystemManager() const { return systemManager; }
    EventCoordinator* getEventCoordinator() const { return eventCoordinator; }
    
    // 持续优化组件访问
    ContinuousOptimizer* getContinuousOptimizer() const { return continuousOptimizer; }
    IntelligentAnalyzer* getIntelligentAnalyzer() const { return intelligentAnalyzer; }
    AdaptiveConfigManager* getAdaptiveConfigManager() const { return adaptiveConfigManager; }
    LoadBalancer* getLoadBalancer() const { return loadBalancer; }
    MLPerformancePredictor* getMLPerformancePredictor() const { return mlPerformancePredictor; }
    
    // 应用程序状态
    bool isApplicationInitialized() const { return m_applicationInitialized; }
    bool isApplicationShuttingDown() const { return m_applicationShuttingDown; }

protected:
    // 窗口事件处理
    void closeEvent(QCloseEvent* event) override;
    void changeEvent(QEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;
    
    // 错误处理
    void handleCriticalError(const QString& error);
    void handleRecoverableError(const QString& error);

private slots:
    // 管理器事件处理
    void onUIManagerEvent(const QString& eventType, const QVariant& data);
    void onBusinessLogicManagerEvent(const QString& eventType, const QVariant& data);
    void onSystemManagerEvent(const QString& eventType, const QVariant& data);
    void onEventCoordinatorEvent(const QString& eventType, const QVariant& data);
    
    // 应用程序事件
    void onApplicationStarted();
    void onApplicationClosing();
    void onCriticalError(const QString& error);
    void onSystemShutdown();
    
    // 定时任务
    void onPeriodicUpdate();
    void onHeartbeat();

signals:
    // 应用程序生命周期信号
    void applicationInitialized();
    void applicationReady();
    void applicationShuttingDown();
    void applicationClosed();
    
    // 错误信号
    void criticalErrorOccurred(const QString& error);
    void recoverableErrorOccurred(const QString& error);
    
    // 状态变化信号
    void applicationStateChanged(const QString& state);
    void mainWindowStateChanged(const QString& state);

private:
    // 初始化方法
    void createManagers();
    void setupManagerConnections();
    void initializeManagers();
    void setupApplication();
    void loadApplicationSettings();
    void saveApplicationSettings();
    
    // 关闭方法
    void prepareShutdown();
    void shutdownManagers();
    void saveApplicationState();
    bool confirmExit();
    
    // 事件处理
    void setupEventHandling();
    void processManagerEvents();
    void handleManagerError(const QString& manager, const QString& error);
    
    // 定时器管理
    void setupTimers();
    void stopTimers();
    
    // 状态管理
    void updateApplicationState();
    void validateApplicationState();
    
    // 核心管理器
    UIManager* uiManager;
    BusinessLogicManager* businessLogicManager;
    SystemManager* systemManager;
    EventCoordinator* eventCoordinator;
    
    // 持续优化组件
    ContinuousOptimizer* continuousOptimizer;
    IntelligentAnalyzer* intelligentAnalyzer;
    AdaptiveConfigManager* adaptiveConfigManager;
    LoadBalancer* loadBalancer;
    MLPerformancePredictor* mlPerformancePredictor;
    
    // 应用程序状态
    bool m_applicationInitialized;
    bool m_applicationShuttingDown;
    bool managersInitialized;
    QString currentApplicationState;
    
    // 定时器
    QTimer* heartbeatTimer;
    QTimer* periodicUpdateTimer;
    
    // 错误处理
    int criticalErrorCount;
    int recoverableErrorCount;
    QDateTime lastErrorTime;
    
    // 配置
    bool autoSaveEnabled;
    bool confirmExitEnabled;
    int heartbeatInterval;
    int periodicUpdateInterval;
    int maxCriticalErrors;
    int maxRecoverableErrors;
};