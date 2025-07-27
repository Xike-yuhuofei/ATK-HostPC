# 工业点胶设备上位机控制软件 v2.0.0

## 项目简介

本项目是一款专业的工业点胶设备上位机控制软件，基于Qt C++开发，为精密点胶、自动化装配、电子制造等应用场景提供完整的设备控制与监控解决方案。

## 功能特性

### 🎯 核心功能模块

#### 1. 设备控制模块
- **点胶头运动控制**：启动、停止、暂停、回原点、紧急停止
- **精确定位控制**：X/Y/Z轴精确定位和点动控制
- **参数设置**：点胶量、速度、压力、温度、时间参数配置
- **实时监控**：设备状态、位置、温度、压力实时显示

#### 2. 参数配置管理模块
- **程序管理**：点胶程序导入/导出（JSON格式）
- **轨迹编辑**：可视化轨迹点编辑和路径规划
- **参数验证**：自动参数校验和优化建议
- **模板管理**：参数模板保存和批量应用

#### 3. 过程监控模块
- **实时数据显示**：位置、速度、压力、温度、胶量实时监控
- **曲线图表**：基于Qt Charts的实时数据可视化
- **历史数据**：数据记录、查询、导出功能
- **报警系统**：阈值监控和报警提示

#### 4. 生产数据记录模块
- **批次管理**：生产批次数据自动记录和管理
- **质量追踪**：质量检测数据记录和分析
- **统计分析**：生产统计、效率分析、趋势图表
- **报表生成**：支持多种格式的报表导出

#### 5. 安全防护与权限管理模块
- **用户管理**：多级权限管理（管理员、操作员、技术员等）
- **安全认证**：用户登录、会话管理、权限控制
- **审计日志**：操作记录、安全事件跟踪
- **数据保护**：数据加密、备份恢复

#### 6. 通讯接口模块
- **多协议支持**：串口(RS232/RS485)、TCP/IP、CAN总线、Modbus RTU/TCP
- **设备接入**：运动控制器、点胶控制器、PLC设备支持
- **连接管理**：自动重连、连接状态监控
- **数据传输**：可靠的数据传输和协议解析

### 🔧 技术特性

- **现代化架构**：基于Qt6 C++17，采用MVC设计模式
- **跨平台支持**：Windows、macOS、Linux全平台兼容
- **高性能**：多线程处理，实时响应<50ms
- **工业级稳定性**：7×24小时连续运行设计
- **用户友好**：直观的界面设计，丰富的快捷操作

## 技术架构

```
┌─────────────────────────────────────────────────────────────────┐
│                          UI界面层                               │
│  (设备控制、参数配置、过程监控、数据记录、权限管理)              │
└─────────────────────────▲───────────────────────────────────────┘
                          │ MVVM / MVC
┌─────────────────────────┴───────────────────────────────────────┐
│                     核心业务逻辑层                              │
│ (点胶控制、参数管理、数据处理、权限验证、报警管理)               │
└─────────────────────────▲───────────────────────────────────────┘
                  │
┌─────────────────────────┴───────────────────────────────────────┐
│                     硬件通讯层                                  │
│        (串口、TCP/IP、CAN、Modbus、运动控制器)                   │
└─────────────────────────────────────────────────────────────────┘
```

## 系统要求

### 最低配置
- **操作系统**：Windows 10/11 x64, macOS 10.15+, Ubuntu 18.04+
- **处理器**：Intel i5 或同等性能
- **内存**：8GB RAM
- **存储**：500GB可用空间
- **显示**：1920x1080分辨率

### 推荐配置
- **处理器**：Intel i7 或同等性能
- **内存**：16GB RAM
- **存储**：1TB SSD
- **显示**：2560x1440分辨率

### 开发环境
- **Qt版本**：Qt 6.5.0 或更高版本
- **编译器**：MSVC 2019/2022 (Windows), GCC 9+ (Linux), Clang 12+ (macOS)
- **CMake**：3.16 或更高版本

## 构建方法

### 快速构建

#### Linux/macOS
```bash
# 克隆项目
git clone <repository-url>
cd ATK_HostPC

# 构建项目
./build.sh Release

# 运行程序
./build/GlueDispensePC
```

#### Windows
```batch
# 克隆项目
git clone <repository-url>
cd ATK_HostPC

# 构建项目
build.bat Release

# 运行程序
build\Release\GlueDispensePC.exe
```

### 详细构建步骤

1. **安装依赖**
   ```bash
   # Ubuntu/Debian
   sudo apt-get install qt6-base-dev qt6-charts-dev qt6-serialport-dev qt6-serialbus-dev cmake build-essential
   
   # macOS (使用Homebrew)
   brew install qt6 cmake
   
   # Windows - 下载并安装Qt6和Visual Studio
```

2. **配置项目**
```bash
   mkdir build && cd build
   cmake -DCMAKE_BUILD_TYPE=Release ..
```

3. **编译项目**
```bash
   cmake --build . --config Release -j$(nproc)
```

4. **创建安装包**
```bash
   cpack
   ```

### 构建选项

- `Debug` - 调试版本，包含调试信息
- `Release` - 发布版本，性能优化
- `test` - 运行单元测试
- `package` - 创建安装包

## 使用说明

### 首次启动

1. **权限设置**：首次启动需要管理员权限进行初始化设置
2. **设备连接**：配置通讯参数，连接点胶设备
3. **参数配置**：设置点胶参数和运动参数
4. **用户管理**：创建操作员账户和权限分配

### 基本操作

1. **设备控制**
   - 点击"连接设备"建立通讯连接
   - 使用控制面板进行设备操作
   - 通过参数面板调整工艺参数

2. **程序管理**
   - 导入/导出点胶程序文件
   - 编辑轨迹点和参数
   - 保存为模板供后续使用

3. **数据监控**
   - 实时查看设备状态和数据
   - 设置报警阈值
   - 导出历史数据

### 高级功能

- **批次生产**：设置生产批次，自动记录生产数据
- **质量管理**：配置质量检测参数，生成质量报告
- **远程监控**：通过网络接口实现远程监控
- **数据分析**：生成统计报表和趋势分析

## 文件结构

```
ATK_HostPC/
├── src/                    # 源代码
│   ├── main.cpp           # 主程序入口
│   ├── mainwindow.h/cpp   # 主窗口
│   ├── communication/     # 通讯模块
│   ├── ui/                # 界面组件
│   ├── data/              # 数据处理
│   ├── logger/            # 日志管理
│   ├── config/            # 配置管理
│   └── utils/             # 工具函数
├── resources/             # 资源文件
│   ├── icons/            # 图标
│   ├── styles/           # 样式表
│   └── app.qrc           # 资源配置
├── config/               # 配置文件
├── data/                 # 数据文件
├── CMakeLists.txt        # CMake配置
├── build.sh              # Linux/macOS构建脚本
├── build.bat             # Windows构建脚本
├── 开发文档.md            # 详细开发文档
└── README.md             # 项目说明
```

## 配置文件

### 主配置文件 (config.ini)
```ini
[General]
Language=zh_CN
Theme=default
AutoSave=true
AutoSaveInterval=300000

[Communication]
DefaultPort=COM1
DefaultBaudRate=115200
Timeout=5000
AutoReconnect=true

[Device]
MaxSpeed=1000
MaxAcceleration=500
SafetyDistance=10
EmergencyStopEnabled=true

[User]
SessionTimeout=1800
PasswordPolicy=strong
MaxLoginAttempts=3
```

### 设备配置文件 (device.json)
```json
{
  "devices": [
    {
      "name": "点胶控制器",
      "type": "glue_controller",
      "protocol": "modbus_rtu",
      "address": 1,
      "parameters": {
        "volume_range": [0.1, 10.0],
        "pressure_range": [0.1, 5.0],
        "temperature_range": [20, 80]
      }
    }
  ]
}
```

## 故障排除

### 常见问题

1. **设备连接失败**
   - 检查通讯参数配置
   - 确认设备电源和连接线
   - 查看日志文件获取详细错误信息

2. **权限不足**
   - 确保以管理员身份运行
   - 检查用户权限设置
   - 重新登录用户账户

3. **数据丢失**
   - 检查数据库连接
   - 查看备份文件
   - 使用数据恢复功能

### 日志文件

- **应用日志**：`data/logs/application.log`
- **通讯日志**：`data/logs/communication.log`
- **错误日志**：`data/logs/error.log`
- **操作日志**：`data/logs/operation.log`

## 技术支持

### 联系方式
- **技术支持**：support@industrial.com
- **用户手册**：查看软件内置帮助文档
- **在线支持**：https://support.industrial.com

### 版本更新
- **当前版本**：v2.0.0
- **更新检查**：软件内置自动更新检查
- **发布说明**：查看CHANGELOG.md

## 许可证

本软件采用商业许可证，详情请参阅LICENSE文件。

## 贡献指南

欢迎提交问题报告和功能建议。如需贡献代码，请遵循以下步骤：

1. Fork本项目
2. 创建功能分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 创建Pull Request

## 更新日志

### v2.0.0 (2024-01-XX)
- 🎉 重大版本更新，专门针对点胶设备优化
- ✨ 新增生产数据记录模块
- ✨ 新增安全防护与权限管理模块
- ✨ 新增通讯接口扩展模块
- 🔧 完善设备控制和参数配置功能
- 🔧 优化过程监控和数据可视化
- 🐛 修复已知问题，提升稳定性

### v1.0.0 (2023-XX-XX)
- 🎉 初始版本发布
- ✨ 基础设备控制功能
- ✨ 数据监控和图表显示
- ✨ 串口通讯支持

---

**© 2024 Industrial Solutions. All rights reserved.** 