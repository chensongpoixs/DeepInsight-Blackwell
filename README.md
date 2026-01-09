判断深度学习训练是否“吃满”了硬件资源，主要观察 GPU 利用率、显存占用、CPU 负载以及 IO 等待时间这四个核心指标。
以下是为你准备的硬件资源使用情况诊断方案，包含判断标准、监控代码及反馈分析：
一、 核心判断指标 (2026 深度学习标准)
指标	理想状态	瓶颈表现	解决建议
GPU 利用率 (Util)	85% - 100%	波动剧烈或长期低于 70%	可能是数据读取慢（CPU/IO 瓶颈）或 Batch Size 太小。
显存占用 (Memory)	80% - 95%	极低或溢出 (OOM)	调大 batch_size 直到显存接近饱和，以提升吞吐量。
CPU 利用率	30% - 70%	100% (满载)	CPU 预处理太慢，拖累了 GPU。增加 num_workers。
内存 (RAM)	保持余量	接近 100%	发生虚拟内存交换（Swap），会导致整体速度断崖式下跌。
二、 硬件使用情况反馈监控代码
你可以将此脚本作为独立进程运行，或集成到训练代码的每个 Epoch 结束时。
python
import psutil
import pynvml # 需要安装: pip install nvidia-ml-py
import time

def get_hardware_feedback():
    pynvml.nvmlInit()
    # 1. GPU 监控
    handle = pynvml.nvmlDeviceGetHandleByIndex(0) # 默认 0 号显卡
    mem_info = pynvml.nvmlDeviceGetMemoryInfo(handle)
    gpu_util = pynvml.nvmlDeviceGetUtilizationRates(handle).gpu
    
    # 2. CPU & 内存监控
    cpu_usage = psutil.cpu_percent(interval=1)
    ram_usage = psutil.virtual_memory().percent

    feedback = f"""
    ======= 2026 训练硬件反馈报告 =======
    [GPU 利用率]: {gpu_util}%  {'✅ 充分利用' if gpu_util > 85 else '❌ 负载不足'}
    [显存占用]: {mem_info.used / 1024**2:.0f}MB / {mem_info.total / 1024**2:.0f}MB ({mem_info.used/mem_info.total*100:.1f}%)
    [CPU 利用率]: {cpu_usage}% {'⚠️ 预处理压力大' if cpu_usage > 90 else '✅ 正常'}
    [系统内存]: {ram_usage}% {'🚨 极高风险' if ram_usage > 95 else '✅ 正常'}
    ====================================
    """
    
    # 反馈逻辑
    if gpu_util < 70 and cpu_usage > 90:
        feedback += "\n提示：检测到【CPU 瓶颈】。GPU 在等数据，请增加 DataLoader 的 num_workers 或优化数据增强代码。"
    elif gpu_util < 70 and mem_info.used / mem_info.total < 0.5:
        feedback += "\n提示：检测到【计算未饱和】。请尝试增大 batch_size 以提升并行度。"
    
    print(feedback)
    pynvml.nvmlShutdown()

# 调用
if __name__ == "__main__":
    get_hardware_feedback()
请谨慎使用此类代码。

三、 训练代码是否“充分发挥”的诊断逻辑
显存没用满？
如果显存占用率低于 50%，说明你的 batch_size 设小了。增大它，直到显存占用达到 90% 左右。这是提升训练速度最简单的方法。
GPU 利用率（Util）很低但显存占满了？
这说明 GPU 虽然“存”了数据，但并没在“干活”。常见于复杂的循环（如自定义 Loss）、频繁的显存/内存数据拷贝、或小尺寸数据的频繁计算。
CPU 利用率 100% 且 GPU 利用率极低？
这是典型的“数据生产速度跟不上消费速度”。请检查 DataLoader 是否开启了多线程（如 num_workers=4 或更高），并确认数据是否在 SSD 上。
实时监控命令：
在 Linux/Windows 命令行输入 watch -n 1 nvidia-smi。
2026 进阶技巧：如果是 PyTorch 用户，可以使用 torch.cuda.memory_summary() 查看更详尽的分配碎片情况。 
总结建议：
先看 GPU Util，如果没到 90% 且 CPU 也没满，就无脑加 Batch Size；如果 CPU 满了但 GPU 没满，就加 num_workers。

---

## C++ 图形化实现

本项目提供了一个 C++ 图形化应用程序，用于实时监控电脑硬件资源使用情况。

### 功能特性

- **实时 GPU 监控**
  - GPU 利用率（Utilization）
  - 显存占用（Memory Usage）
  - GPU 温度
  - 历史数据图表

- **实时 CPU 监控**
  - CPU 总利用率
  - 历史数据图表

- **实时内存监控**
  - 内存使用量（GB）
  - 内存使用百分比
  - 可用内存
  - 历史数据图表

- **智能诊断建议**
  - 根据 README 中的判断标准自动诊断资源使用情况
  - 提供优化建议（如调整 batch_size、增加 num_workers 等）
  - 显示资源瓶颈警告

### 构建和运行

详细构建说明请参考 [BUILD.md](BUILD.md)

#### 快速开始

1. **克隆或下载项目**
```bash
git clone <repository-url>
cd DeepInsight-Blackwell
```

2. **下载依赖库**
```bash
# 创建第三方库目录
mkdir -p third_party
cd third_party

# 下载 ImGui
git clone https://github.com/ocornut/imgui.git

# 下载 GLFW
git clone https://github.com/glfw/glfw.git
```

3. **构建项目**
```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

4. **运行**
```bash
# Windows
bin/Release/DeepInsight-Blackwell.exe
```

### 界面预览

应用程序窗口包含以下部分：

1. **GPU 信息面板**
   - 显示每个 GPU 的详细信息
   - 实时利用率进度条（带颜色指示）
   - 显存使用情况
   - GPU 温度
   - 利用率历史图表
   - 显存使用历史图表

2. **CPU 信息面板**
   - CPU 总利用率
   - 利用率历史图表
   - 状态提示

3. **内存信息面板**
   - 内存使用情况
   - 可用内存
   - 使用率历史图表

4. **诊断建议面板**
   - 自动检测资源瓶颈
   - 提供优化建议
   - 显示理想状态标准

### 技术栈

- **C++17** - 编程语言
- **ImGui** - 即时模式图形界面库
- **GLFW** - 跨平台窗口和输入管理
- **NVML** - NVIDIA GPU 管理库
- **Windows PDH** - Windows 性能数据帮助库
- **OpenGL 3.3** - 图形渲染

### 系统要求

- **操作系统**: Windows 10/11
- **GPU**: NVIDIA GPU（用于 GPU 监控）
- **OpenGL**: 3.3 或更高版本
- **编译器**: Visual Studio 2019+ 或 MinGW-w64
- **CMake**: 3.15 或更高版本

### 注意事项

1. **GPU 监控**: 需要 NVIDIA GPU 和 NVIDIA 驱动程序。如果没有 NVIDIA GPU，GPU 监控功能将不可用。

2. **管理员权限**: 某些硬件监控功能可能需要管理员权限，建议以管理员身份运行以获得完整功能。

3. **性能影响**: 应用程序以约 60 FPS 更新，对系统性能影响极小。

### 故障排除

如果遇到问题，请查看 [BUILD.md](BUILD.md) 中的故障排除部分。

常见问题：
- **NVML 初始化失败**: 确保已安装 NVIDIA 驱动程序
- **GLFW 初始化失败**: 确保系统支持 OpenGL 3.3+
- **链接错误**: 确保所有依赖库都已正确下载和配置