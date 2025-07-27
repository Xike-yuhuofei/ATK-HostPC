#include "parameterwidget.h"
#include "logger/logmanager.h"
#include <QMessageBox>
#include <QStandardPaths>
#include <QDir>
#include <QDateTime>
#include <QHeaderView>
#include <QInputDialog>
#include <QApplication>
#include <QStyle>
#include <QSplitter>
#include <cmath>
#include <limits>
#include <algorithm>

ParameterWidget::ParameterWidget(QWidget* parent) 
    : QWidget(parent)
    , tabWidget(nullptr)
    , isModified(false)
{
    // 设置目录路径
    QString dataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    programsDirectory = dataDir + "/programs";
    templatesDirectory = dataDir + "/templates";
    
    // 创建目录
    QDir().mkpath(programsDirectory);
    QDir().mkpath(templatesDirectory);
    
    setupUI();
    setupConnections();
    
    // 初始化验证规则
    initializeValidationRules();
    
    // 加载数据
    loadProgramList();
    loadTemplateList();
    
    // 创建自动保存定时器
    autoSaveTimer = new QTimer(this);
    autoSaveTimer->setSingleShot(true);
    autoSaveTimer->setInterval(30000); // 30秒自动保存
    connect(autoSaveTimer, &QTimer::timeout, this, &ParameterWidget::autoSave);
    
    LogManager::getInstance()->info("参数管理界面已创建", "Parameter");
}

ParameterWidget::~ParameterWidget()
{
    if (isModified) {
        // 保存修改
        saveProgramList();
    }
    LogManager::getInstance()->info("参数管理界面已销毁", "Parameter");
}

void ParameterWidget::setupUI()
{
    // 主布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    
    // 创建标签页
    tabWidget = new QTabWidget;
    
    // 程序管理页面
    QWidget* programPage = new QWidget;
    QHBoxLayout* programLayout = new QHBoxLayout(programPage);
    
    setupProgramPanel();
    setupParameterPanel();
    
    QSplitter* programSplitter = new QSplitter(Qt::Horizontal);
    programSplitter->addWidget(programGroup);
    programSplitter->addWidget(parameterGroup);
    programSplitter->setSizes({300, 500});
    
    programLayout->addWidget(programSplitter);
    
    // 轨迹编辑页面
    QWidget* trajectoryPage = new QWidget;
    QVBoxLayout* trajectoryLayout = new QVBoxLayout(trajectoryPage);
    
    setupTrajectoryPanel();
    
    QSplitter* trajectorySplitter = new QSplitter(Qt::Vertical);
    trajectorySplitter->addWidget(trajectoryGroup);
    trajectorySplitter->addWidget(trajectoryStatsGroup);
    trajectorySplitter->setSizes({400, 100});
    
    trajectoryLayout->addWidget(trajectorySplitter);
    
    // 模板管理页面
    QWidget* templatePage = new QWidget;
    QHBoxLayout* templateLayout = new QHBoxLayout(templatePage);
    
    setupTemplatePanel();
    templateLayout->addWidget(templateGroup);
    templateLayout->addStretch();
    
    // 添加到标签页
    tabWidget->addTab(programPage, "程序管理");
    tabWidget->addTab(trajectoryPage, "轨迹编辑");
    tabWidget->addTab(templatePage, "模板管理");
    
    mainLayout->addWidget(tabWidget);
    setLayout(mainLayout);
}

void ParameterWidget::setupProgramPanel()
{
    programGroup = new QGroupBox("程序列表");
    QVBoxLayout* layout = new QVBoxLayout(programGroup);
    
    // 程序树形控件
    programTreeWidget = new QTreeWidget;
    programTreeWidget->setHeaderLabels({"程序名称", "版本", "修改时间"});
    programTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    programTreeWidget->setAlternatingRowColors(true);
    
    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    importProgramButton = new QPushButton("导入");
    importProgramButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    exportProgramButton = new QPushButton("导出");
    exportProgramButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    newProgramButton = new QPushButton("新建");
    newProgramButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    deleteProgramButton = new QPushButton("删除");
    deleteProgramButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    duplicateProgramButton = new QPushButton("复制");
    duplicateProgramButton->setIcon(style()->standardIcon(QStyle::SP_FileLinkIcon));
    
    buttonLayout->addWidget(importProgramButton);
    buttonLayout->addWidget(exportProgramButton);
    buttonLayout->addWidget(newProgramButton);
    buttonLayout->addWidget(deleteProgramButton);
    buttonLayout->addWidget(duplicateProgramButton);
    
    // 程序信息组
    programInfoGroup = new QGroupBox("程序信息");
    QGridLayout* infoLayout = new QGridLayout(programInfoGroup);
    
    infoLayout->addWidget(new QLabel("程序名称:"), 0, 0);
    programNameEdit = new QLineEdit;
    infoLayout->addWidget(programNameEdit, 0, 1);
    
    infoLayout->addWidget(new QLabel("版本号:"), 1, 0);
    programVersionEdit = new QLineEdit;
    infoLayout->addWidget(programVersionEdit, 1, 1);
    
    infoLayout->addWidget(new QLabel("创建时间:"), 2, 0);
    createTimeLabel = new QLabel;
    infoLayout->addWidget(createTimeLabel, 2, 1);
    
    infoLayout->addWidget(new QLabel("修改时间:"), 3, 0);
    modifyTimeLabel = new QLabel;
    infoLayout->addWidget(modifyTimeLabel, 3, 1);
    
    infoLayout->addWidget(new QLabel("程序描述:"), 4, 0);
    programDescriptionEdit = new QTextEdit;
    programDescriptionEdit->setMaximumHeight(80);
    infoLayout->addWidget(programDescriptionEdit, 4, 1);
    
    layout->addWidget(programTreeWidget);
    layout->addLayout(buttonLayout);
    layout->addWidget(programInfoGroup);
}

void ParameterWidget::setupParameterPanel()
{
    parameterGroup = new QGroupBox("参数设置");
    QVBoxLayout* layout = new QVBoxLayout(parameterGroup);
    
    // 参数表格
    parameterTableWidget = new QTableWidget;
    parameterTableWidget->setColumnCount(4);
    parameterTableWidget->setHorizontalHeaderLabels({"参数名称", "当前值", "单位", "描述"});
    parameterTableWidget->horizontalHeader()->setStretchLastSection(true);
    parameterTableWidget->setAlternatingRowColors(true);
    parameterTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    
    // 初始化参数表格
    initializeParameterTable();
    
    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    validateParametersButton = new QPushButton("验证参数");
    validateParametersButton->setIcon(style()->standardIcon(QStyle::SP_DialogApplyButton));
    optimizeParametersButton = new QPushButton("优化参数");
    optimizeParametersButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    resetParametersButton = new QPushButton("重置参数");
    resetParametersButton->setIcon(style()->standardIcon(QStyle::SP_DialogResetButton));
    
    buttonLayout->addWidget(validateParametersButton);
    buttonLayout->addWidget(optimizeParametersButton);
    buttonLayout->addWidget(resetParametersButton);
    buttonLayout->addStretch();
    
    layout->addWidget(parameterTableWidget);
    layout->addLayout(buttonLayout);
}

void ParameterWidget::setupTrajectoryPanel()
{
    trajectoryGroup = new QGroupBox("轨迹编辑");
    QVBoxLayout* layout = new QVBoxLayout(trajectoryGroup);
    
    // 轨迹表格
    trajectoryTableWidget = new QTableWidget;
    trajectoryTableWidget->setColumnCount(8);
    trajectoryTableWidget->setHorizontalHeaderLabels({
        "序号", "X坐标", "Y坐标", "Z坐标", "速度", "胶量", "停留时间", "点胶"
    });
    trajectoryTableWidget->setAlternatingRowColors(true);
    trajectoryTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    
    // 设置列宽
    trajectoryTableWidget->setColumnWidth(0, 50);
    trajectoryTableWidget->setColumnWidth(1, 80);
    trajectoryTableWidget->setColumnWidth(2, 80);
    trajectoryTableWidget->setColumnWidth(3, 80);
    trajectoryTableWidget->setColumnWidth(4, 80);
    trajectoryTableWidget->setColumnWidth(5, 80);
    trajectoryTableWidget->setColumnWidth(6, 80);
    trajectoryTableWidget->setColumnWidth(7, 60);
    
    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    addPointButton = new QPushButton("添加点");
    addPointButton->setIcon(style()->standardIcon(QStyle::SP_FileIcon));
    removePointButton = new QPushButton("删除点");
    removePointButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    editPointButton = new QPushButton("编辑点");
    editPointButton->setIcon(style()->standardIcon(QStyle::SP_FileDialogDetailedView));
    clearTrajectoryButton = new QPushButton("清空轨迹");
    clearTrajectoryButton->setIcon(style()->standardIcon(QStyle::SP_DialogResetButton));
    optimizeTrajectoryButton = new QPushButton("优化轨迹");
    optimizeTrajectoryButton->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
    
    buttonLayout->addWidget(addPointButton);
    buttonLayout->addWidget(removePointButton);
    buttonLayout->addWidget(editPointButton);
    buttonLayout->addWidget(clearTrajectoryButton);
    buttonLayout->addWidget(optimizeTrajectoryButton);
    buttonLayout->addStretch();
    
    layout->addWidget(trajectoryTableWidget);
    layout->addLayout(buttonLayout);
    
    // 轨迹统计面板
    trajectoryStatsGroup = new QGroupBox("轨迹统计");
    QGridLayout* statsLayout = new QGridLayout(trajectoryStatsGroup);
    
    statsLayout->addWidget(new QLabel("总点数:"), 0, 0);
    totalPointsLabel = new QLabel("0");
    statsLayout->addWidget(totalPointsLabel, 0, 1);
    
    statsLayout->addWidget(new QLabel("总距离:"), 0, 2);
    totalDistanceLabel = new QLabel("0.000 mm");
    statsLayout->addWidget(totalDistanceLabel, 0, 3);
    
    statsLayout->addWidget(new QLabel("预计时间:"), 1, 0);
    totalTimeLabel = new QLabel("0.0 s");
    statsLayout->addWidget(totalTimeLabel, 1, 1);
    
    statsLayout->addWidget(new QLabel("总胶量:"), 1, 2);
    totalVolumeLabel = new QLabel("0.000 μL");
    statsLayout->addWidget(totalVolumeLabel, 1, 3);
    
    trajectoryProgressBar = new QProgressBar;
    trajectoryProgressBar->setRange(0, 100);
    trajectoryProgressBar->setValue(0);
    trajectoryProgressBar->setFormat("轨迹完成度: %p%");
    statsLayout->addWidget(trajectoryProgressBar, 2, 0, 1, 4);
}

void ParameterWidget::setupTemplatePanel()
{
    templateGroup = new QGroupBox("参数模板");
    QVBoxLayout* layout = new QVBoxLayout(templateGroup);
    
    // 模板树形控件
    templateTreeWidget = new QTreeWidget;
    templateTreeWidget->setHeaderLabels({"模板名称", "分类", "描述"});
    templateTreeWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    templateTreeWidget->setAlternatingRowColors(true);
    
    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout;
    
    loadTemplateButton = new QPushButton("加载模板");
    loadTemplateButton->setIcon(style()->standardIcon(QStyle::SP_DialogOpenButton));
    saveTemplateButton = new QPushButton("保存模板");
    saveTemplateButton->setIcon(style()->standardIcon(QStyle::SP_DialogSaveButton));
    deleteTemplateButton = new QPushButton("删除模板");
    deleteTemplateButton->setIcon(style()->standardIcon(QStyle::SP_TrashIcon));
    
    buttonLayout->addWidget(loadTemplateButton);
    buttonLayout->addWidget(saveTemplateButton);
    buttonLayout->addWidget(deleteTemplateButton);
    buttonLayout->addStretch();
    
    layout->addWidget(templateTreeWidget);
    layout->addLayout(buttonLayout);
}

void ParameterWidget::setupConnections()
{
    // 程序管理信号
    connect(importProgramButton, &QPushButton::clicked, this, &ParameterWidget::onImportProgram);
    connect(exportProgramButton, &QPushButton::clicked, this, &ParameterWidget::onExportProgram);
    connect(newProgramButton, &QPushButton::clicked, this, &ParameterWidget::onNewProgram);
    connect(deleteProgramButton, &QPushButton::clicked, this, &ParameterWidget::onDeleteProgram);
    connect(duplicateProgramButton, &QPushButton::clicked, this, &ParameterWidget::onDuplicateProgram);
    
    // 程序选择信号
    connect(programTreeWidget, &QTreeWidget::currentItemChanged, 
            this, &ParameterWidget::onProgramSelectionChanged);
    connect(programTreeWidget, &QTreeWidget::itemChanged, 
            this, &ParameterWidget::onProgramItemChanged);
    
    // 程序信息变化信号
    connect(programNameEdit, &QLineEdit::textChanged, this, &ParameterWidget::onParameterChanged);
    connect(programVersionEdit, &QLineEdit::textChanged, this, &ParameterWidget::onParameterChanged);
    connect(programDescriptionEdit, &QTextEdit::textChanged, this, &ParameterWidget::onParameterChanged);
    
    // 参数管理信号
    connect(validateParametersButton, &QPushButton::clicked, this, &ParameterWidget::onValidateParameters);
    connect(optimizeParametersButton, &QPushButton::clicked, this, &ParameterWidget::onOptimizeParameters);
    connect(resetParametersButton, &QPushButton::clicked, this, &ParameterWidget::resetParameters);
    
    connect(parameterTableWidget, &QTableWidget::itemChanged, 
            this, &ParameterWidget::onParameterItemChanged);
    
    // 轨迹编辑信号
    connect(addPointButton, &QPushButton::clicked, this, &ParameterWidget::onAddTrajectoryPoint);
    connect(removePointButton, &QPushButton::clicked, this, &ParameterWidget::onRemoveTrajectoryPoint);
    connect(editPointButton, &QPushButton::clicked, this, &ParameterWidget::onEditTrajectoryPoint);
    connect(clearTrajectoryButton, &QPushButton::clicked, this, &ParameterWidget::onClearTrajectory);
    connect(optimizeTrajectoryButton, &QPushButton::clicked, this, &ParameterWidget::optimizeTrajectory);
    
    connect(trajectoryTableWidget, &QTableWidget::currentItemChanged, 
            this, &ParameterWidget::onTrajectorySelectionChanged);
    connect(trajectoryTableWidget, &QTableWidget::itemChanged, 
            this, &ParameterWidget::onTrajectoryItemChanged);
    
    // 模板管理信号
    connect(loadTemplateButton, &QPushButton::clicked, this, &ParameterWidget::onLoadTemplate);
    connect(saveTemplateButton, &QPushButton::clicked, this, &ParameterWidget::onSaveTemplate);
    connect(deleteTemplateButton, &QPushButton::clicked, this, &ParameterWidget::onDeleteTemplate);
}

void ParameterWidget::initializeParameterTable()
{
    // 参数定义
    QStringList paramNames = {
        "胶量", "点胶速度", "压力", "温度", "停留时间", "上升时间", "下降时间",
        "X轴速度", "Y轴速度", "Z轴速度", "X轴加速度", "Y轴加速度", "Z轴加速度"
    };
    
    QStringList paramUnits = {
        "μL", "mm/s", "Bar", "°C", "ms", "ms", "ms",
        "mm/s", "mm/s", "mm/s", "mm/s²", "mm/s²", "mm/s²"
    };
    
    QStringList paramDescriptions = {
        "单次点胶的胶量", "点胶头移动速度", "点胶压力", "加热温度", "点胶停留时间", "压力上升时间", "压力下降时间",
        "X轴最大速度", "Y轴最大速度", "Z轴最大速度", "X轴加速度", "Y轴加速度", "Z轴加速度"
    };
    
    parameterTableWidget->setRowCount(paramNames.size());
    
    for (int i = 0; i < paramNames.size(); ++i) {
        // 参数名称
        QTableWidgetItem* nameItem = new QTableWidgetItem(paramNames[i]);
        nameItem->setFlags(nameItem->flags() & ~Qt::ItemIsEditable);
        parameterTableWidget->setItem(i, 0, nameItem);
        
        // 当前值
        QTableWidgetItem* valueItem = new QTableWidgetItem("0.0");
        parameterTableWidget->setItem(i, 1, valueItem);
        
        // 单位
        QTableWidgetItem* unitItem = new QTableWidgetItem(paramUnits[i]);
        unitItem->setFlags(unitItem->flags() & ~Qt::ItemIsEditable);
        parameterTableWidget->setItem(i, 2, unitItem);
        
        // 描述
        QTableWidgetItem* descItem = new QTableWidgetItem(paramDescriptions[i]);
        descItem->setFlags(descItem->flags() & ~Qt::ItemIsEditable);
        parameterTableWidget->setItem(i, 3, descItem);
    }
}

void ParameterWidget::initializeValidationRules()
{
    validationRules.clear();
    
    // 添加验证规则
    validationRules.append({"胶量", 0.001, 1000.0, "μL", "胶量范围"});
    validationRules.append({"点胶速度", 0.1, 1000.0, "mm/s", "点胶速度范围"});
    validationRules.append({"压力", 0.1, 10.0, "Bar", "压力范围"});
    validationRules.append({"温度", 10.0, 80.0, "°C", "温度范围"});
    validationRules.append({"停留时间", 1, 10000, "ms", "停留时间范围"});
    validationRules.append({"上升时间", 1, 1000, "ms", "上升时间范围"});
    validationRules.append({"下降时间", 1, 1000, "ms", "下降时间范围"});
}

// 程序管理槽函数
void ParameterWidget::onImportProgram()
{
    QString fileName = QFileDialog::getOpenFileName(this, 
        "导入点胶程序", programsDirectory, "JSON文件 (*.json)");
    
    if (!fileName.isEmpty()) {
        loadProgram(fileName);
        LogManager::getInstance()->info("导入程序: " + fileName, "Parameter");
    }
}

void ParameterWidget::onExportProgram()
{
    QString fileName = QFileDialog::getSaveFileName(this, 
        "导出点胶程序", programsDirectory + "/" + currentProgram.name + ".json", 
        "JSON文件 (*.json)");
    
    if (!fileName.isEmpty()) {
        saveProgram(fileName);
        LogManager::getInstance()->info("导出程序: " + fileName, "Parameter");
    }
}

void ParameterWidget::onNewProgram()
{
    bool ok;
    QString name = QInputDialog::getText(this, "新建程序", "程序名称:", 
                                        QLineEdit::Normal, "新程序", &ok);
    if (ok && !name.isEmpty()) {
        newProgram();
        currentProgram.name = name;
        updateProgramList();
        updateParameterDisplay();
        LogManager::getInstance()->info("新建程序: " + name, "Parameter");
    }
}

void ParameterWidget::onDeleteProgram()
{
    QTreeWidgetItem* item = programTreeWidget->currentItem();
    if (!item) {
        QMessageBox::warning(this, "警告", "请先选择要删除的程序！");
        return;
    }
    
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认删除");
    msgBox.setText("确定要删除选中的程序吗？此操作不可恢复。");
    msgBox.setIcon(QMessageBox::Question);
    
    QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
    QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.setDefaultButton(noButton);
    
    msgBox.exec();
    
    if (msgBox.clickedButton() == yesButton) {
        deleteProgram();
        LogManager::getInstance()->info("删除程序", "Parameter");
    }
}

void ParameterWidget::onDuplicateProgram()
{
    QTreeWidgetItem* item = programTreeWidget->currentItem();
    if (!item) {
        QMessageBox::warning(this, "警告", "请先选择要复制的程序！");
        return;
    }
    
    bool ok;
    QString name = QInputDialog::getText(this, "复制程序", "新程序名称:", 
                                        QLineEdit::Normal, currentProgram.name + "_副本", &ok);
    if (ok && !name.isEmpty()) {
        GlueProgram newProgram = currentProgram;
        newProgram.name = name;
        newProgram.version = "1.0";
        newProgram.createTime = QDateTime::currentDateTime();
        newProgram.modifyTime = QDateTime::currentDateTime();
        
        programList.append(newProgram);
        updateProgramList();
        LogManager::getInstance()->info("复制程序: " + name, "Parameter");
    }
}

void ParameterWidget::onProgramSelectionChanged()
{
    QTreeWidgetItem* item = programTreeWidget->currentItem();
    if (item) {
        QString programName = item->text(0);
        // 查找并加载程序
        for (const GlueProgram& program : programList) {
            if (program.name == programName) {
                setCurrentProgram(program);
                break;
            }
        }
    }
}

void ParameterWidget::onProgramItemChanged(QTreeWidgetItem* item, int column)
{
    if (column == 0) { // 程序名称变更
        QString oldName = currentProgram.name;
        QString newName = item->text(0);
        if (oldName != newName) {
            currentProgram.name = newName;
            currentProgram.modifyTime = QDateTime::currentDateTime();
            isModified = true;
            if (autoSaveTimer) {
                autoSaveTimer->start();
            }
        }
    }
}

// 参数管理槽函数
void ParameterWidget::onParameterChanged()
{
    // 更新当前程序信息
    currentProgram.name = programNameEdit->text();
    currentProgram.version = programVersionEdit->text();
    currentProgram.description = programDescriptionEdit->toPlainText();
    currentProgram.modifyTime = QDateTime::currentDateTime();
    
    isModified = true;
    if (autoSaveTimer) {
        autoSaveTimer->start();
    }
    emit parametersChanged();
}

void ParameterWidget::onParameterItemChanged()
{
    QTableWidgetItem* item = parameterTableWidget->currentItem();
    if (item && item->column() == 1) { // 只有数值列可编辑
        // 更新程序参数
        int row = item->row();
        double value = item->text().toDouble();
        
        switch (row) {
            case 0: currentProgram.params.volume = value; break;
            case 1: currentProgram.params.speed = value; break;
            case 2: currentProgram.params.pressure = value; break;
            case 3: currentProgram.params.temperature = value; break;
            case 4: currentProgram.params.dwellTime = static_cast<int>(value); break;
            case 5: currentProgram.params.riseTime = static_cast<int>(value); break;
            case 6: currentProgram.params.fallTime = static_cast<int>(value); break;
            default: break;
        }
        
        isModified = true;
        if (autoSaveTimer) {
            autoSaveTimer->start();
        }
        emit parametersChanged();
    }
}

void ParameterWidget::onValidateParameters()
{
    QString error;
    if (validateProgram(currentProgram, error)) {
        QMessageBox::information(this, "参数验证", "所有参数都在有效范围内！");
        LogManager::getInstance()->info("参数验证通过", "Parameter");
    } else {
        QMessageBox::warning(this, "参数验证失败", error);
        LogManager::getInstance()->warning("参数验证失败: " + error, "Parameter");
    }
}

void ParameterWidget::onOptimizeParameters()
{
    // 简单的参数优化建议
    QStringList suggestions;
    
    if (currentProgram.params.volume < 0.5) {
        suggestions << "胶量过小，建议增加到0.5μL以上";
    }
    if (currentProgram.params.speed > 500) {
        suggestions << "点胶速度过快，建议降低到500mm/s以下";
    }
    if (currentProgram.params.pressure > 5.0) {
        suggestions << "压力过高，建议降低到5.0Bar以下";
    }
    if (currentProgram.params.dwellTime < 50) {
        suggestions << "停留时间过短，建议增加到50ms以上";
    }
    
    if (suggestions.isEmpty()) {
        QMessageBox::information(this, "参数优化", "当前参数已经比较合理！");
    } else {
        QString message = "参数优化建议：\n\n" + suggestions.join("\n");
        QMessageBox::information(this, "参数优化建议", message);
    }
    
    LogManager::getInstance()->info("执行参数优化", "Parameter");
}

// 轨迹编辑槽函数
void ParameterWidget::onAddTrajectoryPoint()
{
    GlueProgram::TrajectoryPoint point;
    point.x = 0.0;
    point.y = 0.0;
    point.z = 0.0;
    point.speed = currentProgram.params.speed;
    point.volume = currentProgram.params.volume;
    point.dwellTime = currentProgram.params.dwellTime;
    point.isGluePoint = true;
    
    addTrajectoryPoint(point);
    LogManager::getInstance()->info("添加轨迹点", "Parameter");
}

void ParameterWidget::onRemoveTrajectoryPoint()
{
    int currentRow = trajectoryTableWidget->currentRow();
    if (currentRow >= 0) {
        removeTrajectoryPoint(currentRow);
        LogManager::getInstance()->info("删除轨迹点", "Parameter");
    } else {
        QMessageBox::warning(this, "警告", "请先选择要删除的轨迹点！");
    }
}

void ParameterWidget::onEditTrajectoryPoint()
{
    int currentRow = trajectoryTableWidget->currentRow();
    if (currentRow >= 0) {
        // 这里可以打开一个详细的编辑对话框
        QMessageBox::information(this, "提示", "轨迹点编辑功能待实现");
    } else {
        QMessageBox::warning(this, "警告", "请先选择要编辑的轨迹点！");
    }
}

void ParameterWidget::onClearTrajectory()
{
    QMessageBox msgBox(this);
    msgBox.setWindowTitle("确认清空");
    msgBox.setText("确定要清空所有轨迹点吗？此操作不可恢复。");
    msgBox.setIcon(QMessageBox::Question);
    
    QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
    QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
    msgBox.setDefaultButton(noButton);
    
    msgBox.exec();
    
    if (msgBox.clickedButton() == yesButton) {
        clearTrajectory();
        LogManager::getInstance()->info("清空轨迹", "Parameter");
    }
}

void ParameterWidget::onTrajectorySelectionChanged()
{
    int currentRow = trajectoryTableWidget->currentRow();
    removePointButton->setEnabled(currentRow >= 0);
    editPointButton->setEnabled(currentRow >= 0);
}

void ParameterWidget::onTrajectoryItemChanged(QTableWidgetItem* item)
{
    if (!item) return;
    
    int row = item->row();
    int col = item->column();
    
    if (row < currentProgram.trajectory.size()) {
        GlueProgram::TrajectoryPoint& point = currentProgram.trajectory[row];
        
        switch (col) {
            case 1: point.x = item->text().toDouble(); break;
            case 2: point.y = item->text().toDouble(); break;
            case 3: point.z = item->text().toDouble(); break;
            case 4: point.speed = item->text().toDouble(); break;
            case 5: point.volume = item->text().toDouble(); break;
            case 6: point.dwellTime = item->text().toInt(); break;
            case 7: point.isGluePoint = (item->text() == "是"); break;
            default: break;
        }
        
        calculateTrajectoryTime();
        isModified = true;
        autoSaveTimer->start();
        emit trajectoryChanged();
    }
}

// 模板管理槽函数
void ParameterWidget::onLoadTemplate()
{
    QTreeWidgetItem* item = templateTreeWidget->currentItem();
    if (item) {
        QString templateName = item->text(0);
        loadTemplate(templateName);
        LogManager::getInstance()->info("加载模板: " + templateName, "Parameter");
    } else {
        QMessageBox::warning(this, "警告", "请先选择要加载的模板！");
    }
}

void ParameterWidget::onSaveTemplate()
{
    bool ok;
    QString name = QInputDialog::getText(this, "保存模板", "模板名称:", 
                                        QLineEdit::Normal, "新模板", &ok);
    if (ok && !name.isEmpty()) {
        saveTemplate(name);
        LogManager::getInstance()->info("保存模板: " + name, "Parameter");
    }
}

void ParameterWidget::onDeleteTemplate()
{
    QTreeWidgetItem* item = templateTreeWidget->currentItem();
    if (item) {
        QString templateName = item->text(0);
        QMessageBox msgBox(this);
        msgBox.setWindowTitle("确认删除");
        msgBox.setText("确定要删除模板 \"" + templateName + "\" 吗？");
        msgBox.setIcon(QMessageBox::Question);
        
        QPushButton* yesButton = msgBox.addButton("确定", QMessageBox::YesRole);
        QPushButton* noButton = msgBox.addButton("取消", QMessageBox::NoRole);
        msgBox.setDefaultButton(noButton);
        
        msgBox.exec();
        
        if (msgBox.clickedButton() == yesButton) {
            deleteTemplate(templateName);
            LogManager::getInstance()->info("删除模板: " + templateName, "Parameter");
        }
    } else {
        QMessageBox::warning(this, "警告", "请先选择要删除的模板！");
    }
}

// 核心功能实现
void ParameterWidget::loadProgram(const QString& filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, "错误", "无法打开文件: " + filePath);
        return;
    }
    
    QByteArray data = file.readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    QJsonObject obj = doc.object();
    
    // 解析程序数据
    GlueProgram program;
    program.name = obj["name"].toString();
    program.version = obj["version"].toString();
    program.description = obj["description"].toString();
    program.createTime = QDateTime::fromString(obj["createTime"].toString(), Qt::ISODate);
    program.modifyTime = QDateTime::fromString(obj["modifyTime"].toString(), Qt::ISODate);
    
    // 解析参数
    QJsonObject params = obj["parameters"].toObject();
    program.params.volume = params["volume"].toDouble();
    program.params.speed = params["speed"].toDouble();
    program.params.pressure = params["pressure"].toDouble();
    program.params.temperature = params["temperature"].toDouble();
    program.params.dwellTime = params["dwellTime"].toInt();
    program.params.riseTime = params["riseTime"].toInt();
    program.params.fallTime = params["fallTime"].toInt();
    
    // 解析轨迹
    QJsonArray trajectory = obj["trajectory"].toArray();
    program.trajectory.clear();
    for (const QJsonValue& value : trajectory) {
        QJsonObject pointObj = value.toObject();
        GlueProgram::TrajectoryPoint point;
        point.x = pointObj["x"].toDouble();
        point.y = pointObj["y"].toDouble();
        point.z = pointObj["z"].toDouble();
        point.speed = pointObj["speed"].toDouble();
        point.volume = pointObj["volume"].toDouble();
        point.dwellTime = pointObj["dwellTime"].toInt();
        point.isGluePoint = pointObj["isGluePoint"].toBool();
        program.trajectory.append(point);
    }
    
    setCurrentProgram(program);
    currentProgramPath = filePath;
    
    // 添加到程序列表
    bool found = false;
    for (int i = 0; i < programList.size(); ++i) {
        if (programList[i].name == program.name) {
            programList[i] = program;
            found = true;
            break;
        }
    }
    if (!found) {
        programList.append(program);
    }
    
    updateProgramList();
    updateParameterDisplay();
    updateTrajectoryDisplay();
}

void ParameterWidget::saveProgram(const QString& filePath)
{
    QJsonObject obj;
    obj["name"] = currentProgram.name;
    obj["version"] = currentProgram.version;
    obj["description"] = currentProgram.description;
    obj["createTime"] = currentProgram.createTime.toString(Qt::ISODate);
    obj["modifyTime"] = currentProgram.modifyTime.toString(Qt::ISODate);
    
    // 保存参数
    QJsonObject params;
    params["volume"] = currentProgram.params.volume;
    params["speed"] = currentProgram.params.speed;
    params["pressure"] = currentProgram.params.pressure;
    params["temperature"] = currentProgram.params.temperature;
    params["dwellTime"] = currentProgram.params.dwellTime;
    params["riseTime"] = currentProgram.params.riseTime;
    params["fallTime"] = currentProgram.params.fallTime;
    obj["parameters"] = params;
    
    // 保存轨迹
    QJsonArray trajectory;
    for (const GlueProgram::TrajectoryPoint& point : currentProgram.trajectory) {
        QJsonObject pointObj;
        pointObj["x"] = point.x;
        pointObj["y"] = point.y;
        pointObj["z"] = point.z;
        pointObj["speed"] = point.speed;
        pointObj["volume"] = point.volume;
        pointObj["dwellTime"] = point.dwellTime;
        pointObj["isGluePoint"] = point.isGluePoint;
        trajectory.append(pointObj);
    }
    obj["trajectory"] = trajectory;
    
    QJsonDocument doc(obj);
    
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        QMessageBox::warning(this, "错误", "无法保存文件: " + filePath);
        return;
    }
    
    file.write(doc.toJson());
    currentProgramPath = filePath;
    isModified = false;
}

void ParameterWidget::newProgram()
{
    currentProgram = GlueProgram();
    currentProgramPath.clear();
    isModified = false;
    
    updateParameterDisplay();
    updateTrajectoryDisplay();
}

void ParameterWidget::deleteProgram()
{
    QTreeWidgetItem* item = programTreeWidget->currentItem();
    if (item) {
        QString programName = item->text(0);
        
        // 从列表中移除
        for (int i = 0; i < programList.size(); ++i) {
            if (programList[i].name == programName) {
                programList.removeAt(i);
                break;
            }
        }
        
        // 删除文件
        QString filePath = programsDirectory + "/" + programName + ".json";
        QFile::remove(filePath);
        
        updateProgramList();
        
        // 如果删除的是当前程序，创建新程序
        if (currentProgram.name == programName) {
            newProgram();
        }
    }
}

void ParameterWidget::setCurrentProgram(const GlueProgram& program)
{
    currentProgram = program;
    updateParameterDisplay();
    updateTrajectoryDisplay();
    emit programChanged(program);
}

GlueProgram ParameterWidget::getCurrentProgram() const
{
    return currentProgram;
}

void ParameterWidget::addTrajectoryPoint(const GlueProgram::TrajectoryPoint& point)
{
    currentProgram.trajectory.append(point);
    updateTrajectoryDisplay();
    calculateTrajectoryTime();
    isModified = true;
    if (autoSaveTimer) {
        autoSaveTimer->start();
    }
    emit trajectoryChanged();
}

void ParameterWidget::removeTrajectoryPoint(int index)
{
    if (index >= 0 && index < currentProgram.trajectory.size()) {
        currentProgram.trajectory.removeAt(index);
        updateTrajectoryDisplay();
        calculateTrajectoryTime();
        isModified = true;
        if (autoSaveTimer) {
            autoSaveTimer->start();
        }
        emit trajectoryChanged();
    }
}

void ParameterWidget::clearTrajectory()
{
    currentProgram.trajectory.clear();
    updateTrajectoryDisplay();
    calculateTrajectoryTime();
    isModified = true;
    if (autoSaveTimer) {
        autoSaveTimer->start();
    }
    emit trajectoryChanged();
}

void ParameterWidget::updateProgramList()
{
    programTreeWidget->clear();
    
    for (const GlueProgram& program : programList) {
        QTreeWidgetItem* item = new QTreeWidgetItem(programTreeWidget);
        item->setText(0, program.name);
        item->setText(1, program.version);
        item->setText(2, program.modifyTime.toString("yyyy-MM-dd hh:mm"));
        item->setFlags(item->flags() | Qt::ItemIsEditable);
    }
}

void ParameterWidget::updateParameterDisplay()
{
    // 更新程序信息
    programNameEdit->setText(currentProgram.name);
    programVersionEdit->setText(currentProgram.version);
    programDescriptionEdit->setPlainText(currentProgram.description);
    createTimeLabel->setText(currentProgram.createTime.toString("yyyy-MM-dd hh:mm:ss"));
    modifyTimeLabel->setText(currentProgram.modifyTime.toString("yyyy-MM-dd hh:mm:ss"));
    
    // 更新参数表格
    if (parameterTableWidget->rowCount() >= 7) {
        parameterTableWidget->item(0, 1)->setText(QString::number(currentProgram.params.volume, 'f', 3));
        parameterTableWidget->item(1, 1)->setText(QString::number(currentProgram.params.speed, 'f', 2));
        parameterTableWidget->item(2, 1)->setText(QString::number(currentProgram.params.pressure, 'f', 2));
        parameterTableWidget->item(3, 1)->setText(QString::number(currentProgram.params.temperature, 'f', 1));
        parameterTableWidget->item(4, 1)->setText(QString::number(currentProgram.params.dwellTime));
        parameterTableWidget->item(5, 1)->setText(QString::number(currentProgram.params.riseTime));
        parameterTableWidget->item(6, 1)->setText(QString::number(currentProgram.params.fallTime));
    }
}

void ParameterWidget::updateTrajectoryDisplay()
{
    trajectoryTableWidget->setRowCount(currentProgram.trajectory.size());
    
    for (int i = 0; i < currentProgram.trajectory.size(); ++i) {
        const GlueProgram::TrajectoryPoint& point = currentProgram.trajectory[i];
        
        trajectoryTableWidget->setItem(i, 0, new QTableWidgetItem(QString::number(i + 1)));
        trajectoryTableWidget->setItem(i, 1, new QTableWidgetItem(QString::number(point.x, 'f', 3)));
        trajectoryTableWidget->setItem(i, 2, new QTableWidgetItem(QString::number(point.y, 'f', 3)));
        trajectoryTableWidget->setItem(i, 3, new QTableWidgetItem(QString::number(point.z, 'f', 3)));
        trajectoryTableWidget->setItem(i, 4, new QTableWidgetItem(QString::number(point.speed, 'f', 2)));
        trajectoryTableWidget->setItem(i, 5, new QTableWidgetItem(QString::number(point.volume, 'f', 3)));
        trajectoryTableWidget->setItem(i, 6, new QTableWidgetItem(QString::number(point.dwellTime)));
        trajectoryTableWidget->setItem(i, 7, new QTableWidgetItem(point.isGluePoint ? "是" : "否"));
        
        // 设置序号列不可编辑
        trajectoryTableWidget->item(i, 0)->setFlags(trajectoryTableWidget->item(i, 0)->flags() & ~Qt::ItemIsEditable);
    }
    
    // 更新统计信息
    totalPointsLabel->setText(QString::number(currentProgram.trajectory.size()));
    
    double totalVolume = 0;
    for (const GlueProgram::TrajectoryPoint& point : currentProgram.trajectory) {
        if (point.isGluePoint) {
            totalVolume += point.volume;
        }
    }
    totalVolumeLabel->setText(QString::number(totalVolume, 'f', 3) + " μL");
}

void ParameterWidget::calculateTrajectoryTime()
{
    if (currentProgram.trajectory.size() < 2) {
        totalDistanceLabel->setText("0.000 mm");
        totalTimeLabel->setText("0.0 s");
        return;
    }
    
    double totalDistance = 0;
    double totalTime = 0;
    
    for (int i = 1; i < currentProgram.trajectory.size(); ++i) {
        const GlueProgram::TrajectoryPoint& prev = currentProgram.trajectory[i - 1];
        const GlueProgram::TrajectoryPoint& curr = currentProgram.trajectory[i];
        
        // 计算距离
        double dx = curr.x - prev.x;
        double dy = curr.y - prev.y;
        double dz = curr.z - prev.z;
        double distance = std::sqrt(dx * dx + dy * dy + dz * dz);
        totalDistance += distance;
        
        // 计算时间
        double speed = std::max(curr.speed, 1.0); // 避免除零
        double moveTime = distance / speed;
        totalTime += moveTime;
        
        // 添加停留时间
        if (curr.isGluePoint) {
            totalTime += curr.dwellTime / 1000.0; // 转换为秒
        }
    }
    
    totalDistanceLabel->setText(QString::number(totalDistance, 'f', 3) + " mm");
    totalTimeLabel->setText(QString::number(totalTime, 'f', 1) + " s");
}

bool ParameterWidget::validateProgram(const GlueProgram& program, QString& error)
{
    // 验证基本参数
    for (const ValidationRule& rule : validationRules) {
        double value = 0;
        
        if (rule.parameter == "胶量") value = program.params.volume;
        else if (rule.parameter == "点胶速度") value = program.params.speed;
        else if (rule.parameter == "压力") value = program.params.pressure;
        else if (rule.parameter == "温度") value = program.params.temperature;
        else if (rule.parameter == "停留时间") value = program.params.dwellTime;
        else if (rule.parameter == "上升时间") value = program.params.riseTime;
        else if (rule.parameter == "下降时间") value = program.params.fallTime;
        
        if (value < rule.minValue || value > rule.maxValue) {
            error = QString("%1 超出范围 [%2, %3] %4")
                    .arg(rule.parameter)
                    .arg(rule.minValue)
                    .arg(rule.maxValue)
                    .arg(rule.unit);
            return false;
        }
    }
    
    // 验证轨迹
    if (program.trajectory.isEmpty()) {
        error = "轨迹为空";
        return false;
    }
    
    return true;
}

void ParameterWidget::loadProgramList()
{
    programList.clear();
    
    QDir dir(programsDirectory);
    QStringList filters;
    filters << "*.json";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo& fileInfo : fileList) {
        loadProgram(fileInfo.absoluteFilePath());
    }
    
    updateProgramList();
}

void ParameterWidget::saveProgramList()
{
    for (const GlueProgram& program : programList) {
        QString filePath = programsDirectory + "/" + program.name + ".json";
        // 这里可以调用saveProgram，但为了避免重复，直接保存当前程序
        if (program.name == currentProgram.name) {
            saveProgram(filePath);
        }
    }
}

void ParameterWidget::loadTemplateList()
{
    templateList.clear();
    
    // 添加默认模板
    addDefaultTemplates();
    
    // 从文件加载模板
    QDir dir(templatesDirectory);
    QStringList filters;
    filters << "*.json";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo& fileInfo : fileList) {
        QFile file(fileInfo.absoluteFilePath());
        if (file.open(QIODevice::ReadOnly)) {
            QByteArray data = file.readAll();
            QJsonDocument doc = QJsonDocument::fromJson(data);
            QJsonObject obj = doc.object();
            
            ParameterTemplate tmpl;
            tmpl.name = obj["name"].toString();
            tmpl.category = obj["category"].toString();
            tmpl.description = obj["description"].toString();
            tmpl.parameters = obj["parameters"].toObject();
            
            templateList.append(tmpl);
            file.close();
        }
    }
    
    updateTemplateList();
}

void ParameterWidget::saveTemplateList()
{
    // 保存模板列表到文件
}

void ParameterWidget::updateTemplateList()
{
    templateTreeWidget->clear();
    
    for (const ParameterTemplate& tmpl : templateList) {
        QTreeWidgetItem* item = new QTreeWidgetItem(templateTreeWidget);
        item->setText(0, tmpl.name);
        item->setText(1, tmpl.category);
        item->setText(2, tmpl.description);
    }
}

void ParameterWidget::loadTemplate(const QString& templateName)
{
    // 查找模板
    ParameterTemplate* foundTemplate = nullptr;
    for (ParameterTemplate& tmpl : templateList) {
        if (tmpl.name == templateName) {
            foundTemplate = &tmpl;
            break;
        }
    }
    
    if (!foundTemplate) {
        QMessageBox::warning(this, "错误", "未找到模板: " + templateName);
        return;
    }
    
    // 应用模板参数
    QJsonObject params = foundTemplate->parameters;
    if (params.contains("volume")) {
        currentProgram.params.volume = params["volume"].toDouble();
    }
    if (params.contains("speed")) {
        currentProgram.params.speed = params["speed"].toDouble();
    }
    if (params.contains("pressure")) {
        currentProgram.params.pressure = params["pressure"].toDouble();
    }
    if (params.contains("temperature")) {
        currentProgram.params.temperature = params["temperature"].toDouble();
    }
    if (params.contains("dwellTime")) {
        currentProgram.params.dwellTime = params["dwellTime"].toInt();
    }
    if (params.contains("riseTime")) {
        currentProgram.params.riseTime = params["riseTime"].toInt();
    }
    if (params.contains("fallTime")) {
        currentProgram.params.fallTime = params["fallTime"].toInt();
    }
    
    // 更新显示
    updateParameterDisplay();
    isModified = true;
    if (autoSaveTimer) {
        autoSaveTimer->start();
    };
    
    QMessageBox::information(this, "成功", "已加载模板: " + templateName);
    LogManager::getInstance()->info("加载模板: " + templateName, "Parameter");
    emit templateChanged(templateName);
}

void ParameterWidget::saveTemplate(const QString& templateName)
{
    bool ok;
    QString name = templateName;
    if (name.isEmpty()) {
        name = QInputDialog::getText(this, "保存模板", "模板名称:", 
                                    QLineEdit::Normal, "新模板", &ok);
        if (!ok || name.isEmpty()) {
            return;
        }
    }
    
    QString category = QInputDialog::getText(this, "保存模板", "模板分类:", 
                                           QLineEdit::Normal, "自定义", &ok);
    if (!ok) {
        category = "自定义";
    }
    
    QString description = QInputDialog::getText(this, "保存模板", "模板描述:", 
                                              QLineEdit::Normal, "", &ok);
    if (!ok) {
        description = "";
    }
    
    // 创建模板对象
    ParameterTemplate newTemplate;
    newTemplate.name = name;
    newTemplate.category = category;
    newTemplate.description = description;
    
    // 保存当前参数
    QJsonObject params;
    params["volume"] = currentProgram.params.volume;
    params["speed"] = currentProgram.params.speed;
    params["pressure"] = currentProgram.params.pressure;
    params["temperature"] = currentProgram.params.temperature;
    params["dwellTime"] = currentProgram.params.dwellTime;
    params["riseTime"] = currentProgram.params.riseTime;
    params["fallTime"] = currentProgram.params.fallTime;
    newTemplate.parameters = params;
    
    // 检查是否已存在同名模板
    bool exists = false;
    for (int i = 0; i < templateList.size(); ++i) {
        if (templateList[i].name == name) {
            templateList[i] = newTemplate; // 覆盖现有模板
            exists = true;
            break;
        }
    }
    
    if (!exists) {
        templateList.append(newTemplate);
    }
    
    // 保存到文件
    QString filePath = templatesDirectory + "/" + name + ".json";
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc;
        QJsonObject obj;
        obj["name"] = newTemplate.name;
        obj["category"] = newTemplate.category;
        obj["description"] = newTemplate.description;
        obj["parameters"] = newTemplate.parameters;
        obj["createTime"] = QDateTime::currentDateTime().toString(Qt::ISODate);
        
        doc.setObject(obj);
        file.write(doc.toJson());
        file.close();
        
        updateTemplateList();
        QMessageBox::information(this, "成功", "模板已保存: " + name);
        LogManager::getInstance()->info("保存模板: " + name, "Parameter");
        emit templateChanged(name);
    } else {
        QMessageBox::warning(this, "错误", "无法保存模板文件: " + filePath);
    }
}

void ParameterWidget::deleteTemplate(const QString& templateName)
{
    // 从列表中移除
    for (int i = 0; i < templateList.size(); ++i) {
        if (templateList[i].name == templateName) {
            templateList.removeAt(i);
            break;
        }
    }
    
    // 删除文件
    QString filePath = templatesDirectory + "/" + templateName + ".json";
    QFile::remove(filePath);
    
    updateTemplateList();
    QMessageBox::information(this, "成功", "已删除模板: " + templateName);
    LogManager::getInstance()->info("删除模板: " + templateName, "Parameter");
}

void ParameterWidget::autoSave()
{
    if (isModified) {
        // 自动保存当前程序
        if (!currentProgram.name.isEmpty()) {
            QString filePath = programsDirectory + "/" + currentProgram.name + ".json";
            saveProgram(filePath);
            LogManager::getInstance()->info("自动保存程序: " + currentProgram.name, "Parameter");
        }
        
        // 保存程序列表
        saveProgramList();
        isModified = false;
    }
}

void ParameterWidget::addDefaultTemplates()
{
    // 高精度模板
    ParameterTemplate highPrecision;
    highPrecision.name = "高精度点胶";
    highPrecision.category = "精密应用";
    highPrecision.description = "适用于精密电子产品的高精度点胶";
    
    QJsonObject highPrecisionParams;
    highPrecisionParams["volume"] = 0.1;
    highPrecisionParams["speed"] = 5.0;
    highPrecisionParams["pressure"] = 1.5;
    highPrecisionParams["temperature"] = 25.0;
    highPrecisionParams["dwellTime"] = 200;
    highPrecisionParams["riseTime"] = 100;
    highPrecisionParams["fallTime"] = 100;
    highPrecision.parameters = highPrecisionParams;
    
    templateList.append(highPrecision);
    
    // 高速生产模板
    ParameterTemplate highSpeed;
    highSpeed.name = "高速生产";
    highSpeed.category = "批量生产";
    highSpeed.description = "适用于大批量生产的快速点胶";
    
    QJsonObject highSpeedParams;
    highSpeedParams["volume"] = 2.0;
    highSpeedParams["speed"] = 50.0;
    highSpeedParams["pressure"] = 3.0;
    highSpeedParams["temperature"] = 30.0;
    highSpeedParams["dwellTime"] = 50;
    highSpeedParams["riseTime"] = 30;
    highSpeedParams["fallTime"] = 30;
    highSpeed.parameters = highSpeedParams;
    
    templateList.append(highSpeed);
    
    // 标准模板
    ParameterTemplate standard;
    standard.name = "标准点胶";
    standard.category = "通用应用";
    standard.description = "通用的标准点胶参数";
    
    QJsonObject standardParams;
    standardParams["volume"] = 1.0;
    standardParams["speed"] = 10.0;
    standardParams["pressure"] = 2.0;
    standardParams["temperature"] = 25.0;
    standardParams["dwellTime"] = 100;
    standardParams["riseTime"] = 50;
    standardParams["fallTime"] = 50;
    standard.parameters = standardParams;
    
    templateList.append(standard);
    
    // 厚胶层模板
    ParameterTemplate thickLayer;
    thickLayer.name = "厚胶层点胶";
    thickLayer.category = "特殊应用";
    thickLayer.description = "适用于需要厚胶层的应用场景";
    
    QJsonObject thickLayerParams;
    thickLayerParams["volume"] = 5.0;
    thickLayerParams["speed"] = 3.0;
    thickLayerParams["pressure"] = 4.0;
    thickLayerParams["temperature"] = 35.0;
    thickLayerParams["dwellTime"] = 300;
    thickLayerParams["riseTime"] = 150;
    thickLayerParams["fallTime"] = 150;
    thickLayer.parameters = thickLayerParams;
    
    templateList.append(thickLayer);
}

void ParameterWidget::optimizeTrajectory()
{
    if (currentProgram.trajectory.isEmpty()) {
        QMessageBox::warning(this, "警告", "轨迹为空，无法优化！");
        return;
    }
    
    QStringList optimizationOptions;
    optimizationOptions << "距离优化" << "时间优化" << "速度平滑" << "重复点清理";
    
    bool ok;
    QString selectedOption = QInputDialog::getItem(this, "轨迹优化", "选择优化方式:", 
                                                  optimizationOptions, 0, false, &ok);
    
    if (!ok || selectedOption.isEmpty()) {
        return;
    }
    
    int originalPoints = currentProgram.trajectory.size();
    double originalDistance = calculateTotalDistance();
    double originalTime = calculateTotalTime();
    
    if (selectedOption == "距离优化") {
        optimizeByDistance();
    } else if (selectedOption == "时间优化") {
        optimizeByTime();
    } else if (selectedOption == "速度平滑") {
        smoothSpeed();
    } else if (selectedOption == "重复点清理") {
        removeDuplicatePoints();
    }
    
    int newPoints = currentProgram.trajectory.size();
    double newDistance = calculateTotalDistance();
    double newTime = calculateTotalTime();
    
    // 显示优化结果
    QString result = QString("轨迹优化完成！\n\n"
                            "点数: %1 → %2\n"
                            "距离: %3 → %4\n"
                            "时间: %5 → %6\n\n"
                            "优化方式: %7")
                    .arg(originalPoints)
                    .arg(newPoints)
                    .arg(formatDistance(originalDistance))
                    .arg(formatDistance(newDistance))
                    .arg(formatTime(originalTime))
                    .arg(formatTime(newTime))
                    .arg(selectedOption);
    
    QMessageBox::information(this, "优化结果", result);
    
    updateTrajectoryDisplay();
    calculateTrajectoryTime();
    isModified = true;
    if (autoSaveTimer) {
        autoSaveTimer->start();
    }
    
    LogManager::getInstance()->info("轨迹优化: " + selectedOption, "Parameter");
}

void ParameterWidget::optimizeByDistance()
{
    // 最近邻算法优化路径距离
    if (currentProgram.trajectory.size() < 2) {
        return;
    }
    
    QList<GlueProgram::TrajectoryPoint> optimizedPath;
    QList<GlueProgram::TrajectoryPoint> remainingPoints = currentProgram.trajectory;
    
    // 从第一个点开始
    GlueProgram::TrajectoryPoint currentPoint = remainingPoints.takeFirst();
    optimizedPath.append(currentPoint);
    
    // 使用最近邻算法
    while (!remainingPoints.isEmpty()) {
        double minDistance = std::numeric_limits<double>::max();
        int nearestIndex = 0;
        
        for (int i = 0; i < remainingPoints.size(); ++i) {
            const GlueProgram::TrajectoryPoint& point = remainingPoints[i];
            double distance = sqrt(pow(point.x - currentPoint.x, 2) + 
                                 pow(point.y - currentPoint.y, 2) + 
                                 pow(point.z - currentPoint.z, 2));
            
            if (distance < minDistance) {
                minDistance = distance;
                nearestIndex = i;
            }
        }
        
        currentPoint = remainingPoints.takeAt(nearestIndex);
        optimizedPath.append(currentPoint);
    }
    
    currentProgram.trajectory = optimizedPath;
}

void ParameterWidget::optimizeByTime()
{
    // 时间优化：调整速度以减少总时间
    for (GlueProgram::TrajectoryPoint& point : currentProgram.trajectory) {
        if (point.isGluePoint) {
            // 点胶点保持较低速度以确保精度
            point.speed = std::min(point.speed, currentProgram.params.speed * 0.8);
        } else {
            // 移动点可以使用更高速度
            point.speed = std::min(point.speed * 1.5, currentProgram.params.speed * 1.2);
        }
    }
}

void ParameterWidget::smoothSpeed()
{
    // 速度平滑：使相邻点的速度变化更平滑
    if (currentProgram.trajectory.size() < 3) {
        return;
    }
    
    for (int i = 1; i < currentProgram.trajectory.size() - 1; ++i) {
        GlueProgram::TrajectoryPoint& prevPoint = currentProgram.trajectory[i-1];
        GlueProgram::TrajectoryPoint& currPoint = currentProgram.trajectory[i];
        GlueProgram::TrajectoryPoint& nextPoint = currentProgram.trajectory[i+1];
        
        // 使用三点平均值平滑速度
        double smoothedSpeed = (prevPoint.speed + currPoint.speed + nextPoint.speed) / 3.0;
        currPoint.speed = smoothedSpeed;
    }
}

void ParameterWidget::removeDuplicatePoints()
{
    // 清理重复点
    const double tolerance = 0.01; // 0.01mm容差
    
    for (int i = currentProgram.trajectory.size() - 1; i > 0; --i) {
        const GlueProgram::TrajectoryPoint& current = currentProgram.trajectory[i];
        const GlueProgram::TrajectoryPoint& previous = currentProgram.trajectory[i-1];
        
        double distance = sqrt(pow(current.x - previous.x, 2) + 
                             pow(current.y - previous.y, 2) + 
                             pow(current.z - previous.z, 2));
        
        if (distance < tolerance) {
            currentProgram.trajectory.removeAt(i);
        }
    }
}

double ParameterWidget::calculateTotalDistance() const
{
    double totalDistance = 0.0;
    
    for (int i = 1; i < currentProgram.trajectory.size(); ++i) {
        const GlueProgram::TrajectoryPoint& current = currentProgram.trajectory[i];
        const GlueProgram::TrajectoryPoint& previous = currentProgram.trajectory[i-1];
        
        double distance = sqrt(pow(current.x - previous.x, 2) + 
                             pow(current.y - previous.y, 2) + 
                             pow(current.z - previous.z, 2));
        
        totalDistance += distance;
    }
    
    return totalDistance;
}

double ParameterWidget::calculateTotalTime() const
{
    double totalTime = 0.0;
    
    for (int i = 1; i < currentProgram.trajectory.size(); ++i) {
        const GlueProgram::TrajectoryPoint& current = currentProgram.trajectory[i];
        const GlueProgram::TrajectoryPoint& previous = currentProgram.trajectory[i-1];
        
        double distance = sqrt(pow(current.x - previous.x, 2) + 
                             pow(current.y - previous.y, 2) + 
                             pow(current.z - previous.z, 2));
        
        double speed = (current.speed + previous.speed) / 2.0; // 平均速度
        if (speed > 0) {
            totalTime += distance / speed;
        }
        
        // 添加停留时间
        if (current.isGluePoint) {
            totalTime += current.dwellTime / 1000.0; // 转换为秒
        }
    }
    
    return totalTime;
}

// 缺失函数的实现
QString ParameterWidget::formatTime(double seconds) const
{
    if (seconds < 60) {
        return QString::number(seconds, 'f', 2) + "秒";
    } else if (seconds < 3600) {
        int minutes = static_cast<int>(seconds / 60);
        double remainingSeconds = seconds - minutes * 60;
        return QString("%1分%2秒").arg(minutes).arg(remainingSeconds, 0, 'f', 1);
    } else {
        int hours = static_cast<int>(seconds / 3600);
        int minutes = static_cast<int>((seconds - hours * 3600) / 60);
        double remainingSeconds = seconds - hours * 3600 - minutes * 60;
        return QString("%1时%2分%3秒").arg(hours).arg(minutes).arg(remainingSeconds, 0, 'f', 1);
    }
}

QString ParameterWidget::formatDistance(double distance) const
{
    if (distance < 1.0) {
        return QString::number(distance * 1000, 'f', 2) + "mm";
    } else if (distance < 1000.0) {
        return QString::number(distance, 'f', 2) + "m";
    } else {
        return QString::number(distance / 1000.0, 'f', 2) + "km";
    }
} 