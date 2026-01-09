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
    float currentVoltage = 0.0f;      // 实时电压 (V)
    float maxVoltage = 0.0f;          // 最大电压 (V)
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

struct MemoryModuleInfo {
    std::string name = "Unknown";      // 内存条名称/位置
    float capacity = 0.0f;             // 容量 (GB)
    unsigned int speed = 0;            // 速度 (MHz)
    std::string type = "Unknown";      // 类型 (DDR3/DDR4/DDR5)
    unsigned int channel = 0;          // 内存通道编号 (0, 1, 2, ...)
    
    // 带宽信息
    float maxBandwidth = 0.0f;         // 最大带宽 (GB/s)
    float realTimeBandwidth = 0.0f;    // 实时带宽 (GB/s)
    float utilization = 0.0f;          // 利用率 (%)
    
    // 历史数据
    std::vector<float> bandwidthHistory;
    static constexpr size_t MAX_HISTORY = 120;
};

struct MemoryInfo {
    float used = 0.0f;                 // 已使用内存 (GB)
    float total = 0.0f;                // 总内存 (GB)
    float percent = 0.0f;              // 使用百分比
    float available = 0.0f;            // 可用内存 (GB)
    
    // 多个内存条信息
    std::vector<MemoryModuleInfo> modules;
    
    // 历史数据
    std::vector<float> percentHistory;
    static constexpr size_t MAX_HISTORY = 120;
};

struct DiskInfo {
    std::string name = "Unknown";      // 磁盘名称 (C:, D:, 等)
    std::string model = "Unknown";     // 磁盘型号
    std::string type = "Unknown";      // 磁盘类型 (HDD/SSD/NVMe)
    float totalSize = 0.0f;            // 总容量 (GB)
    
    // IO 带宽信息
    float maxReadBandwidth = 0.0f;      // 最大读取带宽 (GB/s)
    float maxWriteBandwidth = 0.0f;    // 最大写入带宽 (GB/s)
    float realTimeReadBandwidth = 0.0f; // 实时读取带宽 (GB/s)
    float realTimeWriteBandwidth = 0.0f; // 实时写入带宽 (GB/s)
    float readUtilization = 0.0f;     // 读取利用率 (%)
    float writeUtilization = 0.0f;     // 写入利用率 (%)
    
    // 历史数据
    std::vector<float> readBandwidthHistory;
    std::vector<float> writeBandwidthHistory;
    static constexpr size_t MAX_HISTORY = 120;
};

struct SystemBandwidthInfo {
    float totalSystemBandwidth = 0.0f;  // 总系统带宽 (GB/s) - 主板总带宽
    
    // PCIe 总线带宽（CPU 与 GPU 的桥梁）
    float pcieMaxBandwidth = 0.0f;      // PCIe最大带宽 (GB/s)
    float pcieRealTimeBandwidth = 0.0f; // PCIe实时带宽 (GB/s)
    float pcieUtilization = 0.0f;       // PCIe利用率 (%)
    
    // 内存带宽（CPU 与 RAM 的桥梁）
    float memoryMaxBandwidth = 0.0f;     // 内存最大带宽 (GB/s)
    float memoryRealTimeBandwidth = 0.0f; // 内存实时带宽 (GB/s)
    float memoryUtilization = 0.0f;     // 内存利用率 (%)
    
    // 存储/IO 带宽（硬盘与内存的桥梁）
    float storageMaxBandwidth = 0.0f;    // 存储最大带宽 (GB/s) - 估算值
    float storageRealTimeBandwidth = 0.0f; // 存储实时带宽 (GB/s) - 估算值
    float storageUtilization = 0.0f;    // 存储利用率 (%)
    
    // 显存带宽（GPU 内部带宽 - 极重要）
    float vramMaxBandwidth = 0.0f;      // 显存最大带宽 (GB/s)
    float vramRealTimeBandwidth = 0.0f; // 显存实时带宽 (GB/s)
    float vramUtilization = 0.0f;      // 显存利用率 (%)
    
    // 兼容性字段（保留）
    float cpuBandwidth = 0.0f;         // CPU带宽 (GB/s) - 已弃用，使用memoryBandwidth
    float memoryBandwidth = 0.0f;      // 内存带宽 (GB/s) - 已弃用，使用memoryMaxBandwidth
    float pcieTotalBandwidth = 0.0f;   // 总PCIe带宽 (GB/s) - 已弃用，使用pcieMaxBandwidth
    std::string memoryType = "Unknown"; // 内存类型
    unsigned int memorySpeed = 0;      // 内存速度 (MHz)
    
    // 历史数据
    std::vector<float> totalBandwidthHistory;
    std::vector<float> cpuBandwidthHistory;
    std::vector<float> memoryBandwidthHistory;
    std::vector<float> pcieBandwidthHistory;
    std::vector<float> storageBandwidthHistory;
    std::vector<float> vramBandwidthHistory;
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
    size_t GetMemoryModuleCount() const { return memoryInfo_.modules.size(); }
    size_t GetDiskCount() const { return diskInfos_.size(); }
    const DiskInfo& GetDiskInfo(size_t index) const;

private:
    bool InitializeNVML();
    void UpdateGPU();
    void UpdateCPU();
    void UpdateMemory();
    void UpdateSystemBandwidth();
    void UpdateMemoryModules();
    void UpdateDisks();

    std::vector<GPUInfo> gpuInfos_;
    CPUInfo cpuInfo_;
    MemoryInfo memoryInfo_;
    SystemBandwidthInfo systemBandwidthInfo_;
    std::vector<DiskInfo> diskInfos_;

    bool nvmlInitialized_ = false;
    PDH_HQUERY cpuQuery_ = nullptr;
    PDH_HCOUNTER cpuCounter_ = nullptr;
    PDH_HQUERY diskQuery_ = nullptr;
    SYSTEM_INFO sysInfo_;
    
    // CPU性能计数器
    ULARGE_INTEGER lastCPU_, lastSysCPU_, lastUserCPU_;
    int numProcessors_ = 0;
    HANDLE self_ = nullptr;
    
    // 磁盘性能计数器
    struct DiskCounter {
        PDH_HCOUNTER readCounter = nullptr;
        PDH_HCOUNTER writeCounter = nullptr;
        std::string diskName;
    };
    std::vector<DiskCounter> diskCounters_;
};

