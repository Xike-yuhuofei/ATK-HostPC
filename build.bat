@echo off
chcp 65001 > nul
setlocal enabledelayedexpansion

rem 工业点胶设备上位机控制软件构建脚本 (Windows)
rem 版本: 2.0.0
rem 作者: Industrial Solutions

echo ==========================================
echo 工业点胶设备上位机控制软件构建脚本
echo 版本: 2.0.0
echo ==========================================

rem 设置构建类型
set BUILD_TYPE=%1
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Release
echo 构建类型: %BUILD_TYPE%

rem 创建构建目录
set BUILD_DIR=build
if exist "%BUILD_DIR%" (
    echo 清理旧的构建目录...
    rmdir /s /q "%BUILD_DIR%"
)

echo 创建构建目录: %BUILD_DIR%
mkdir "%BUILD_DIR%"
cd "%BUILD_DIR%"

rem 检查Qt环境
echo 检查Qt环境...
where qmake >nul 2>&1
if %errorlevel% neq 0 (
    echo 错误: 未找到Qt环境，请确保Qt6已正确安装并添加到PATH
    echo 请检查以下路径是否在PATH中:
    echo   - Qt安装目录\bin
    echo   - 例如: C:\Qt\6.5.0\msvc2019_64\bin
    pause
    exit /b 1
)

for /f "tokens=*" %%i in ('qmake -query QT_VERSION') do set QT_VERSION=%%i
echo Qt版本: %QT_VERSION%

rem 检查CMake
echo 检查CMake...
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo 错误: 未找到CMake，请先安装CMake
    echo 下载地址: https://cmake.org/download/
    pause
    exit /b 1
)

for /f "tokens=3" %%i in ('cmake --version ^| findstr /r "cmake version"') do set CMAKE_VERSION=%%i
echo CMake版本: %CMAKE_VERSION%

rem 检查编译器
echo 检查编译器...
where cl >nul 2>&1
if %errorlevel% neq 0 (
    echo 警告: 未找到MSVC编译器，尝试使用MinGW...
    where g++ >nul 2>&1
    if %errorlevel% neq 0 (
        echo 错误: 未找到可用的C++编译器
        echo 请安装Visual Studio或MinGW
        pause
        exit /b 1
    ) else (
        echo 使用MinGW编译器
        set GENERATOR=-G "MinGW Makefiles"
    )
) else (
    echo 使用MSVC编译器
    set GENERATOR=-G "Visual Studio 16 2019" -A x64
)

rem 配置项目
echo 配置项目...
set CMAKE_ARGS=-DCMAKE_BUILD_TYPE=%BUILD_TYPE% %GENERATOR%

echo 执行CMake配置: cmake %CMAKE_ARGS% ..
cmake %CMAKE_ARGS% ..
if %errorlevel% neq 0 (
    echo 错误: CMake配置失败
    pause
    exit /b 1
)

rem 构建项目
echo 开始构建项目...
set CPU_COUNT=%NUMBER_OF_PROCESSORS%
if "%CPU_COUNT%"=="" set CPU_COUNT=4
echo 使用 %CPU_COUNT% 个CPU核心进行并行构建

cmake --build . --config %BUILD_TYPE% -j %CPU_COUNT%
if %errorlevel% neq 0 (
    echo 错误: 项目构建失败
    pause
    exit /b 1
)

echo 构建完成！

rem 显示构建结果
echo ==========================================
echo 构建结果:
set EXECUTABLE=GlueDispensePC.exe
if "%BUILD_TYPE%"=="Debug" (
    set EXECUTABLE=%BUILD_TYPE%\GlueDispensePC.exe
) else (
    set EXECUTABLE=%BUILD_TYPE%\GlueDispensePC.exe
)

if exist "%EXECUTABLE%" (
    echo ✓ 可执行文件已生成: %EXECUTABLE%
    
    rem 显示文件信息
    for %%A in ("%EXECUTABLE%") do (
        echo   文件大小: %%~zA 字节
        echo   修改时间: %%~tA
    )
    
    echo   构建类型: %BUILD_TYPE%
    echo   构建时间: %date% %time%
) else (
    echo ✗ 未找到可执行文件: %EXECUTABLE%
    dir /b *.exe
    pause
    exit /b 1
)

echo ==========================================

rem 可选：运行测试
if "%2"=="test" (
    echo 运行测试...
    ctest --output-on-failure
    if %errorlevel% neq 0 (
        echo 警告: 测试失败
    ) else (
        echo ✓ 所有测试通过
    )
)

rem 可选：创建安装包
if "%2"=="package" (
    echo 创建安装包...
    cpack
    if %errorlevel% neq 0 (
        echo 警告: 安装包创建失败
    ) else (
        echo ✓ 安装包创建成功
        dir /b *.exe *.msi 2>nul
    )
)

if "%3"=="package" (
    echo 创建安装包...
    cpack
    if %errorlevel% neq 0 (
        echo 警告: 安装包创建失败
    ) else (
        echo ✓ 安装包创建成功
        dir /b *.exe *.msi 2>nul
    )
)

echo 构建脚本执行完成！
echo 使用方法:
echo   build.bat [Debug^|Release] [test] [package]
echo   示例: build.bat Release test package

rem 可选：运行程序
if "%2"=="run" (
    echo 启动程序...
    start "" "%EXECUTABLE%"
)

if "%3"=="run" (
    echo 启动程序...
    start "" "%EXECUTABLE%"
)

pause 