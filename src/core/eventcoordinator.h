#pragma once

#include <QObject>
#include <QHash>
#include <QQueue>
#include <QMutex>
#include <QTimer>
#include <QVariant>
#include <QJsonObject>

// 前置声明
class UIManager;
class BusinessLogicManager;
class SystemManager;

/**
 * @brief 事件协调器类
 * 
 * 负责各个管理器之间的事件协调和分发，实现：
 * - 事件路由和分发
 * - 事件优先级管理
 * - 事件队列管理
 * - 跨模块通信
 * - 事件历史记录
 */
class EventCoordinator : public QObject
{
    Q_OBJECT

public:
    // 事件类型枚举
    enum class EventType {
        // UI事件
        UIAction,
        UIStateChange,
        UIError,
        
        // 业务逻辑事件
        DeviceControl,
        DataProcessing,
        ParameterChange,
        AlarmTrigger,
        
        // 系统事件
        SystemStartup,
        SystemShutdown,
        ConfigurationChange,
        UserSession,
        
        // 通用事件
        Error,
        Warning,
        Information,
        Custom
    };
    
    // 事件优先级枚举
    enum class EventPriority {
        Critical = 0,   // 最高优先级
        High = 1,
        Normal = 2,
        Low = 3,
        Background = 4  // 最低优先级
    };
    
    // 事件结构
    struct Event {
        EventType type;
        EventPriority priority;
        QString source;
        QString target;
        QString action;
        QVariant data;
        QDateTime timestamp;
        bool processed;
        int retryCount;
        QString eventId;
        
        Event() : type(EventType::Custom), priority(EventPriority::Normal), 
                 processed(false), retryCount(0) {
            timestamp = QDateTime::currentDateTime();
        }
    };

public:
    explicit EventCoordinator(QObject* parent = nullptr);
    ~EventCoordinator();

    // 初始化和关闭
    void initialize();
    void shutdown();
    
    // 管理器注册
    void registerUIManager(UIManager* manager);
    void registerBusinessLogicManager(BusinessLogicManager* manager);
    void registerSystemManager(SystemManager* manager);
    
    // 事件分发
    void dispatchEvent(const Event& event);
    void dispatchEvent(EventType type, const QString& source, const QString& target, 
                      const QString& action, const QVariant& data = QVariant(),
                      EventPriority priority = EventPriority::Normal);
    
    // 事件订阅
    void subscribeToEvents(const QString& subscriber, const QList<EventType>& eventTypes);
    void unsubscribeFromEvents(const QString& subscriber, const QList<EventType>& eventTypes);
    void unsubscribeFromAllEvents(const QString& subscriber);
    
    // 事件路由
    void routeEvent(const Event& event);
    void routeUIEvent(const Event& event);
    void routeBusinessLogicEvent(const Event& event);
    void routeSystemEvent(const Event& event);
    
    // 事件队列管理
    void startEventProcessing();
    void stopEventProcessing();
    void clearEventQueue();
    int getQueueSize() const;
    QList<Event> getPendingEvents() const;
    
    // 事件历史
    void enableEventHistory(bool enabled);
    QList<Event> getEventHistory(int maxCount = 100) const;
    QList<Event> getEventHistory(EventType type, int maxCount = 100) const;
    void clearEventHistory();
    
    // 事件统计
    QJsonObject getEventStatistics() const;
    int getEventCount(EventType type) const;
    int getTotalEventCount() const;
    double getAverageProcessingTime() const;
    
    // 错误处理
    void handleEventError(const Event& event, const QString& error);
    void retryFailedEvent(const QString& eventId);
    void clearFailedEvents();
    
    // 状态查询
    bool isProcessingEvents() const;
    bool isEventHistoryEnabled() const;
    int getMaxRetryCount() const;
    void setMaxRetryCount(int maxRetries);

public slots:
    // 事件处理槽
    void processNextEvent();
    void processEventQueue();
    void handleEventTimeout();
    
    // UI事件处理
    void onUIActionTriggered(const QString& action, const QVariant& data = QVariant());
    void onUIStateChanged(const QString& state, const QVariant& data = QVariant());
    void onUIError(const QString& error);
    
    // 业务逻辑事件处理
    void onDeviceControlRequested(const QString& command, const QVariant& data = QVariant());
    void onDataProcessingRequested(const QString& operation, const QVariant& data = QVariant());
    void onParameterChangeRequested(const QString& parameter, const QVariant& value);
    void onAlarmTriggered(const QString& alarmType, const QString& message);
    
    // 系统事件处理
    void onSystemStartup();
    void onSystemShutdown();
    void onConfigurationChanged(const QString& key, const QVariant& value);
    void onUserSessionChanged(const QString& sessionState, const QVariant& data = QVariant());
    
    // 通用事件处理
    void onError(const QString& error, const QString& context = QString());
    void onWarning(const QString& warning, const QString& context = QString());
    void onInformation(const QString& info, const QString& context = QString());

signals:
    // 事件分发信号
    void eventDispatched(const Event& event);
    void eventProcessed(const Event& event);
    void eventFailed(const Event& event, const QString& error);
    void eventRetried(const Event& event);
    
    // UI事件信号
    void uiActionEvent(const QString& action, const QVariant& data);
    void uiStateChangeEvent(const QString& state, const QVariant& data);
    void uiErrorEvent(const QString& error);
    
    // 业务逻辑事件信号
    void deviceControlEvent(const QString& command, const QVariant& data);
    void dataProcessingEvent(const QString& operation, const QVariant& data);
    void parameterChangeEvent(const QString& parameter, const QVariant& value);
    void alarmEvent(const QString& alarmType, const QString& message);
    
    // 系统事件信号
    void systemStartupEvent();
    void systemShutdownEvent();
    void configurationChangeEvent(const QString& key, const QVariant& value);
    void userSessionEvent(const QString& sessionState, const QVariant& data);
    
    // 通用事件信号
    void errorEvent(const QString& error, const QString& context);
    void warningEvent(const QString& warning, const QString& context);
    void informationEvent(const QString& info, const QString& context);
    
    // 状态变化信号
    void eventProcessingStarted();
    void eventProcessingStopped();
    void eventQueueCleared();
    void eventHistoryCleared();
    void eventStatisticsUpdated();

private:
    void initializeEventProcessing();
    void setupEventRouting();
    void setupEventTimers();
    void processEvent(const Event& event);
    void addEventToHistory(const Event& event);
    void updateEventStatistics(const Event& event);
    void cleanupExpiredEvents();
    
    // 事件路由内部方法
    void routeToUIManager(const Event& event);
    void routeToBusinessLogicManager(const Event& event);
    void routeToSystemManager(const Event& event);
    void routeToSubscribers(const Event& event);
    
    // 事件队列管理内部方法
    void enqueueEvent(const Event& event);
    Event dequeueEvent();
    void prioritizeEvents();
    void removeExpiredEvents();
    
    // 事件验证
    bool validateEvent(const Event& event) const;
    bool isEventTypeValid(EventType type) const;
    bool isEventPriorityValid(EventPriority priority) const;
    
    // 生成事件ID
    QString generateEventId() const;
    
    // 管理器引用
    UIManager* uiManager;
    BusinessLogicManager* businessLogicManager;
    SystemManager* systemManager;
    
    // 事件队列
    QQueue<Event> eventQueue;
    QMutex eventQueueMutex;
    
    // 事件历史
    QList<Event> eventHistory;
    QMutex eventHistoryMutex;
    bool eventHistoryEnabled;
    int maxHistorySize;
    
    // 事件订阅
    QHash<QString, QList<EventType>> eventSubscriptions;
    QMutex subscriptionMutex;
    
    // 失败事件管理
    QHash<QString, Event> failedEvents;
    QMutex failedEventsMutex;
    int maxRetryCount;
    
    // 事件统计
    struct EventStatistics {
        QHash<EventType, int> eventCounts;
        QHash<EventType, qint64> totalProcessingTime;
        QHash<EventType, int> failedEventCounts;
        QDateTime startTime;
        QDateTime lastUpdate;
    } eventStats;
    QMutex eventStatsMutex;
    
    // 定时器
    QTimer* eventProcessingTimer;
    QTimer* eventCleanupTimer;
    QTimer* eventTimeoutTimer;
    
    // 配置参数
    bool eventProcessingEnabled;
    bool eventLoggingEnabled;
    int eventProcessingInterval;
    int eventCleanupInterval;
    int eventTimeoutInterval;
    int maxEventQueueSize;
    int eventExpirationTime;
    
    // 状态标志
    bool initialized;
    bool processingEvents;
    qint64 eventIdCounter;
}; 