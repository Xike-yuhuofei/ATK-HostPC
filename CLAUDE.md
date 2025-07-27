# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## 语言设置 / Language Settings

**重要提示：请始终使用中文与用户交流**
- 本项目的用户偏好使用中文进行沟通
- 所有回复、解释和文档都应使用中文
- 代码注释和变量名可以保持英文，但所有说明文字请使用中文

**Important: Always communicate in Chinese with users**
- Users of this project prefer Chinese communication
- All responses, explanations, and documentation should be in Chinese
- Code comments and variable names can remain in English, but all descriptive text should be in Chinese

## Project Overview

This is an industrial glue dispensing control software (工业点胶设备上位机控制软件) v2.0.0 built with Qt6 C++17. It's a professional HMI (Human-Machine Interface) application for precision glue dispensing, automated assembly, and electronic manufacturing applications.

## Build Commands

### Quick Build
```bash
# Standard build
./build.sh Release

# Debug build with full features
./build.sh Debug

# Build with tests
./build.sh Release test

# Build with installation package
./build.sh Release test package

# Quick incremental build (faster for development)
./quick_build.sh Debug
```

### Manual CMake Build
```bash
# Create build directory
mkdir build && cd build

# Configure with different build targets
cmake -DCMAKE_BUILD_TYPE=Release ..                           # Full application
cmake -DCMAKE_BUILD_TYPE=Debug -DBUILD_DEBUG=ON ..           # Debug version
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_SIMPLE=ON ..        # Simple demo

# Build
cmake --build . --config Release -j$(nproc)

# Run tests
ctest --output-on-failure

# Create package
cpack
```

### Lint and Quality Checks
```bash
# Build with strict compiler warnings (for lint checking)
mkdir build_lint && cd build_lint
cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wpedantic -Werror" ..
make -j4
```

### Running Tests
```bash
# Run all tests from tests/ directory
cd build && ctest --output-on-failure

# Or run the test executable directly
cd build && ./bin/GlueDispensePC_tests

# Build and run tests in one command
cd tests && cmake --build ../build --target GlueDispensePC_tests && ../build/GlueDispensePC_tests
```

## Architecture Overview

### Layered Architecture
The application follows a layered MVC/MVVM architecture:

1. **UI Layer** (`src/ui/`, `src/mainwindow.*`)
   - Device control widgets, parameter configuration, process monitoring
   - Data recording widgets, security/permission management
   - Qt-based widgets with real-time data visualization

2. **Core Business Logic** (`src/core/`)
   - `BusinessLogicManager`: Main business operations coordinator
   - `SystemManager`: System-level management (config, logging, security, monitoring)
   - `UIManager`: UI component management and menu/toolbar setup
   - `EventCoordinator`: Event handling and inter-component communication

3. **Communication Layer** (`src/communication/`)
   - Multi-protocol support: Serial (RS232/RS485), TCP/IP, CAN bus, Modbus RTU/TCP
   - Worker-based threading pattern for device communication
   - Protocol parsers and data processing workers

### Key Components

#### Data Models (`src/data/`)
- **BaseDataModel**: Abstract interface for all data models with serialization/validation
- **ProductionRecord**: Manufacturing batch data and quality tracking
- **BatchRecord**: Batch management with completion/quality tracking
- **AlarmRecord**: Alarm management with levels (Info/Warning/Error/Critical/Emergency)
- **SensorData**: Real-time sensor readings with status monitoring
- **User**: Multi-level permission management (Admin/Operator/Technician)
- **SystemConfig**: System configuration with encrypted storage support
- **DeviceConfig**: Device connection parameters and settings

#### Communication Workers
- **SerialWorker/TCPWorker/CANWorker**: Protocol-specific communication handlers
- **ModbusWorker**: Modbus RTU/TCP protocol implementation
- **DataProcessWorker**: Real-time data processing and filtering
- **ProtocolParser**: Message parsing and validation

#### Configuration Management (`src/config/`)
- **ConfigManager**: Centralized configuration with auto-save
- **AdaptiveConfigManager**: Dynamic configuration adaptation based on performance metrics
- Supports user settings, device parameters, and system preferences
- JSON-based configuration files with validation

#### Performance Management (`src/core/`)
- **PerformanceMonitor**: Real-time system performance monitoring
- **PerformanceConfigManager**: Performance-aware configuration management
- **MemoryOptimizer**: Memory usage optimization and garbage collection
- **UIUpdateOptimizer**: UI refresh rate optimization for smooth rendering
- **ContinuousOptimizer**: Continuous system optimization based on usage patterns
- **MLPerformancePredictor**: Machine learning-based performance prediction
- **IntelligentAnalyzer**: Intelligent system analysis and recommendations
- **LoadBalancer**: Load balancing for communication and processing tasks
- Performance configuration in `config/performance_config.json`

#### Logging System (`src/logger/`)
- **LogManager**: Multi-level logging (Debug/Info/Warning/Error/Critical)
- Separate log files for application, communication, errors, and operations
- Automatic log rotation and archival

## Build Targets

The CMakeLists.txt defines three build targets:

1. **GlueDispensePC** (`BUILD_FULL=ON`, default)
   - Full-featured application with all modules
   - Requires Qt6 components: Core, Widgets, SerialPort, Network, Sql, Charts, Concurrent, SerialBus, Bluetooth, Multimedia, PrintSupport, Xml

2. **SimpleDemo** (`BUILD_SIMPLE=ON`)
   - Minimal demo application
   - Only requires Qt6 Core and Widgets

3. **DebugGlueDispensePC** (`BUILD_DEBUG=ON`)
   - Debug version with essential communication modules
   - Includes SimpleMainWindowTest for UI testing
   - Defines DEBUG_MODE compilation flag

## Qt-Specific Patterns

### MOC (Meta-Object Compiler) Integration
- All QObject-derived classes use Qt's signal/slot mechanism
- MOC files are auto-generated in `*_autogen/` directories
- Use `Q_OBJECT` macro for classes with signals/slots

### Resource Management
- Icons and styles in `resources/` with `app.qrc` configuration
- Use `Q_DECLARE_METATYPE` for custom types in containers

### Threading
- Worker objects moved to separate threads for I/O operations
- Use Qt's signal/slot for thread-safe communication
- `QMutexLocker` for thread synchronization (note: mutex members should be `mutable` for const member functions)

## Data Flow

1. **Device Communication**: Hardware → Communication Workers → Protocol Parsers → Business Logic
2. **UI Updates**: Business Logic → UI Manager → Widget Updates (via signals/slots)
3. **Data Persistence**: Business Logic → Database Manager → SQLite/File Storage
4. **Configuration**: Config Manager ↔ All Components (centralized settings)

## Testing Framework

Custom test framework in `tests/` with Qt-style testing:
- **TestRunner**: Singleton test suite management and reporting
- **TestBase**: Base class for all test suites with assertion methods
- **DataModelsTest**: Unit tests for data model validation and serialization
- Test results saved to `test_report.txt` with detailed execution statistics
- Supports async testing with signal/slot verification
- Custom assertion macros: `ASSERT_EQ`, `ASSERT_TRUE`, `ASSERT_FALSE`

## Common Development Patterns

### Adding New Device Communication
1. Create worker class inheriting communication base
2. Implement protocol parser for message handling
3. Register with communication manager
4. Add device configuration to device.json

### Adding New UI Components
1. Create widget class with Qt Designer or code
2. Register with UIManager for menu/toolbar integration
3. Connect to business logic via signals/slots
4. Add proper data binding for real-time updates

### Data Model Extension
1. Inherit from BaseDataModel
2. Implement required virtual methods (serialization, validation)
3. Add Q_DECLARE_METATYPE registration
4. Update database schema if needed

## Code Quality Standards

- Use Qt 6 modern APIs (avoid deprecated functions like old addAction signatures)
- Member variables should follow declaration order in constructor initialization lists
- Use `Q_UNUSED(param)` for intentionally unused parameters
- Implement proper const-correctness (`mutable` for mutex in const methods)
- Use `static_cast` for enum conversions to maintain type safety

## Important Notes

- The codebase uses Chinese comments and UI text for internationalization
- AlarmRecord uses int fields (alarmLevel, alarmType, alarmStatus) that map to enum values
- Communication is designed for industrial real-time requirements (<50ms response)
- The application supports 24/7 continuous operation with automatic reconnection
- All data models implement BaseDataModel interface for consistent serialization/validation
- Use `ICommunication` interface for all communication implementations
- The system uses Qt6 signal/slot pattern extensively for thread-safe communication
- Database operations are handled through async managers for better performance
- Performance monitoring and optimization are built-in features (see `config/performance_config.json`)
- The system includes stress testing capabilities with configurable parameters

## Hardware Integration

The software is designed to interface with:
- **STM32F407 control cards** for motion control
- **Glue dispensing controllers** for precise fluid control
- **PLC systems** for industrial automation integration
- **CAN bus networks** for distributed control systems
- **Modbus devices** for sensor and actuator connectivity