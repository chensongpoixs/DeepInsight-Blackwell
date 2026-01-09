# DeepInsight Blackwell - 构建说明

## 依赖项

本项目需要以下依赖库：

1. **ImGui** - 图形界面库
2. **GLFW** - 窗口管理库
3. **NVIDIA Management Library (NVML)** - GPU监控库
4. **Windows Performance Data Helper (PDH)** - CPU监控（Windows自带）
5. **OpenGL** - 图形渲染库

## 准备工作

### 1. 安装 CMake (3.15 或更高版本)

下载并安装 CMake: https://cmake.org/download/

### 2. 下载第三方库

#### 方法一：使用 Git Submodules（推荐）

```bash
# 在项目根目录执行
mkdir -p third_party
cd third_party

# 下载 ImGui
git clone https://github.com/ocornut/imgui.git

# 下载 GLFW
git clone https://github.com/glfw/glfw.git
```

#### 方法二：手动下载

1. **ImGui**: 从 https://github.com/ocornut/imgui/releases 下载最新版本
   - 解压到 `third_party/imgui/`

2. **GLFW**: 从 https://github.com/glfw/glfw/releases 下载最新版本
   - 解压到 `third_party/glfw/`

### 3. 准备 ImGui 绑定文件

ImGui 需要 GLFW 和 OpenGL3 的绑定文件。这些文件通常已经包含在 ImGui 仓库中：
- `third_party/imgui/backends/imgui_impl_glfw.cpp`
- `third_party/imgui/backends/imgui_impl_glfw.h`
- `third_party/imgui/backends/imgui_impl_opengl3.cpp`
- `third_party/imgui/backends/imgui_impl_opengl3.h`

如果使用新版本的 ImGui，需要更新 CMakeLists.txt 以包含这些文件。

### 4. 安装 NVIDIA CUDA Toolkit（用于 NVML）

**重要**: NVML 库已包含在 CUDA Toolkit 中。项目会自动通过 CUDA 环境变量查找 NVML：

1. **推荐方式**: 设置 `CUDA_PATH` 环境变量
   - 如果已安装 CUDA Toolkit，`CUDA_PATH` 环境变量通常会自动设置
   - 例如: `CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.0`

2. **自动查找**: 如果未设置环境变量，CMake 会自动在常见 CUDA 安装路径中查找：
   - `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.0`
   - `C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v11.x`
   - 等等...

3. **手动指定**: 如果以上方法都失败，CMake 会尝试在系统路径中查找 NVML 库

**安装 CUDA Toolkit**: https://developer.nvidia.com/cuda-downloads

## 构建步骤

### Windows (Visual Studio)

```bash
# 在项目根目录创建构建目录
mkdir build
cd build

# 生成 Visual Studio 解决方案
cmake .. -G "Visual Studio 17 2022" -A x64

# 构建项目
cmake --build . --config Release
```

### Windows (MinGW)

```bash
mkdir build
cd build

# 使用 MinGW Makefiles
cmake .. -G "MinGW Makefiles"

# 构建
cmake --build .
```

### 使用命令行编译

如果使用 Visual Studio 命令行工具：

```bash
# 打开 Developer Command Prompt for VS
cd <项目目录>
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

## 运行

构建完成后，可执行文件位于：
- `build/bin/Release/DeepInsight-Blackwell.exe` (Release 模式)
- `build/bin/Debug/DeepInsight-Blackwell.exe` (Debug 模式)

## 故障排除

### 1. 找不到 NVML

如果链接时出现 `nvml.lib` 或 `nvml.h` 未找到的错误：

**解决方案**:
1. **确保已安装 CUDA Toolkit**: NVML 库和头文件都包含在 CUDA Toolkit 中
   - 下载并安装: https://developer.nvidia.com/cuda-downloads
   - 安装后通常会自动设置 `CUDA_PATH` 环境变量

2. **检查 CUDA_PATH 环境变量**:
   ```powershell
   echo $env:CUDA_PATH
   ```
   如果未设置，可以在系统环境变量中手动设置，指向 CUDA 安装目录
   - 例如: `CUDA_PATH=C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.0`

3. **验证 NVML 文件存在**:
   - 头文件应位于: `<CUDA_PATH>/include/nvml.h`
   - 库文件应位于: `<CUDA_PATH>/lib/x64/nvml.lib` (Windows x64)

4. **如果 CMake 配置失败**:
   - 查看 CMake 输出消息，确认是否找到了 CUDA 路径
   - CMake 会自动尝试在常见安装路径中查找（v10.0 到 v12.0）

5. **手动指定 CUDA 路径** (如果自动查找失败):
   ```powershell
   cmake .. -DCUDA_PATH="C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.0"
   ```

6. **检查 CUDA 安装**: 确保 CUDA Toolkit 正确安装，并且包含 NVML 组件

### 2. GLFW 初始化失败

确保系统已安装 OpenGL 3.3 或更高版本的驱动程序。

### 3. ImGui 编译错误

确保已正确下载 ImGui 并包含所有必要的文件，特别是 `backends` 目录中的文件。

### 4. 中文字体显示问题

默认情况下，ImGui 使用系统默认字体。如果需要显示中文：
1. 下载中文字体（如 SimHei.ttf）
2. 取消 `ImGuiApp.cpp` 中字体加载代码的注释
3. 确保字体文件路径正确

## 开发环境推荐

- **IDE**: Visual Studio 2022 或 CLion
- **编译器**: MSVC 或 MinGW-w64
- **CMake**: 3.15 或更高版本

