#pragma once

#include <vector>
#include <string>
#include <memory>
#include <windows.h>
#include <pdh.h>

#ifdef __cplusplus
extern "C" {
#endif
#include <nvml.h>
#ifdef __cplusplus
}
#endif

struct GPUInfo {
    float utilization = 0.0f;          // GPU利用率 (%)
    float memoryUsed = 0.0f;           // 显存使用 (MB)
    float memoryTotal = 0.0f;          // 显存总量 (MB)
    float memoryPercent = 0.0f;        // 显存使用百分比
    float temperature = 0.0f;          // 温度 (°C)
    std::string name = "Unknown";      // GPU名称
    bool available = false;
    
    // GPU-Z Sessions 风格的数据
    unsigned int gpuClock = 0;         // GPU时钟频率 (MHz)
    unsigned int memoryClock = 0;      // 显存时钟频率 (MHz)
    unsigned int fanSpeed = 0;         // 风扇转速 (%)
    unsigned int powerUsage = 0;       // 功耗 (W) - 如果可用
    float memoryControllerLoad = 0.0f; // 显存控制器负载 (%)
    float videoEngineLoad = 0.0f;     // 视频引擎负载 (%) - 如果可用
    
    // 电压信息
    float currentVoltage = 0.0f;      // 实时电压 (mV)
    float maxVoltage = 0.0f;          // 最大电压 (mV)
    float voltagePercent = 0.0f;      // 电压百分比 (%)
    
    // PCIe 带宽信息
    unsigned int pcieLinkWidth = 0;     // PCIe 链路宽度 (lanes)
    unsigned int pcieLinkSpeed = 0;     // PCIe 链路速度 (GT/s, Generation * 2.5)
    float pcieBandwidth = 0.0f;        // PCIe 带宽 (GB/s)
    float pcieRxThroughput = 0.0f;     // PCIe 接收吞吐量 (MB/s)
    float pcieTxThroughput = 0.0f;     // PCIe 发送吞吐量 (MB/s)
    
    // 数据传输等待时间（毫秒）
    float dataTransferWaitTime = 0.0f; // CPU到GPU数据传输等待时间
    
    // 历史数据用于绘制图表
    std::vector<float> utilizationHistory;
    std::vector<float> memoryHistory;
    std::vector<float> temperatureHistory;
    std::vector<float> pcieRxHistory;
    std::vector<float> pcieTxHistory;
    std::vector<float> transferWaitHistory;
    static constexpr size_t MAX_HISTORY = 120;  // 保存2分钟的数据（1秒更新）
};

struct CPUInfo {
    float utilization = 0.0f;          // CPU利用率 (%)
    std::vector<float> coreUtilization; // 每个核心的利用率
    float temperature = 0.0f;          // CPU温度（如果可获取）
    
    // 历史数据
    std::vector<float> utilizationHistory;
    static constexpr size_t MAX_HISTORY = 120;
};

struct MemoryInfo {
    float used = 0.0f;                 // 已使用内存 (GB)
    float total = 0.0f;                // 总内存 (GB)
    float percent = 0.0f;              // 使用百分比
    float available = 0.0f;            // 可用内存 (GB)
    
    // 历史数据
    std::vector<float> percentHistory;
    static constexpr size_t MAX_HISTORY = 120;
};

struct SystemBandwidthInfo {
    float totalSystemBandwidth = 0.0f;  // 总系统带宽 (GB/s) - 主板总带宽
    float cpuBandwidth = 0.0f;         // CPU带宽 (GB/s)
    float memoryBandwidth = 0.0f;      // 内存带宽 (GB/s)
    float pcieTotalBandwidth = 0.0f;   // 总PCIe带宽 (GB/s)
    std::string memoryType = "Unknown"; // 内存类型
    unsigned int memorySpeed = 0;      // 内存速度 (MHz)
    
    // 历史数据
    std::vector<float> totalBandwidthHistory;
    std::vector<float> cpuBandwidthHistory;
    std::vector<float> memoryBandwidthHistory;
    static constexpr size_t MAX_HISTORY = 120;
};

class HardwareMonitor {
public:
    HardwareMonitor();
    ~HardwareMonitor();

    bool Initialize();
    void Update();
    void Shutdown();

    // 获取信息
    const GPUInfo& GetGPUInfo(int index = 0) const;
    const CPUInfo& GetCPUInfo() const { return cpuInfo_; }
    const MemoryInfo& GetMemoryInfo() const { return memoryInfo_; }
    const SystemBandwidthInfo& GetSystemBandwidthInfo() const { return systemBandwidthInfo_; }
    size_t GetGPUCount() const { return gpuInfos_.size(); }

private:
    bool InitializeNVML();
    void UpdateGPU();
    void UpdateCPU();
    void UpdateMemory();
    void UpdateSystemBandwidth();

    std::vector<GPUInfo> gpuInfos_;
    CPUInfo cpuInfo_;
    MemoryInfo memoryInfo_;
    SystemBandwidthInfo systemBandwidthInfo_;

    bool nvmlInitialized_ = false;
    PDH_HQUERY cpuQuery_ = nullptr;
    PDH_HCOUNTER cpuCounter_ = nullptr;
    SYSTEM_INFO sysInfo_;
    
    // CPU性能计数器
    ULARGE_INTEGER lastCPU_, lastSysCPU_, lastUserCPU_;
    int numProcessors_ = 0;
    HANDLE self_ = nullptr;
};

