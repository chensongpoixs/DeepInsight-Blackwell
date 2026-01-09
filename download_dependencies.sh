#!/bin/bash
# Bash 脚本：下载项目依赖库

echo "开始下载项目依赖库..."

# 创建第三方库目录
if [ ! -d "third_party" ]; then
    mkdir -p third_party
    echo "创建目录: third_party"
fi

# 检查 Git 是否安装
if ! command -v git &> /dev/null; then
    echo "错误: 未找到 Git。请先安装 Git。"
    exit 1
fi

# 下载 ImGui
if [ -d "third_party/imgui" ]; then
    echo "ImGui 已存在，跳过下载"
else
    echo "正在下载 ImGui..."
    git clone https://github.com/ocornut/imgui.git third_party/imgui
    if [ $? -eq 0 ]; then
        echo "ImGui 下载成功！"
    else
        echo "ImGui 下载失败！"
        exit 1
    fi
fi

# 下载 GLFW
if [ -d "third_party/glfw" ]; then
    echo "GLFW 已存在，跳过下载"
else
    echo "正在下载 GLFW..."
    git clone https://github.com/glfw/glfw.git third_party/glfw
    if [ $? -eq 0 ]; then
        echo "GLFW 下载成功！"
    else
        echo "GLFW 下载失败！"
        exit 1
    fi
fi

echo ""
echo "所有依赖库下载完成！"
echo "接下来可以运行 CMake 构建项目："
echo "  mkdir build"
echo "  cd build"
echo "  cmake .."
echo "  cmake --build . --config Release"

