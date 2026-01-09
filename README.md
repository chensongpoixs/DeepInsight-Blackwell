# DeepInsight Blackwell - 硬件资源监控工具

一个专业的 C++ 图形化硬件监控应用程序，专为深度学习训练优化设计，实时监控 GPU、CPU、内存等硬件资源使用情况，并提供智能诊断建议。

## 📋 目录

- [功能特性](#功能特性)
- [核心判断指标](#核心判断指标)
- [快速开始](#快速开始)
- [构建说明](#构建说明)
- [界面说明](#界面说明)
- [技术栈](#技术栈)
- [系统要求](#系统要求)
- [故障排除](#故障排除)

## ✨ 功能特性

### 🎮 GPU 监控

#### 圆形图表显示（顶部）
- **第一行（2个圆形）**：
  - GPU 利用率（%）
  - 显存占用（%）
- **第二行（3个圆形）**：
  - CPU 利用率（%）
  - 内存使用（%）
  - CPU 同步到 GPU 等待时长（%）

#### 详细信息表格
- **计算性能**：GPU 利用率（%），带状态图标和历史图表
- **显存占用**：显存使用量（MB），使用百分比，历史图表
- **温度**：GPU 温度（°C），带颜色警告和历史图表
- **PCIe 带宽**：
  - 理论带宽（GB/s）
  - 实时带宽（GB/s）
  - 利用率百分比（%）
  - PCIe 链路信息（x16 @ PCIe 4.0）
  - 接收/发送吞吐量（MB/s）
  - 历史图表
- **功率**：
  - 最大功率（W）
  - 实时功率（W）
  - 功率百分比（%）
- **PCIe 吞吐量**：接收/发送吞吐量（MB/s）
- **传输等待**：CPU→GPU 数据传输等待时间（ms），带历史图表
- **GPU 名称**：显示 GPU 型号

### ⚡ CPU 监控

- **CPU 利用率**：总利用率（%），带状态图标
- **状态提示**：自动检测 CPU 瓶颈并提供建议
- **历史图表**：CPU 利用率历史趋势

### 💾 内存监控

- **内存使用**：已使用/总内存（GB），使用百分比
- **可用内存**：可用内存量（GB）
- **状态提示**：内存使用警告（Swap 风险）
- **历史图表**：内存使用历史趋势

### 🌐 主机带宽模块

#### 圆形图表显示（3个圆形）
- **主板总带宽**：总系统带宽（GB/s），百分比显示
- **CPU 带宽**：CPU 带宽（GB/s），百分比显示
- **内存带宽**：内存带宽（GB/s），百分比显示

#### 详细信息
- **总系统带宽**：主板总带宽（GB/s）
- **内存带宽信息**：
  - 带宽（GB/s）
  - 类型（DDR4/DDR5）
  - 速度（MHz）
- **PCIe 总带宽**：所有 GPU 的 PCIe 带宽总和（GB/s）
- **历史图表**：总系统带宽历史趋势

### 💡 智能诊断建议

根据 2026 深度学习标准自动诊断资源使用情况：

- **GPU 利用率检测**：
  - 低于 70% 且 CPU 满载 → 建议增加 `num_workers`
  - 低于 70% 且显存未满 → 建议增大 `batch_size`
  - 显存已满但利用率低 → 可能是复杂计算或数据拷贝问题

- **CPU 瓶颈检测**：
  - CPU 利用率 > 90% → 警告：CPU 预处理压力大，建议增加 `num_workers`

- **内存风险检测**：
  - 内存使用 > 95% → 警告：可能出现 Swap，导致性能断崖式下跌

- **通用建议**：
  - GPU 利用率应保持在 85% - 100%
  - 显存占用应保持在 80% - 95%
  - CPU 利用率应在 30% - 70%
  - 内存使用应保持充足余量（< 95%）

## 📊 核心判断指标

| 指标 | 理想状态 | 瓶颈表现 | 解决建议 |
|------|---------|---------|---------|
| GPU 利用率 (Util) | 85% - 100% | 波动剧烈或长期低于 70% | 可能是数据读取慢（CPU/IO 瓶颈）或 Batch Size 太小 |
| 显存占用 (Memory) | 80% - 95% | 极低或溢出 (OOM) | 调大 batch_size 直到显存接近饱和，以提升吞吐量 |
| CPU 利用率 | 30% - 70% | 100% (满载) | CPU 预处理太慢，拖累了 GPU。增加 num_workers |
| 内存 (RAM) | 保持余量 | 接近 100% | 发生虚拟内存交换（Swap），会导致整体速度断崖式下跌 |

### 诊断逻辑

1. **显存没用满？**
   - 如果显存占用率低于 50%，说明你的 batch_size 设小了。增大它，直到显存占用达到 90% 左右。

2. **GPU 利用率很低但显存占满了？**
   - 这说明 GPU 虽然"存"了数据，但并没在"干活"。常见于复杂的循环（如自定义 Loss）、频繁的显存/内存数据拷贝、或小尺寸数据的频繁计算。

3. **CPU 利用率 100% 且 GPU 利用率极低？**
   - 这是典型的"数据生产速度跟不上消费速度"。请检查 DataLoader 是否开启了多线程（如 num_workers=4 或更高），并确认数据是否在 SSD 上。

### 总结建议

**先看 GPU Util，如果没到 90% 且 CPU 也没满，就无脑加 Batch Size；如果 CPU 满了但 GPU 没满，就加 num_workers。**

## 🚀 快速开始

详细快速开始指南请参考 [QUICKSTART.md](QUICKSTART.md)

### 1. 克隆项目

```bash
git clone <repository-url>
cd DeepInsight-Blackwell
```

### 2. 下载依赖

**Windows (PowerShell):**
```powershell
.\download_dependencies.ps1
```

**Linux/Mac:**
```bash
chmod +x download_dependencies.sh
./download_dependencies.sh
```

### 3. 构建项目

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

### 4. 运行

```bash
# Windows
bin\Release\DeepInsightBlackwell.exe

# Linux
./bin/DeepInsightBlackwell
```

## 🔧 构建说明

详细构建说明请参考 [BUILD.md](BUILD.md)

### 依赖项

- **ImGui** - 即时模式图形界面库
- **GLFW** - 跨平台窗口和输入管理
- **NVML** - NVIDIA GPU 管理库（通过 CUDA 环境变量查找）
- **OpenGL 3.3+** - 图形渲染
- **Windows PDH** - Windows 性能数据帮助库
- **psapi** - Windows 进程和系统信息库

### CMake 配置

项目使用 CMake 构建系统，会自动：
- 查找 CUDA 环境变量中的 NVML 库和头文件
- 配置 ImGui 和 GLFW
- 设置 OpenGL 链接
- 配置 Windows 特定库（PDH、psapi）

## 🖥️ 界面说明

### 主窗口布局

1. **顶部圆形指标区域**
   - 第一行：GPU 利用率、显存占用（2个圆形）
   - 第二行：CPU 利用率、内存使用、CPU 同步到 GPU 等待时长（3个圆形）
   - 自适应窗口大小，自动调整圆形大小和布局

2. **GPU 详细信息面板**
   - 表格形式显示所有 GPU 指标
   - 包含历史趋势图表
   - 实时更新数据

3. **CPU 和内存信息面板**
   - 并排显示 CPU 和内存详细信息
   - 状态提示和优化建议
   - 历史趋势图表

4. **主机带宽模块**
   - 顶部：3个圆形图表（主板总带宽、CPU 带宽、内存带宽）
   - 详细信息表格
   - 历史趋势图表

5. **诊断建议面板**
   - 自动检测资源瓶颈
   - 提供优化建议
   - 显示理想状态标准

### 视觉特性

- **圆形进度条**：直观显示百分比数据
- **颜色编码**：
  - 🟢 绿色：正常状态
  - 🟠 橙色：需要注意
  - 🔴 红色：警告状态
- **历史图表**：迷你趋势图显示数据变化
- **自适应布局**：根据窗口大小自动调整
- **中文支持**：自动加载 Windows 系统字体

## 🛠️ 技术栈

- **C++17** - 编程语言
- **ImGui** - 即时模式图形界面库
- **GLFW** - 跨平台窗口和输入管理
- **NVML** - NVIDIA GPU 管理库
- **Windows PDH** - Windows 性能数据帮助库
- **OpenGL 3.3+** - 图形渲染
- **CMake** - 构建系统

## 💻 系统要求

- **操作系统**: Windows 10/11
- **GPU**: NVIDIA GPU（用于 GPU 监控）
- **驱动程序**: NVIDIA 驱动程序（包含 NVML 支持）
- **CUDA**: 已安装 CUDA（用于查找 NVML 库）
- **OpenGL**: 3.3 或更高版本
- **编译器**: 
  - Visual Studio 2019+ (Windows)
  - MinGW-w64 (Windows)
- **CMake**: 3.15 或更高版本

## ⚠️ 注意事项

1. **GPU 监控**: 需要 NVIDIA GPU 和 NVIDIA 驱动程序。如果没有 NVIDIA GPU，GPU 监控功能将不可用。

2. **CUDA 环境**: NVML 库通过 CUDA 环境变量（`CUDA_PATH`）自动查找。确保 CUDA 已正确安装并配置环境变量。

3. **管理员权限**: 某些硬件监控功能可能需要管理员权限，建议以管理员身份运行以获得完整功能。

4. **性能影响**: 应用程序以约 60 FPS 更新，对系统性能影响极小。

5. **电压数据**: 由于 NVML API 不提供直接获取 GPU 电压的函数，电压数据基于功耗进行估算，仅供参考。

## 🔍 故障排除

### 常见问题

1. **NVML 初始化失败**
   - 确保已安装 NVIDIA 驱动程序
   - 检查 CUDA 环境变量是否正确设置
   - 确保系统中有 NVIDIA GPU

2. **GLFW 初始化失败**
   - 确保系统支持 OpenGL 3.3+
   - 更新显卡驱动程序

3. **链接错误（NVML）**
   - 检查 `CUDA_PATH` 环境变量
   - 确保 NVML 库文件存在于 CUDA 安装目录
   - 查看 [BUILD.md](BUILD.md) 中的详细说明

4. **界面显示乱码**
   - 应用程序会自动加载 Windows 系统字体
   - 如果仍有问题，确保系统安装了中文字体（如微软雅黑）

5. **圆形图表不显示**
   - 检查窗口大小是否足够
   - 确保 GPU 数据可用

### 获取帮助

如果遇到其他问题，请：
1. 查看 [BUILD.md](BUILD.md) 中的详细构建说明
2. 查看 [QUICKSTART.md](QUICKSTART.md) 中的快速开始指南
3. 检查控制台输出的错误信息

## 📝 项目结构

```
DeepInsight-Blackwell/
├── src/
│   ├── main.cpp              # 主程序入口
│   ├── HardwareMonitor.h     # 硬件监控类定义
│   ├── HardwareMonitor.cpp   # 硬件监控实现
│   ├── ImGuiApp.h            # ImGui 应用类定义
│   └── ImGuiApp.cpp          # ImGui 应用实现
├── third_party/
│   ├── imgui/                # ImGui 库
│   └── glfw/                 # GLFW 库
├── CMakeLists.txt            # CMake 构建配置
├── BUILD.md                  # 详细构建说明
├── QUICKSTART.md             # 快速开始指南
├── download_dependencies.ps1 # Windows 依赖下载脚本
└── download_dependencies.sh  # Linux/Mac 依赖下载脚本
```

## 📄 许可证

请查看项目根目录中的许可证文件。

## 🙏 致谢

- [ImGui](https://github.com/ocornut/imgui) - 优秀的即时模式 GUI 库
- [GLFW](https://github.com/glfw/glfw) - 跨平台窗口管理库
- [NVIDIA NVML](https://developer.nvidia.com/nvidia-management-library-nvml) - GPU 管理库

---

**DeepInsight Blackwell** - 让硬件监控更直观、更智能 🚀
