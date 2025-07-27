#pragma once

#include <QObject>
#include <QMainWindow>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QAction>
#include <QLabel>
#include <QTabWidget>
#include <QSplitter>
#include <QDockWidget>
#include <QProgressBar>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QTextEdit>
#include <QTableWidget>
#include <QTimer>

// 前置声明
class DeviceControlWidget;
class DataMonitorWidget;
class ParameterWidget;
class AlarmWidget;
class ChartWidget;
class DataRecordWidget;
class SecurityWidget;
class CommunicationWidget;

/**
 * @brief UI管理器类
 * 
 * 负责MainWindow的UI界面创建和管理，包括：
 * - 菜单栏和工具栏
 * - 状态栏
 * - 中央窗口部件
 * - 停靠窗口
 * - 系统托盘
 * - 主题和样式
 */
class UIManager : public QObject
{
    Q_OBJECT

public:
    explicit UIManager(QMainWindow* mainWindow, QObject* parent = nullptr);
    ~UIManager();

    // 初始化UI
    void initializeUI();
    
    // 菜单和工具栏管理
    void createMenus();
    void createToolBars();
    void createStatusBar();
    void createCentralWidget();
    void createDockWidgets();
    void createSystemTray();
    
    // 界面更新
    void updateStatusBar(const QString& message = QString());
    void updateConnectionStatus(bool connected);
    void updateDeviceStatus(const QString& status);
    void updateUserStatus(const QString& user);
    void updateStatistics(qint64 bytesSent, qint64 bytesReceived);
    void updateTimeDisplay();
    
    // 进度条控制
    void showProgressBar(const QString& text, int maximum = 100);
    void hideProgressBar();
    void updateProgress(int value);
    
    // 界面控制
    void toggleFullScreen();
    void resetLayout();
    void showStatusBar(bool show);
    void showToolBar(bool show);
    void minimizeToTray();
    void restoreFromTray();
    
    // 主题和样式
    void applyTheme(const QString& themeName);
    void loadThemeSettings();
    void saveThemeSettings();
    
    // 快捷键和提示
    void setupShortcuts();
    void setupToolTips();
    
    // 访问器
    QTabWidget* getTabWidget() const { return tabWidget; }
    QAction* getAction(const QString& name) const;
    QDockWidget* getDockWidget(const QString& name) const;
    QWidget* getWidget(const QString& name) const;
    
    // 状态查询
    bool isFullScreen() const { return isFullScreenMode; }
    bool isMinimizedToTray() const { return isMinimizedToTrayMode; }

public slots:
    // 菜单响应槽
    void onFileOpen();
    void onFileSave();
    void onFileImport();
    void onFileExport();
    void onFileExit();
    void onEditUndo();
    void onEditRedo();
    void onEditCut();
    void onEditCopy();
    void onEditPaste();
    void onEditPreferences();
    void onViewFullScreen();
    void onViewResetLayout();
    void onViewShowStatusBar();
    void onViewShowToolBar();
    void onHelpAbout();
    void onHelpManual();
    void onHelpSupport();
    void onHelpUpdate();
    
    // 界面事件槽
    void onTabChanged(int index);
    void onDockWidgetVisibilityChanged(bool visible);
    void onTrayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void onShowMainWindow();
    void onMinimizeToTray();

signals:
    // UI事件信号
    void fileOpenRequested();
    void fileSaveRequested();
    void fileImportRequested();
    void fileExportRequested();
    void exitRequested();
    void preferencesRequested();
    void aboutRequested();
    void manualRequested();
    void supportRequested();
    void updateRequested();
    
    // 编辑操作信号
    void undoRequested();
    void redoRequested();
    void cutRequested();
    void copyRequested();
    void pasteRequested();
    
    // 界面状态变化信号
    void fullScreenToggled(bool enabled);
    void layoutReset();
    void statusBarToggled(bool visible);
    void toolBarToggled(bool visible);
    void tabChanged(int index);
    void dockWidgetVisibilityChanged(const QString& name, bool visible);
    void trayIconActivated();
    void mainWindowRestoreRequested();
    void minimizeToTrayRequested();

private:
    void setupMenuActions();
    void setupToolBarActions();
    void setupStatusLabels();
    void setupTabWidget();
    void setupDockWidgets();
    void setupSystemTrayIcon();
    void createFunctionalWidgets();
    void setupLayoutAndConnections();
    void loadSettings();
    void saveSettings();
    
    // 主窗口引用
    QMainWindow* mainWindow;
    
    // UI组件
    QWidget* centralWidget;
    QTabWidget* tabWidget;
    QSplitter* mainSplitter;
    
    // 菜单和工具栏
    QMenu* fileMenu;
    QMenu* editMenu;
    QMenu* viewMenu;
    QMenu* toolsMenu;
    QMenu* helpMenu;
    QToolBar* mainToolBar;
    QToolBar* deviceToolBar;
    QToolBar* communicationToolBar;
    QToolBar* dataToolBar;
    
    // 状态栏组件
    QLabel* statusLabel;
    QLabel* connectionLabel;
    QLabel* deviceStatusLabel;
    QLabel* userLabel;
    QLabel* timeLabel;
    QLabel* statisticsLabel;
    QProgressBar* progressBar;
    
    // 停靠窗口
    QDockWidget* logDockWidget;
    QDockWidget* alarmDockWidget;
    QDockWidget* statisticsDockWidget;
    QDockWidget* connectionDockWidget;
    
    // 停靠窗口内容
    QTextEdit* logTextEdit;
    QTableWidget* alarmTableWidget;
    QTableWidget* statisticsTableWidget;
    QTableWidget* connectionTableWidget;
    
    // 系统托盘
    QSystemTrayIcon* trayIcon;
    QMenu* trayMenu;
    
    // 功能模块窗口部件
    DeviceControlWidget* deviceControlWidget;
    DataMonitorWidget* dataMonitorWidget;
    ParameterWidget* parameterWidget;
    AlarmWidget* alarmWidget;
    ChartWidget* chartWidget;
    DataRecordWidget* dataRecordWidget;
    SecurityWidget* securityWidget;
    CommunicationWidget* communicationWidget;
    
    // 动作映射
    QHash<QString, QAction*> actionMap;
    QHash<QString, QDockWidget*> dockWidgetMap;
    QHash<QString, QWidget*> widgetMap;
    
    // 菜单动作
    QAction* openAction;
    QAction* saveAction;
    QAction* importAction;
    QAction* exportAction;
    QAction* exitAction;
    QAction* undoAction;
    QAction* redoAction;
    QAction* cutAction;
    QAction* copyAction;
    QAction* pasteAction;
    QAction* preferencesAction;
    QAction* fullScreenAction;
    QAction* resetLayoutAction;
    QAction* showStatusBarAction;
    QAction* showToolBarAction;
    QAction* aboutAction;
    QAction* manualAction;
    QAction* supportAction;
    QAction* updateAction;
    
    // 状态标志
    bool isFullScreenMode;
    bool isMinimizedToTrayMode;
    
    // 定时器
    QTimer* timeUpdateTimer;
}; 