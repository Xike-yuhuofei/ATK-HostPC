#pragma once

#include <QMainWindow>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QGroupBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QComboBox>
#include <QTableWidget>
#include <QHeaderView>
#include <QProgressBar>
#include <QStatusBar>
#include <QMenuBar>
#include <QToolBar>
#include <QTextEdit>
#include <QSplitter>
#include <QTimer>
#include <QDateTime>
#include <QIcon>
#include <QStyleFactory>
#include <QMessageBox>

/**
 * @brief 简化版MainWindow类
 * 
 * 这个版本移除了复杂的管理器依赖，专注于基本的UI功能
 */
class SimpleMainWindow : public QMainWindow
{
    Q_OBJECT

public:
    SimpleMainWindow(QWidget *parent = nullptr);
    ~SimpleMainWindow();

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void updateStatus();
    void onStartClicked();
    void onStopClicked();
    void onEmergencyClicked();
    void onAboutClicked();
    void onSettingsClicked();

private:
    void setupUI();
    void setupMenus();
    void setupStatusBar();
    void applyModernStyle();
    
    QWidget* createControlPanel();
    QTabWidget* createMainTabs();
    QWidget* createMonitorTab();
    QWidget* createAlarmTab();
    QWidget* createDataTab();
    QWidget* createSettingsTab();
    
    void addRandomData();

private:
    // UI组件
    QLabel* m_statusLabel;
    QProgressBar* m_progressBar;
    QTableWidget* m_dataTable;
    QTextEdit* m_logText;
    
    // 定时器
    QTimer* m_updateTimer;
    
    // 状态
    bool m_isRunning;
    int m_dataCounter;
}; 