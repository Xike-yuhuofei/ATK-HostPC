#!/bin/bash

# 工业点胶设备上位机 - 清理脚本
echo "=========================================="
echo "工业点胶设备上位机 - 清理构建文件"
echo "=========================================="

# 进入项目目录
cd "$(dirname "$0")"

echo "正在清理构建文件..."

# 删除所有构建目录
rm -rf build build_* cmake-build-*

# 删除Qt自动生成的文件
find . -name "*_autogen" -type d -exec rm -rf {} + 2>/dev/null
find . -name "moc_*.cpp" -delete 2>/dev/null
find . -name "moc_*.h" -delete 2>/dev/null
find . -name "qrc_*.cpp" -delete 2>/dev/null
find . -name "ui_*.h" -delete 2>/dev/null

# 删除编译产物
find . -name "*.o" -delete 2>/dev/null
find . -name "*.obj" -delete 2>/dev/null
find . -name "*.gch" -delete 2>/dev/null
find . -name "*.pch" -delete 2>/dev/null

# 删除CMake缓存文件
find . -name "CMakeCache.txt" -delete 2>/dev/null
find . -name "cmake_install.cmake" -delete 2>/dev/null
find . -name "Makefile" -delete 2>/dev/null

# 删除临时文件
find . -name "*.tmp" -delete 2>/dev/null
find . -name "*.temp" -delete 2>/dev/null
find . -name "*.bak" -delete 2>/dev/null
find . -name "*~" -delete 2>/dev/null

# 删除系统生成的文件
find . -name ".DS_Store" -delete 2>/dev/null
find . -name "Thumbs.db" -delete 2>/dev/null

# 删除日志文件（可选）
if [ "$1" = "--logs" ]; then
    echo "清理日志文件..."
    find . -name "*.log" -delete 2>/dev/null
    rm -rf logs/ data/logs/
fi

# 删除运行时数据（可选）
if [ "$1" = "--all" ]; then
    echo "清理所有运行时数据..."
    rm -rf data/ logs/
    find . -name "*.db" -delete 2>/dev/null
    find . -name "*.sqlite*" -delete 2>/dev/null
fi

echo "✓ 清理完成！"
echo ""
echo "清理级别:"
echo "  ./clean.sh       - 清理构建文件和临时文件"
echo "  ./clean.sh --logs - 同时清理日志文件"
echo "  ./clean.sh --all  - 清理所有生成的文件（包括数据库）"
echo ""
echo "=========================================="