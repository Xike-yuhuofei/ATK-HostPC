#ifndef SECURITYWIDGET_H
#define SECURITYWIDGET_H

#include <QWidget>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFormLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QLineEdit>
#include <QTextEdit>
#include <QGroupBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QDateTimeEdit>
#include <QListWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QSplitter>
#include <QProgressBar>
#include <QSlider>
#include <QDial>
#include <QTimer>
#include <QMessageBox>
#include <QInputDialog>
#include <QFileDialog>
#include <QStandardItemModel>
#include <QHeaderView>
#include <QSortFilterProxyModel>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QDateTime>
#include <QCryptographicHash>
#include <QRandomGenerator>
#include <QSettings>
#include <QApplication>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkInterface>
#include <QHostInfo>
#include <QSysInfo>
#include <QStorageInfo>
#include <QProcess>

// 用户角色枚举
enum class UserRole {
    Guest = 0,          // 访客
    Operator = 1,       // 操作员
    Technician = 2,     // 技术员
    Engineer = 3,       // 工程师
    Administrator = 4   // 管理员
};

// 权限类型枚举
enum class Permission {
    ViewData = 0,       // 查看数据
    ModifyParams = 1,   // 修改参数
    ControlDevice = 2,  // 控制设备
    ManageUsers = 3,    // 用户管理
    SystemConfig = 4,   // 系统配置
    DataExport = 5,     // 数据导出
    EmergencyStop = 6,  // 紧急停止
    Maintenance = 7,    // 维护模式
    Backup = 8,         // 备份恢复
    Audit = 9          // 审计日志
};

// 用户信息结构
struct UserInfo {
    int userId;                     // 用户ID
    QString username;               // 用户名
    QString fullName;               // 全名
    QString email;                  // 邮箱
    QString phone;                  // 电话
    QString department;             // 部门
    UserRole role;                  // 角色
    QList<Permission> permissions;  // 权限列表
    QString passwordHash;           // 密码哈希
    QString salt;                   // 盐值
    QDateTime lastLogin;            // 最后登录时间
    QDateTime lastPasswordChange;   // 最后密码修改时间
    int loginAttempts;              // 登录尝试次数
    bool isLocked;                  // 是否锁定
    bool isActive;                  // 是否激活
    QDateTime createdAt;            // 创建时间
    QDateTime updatedAt;            // 更新时间
    QString notes;                  // 备注
};

// 操作记录结构
struct OperationRecord {
    int recordId;                   // 记录ID
    int userId;                     // 用户ID
    QString username;               // 用户名
    QString operation;              // 操作类型
    QString description;            // 操作描述
    QString targetObject;           // 操作对象
    QJsonObject oldValue;           // 旧值
    QJsonObject newValue;           // 新值
    QDateTime timestamp;            // 时间戳
    QString ipAddress;              // IP地址
    QString macAddress;             // MAC地址
    QString sessionId;              // 会话ID
    bool isSuccess;                 // 是否成功
    QString errorMessage;           // 错误信息
    int riskLevel;                  // 风险等级(1-5)
    QString approvalStatus;         // 审批状态
    QString approver;               // 审批人
    QDateTime approvalTime;         // 审批时间
};

// 安全配置结构
struct SecurityConfig {
    int maxLoginAttempts;           // 最大登录尝试次数
    int lockoutDuration;            // 锁定持续时间(分钟)
    int passwordMinLength;          // 密码最小长度
    int passwordMaxAge;             // 密码最大有效期(天)
    bool requireUppercase;          // 要求大写字母
    bool requireLowercase;          // 要求小写字母
    bool requireNumbers;            // 要求数字
    bool requireSpecialChars;       // 要求特殊字符
    int sessionTimeout;             // 会话超时时间(分钟)
    bool enableTwoFactor;           // 启用双因子认证
    bool enableAuditLog;            // 启用审计日志
    bool enableOperationApproval;   // 启用操作审批
    QStringList trustedIPs;         // 可信IP列表
    QStringList blacklistIPs;       // 黑名单IP列表
    int maxConcurrentSessions;      // 最大并发会话数
    bool enableAutoBackup;          // 启用自动备份
    int backupInterval;             // 备份间隔(小时)
    bool enableEncryption;          // 启用数据加密
    QString encryptionKey;          // 加密密钥
    bool enableNetworkMonitoring;   // 启用网络监控
    bool enableSystemMonitoring;    // 启用系统监控
};

// 安全事件结构
struct SecurityEvent {
    int eventId;                    // 事件ID
    QString eventType;              // 事件类型
    QString eventLevel;             // 事件级别
    QString eventMessage;           // 事件消息
    QString sourceIP;               // 源IP地址
    QString targetUser;             // 目标用户
    QString targetResource;         // 目标资源
    QDateTime timestamp;            // 时间戳
    QString details;                // 详细信息
    bool isHandled;                 // 是否已处理
    QString handler;                // 处理人
    QDateTime handledTime;          // 处理时间
    QString solution;               // 解决方案
    QString notes;                  // 备注
};

class SecurityWidget : public QWidget
{
    Q_OBJECT

public:
    explicit SecurityWidget(QWidget *parent = nullptr);
    ~SecurityWidget();

    // 用户管理接口
    bool createUser(const UserInfo& user);
    bool updateUser(const UserInfo& user);
    bool deleteUser(int userId);
    UserInfo getUserInfo(int userId);
    QList<UserInfo> getAllUsers();
    bool changePassword(int userId, const QString& oldPassword, const QString& newPassword);
    bool resetPassword(int userId, const QString& newPassword);
    bool lockUser(int userId, bool lock);
    bool activateUser(int userId, bool active);
    
    // 权限管理接口
    bool hasPermission(int userId, Permission permission);
    bool grantPermission(int userId, Permission permission);
    bool revokePermission(int userId, Permission permission);
    QList<Permission> getUserPermissions(int userId);
    bool setUserRole(int userId, UserRole role);
    
    // 认证接口
    bool authenticate(const QString& username, const QString& password);
    bool authenticateWithToken(const QString& token);
    QString generateToken(int userId);
    bool validateToken(const QString& token);
    void logout(int userId);
    void logoutAll();
    
    // 审计日志接口
    void logOperation(int userId, const QString& operation, const QString& description,
                     const QString& targetObject, const QJsonObject& oldValue = QJsonObject(),
                     const QJsonObject& newValue = QJsonObject());
    void logSecurityEvent(const QString& eventType, const QString& eventLevel,
                         const QString& eventMessage, const QString& sourceIP = QString());
    QList<OperationRecord> getOperationRecords(const QDateTime& startTime, const QDateTime& endTime);
    QList<SecurityEvent> getSecurityEvents(const QDateTime& startTime, const QDateTime& endTime);
    
    // 安全配置接口
    SecurityConfig getSecurityConfig();
    bool setSecurityConfig(const SecurityConfig& config);
    bool validateSecurityConfig(const SecurityConfig& config);
    
    // 安全检查接口
    bool checkOperationPermission(int userId, const QString& operation);
    bool checkResourceAccess(int userId, const QString& resource);
    bool checkNetworkSecurity(const QString& ipAddress);
    bool checkSystemSecurity();
    
    // 数据加密接口
    QString encryptData(const QString& data);
    QString decryptData(const QString& encryptedData);
    QString generateSalt();
    QString hashPassword(const QString& password, const QString& salt);
    
    // 当前用户信息
    UserInfo getCurrentUser() const { return m_currentUser; }
    bool isUserLoggedIn() const { return m_isLoggedIn; }
    int getCurrentUserId() const { return m_currentUser.userId; }
    UserRole getCurrentUserRole() const { return m_currentUser.role; }

public slots:
    void onUserLogin(const QString& username, const QString& password);
    void onUserLogout();
    void onPasswordChange();
    void onUserLockout(int userId);
    void onSecurityAlert(const QString& message);
    void onSystemShutdown();
    void onEmergencyStop();

private slots:
    void onLoginClicked();
    void onLogoutClicked();
    void onChangePasswordClicked();
    void onCreateUserClicked();
    void onEditUserClicked();
    void onDeleteUserClicked();
    void onLockUserClicked();
    void onUnlockUserClicked();
    void onResetPasswordClicked();
    void onRefreshUsersClicked();
    void onUserSelectionChanged();
    void onRoleChanged();
    void onPermissionChanged();
    void onSecurityConfigChanged();
    void onSaveConfigClicked();
    void onResetConfigClicked();
    void onRefreshLogsClicked();
    void onClearLogsClicked();
    void onExportLogsClicked();
    void onViewLogDetailsClicked();
    void onAcknowledgeEventClicked();
    void onSessionTimeout();
    void onMonitoringUpdate();
    void onBackupClicked();
    void onRestoreClicked();
    void onTestSecurityClicked();
    void onGenerateReportClicked();

private:
    void setupUI();
    void setupDatabase();
    void setupConnections();
    void setupLoginTab();
    void setupUserManagementTab();
    void setupPermissionTab();
    void setupSecurityConfigTab();
    void setupAuditLogTab();
    void setupMonitoringTab();
    void setupBackupTab();
    
    void loadUsers();
    void loadOperationRecords();
    void loadSecurityEvents();
    void loadSecurityConfig();
    void updateUserTable();
    void updatePermissionTree();
    void updateLogTable();
    void updateEventTable();
    void updateMonitoringInfo();
    void updateSecurityStatus();
    
    bool initializeDatabase();
    bool createTables();
    bool insertUser(const UserInfo& user);
    bool updateUserInDB(const UserInfo& user);
    bool deleteUserFromDB(int userId);
    bool insertOperationRecord(const OperationRecord& record);
    bool insertSecurityEvent(const SecurityEvent& event);
    bool updateSecurityConfigInDB(const SecurityConfig& config);
    
    void showLoginDialog();
    void showUserDialog(const UserInfo& user = UserInfo());
    void showPasswordDialog(int userId);
    void showPermissionDialog(int userId);
    void showSecurityEventDialog(const SecurityEvent& event);
    void showOperationRecordDialog(const OperationRecord& record);
    
    bool validateUser(const UserInfo& user);
    bool validatePassword(const QString& password);
    bool validatePermissions(const QList<Permission>& permissions);
    bool validateIPAddress(const QString& ipAddress);
    
    void applySecurityPolicies();
    void enforcePasswordPolicy();
    void enforceSessionPolicy();
    void enforceAccessPolicy();
    void enforceNetworkPolicy();
    
    void startMonitoring();
    void stopMonitoring();
    void checkSystemResources();
    void checkNetworkConnections();
    void checkDatabaseIntegrity();
    void checkFilePermissions();
    
    void generateSecurityReport();
    void exportAuditLog();
    void backupSecurityData();
    void restoreSecurityData();
    
    QString formatDateTime(const QDateTime& dateTime);
    QString formatUserRole(UserRole role);
    QString formatPermission(Permission permission);
    QString formatSecurityLevel(const QString& level);
    QColor getSecurityLevelColor(const QString& level);
    QIcon getSecurityIcon(const QString& type);

private:
    // UI组件
    QTabWidget* m_tabWidget;
    
    // 登录页面
    QWidget* m_loginTab;
    QLineEdit* m_usernameEdit;
    QLineEdit* m_passwordEdit;
    QPushButton* m_loginBtn;
    QPushButton* m_logoutBtn;
    QLabel* m_loginStatusLabel;
    QLabel* m_currentUserLabel;
    QLabel* m_lastLoginLabel;
    QLabel* m_sessionTimeLabel;
    
    // 用户管理页面
    QWidget* m_userManagementTab;
    QTableWidget* m_userTable;
    QPushButton* m_createUserBtn;
    QPushButton* m_editUserBtn;
    QPushButton* m_deleteUserBtn;
    QPushButton* m_lockUserBtn;
    QPushButton* m_unlockUserBtn;
    QPushButton* m_resetPasswordBtn;
    QPushButton* m_refreshUsersBtn;
    QComboBox* m_userRoleFilter;
    QComboBox* m_userStatusFilter;
    QLineEdit* m_userSearchEdit;
    
    // 权限管理页面
    QWidget* m_permissionTab;
    QTreeWidget* m_permissionTree;
    QListWidget* m_roleList;
    QPushButton* m_grantPermissionBtn;
    QPushButton* m_revokePermissionBtn;
    QPushButton* m_savePermissionsBtn;
    QLabel* m_selectedUserLabel;
    QLabel* m_userRoleLabel;
    
    // 安全配置页面
    QWidget* m_securityConfigTab;
    QSpinBox* m_maxLoginAttemptsSpinBox;
    QSpinBox* m_lockoutDurationSpinBox;
    QSpinBox* m_passwordMinLengthSpinBox;
    QSpinBox* m_passwordMaxAgeSpinBox;
    QCheckBox* m_requireUppercaseCheckBox;
    QCheckBox* m_requireLowercaseCheckBox;
    QCheckBox* m_requireNumbersCheckBox;
    QCheckBox* m_requireSpecialCharsCheckBox;
    QSpinBox* m_sessionTimeoutSpinBox;
    QCheckBox* m_enableTwoFactorCheckBox;
    QCheckBox* m_enableAuditLogCheckBox;
    QCheckBox* m_enableOperationApprovalCheckBox;
    QTextEdit* m_trustedIPsEdit;
    QTextEdit* m_blacklistIPsEdit;
    QSpinBox* m_maxConcurrentSessionsSpinBox;
    QPushButton* m_saveConfigBtn;
    QPushButton* m_resetConfigBtn;
    QPushButton* m_testSecurityBtn;
    
    // 审计日志页面
    QWidget* m_auditLogTab;
    QTableWidget* m_operationTable;
    QTableWidget* m_eventTable;
    QComboBox* m_logTypeFilter;
    QComboBox* m_logLevelFilter;
    QDateTimeEdit* m_logStartDateEdit;
    QDateTimeEdit* m_logEndDateEdit;
    QPushButton* m_refreshLogsBtn;
    QPushButton* m_clearLogsBtn;
    QPushButton* m_exportLogsBtn;
    QPushButton* m_viewLogDetailsBtn;
    QPushButton* m_acknowledgeEventBtn;
    QLabel* m_totalRecordsLabel;
    QLabel* m_unhandledEventsLabel;
    
    // 监控页面
    QWidget* m_monitoringTab;
    QLabel* m_systemStatusLabel;
    QLabel* m_networkStatusLabel;
    QLabel* m_databaseStatusLabel;
    QLabel* m_securityStatusLabel;
    QProgressBar* m_cpuUsageBar;
    QProgressBar* m_memoryUsageBar;
    QProgressBar* m_diskUsageBar;
    QProgressBar* m_networkUsageBar;
    QTableWidget* m_activeSessionsTable;
    QTableWidget* m_networkConnectionsTable;
    QPushButton* m_refreshMonitoringBtn;
    QPushButton* m_generateReportBtn;
    
    // 备份页面
    QWidget* m_backupTab;
    QLineEdit* m_backupPathEdit;
    QPushButton* m_browseBackupBtn;
    QPushButton* m_backupBtn;
    QPushButton* m_restoreBtn;
    QProgressBar* m_backupProgress;
    QTableWidget* m_backupHistoryTable;
    QCheckBox* m_autoBackupCheckBox;
    QSpinBox* m_backupIntervalSpinBox;
    QLabel* m_lastBackupLabel;
    QLabel* m_backupSizeLabel;
    
    // 数据模型
    QStandardItemModel* m_userModel;
    QStandardItemModel* m_operationModel;
    QStandardItemModel* m_eventModel;
    QStandardItemModel* m_sessionModel;
    QStandardItemModel* m_connectionModel;
    QStandardItemModel* m_backupModel;
    QSortFilterProxyModel* m_userProxy;
    QSortFilterProxyModel* m_operationProxy;
    QSortFilterProxyModel* m_eventProxy;
    
    // 数据存储
    QList<UserInfo> m_users;
    QList<OperationRecord> m_operationRecords;
    QList<SecurityEvent> m_securityEvents;
    SecurityConfig m_securityConfig;
    
    // 当前用户信息
    UserInfo m_currentUser;
    bool m_isLoggedIn;
    QString m_currentToken;
    QDateTime m_loginTime;
    QDateTime m_lastActivity;
    
    // 定时器
    QTimer* m_sessionTimer;
    QTimer* m_monitoringTimer;
    QTimer* m_backupTimer;
    
    // 系统监控
    QProcess* m_systemMonitor;
    bool m_isMonitoring;
    
    // 配置设置
    QSettings* m_settings;
    QString m_configFile;
    QString m_backupDirectory;
    QString m_logDirectory;
    
    // 加密相关
    QString m_encryptionKey;
    QByteArray m_salt;
    
signals:
    void userLoggedIn(const UserInfo& user);
    void userLoggedOut(int userId);
    void userCreated(const UserInfo& user);
    void userUpdated(const UserInfo& user);
    void userDeleted(int userId);
    void userLocked(int userId);
    void userUnlocked(int userId);
    void passwordChanged(int userId);
    void permissionGranted(int userId, Permission permission);
    void permissionRevoked(int userId, Permission permission);
    void securityConfigChanged(const SecurityConfig& config);
    void operationLogged(const OperationRecord& record);
    void securityEventTriggered(const SecurityEvent& event);
    void securityAlert(const QString& message);
    void sessionExpired(int userId);
    void systemMonitoringAlert(const QString& message);
    void backupCompleted(const QString& filename);
    void restoreCompleted(const QString& filename);
    void databaseError(const QString& error);
};

#endif // SECURITYWIDGET_H 