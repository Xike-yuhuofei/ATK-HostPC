#include "securitywidget.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>
#include <QStandardPaths>
#include <QDir>
#include <QUuid>
#include <QRegularExpression>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>

SecurityWidget::SecurityWidget(QWidget *parent)
    : QWidget(parent)
    , m_tabWidget(nullptr)
    , m_isLoggedIn(false)
    , m_sessionTimer(nullptr)
    , m_monitoringTimer(nullptr)
    , m_backupTimer(nullptr)
    , m_systemMonitor(nullptr)
    , m_isMonitoring(false)
    , m_settings(nullptr)
{
    // 初始化默认安全配置
    m_securityConfig.maxLoginAttempts = 3;
    m_securityConfig.lockoutDuration = 30;
    m_securityConfig.passwordMinLength = 8;
    m_securityConfig.passwordMaxAge = 90;
    m_securityConfig.requireUppercase = true;
    m_securityConfig.requireLowercase = true;
    m_securityConfig.requireNumbers = true;
    m_securityConfig.requireSpecialChars = true;
    m_securityConfig.sessionTimeout = 30;
    m_securityConfig.enableTwoFactor = false;
    m_securityConfig.enableAuditLog = true;
    m_securityConfig.enableOperationApproval = false;
    m_securityConfig.maxConcurrentSessions = 5;
    m_securityConfig.enableAutoBackup = true;
    m_securityConfig.backupInterval = 24;
    m_securityConfig.enableEncryption = true;
    m_securityConfig.enableNetworkMonitoring = true;
    m_securityConfig.enableSystemMonitoring = true;
    
    setupUI();
    setupDatabase();
    setupConnections();
    
    // 初始化定时器
    m_sessionTimer = new QTimer(this);
    m_sessionTimer->setInterval(60000); // 1分钟检查一次
    connect(m_sessionTimer, &QTimer::timeout, this, &SecurityWidget::onSessionTimeout);
    
    m_monitoringTimer = new QTimer(this);
    m_monitoringTimer->setInterval(30000); // 30秒更新一次
    connect(m_monitoringTimer, &QTimer::timeout, this, &SecurityWidget::onMonitoringUpdate);
    
    m_backupTimer = new QTimer(this);
    m_backupTimer->setInterval(m_securityConfig.backupInterval * 3600000); // 转换为毫秒
    connect(m_backupTimer, &QTimer::timeout, this, &SecurityWidget::onBackupClicked);
    
    // 初始化配置文件
    m_configFile = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation) + "/security.ini";
    m_settings = new QSettings(m_configFile, QSettings::IniFormat, this);
    
    // 设置目录
    m_backupDirectory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/GlueDispenser/Backups";
    m_logDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Logs";
    QDir().mkpath(m_backupDirectory);
    QDir().mkpath(m_logDirectory);
    
    // 生成加密密钥
    m_encryptionKey = QUuid::createUuid().toString(QUuid::WithoutBraces);
    m_salt = generateSalt().toUtf8();
    
    // 加载数据
    loadUsers();
    loadOperationRecords();
    loadSecurityEvents();
    loadSecurityConfig();
    
    // 启动监控
    if (m_securityConfig.enableSystemMonitoring) {
        startMonitoring();
    }
    
    // 启动定时器
    m_sessionTimer->start();
    if (m_securityConfig.enableAutoBackup) {
        m_backupTimer->start();
    }
}

SecurityWidget::~SecurityWidget()
{
    if (m_sessionTimer) {
        m_sessionTimer->stop();
    }
    if (m_monitoringTimer) {
        m_monitoringTimer->stop();
    }
    if (m_backupTimer) {
        m_backupTimer->stop();
    }
    
    stopMonitoring();
    
    if (m_isLoggedIn) {
        logOperation(m_currentUser.userId, "用户登出", "系统关闭时自动登出", "系统");
    }
}

void SecurityWidget::setupUI()
{
    auto mainLayout = new QVBoxLayout(this);
    
    // 创建标签页控件
    m_tabWidget = new QTabWidget(this);
    
    // 设置标签页
    setupLoginTab();
    setupUserManagementTab();
    setupPermissionTab();
    setupSecurityConfigTab();
    setupAuditLogTab();
    setupMonitoringTab();
    setupBackupTab();
    
    mainLayout->addWidget(m_tabWidget);
    
    // 设置样式
    setStyleSheet(R"(
        QTabWidget::pane {
            border: 1px solid #c0c0c0;
            background-color: white;
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
        QPushButton {
            background-color: #2196F3;
            color: white;
            border: none;
            padding: 8px 16px;
            border-radius: 4px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #1976D2;
        }
        QPushButton:pressed {
            background-color: #1565C0;
        }
        QPushButton:disabled {
            background-color: #cccccc;
            color: #666666;
        }
        QPushButton.danger {
            background-color: #f44336;
        }
        QPushButton.danger:hover {
            background-color: #d32f2f;
        }
        QPushButton.success {
            background-color: #4CAF50;
        }
        QPushButton.success:hover {
            background-color: #45a049;
        }
        QPushButton.warning {
            background-color: #FF9800;
        }
        QPushButton.warning:hover {
            background-color: #F57C00;
        }
        QLineEdit {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            padding: 8px;
            background-color: white;
        }
        QLineEdit:focus {
            border: 2px solid #2196F3;
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
            background-color: #2196F3;
            color: white;
        }
        QHeaderView::section {
            background-color: #f0f0f0;
            border: 1px solid #c0c0c0;
            padding: 8px;
            font-weight: bold;
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
        QProgressBar {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            text-align: center;
            background-color: #f0f0f0;
        }
        QProgressBar::chunk {
            background-color: #2196F3;
            border-radius: 3px;
        }
        QCheckBox {
            spacing: 5px;
        }
        QCheckBox::indicator {
            width: 16px;
            height: 16px;
        }
        QCheckBox::indicator:unchecked {
            border: 1px solid #c0c0c0;
            background-color: white;
        }
        QCheckBox::indicator:checked {
            border: 1px solid #2196F3;
            background-color: #2196F3;
        }
        QComboBox {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            padding: 4px;
            background-color: white;
        }
        QComboBox:focus {
            border: 2px solid #2196F3;
        }
        QSpinBox {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            padding: 4px;
            background-color: white;
        }
        QSpinBox:focus {
            border: 2px solid #2196F3;
        }
        QTextEdit {
            border: 1px solid #c0c0c0;
            border-radius: 4px;
            background-color: white;
        }
        QTextEdit:focus {
            border: 2px solid #2196F3;
        }
        QLabel {
            color: #333333;
        }
        QLabel.status {
            font-weight: bold;
            padding: 4px 8px;
            border-radius: 4px;
        }
        QLabel.status-online {
            background-color: #4CAF50;
            color: white;
        }
        QLabel.status-offline {
            background-color: #f44336;
            color: white;
        }
        QLabel.status-warning {
            background-color: #FF9800;
            color: white;
        }
    )");
}

void SecurityWidget::setupLoginTab()
{
    m_loginTab = new QWidget();
    m_tabWidget->addTab(m_loginTab, "用户登录");
    
    auto layout = new QVBoxLayout(m_loginTab);
    layout->setAlignment(Qt::AlignCenter);
    
    // 创建登录表单
    auto loginGroup = new QGroupBox("用户登录", m_loginTab);
    loginGroup->setMaximumWidth(400);
    loginGroup->setMinimumWidth(350);
    auto loginLayout = new QFormLayout(loginGroup);
    
    // 用户名输入
    m_usernameEdit = new QLineEdit();
    m_usernameEdit->setPlaceholderText("请输入用户名");
    loginLayout->addRow("用户名:", m_usernameEdit);
    
    // 密码输入
    m_passwordEdit = new QLineEdit();
    m_passwordEdit->setEchoMode(QLineEdit::Password);
    m_passwordEdit->setPlaceholderText("请输入密码");
    loginLayout->addRow("密码:", m_passwordEdit);
    
    // 登录按钮
    auto buttonLayout = new QHBoxLayout();
    m_loginBtn = new QPushButton("登录");
    m_loginBtn->setProperty("class", "success");
    m_logoutBtn = new QPushButton("登出");
    m_logoutBtn->setProperty("class", "danger");
    m_logoutBtn->setEnabled(false);
    buttonLayout->addWidget(m_loginBtn);
    buttonLayout->addWidget(m_logoutBtn);
    loginLayout->addRow(buttonLayout);
    
    layout->addWidget(loginGroup);
    
    // 创建状态信息面板
    auto statusGroup = new QGroupBox("登录状态", m_loginTab);
    statusGroup->setMaximumWidth(400);
    statusGroup->setMinimumWidth(350);
    auto statusLayout = new QFormLayout(statusGroup);
    
    m_loginStatusLabel = new QLabel("未登录");
    m_loginStatusLabel->setProperty("class", "status status-offline");
    statusLayout->addRow("状态:", m_loginStatusLabel);
    
    m_currentUserLabel = new QLabel("无");
    statusLayout->addRow("当前用户:", m_currentUserLabel);
    
    m_lastLoginLabel = new QLabel("无");
    statusLayout->addRow("最后登录:", m_lastLoginLabel);
    
    m_sessionTimeLabel = new QLabel("无");
    statusLayout->addRow("会话时间:", m_sessionTimeLabel);
    
    layout->addWidget(statusGroup);
    
    // 创建密码修改按钮
    auto changePasswordBtn = new QPushButton("修改密码");
    changePasswordBtn->setProperty("class", "warning");
    changePasswordBtn->setEnabled(false);
    layout->addWidget(changePasswordBtn);
    
    connect(changePasswordBtn, &QPushButton::clicked, this, &SecurityWidget::onChangePasswordClicked);
}

void SecurityWidget::setupUserManagementTab()
{
    m_userManagementTab = new QWidget();
    m_tabWidget->addTab(m_userManagementTab, "用户管理");
    
    auto layout = new QVBoxLayout(m_userManagementTab);
    
    // 创建控制面板
    auto controlPanel = new QGroupBox("用户管理", m_userManagementTab);
    auto controlLayout = new QHBoxLayout(controlPanel);
    
    // 用户角色过滤
    controlLayout->addWidget(new QLabel("角色:"));
    m_userRoleFilter = new QComboBox();
    m_userRoleFilter->addItems({"全部", "访客", "操作员", "技术员", "工程师", "管理员"});
    controlLayout->addWidget(m_userRoleFilter);
    
    // 用户状态过滤
    controlLayout->addWidget(new QLabel("状态:"));
    m_userStatusFilter = new QComboBox();
    m_userStatusFilter->addItems({"全部", "激活", "锁定", "未激活"});
    controlLayout->addWidget(m_userStatusFilter);
    
    // 搜索框
    controlLayout->addWidget(new QLabel("搜索:"));
    m_userSearchEdit = new QLineEdit();
    m_userSearchEdit->setPlaceholderText("输入用户名或姓名");
    controlLayout->addWidget(m_userSearchEdit);
    
    // 操作按钮
    m_createUserBtn = new QPushButton("创建用户");
    m_createUserBtn->setProperty("class", "success");
    m_editUserBtn = new QPushButton("编辑用户");
    m_deleteUserBtn = new QPushButton("删除用户");
    m_deleteUserBtn->setProperty("class", "danger");
    m_lockUserBtn = new QPushButton("锁定用户");
    m_lockUserBtn->setProperty("class", "warning");
    m_unlockUserBtn = new QPushButton("解锁用户");
    m_unlockUserBtn->setProperty("class", "success");
    m_resetPasswordBtn = new QPushButton("重置密码");
    m_resetPasswordBtn->setProperty("class", "warning");
    m_refreshUsersBtn = new QPushButton("刷新");
    
    controlLayout->addWidget(m_createUserBtn);
    controlLayout->addWidget(m_editUserBtn);
    controlLayout->addWidget(m_deleteUserBtn);
    controlLayout->addWidget(m_lockUserBtn);
    controlLayout->addWidget(m_unlockUserBtn);
    controlLayout->addWidget(m_resetPasswordBtn);
    controlLayout->addWidget(m_refreshUsersBtn);
    
    controlLayout->addStretch();
    layout->addWidget(controlPanel);
    
    // 创建用户表格
    m_userTable = new QTableWidget(0, 12, m_userManagementTab);
    QStringList headers = {"用户ID", "用户名", "全名", "邮箱", "电话", "部门", 
                          "角色", "状态", "最后登录", "创建时间", "更新时间", "备注"};
    m_userTable->setHorizontalHeaderLabels(headers);
    m_userTable->setAlternatingRowColors(true);
    m_userTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_userTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_userTable->setSortingEnabled(true);
    m_userTable->horizontalHeader()->setStretchLastSection(true);
    m_userTable->verticalHeader()->setVisible(false);
    
    layout->addWidget(m_userTable);
    
    // 创建数据模型
    m_userModel = new QStandardItemModel(this);
    m_userModel->setHorizontalHeaderLabels(headers);
    
    m_userProxy = new QSortFilterProxyModel(this);
    m_userProxy->setSourceModel(m_userModel);
    m_userProxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    
    // 初始状态下禁用操作按钮
    m_editUserBtn->setEnabled(false);
    m_deleteUserBtn->setEnabled(false);
    m_lockUserBtn->setEnabled(false);
    m_unlockUserBtn->setEnabled(false);
    m_resetPasswordBtn->setEnabled(false);
}

// ... existing code ... 