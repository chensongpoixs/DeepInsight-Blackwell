# 快速启动指南

## 前置要求

- Windows 10/11
- CMake 3.15 或更高版本
- Git（用于下载依赖库）
- Visual Studio 2019+ 或 MinGW-w64（用于编译）
- NVIDIA GPU 和驱动程序（用于 GPU 监控功能）

## 快速构建步骤

### 1. 下载依赖库

**Windows (PowerShell):**
```powershell
.\download_dependencies.ps1
```

**Linux/Mac (Bash):**
```bash
chmod +x download_dependencies.sh
./download_dependencies.sh
```

**手动下载:**
```bash
mkdir -p third_party
cd third_party
git clone https://github.com/ocornut/imgui.git
git clone https://github.com/glfw/glfw.git
```

### 2. 构建项目

**使用 Visual Studio (推荐):**
```powershell
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022" -A x64
cmake --build . --config Release
```

**使用 MinGW:**
```bash
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

### 3. 运行程序

**Windows:**
```powershell
.\build\bin\Release\DeepInsight-Blackwell.exe
```

## 验证安装

如果一切正常，你应该看到一个图形窗口，显示：
- GPU 信息（如果有 NVIDIA GPU）
- CPU 利用率
- 内存使用情况
- 实时图表
- 诊断建议

## 常见问题

### 问题1: NVML 初始化失败

**症状**: 程序运行但 GPU 信息显示"不可用"

**解决方法**:
- 确保已安装 NVIDIA 驱动程序
- 确保系统中有 NVIDIA GPU
- 如果没有 NVIDIA GPU，程序仍可运行，但只能监控 CPU 和内存

### 问题2: GLFW 初始化失败

**症状**: 程序无法启动，提示 GLFW 错误

**解决方法**:
- 确保系统已安装 OpenGL 3.3+ 驱动程序
- 更新显卡驱动程序
- 在支持 OpenGL 的系统上运行

### 问题3: CMake 找不到依赖库

**症状**: CMake 配置失败，提示找不到 imgui 或 glfw

**解决方法**:
- 确保已运行 `download_dependencies.ps1` 或手动下载依赖
- 检查 `third_party/imgui` 和 `third_party/glfw` 目录是否存在
- 确保目录结构正确

### 问题4: 编译错误

**症状**: 编译时出现链接错误或找不到头文件

**解决方法**:
- 确保所有依赖库都已正确下载
- 检查 CMake 输出，确认找到了所有库
- 尝试清理构建目录并重新构建：
  ```bash
  rm -rf build
  mkdir build
  cd build
  cmake ..
  cmake --build . --config Release
  ```

## 下一步

- 查看 [README.md](README.md) 了解项目详细信息
- 查看 [BUILD.md](BUILD.md) 了解详细的构建说明
- 根据界面提示优化你的深度学习训练配置

## 功能说明

程序会根据 README.md 中的标准自动诊断硬件资源使用情况：

- **GPU 利用率 < 70% 且 CPU > 90%**: 提示 CPU 瓶颈，建议增加 num_workers
- **GPU 利用率 < 70% 且显存 < 50%**: 提示计算未饱和，建议增大 batch_size
- **显存 > 95%**: 警告可能 OOM
- **内存 > 95%**: 警告可能出现 Swap，导致性能下降

