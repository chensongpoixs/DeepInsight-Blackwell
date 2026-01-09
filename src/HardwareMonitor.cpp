#include "HardwareMonitor.h"
#include <pdh.h>
#include <psapi.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cerrno>
#include <nvml.h>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")

HardwareMonitor::HardwareMonitor() {
    GetSystemInfo(&sysInfo_);
    numProcessors_ = sysInfo_.dwNumberOfProcessors;
    self_ = GetCurrentProcess();
}

HardwareMonitor::~HardwareMonitor() {
    Shutdown();
}

bool HardwareMonitor::Initialize() {
    // 初始化NVML
    if (!InitializeNVML()) {
        std::cerr << "警告: NVML初始化失败，GPU监控可能不可用" << std::endl;
    }

    // 初始化CPU性能计数器
    PdhOpenQuery(NULL, NULL, &cpuQuery_);
    PdhAddCounter(cpuQuery_, "\\Processor(_Total)\\% Processor Time", NULL, &cpuCounter_);
    PdhCollectQueryData(cpuQuery_);

    // 初始化CPU时间
    FILETIME ftime, fsys, fuser;
    GetSystemTimeAsFileTime(&ftime);
    memcpy(&lastCPU_, &ftime, sizeof(FILETIME));

    GetProcessTimes(self_, &ftime, &ftime, &fsys, &fuser);
    memcpy(&lastSysCPU_, &fsys, sizeof(FILETIME));
    memcpy(&lastUserCPU_, &fuser, sizeof(FILETIME));

    return true;
}

bool HardwareMonitor::InitializeNVML() {
    nvmlReturn_t result = nvmlInit();
    if (result != NVML_SUCCESS) {
        return false;
    }
    nvmlInitialized_ = true;

    unsigned int deviceCount = 0;
    result = nvmlDeviceGetCount(&deviceCount);
    if (result != NVML_SUCCESS || deviceCount == 0) {
        return false;
    }

    gpuInfos_.resize(deviceCount);
    for (unsigned int i = 0; i < deviceCount; i++) {
        gpuInfos_[i].available = true;
        gpuInfos_[i].utilizationHistory.reserve(GPUInfo::MAX_HISTORY);
        gpuInfos_[i].memoryHistory.reserve(GPUInfo::MAX_HISTORY);
        gpuInfos_[i].temperatureHistory.reserve(GPUInfo::MAX_HISTORY);
        gpuInfos_[i].pcieRxHistory.reserve(GPUInfo::MAX_HISTORY);
        gpuInfos_[i].pcieTxHistory.reserve(GPUInfo::MAX_HISTORY);
        gpuInfos_[i].transferWaitHistory.reserve(GPUInfo::MAX_HISTORY);
    }
    
    // 初始化系统带宽信息历史数据
    systemBandwidthInfo_.totalBandwidthHistory.reserve(SystemBandwidthInfo::MAX_HISTORY);
    systemBandwidthInfo_.cpuBandwidthHistory.reserve(SystemBandwidthInfo::MAX_HISTORY);
    systemBandwidthInfo_.memoryBandwidthHistory.reserve(SystemBandwidthInfo::MAX_HISTORY);

    return true;
}

void HardwareMonitor::Update() {
    UpdateGPU();
    UpdateCPU();
    UpdateMemory();
    UpdateSystemBandwidth();
}

void HardwareMonitor::UpdateGPU() {
    if (!nvmlInitialized_) {
        return;
    }

    for (size_t i = 0; i < gpuInfos_.size(); i++) {
        GPUInfo& gpu = gpuInfos_[i];
        nvmlDevice_t device;
        
        if (nvmlDeviceGetHandleByIndex(i, &device) != NVML_SUCCESS) {
            gpu.available = false;
            continue;
        }

        gpu.available = true;

        // 获取GPU名称
        char name[NVML_DEVICE_NAME_BUFFER_SIZE];
        if (nvmlDeviceGetName(device, name, NVML_DEVICE_NAME_BUFFER_SIZE) == NVML_SUCCESS) {
            gpu.name = name;
        }

        // 获取利用率
        nvmlUtilization_t utilization;
        if (nvmlDeviceGetUtilizationRates(device, &utilization) == NVML_SUCCESS) {
            gpu.utilization = static_cast<float>(utilization.gpu);
        }

        // 获取显存信息
        nvmlMemory_t memory;
        if (nvmlDeviceGetMemoryInfo(device, &memory) == NVML_SUCCESS) {
            gpu.memoryTotal = static_cast<float>(memory.total) / (1024.0f * 1024.0f);  // MB
            gpu.memoryUsed = static_cast<float>(memory.used) / (1024.0f * 1024.0f);    // MB
            gpu.memoryPercent = (gpu.memoryUsed / gpu.memoryTotal) * 100.0f;
        }

        // 获取温度
        unsigned int temp;
        if (nvmlDeviceGetTemperature(device, NVML_TEMPERATURE_GPU, &temp) == NVML_SUCCESS) {
            gpu.temperature = static_cast<float>(temp);
        }

        // 获取 GPU-Z Sessions 风格的数据
        // GPU时钟频率
        unsigned int gpuClock;
        if (nvmlDeviceGetClockInfo(device, NVML_CLOCK_GRAPHICS, &gpuClock) == NVML_SUCCESS) {
            gpu.gpuClock = gpuClock;
        }
        
        // 显存时钟频率
        unsigned int memoryClock;
        if (nvmlDeviceGetClockInfo(device, NVML_CLOCK_MEM, &memoryClock) == NVML_SUCCESS) {
            gpu.memoryClock = memoryClock;
        }
        
        // 风扇转速
        unsigned int fanSpeed;
        if (nvmlDeviceGetFanSpeed(device, &fanSpeed) == NVML_SUCCESS) {
            gpu.fanSpeed = fanSpeed;
        }
        
        // 功耗（如果可用）
        unsigned int power;
        if (nvmlDeviceGetPowerUsage(device, &power) == NVML_SUCCESS) {
            gpu.powerUsage = power / 1000; // 转换为瓦特（NVML返回的是毫瓦）
        }
        
        // 显存控制器负载（使用显存利用率作为近似值）
        //nvmlUtilization_t utilization;
        if (nvmlDeviceGetUtilizationRates(device, &utilization) == NVML_SUCCESS) {
            gpu.memoryControllerLoad = static_cast<float>(utilization.memory);
        }
        
        // 视频引擎负载（编码器利用率）
        unsigned int encoderUtil;
        if (nvmlDeviceGetEncoderUtilization(device, &encoderUtil, nullptr) == NVML_SUCCESS) {
            gpu.videoEngineLoad = static_cast<float>(encoderUtil);
        }

        // 获取电压信息
        // 注意：NVML API 不提供直接获取 GPU 电压的函数
        // 这里使用基于功耗和性能状态的估算方法
        // 电压与功耗的关系：P = V^2 / R，因此 V ≈ sqrt(P * R)
        // 这里使用简化的线性关系进行估算
        
        // 获取功耗限制（用于估算最大电压）
        unsigned int minPowerLimit = 0;
        unsigned int maxPowerLimit = 0;
        bool hasPowerLimit = false;
        
        if (nvmlDeviceGetPowerManagementLimitConstraints(device, &minPowerLimit, &maxPowerLimit) == NVML_SUCCESS) {
            hasPowerLimit = true;
        }
        
        // 估算最大电压（单位：V）
        // 大多数现代GPU的最大电压在1.0-1.2V之间
        // 基于功耗限制估算：高功耗GPU通常有更高的电压
        if (hasPowerLimit && maxPowerLimit > 0) {
            // 估算公式：0.8V + (功耗限制/1000W) * 0.4V
            // 例如：300W GPU ≈ 0.92V, 450W GPU ≈ 0.98V
            float powerLimitW = static_cast<float>(maxPowerLimit) / 1000.0f;
            gpu.maxVoltage = 0.8f + powerLimitW * 0.0004f; // 0.8-1.0V范围
            gpu.maxVoltage = std::min(1.2f, std::max(0.8f, gpu.maxVoltage)); // 限制在合理范围
        } else {
            // 使用默认值（大多数现代GPU的最大电压在1.0-1.2V之间）
            gpu.maxVoltage = 1.0f; // 默认1.0V
        }
        
        // 估算实时电压（基于当前功耗，单位：V）
        if (gpu.powerUsage > 0 && gpu.maxVoltage > 0.0f) {
            // 使用功耗百分比估算电压
            // 假设：功耗与电压的平方成正比（P = V^2 / R）
            // 简化：V ≈ V_max * sqrt(P / P_max)
            float powerPercent = 0.0f;
            if (hasPowerLimit && maxPowerLimit > 0) {
                float maxPowerW = static_cast<float>(maxPowerLimit) / 1000.0f;
                powerPercent = (static_cast<float>(gpu.powerUsage) / maxPowerW) * 100.0f;
                powerPercent = std::min(100.0f, std::max(0.0f, powerPercent));
            } else {
                // 如果没有功耗限制，使用简化的线性关系
                // 假设：功耗在0-100%时，电压在0.8-1.0V
                powerPercent = std::min(100.0f, (static_cast<float>(gpu.powerUsage) / 300.0f) * 100.0f);
            }
            
            // 使用平方根关系估算电压（更符合物理规律）
            float voltageRatio = sqrtf(powerPercent / 100.0f);
            gpu.currentVoltage = 0.8f + (gpu.maxVoltage - 0.8f) * voltageRatio;
            gpu.currentVoltage = std::min(gpu.maxVoltage, std::max(0.8f, gpu.currentVoltage));
        } else {
            // 如果没有功耗数据，使用默认值
            gpu.currentVoltage = 0.0f;
        }
        
        // 计算电压百分比
        if (gpu.maxVoltage > 0.0f && gpu.currentVoltage > 0.0f) {
            gpu.voltagePercent = ((gpu.currentVoltage - 0.8f) / (gpu.maxVoltage - 0.8f)) * 100.0f;
            gpu.voltagePercent = std::min(100.0f, std::max(0.0f, gpu.voltagePercent));
        } else {
            gpu.voltagePercent = 0.0f;
        }

        // 获取 PCIe 信息
        unsigned int pcieLinkWidth = 0;
        unsigned int pcieLinkSpeed = 0;
        if (nvmlDeviceGetCurrPcieLinkWidth(device, &pcieLinkWidth) == NVML_SUCCESS) {
            gpu.pcieLinkWidth = pcieLinkWidth;
        }
        if (nvmlDeviceGetCurrPcieLinkGeneration(device, &pcieLinkSpeed) == NVML_SUCCESS) {
            // PCIe 速度: Generation * 2.5 GT/s (GigaTransfers per second)
            // 例如: PCIe 3.0 = 3 * 2.5 = 7.5 GT/s, PCIe 4.0 = 4 * 2.5 = 10 GT/s
            gpu.pcieLinkSpeed = pcieLinkSpeed;
            // 计算带宽: (LinkWidth * LinkSpeed * 2.5) / 8 = GB/s (双向)
            // 单向带宽 = (LinkWidth * LinkSpeed * 2.5) / 16
            gpu.pcieBandwidth = (static_cast<float>(gpu.pcieLinkWidth) * 
                                static_cast<float>(pcieLinkSpeed) * 2.5f) / 8.0f;
        }

        // 获取 PCIe 吞吐量
        // 注意：标准 NVML API 可能不直接提供 PCIe 吞吐量数据
        // 这里使用基于 GPU 活动状态的估算方法
        
        // 方法：基于 GPU 利用率和显存活动来估算 PCIe 数据传输
        // 这是一个简化的估算方法，实际吞吐量需要通过其他工具（如 nvidia-smi）或 CUDA 事件获取
        if (gpu.utilization > 0.0f && gpu.pcieBandwidth > 0.0f) {
            // 估算 PCIe 活动：基于 GPU 利用率和显存使用情况
            // 假设 GPU 工作时会有数据在 CPU 和 GPU 之间传输
            float activityFactor = (gpu.utilization / 100.0f) * 0.3f + 
                                  (gpu.memoryPercent / 100.0f) * 0.2f; // 综合活动因子
            
            // 估算吞吐量（基于理论带宽和活动因子）
            float estimatedTotalThroughput = gpu.pcieBandwidth * 1024.0f * activityFactor; // MB/s
            
            // 分配接收和发送（通常接收更多，因为数据从 CPU 传输到 GPU）
            gpu.pcieRxThroughput = estimatedTotalThroughput * 0.65f; // 接收占65%
            gpu.pcieTxThroughput = estimatedTotalThroughput * 0.35f; // 发送占35%
        } else {
            gpu.pcieRxThroughput = 0.0f;
            gpu.pcieTxThroughput = 0.0f;
        }
        
        // 注意：要获取精确的 PCIe 吞吐量，可以考虑：
        // 1. 使用 nvidia-smi 命令行工具解析输出
        // 2. 使用 CUDA 事件监控数据传输
        // 3. 使用性能计数器（如果系统支持）

        // 估算CPU到GPU数据传输等待时间
        // 等待时间主要取决于：
        // 1. GPU显存控制器负载（高负载时数据传输可能排队）
        // 2. GPU利用率（GPU忙碌时可能无法及时处理数据传输）
        // 3. PCIe带宽利用率（带宽饱和时会有延迟）
        // 4. 显存使用率（显存接近满载时可能有延迟）
        
        gpu.dataTransferWaitTime = 0.0f; // 默认无等待
        
        if (gpu.pcieBandwidth > 0.0f) {
            // 计算PCIe利用率（基于估算的吞吐量）
            float pcieUtilization = 0.0f;
            if (gpu.pcieRxThroughput > 0.0f || gpu.pcieTxThroughput > 0.0f) {
                float totalThroughputGB = (gpu.pcieRxThroughput + gpu.pcieTxThroughput) / 1024.0f;
                pcieUtilization = (totalThroughputGB / gpu.pcieBandwidth) * 100.0f;
                pcieUtilization = std::max(0.0f, std::min(100.0f, pcieUtilization));
            }
            
            // 因子1：显存控制器负载（这是最直接的指标）
            // 显存控制器负载高时，数据传输会排队等待
            float memoryControllerFactor = gpu.memoryControllerLoad / 100.0f;
            
            // 因子2：GPU利用率（GPU忙碌时，数据传输可能被延迟处理）
            float gpuUtilizationFactor = gpu.utilization / 100.0f;
            
            // 因子3：PCIe利用率（带宽饱和时会有延迟）
            float pcieUtilizationFactor = pcieUtilization / 100.0f;
            
            // 因子4：显存使用率（显存接近满载时，新数据传输可能需要等待空间）
            float memoryUsageFactor = gpu.memoryPercent / 100.0f;
            
            // 综合计算等待时间（毫秒）
            // 等待时间主要由显存控制器负载和GPU利用率决定
            // 当这些指标高时，CPU到GPU的数据传输需要等待
            
            float baseWaitTime = 0.0f;
            
            // 基础等待时间：显存控制器负载是主要因素
            if (memoryControllerFactor > 0.7f) {
                // 显存控制器高负载时，等待时间显著增加
                baseWaitTime = (memoryControllerFactor - 0.7f) * 5.0f; // 0-1.5ms
            }
            
            // GPU利用率影响：GPU忙碌时，数据传输可能被延迟
            if (gpuUtilizationFactor > 0.8f) {
                baseWaitTime += (gpuUtilizationFactor - 0.8f) * 3.0f; // 0-0.6ms
            }
            
            // PCIe带宽饱和影响：带宽利用率高时，数据传输会排队
            if (pcieUtilizationFactor > 0.75f) {
                baseWaitTime += (pcieUtilizationFactor - 0.75f) * 4.0f; // 0-1.0ms
            }
            
            // 显存使用率影响：显存接近满载时，新数据传输需要等待
            if (memoryUsageFactor > 0.85f) {
                baseWaitTime += (memoryUsageFactor - 0.85f) * 2.0f; // 0-0.3ms
            }
            
            // 当GPU和显存都处于低负载时，即使有数据传输，等待时间也很短
            if (gpuUtilizationFactor < 0.3f && memoryControllerFactor < 0.3f) {
                baseWaitTime *= 0.3f; // 低负载时，等待时间大幅减少
            }
            
            gpu.dataTransferWaitTime = baseWaitTime;
            
            // 限制等待时间在合理范围内（0-10ms）
            gpu.dataTransferWaitTime = std::max(0.0f, std::min(10.0f, gpu.dataTransferWaitTime));
        } else {
            // 如果没有PCIe带宽信息，基于GPU和显存控制器负载估算
            float memoryControllerFactor = gpu.memoryControllerLoad / 100.0f;
            float gpuUtilizationFactor = gpu.utilization / 100.0f;
            
            if (memoryControllerFactor > 0.7f || gpuUtilizationFactor > 0.8f) {
                gpu.dataTransferWaitTime = (memoryControllerFactor * 2.0f + gpuUtilizationFactor * 1.0f);
                gpu.dataTransferWaitTime = std::max(0.0f, std::min(5.0f, gpu.dataTransferWaitTime));
            } else {
                gpu.dataTransferWaitTime = 0.0f;
            }
        }

        // 更新历史数据
        gpu.utilizationHistory.push_back(gpu.utilization);
        gpu.memoryHistory.push_back(gpu.memoryPercent);
        gpu.temperatureHistory.push_back(gpu.temperature);
        gpu.pcieRxHistory.push_back(gpu.pcieRxThroughput);
        gpu.pcieTxHistory.push_back(gpu.pcieTxThroughput);
        gpu.transferWaitHistory.push_back(gpu.dataTransferWaitTime);

        if (gpu.utilizationHistory.size() > GPUInfo::MAX_HISTORY) {
            gpu.utilizationHistory.erase(gpu.utilizationHistory.begin());
            gpu.memoryHistory.erase(gpu.memoryHistory.begin());
            gpu.temperatureHistory.erase(gpu.temperatureHistory.begin());
            gpu.pcieRxHistory.erase(gpu.pcieRxHistory.begin());
            gpu.pcieTxHistory.erase(gpu.pcieTxHistory.begin());
            gpu.transferWaitHistory.erase(gpu.transferWaitHistory.begin());
        }
    }
}

void HardwareMonitor::UpdateCPU() {
    // 使用PDH获取CPU利用率
    PDH_FMT_COUNTERVALUE counterVal;
    PdhCollectQueryData(cpuQuery_);
    
    if (PdhGetFormattedCounterValue(cpuCounter_, PDH_FMT_DOUBLE, NULL, &counterVal) == ERROR_SUCCESS) {
        cpuInfo_.utilization = static_cast<float>(counterVal.doubleValue);
    }

    // 更新历史数据
    cpuInfo_.utilizationHistory.push_back(cpuInfo_.utilization);
    if (cpuInfo_.utilizationHistory.size() > CPUInfo::MAX_HISTORY) {
        cpuInfo_.utilizationHistory.erase(cpuInfo_.utilizationHistory.begin());
    }
}

void HardwareMonitor::UpdateMemory() {
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);

    memoryInfo_.total = static_cast<float>(memInfo.ullTotalPhys) / (1024.0f * 1024.0f * 1024.0f);  // GB
    memoryInfo_.available = static_cast<float>(memInfo.ullAvailPhys) / (1024.0f * 1024.0f * 1024.0f);  // GB
    memoryInfo_.used = memoryInfo_.total - memoryInfo_.available;
    memoryInfo_.percent = static_cast<float>(memInfo.dwMemoryLoad);

    // 更新历史数据
    memoryInfo_.percentHistory.push_back(memoryInfo_.percent);
    if (memoryInfo_.percentHistory.size() > MemoryInfo::MAX_HISTORY) {
        memoryInfo_.percentHistory.erase(memoryInfo_.percentHistory.begin());
    }
}

void HardwareMonitor::UpdateSystemBandwidth() {
    // 计算总 PCIe 带宽（所有 GPU 的总和）
    float totalPcieBandwidth = 0.0f;
    for (const auto& gpu : gpuInfos_) {
        if (gpu.available) {
            totalPcieBandwidth += gpu.pcieBandwidth;
        }
    }
    systemBandwidthInfo_.pcieTotalBandwidth = totalPcieBandwidth;

    // 估算内存带宽（基于内存类型和速度）
    // 这是一个简化的估算，实际值取决于内存配置
    // DDR4-3200 双通道 ≈ 51.2 GB/s, DDR5-4800 双通道 ≈ 76.8 GB/s
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    
    // 根据总内存大小估算内存类型和速度
    float totalMemGB = static_cast<float>(memInfo.ullTotalPhys) / (1024.0f * 1024.0f * 1024.0f);
    if (totalMemGB >= 32.0f) {
        // 假设是较新的系统，使用 DDR4/DDR5
        systemBandwidthInfo_.memoryBandwidth = 50.0f; // GB/s (保守估算)
        systemBandwidthInfo_.memoryType = "DDR4/DDR5";
        systemBandwidthInfo_.memorySpeed = 3200; // MHz
    } else {
        // 较旧的系统
        systemBandwidthInfo_.memoryBandwidth = 25.0f; // GB/s
        systemBandwidthInfo_.memoryType = "DDR3/DDR4";
        systemBandwidthInfo_.memorySpeed = 2400; // MHz
    }

    // 估算CPU带宽（基于CPU核心数和内存带宽）
    // CPU带宽通常与内存带宽相关，现代CPU通常支持双通道或更多通道
    // 这里使用内存带宽作为基础，CPU带宽通常略高于内存带宽
    systemBandwidthInfo_.cpuBandwidth = systemBandwidthInfo_.memoryBandwidth * 1.2f; // 估算为内存带宽的1.2倍

    // 总系统带宽（主板总带宽）= CPU带宽 + 内存带宽 + PCIe带宽
    systemBandwidthInfo_.totalSystemBandwidth = 
        systemBandwidthInfo_.cpuBandwidth + 
        systemBandwidthInfo_.memoryBandwidth + 
        systemBandwidthInfo_.pcieTotalBandwidth;

    // 更新历史数据
    systemBandwidthInfo_.totalBandwidthHistory.push_back(systemBandwidthInfo_.totalSystemBandwidth);
    systemBandwidthInfo_.cpuBandwidthHistory.push_back(systemBandwidthInfo_.cpuBandwidth);
    systemBandwidthInfo_.memoryBandwidthHistory.push_back(systemBandwidthInfo_.memoryBandwidth);
    
    if (systemBandwidthInfo_.totalBandwidthHistory.size() > SystemBandwidthInfo::MAX_HISTORY) {
        systemBandwidthInfo_.totalBandwidthHistory.erase(
            systemBandwidthInfo_.totalBandwidthHistory.begin());
    }
    if (systemBandwidthInfo_.cpuBandwidthHistory.size() > SystemBandwidthInfo::MAX_HISTORY) {
        systemBandwidthInfo_.cpuBandwidthHistory.erase(
            systemBandwidthInfo_.cpuBandwidthHistory.begin());
    }
    if (systemBandwidthInfo_.memoryBandwidthHistory.size() > SystemBandwidthInfo::MAX_HISTORY) {
        systemBandwidthInfo_.memoryBandwidthHistory.erase(
            systemBandwidthInfo_.memoryBandwidthHistory.begin());
    }
}

void HardwareMonitor::Shutdown() {
    if (nvmlInitialized_) {
        nvmlShutdown();
        nvmlInitialized_ = false;
    }

    if (cpuQuery_) {
        PdhCloseQuery(cpuQuery_);
        cpuQuery_ = nullptr;
    }
}

const GPUInfo& HardwareMonitor::GetGPUInfo(int index) const {
    static GPUInfo empty;
    if (index >= 0 && index < static_cast<int>(gpuInfos_.size())) {
        return gpuInfos_[index];
    }
    return empty;
}

