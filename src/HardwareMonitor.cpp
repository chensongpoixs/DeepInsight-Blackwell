#include "HardwareMonitor.h"
#include <pdh.h>
#include <psapi.h>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cerrno>
#include <nvml.h>
#include <comdef.h>
#include <Wbemidl.h>
#include <sstream>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "psapi.lib")
#pragma comment(lib, "wbemuuid.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "oleaut32.lib")

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
    UpdateMemoryModules();
    UpdateDisks();
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

void HardwareMonitor::UpdateMemoryModules() {
    // 使用WMI获取真实的内存条信息
    static bool wmiInitialized = false;
    static bool wmiAvailable = false;
    
    if (!wmiInitialized) {
        // 尝试初始化WMI（只初始化一次）
        HRESULT hres = CoInitializeEx(0, COINIT_MULTITHREADED);
        if (SUCCEEDED(hres) || hres == RPC_E_CHANGED_MODE) {
            hres = CoInitializeSecurity(NULL, -1, NULL, NULL,
                RPC_C_AUTHN_LEVEL_DEFAULT, RPC_C_IMP_LEVEL_IMPERSONATE,
                NULL, EOAC_NONE, NULL);
            wmiAvailable = (SUCCEEDED(hres) || hres == RPC_E_TOO_LATE);
        }
        wmiInitialized = true;
    }
    
    bool modulesUpdated = false;
    
    if (wmiAvailable) {
        // 使用WMI获取内存条信息
        IWbemLocator* pLoc = nullptr;
        HRESULT hres = CoCreateInstance(CLSID_WbemLocator, 0, CLSCTX_INPROC_SERVER,
            IID_IWbemLocator, (LPVOID*)&pLoc);
        
        if (SUCCEEDED(hres)) {
            IWbemServices* pSvc = nullptr;
            hres = pLoc->ConnectServer(_bstr_t(L"ROOT\\CIMV2"), NULL, NULL, 0, NULL, 0, 0, &pSvc);
            
            if (SUCCEEDED(hres)) {
                hres = CoSetProxyBlanket(pSvc, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, NULL,
                    RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE);
                
                if (SUCCEEDED(hres)) {
                    IEnumWbemClassObject* pEnumerator = nullptr;
                    hres = pSvc->ExecQuery(bstr_t("WQL"),
                        bstr_t("SELECT Capacity, Speed, MemoryType, DeviceLocator FROM Win32_PhysicalMemory"),
                        WBEM_FLAG_FORWARD_ONLY | WBEM_FLAG_RETURN_IMMEDIATELY, NULL, &pEnumerator);
                    
                    if (SUCCEEDED(hres)) {
                        memoryInfo_.modules.clear();
                        IWbemClassObject* pclsObj = nullptr;
                        ULONG uReturn = 0;
                        size_t moduleIndex = 0;
                        
                        while (pEnumerator->Next(WBEM_INFINITE, 1, &pclsObj, &uReturn) == WBEM_S_NO_ERROR && uReturn > 0) {
                            MemoryModuleInfo module;
                            
                            // 获取容量
                            VARIANT vtProp;
                            if (SUCCEEDED(pclsObj->Get(L"Capacity", 0, &vtProp, 0, 0))) {
                                if (vtProp.vt == VT_BSTR) {
                                    unsigned long long capacity = _wtoi64(vtProp.bstrVal);
                                    module.capacity = static_cast<float>(capacity) / (1024.0f * 1024.0f * 1024.0f); // GB
                                } else if (vtProp.vt == VT_I8 || vtProp.vt == VT_UI8) {
                                    module.capacity = static_cast<float>(vtProp.ullVal) / (1024.0f * 1024.0f * 1024.0f); // GB
                                }
                                VariantClear(&vtProp);
                            }
                            
                            // 获取速度
                            if (SUCCEEDED(pclsObj->Get(L"Speed", 0, &vtProp, 0, 0))) {
                                if (vtProp.vt == VT_UI4) {
                                    module.speed = vtProp.ulVal;
                                } else if (vtProp.vt == VT_UI2) {
                                    module.speed = vtProp.uiVal;
                                }
                                VariantClear(&vtProp);
                            }
                            
                            // 获取内存类型
                            if (SUCCEEDED(pclsObj->Get(L"MemoryType", 0, &vtProp, 0, 0))) {
                                unsigned int memType = 0;
                                if (vtProp.vt == VT_UI2) {
                                    memType = vtProp.uiVal;
                                } else if (vtProp.vt == VT_UI4) {
                                    memType = vtProp.ulVal;
                                }
                                
                                // 转换内存类型代码为字符串
                                switch (memType) {
                                case 20: module.type = "DDR"; break;
                                case 21: module.type = "DDR2"; break;
                                case 24: module.type = "DDR3"; break;
                                case 26: module.type = "DDR4"; break;
                                case 34: case 35: module.type = "DDR5"; break;
                                default: module.type = "Unknown"; break;
                                }
                                VariantClear(&vtProp);
                            }
                            
                            // 获取设备位置
                            if (SUCCEEDED(pclsObj->Get(L"DeviceLocator", 0, &vtProp, 0, 0))) {
                                if (vtProp.vt == VT_BSTR && vtProp.bstrVal) {
                                    _bstr_t bstrLocator(vtProp.bstrVal);
                                    char* pLocator = bstrLocator;
                                    if (pLocator) {
                                        module.name = std::string(pLocator);
                                    } else {
                                        module.name = "内存条 " + std::to_string(moduleIndex + 1);
                                    }
                                } else {
                                    module.name = "内存条 " + std::to_string(moduleIndex + 1);
                                }
                                VariantClear(&vtProp);
                            } else {
                                module.name = "内存条 " + std::to_string(moduleIndex + 1);
                            }
                            
                            // 估算通道（基于索引，假设双通道）
                            module.channel = static_cast<unsigned int>(moduleIndex % 2);
                            
                            // 如果没有获取到速度，使用默认值
                            if (module.speed == 0) {
                                if (module.type == "DDR5") {
                                    module.speed = 4800;
                                } else if (module.type == "DDR4") {
                                    module.speed = 3200;
                                } else if (module.type == "DDR3") {
                                    module.speed = 1600;
                                } else {
                                    module.speed = 2400;
                                }
                            }
                            
                            // 计算单条内存的最大带宽
                            // 单通道带宽 = 速度(MHz) * 64bit / 8 / 1000
                            module.maxBandwidth = (static_cast<float>(module.speed) * 64.0f) / 8.0f / 1000.0f; // GB/s
                            
                            memoryInfo_.modules.push_back(module);
                            moduleIndex++;
                            
                            pclsObj->Release();
                        }
                        
                        pEnumerator->Release();
                        modulesUpdated = true;
                    }
                    
                    pSvc->Release();
                }
                
                pLoc->Release();
            }
        }
    }
    
    // 如果WMI失败或未初始化，使用估算方法
    if (!modulesUpdated) {
        MEMORYSTATUSEX memInfo;
        memInfo.dwLength = sizeof(MEMORYSTATUSEX);
        GlobalMemoryStatusEx(&memInfo);
        
        float totalMemGB = static_cast<float>(memInfo.ullTotalPhys) / (1024.0f * 1024.0f * 1024.0f);
        
        // 估算内存条数量（假设每个内存条8GB或16GB）
        size_t estimatedModuleCount = 0;
        if (totalMemGB >= 64.0f) {
            estimatedModuleCount = static_cast<size_t>(totalMemGB / 16.0f + 0.5f);
        } else if (totalMemGB >= 32.0f) {
            estimatedModuleCount = static_cast<size_t>(totalMemGB / 8.0f + 0.5f);
        } else {
            estimatedModuleCount = static_cast<size_t>(totalMemGB / 4.0f + 0.5f);
        }
        
        estimatedModuleCount = std::max(static_cast<size_t>(1), std::min(static_cast<size_t>(8), estimatedModuleCount));
        
        // 只在数量变化时重新初始化
        if (memoryInfo_.modules.size() != estimatedModuleCount) {
            memoryInfo_.modules.resize(estimatedModuleCount);
            for (size_t i = 0; i < estimatedModuleCount; i++) {
                MemoryModuleInfo& module = memoryInfo_.modules[i];
                module.name = "内存条 " + std::to_string(i + 1);
                module.capacity = totalMemGB / static_cast<float>(estimatedModuleCount);
                module.channel = static_cast<unsigned int>(i % 2);
                
                if (totalMemGB >= 64.0f) {
                    module.speed = 4800;
                    module.type = "DDR5";
                } else if (totalMemGB >= 32.0f) {
                    module.speed = 3200;
                    module.type = "DDR4";
                } else {
                    module.speed = 2400;
                    module.type = "DDR4";
                }
                
                module.maxBandwidth = (static_cast<float>(module.speed) * 64.0f) / 8.0f / 1000.0f;
            }
        }
    }
    
    // 更新每个内存条的实时带宽和利用率
    float memPercent = memoryInfo_.percent;
    float cpuUtil = cpuInfo_.utilization;
    
    // 计算系统总带宽利用率（基于内存使用率和CPU利用率）
    // 修复：不再乘以channelLoadFactor，这样利用率会更合理
    float systemActivityFactor = (memPercent / 100.0f) * 0.5f + (cpuUtil / 100.0f) * 0.5f;
    systemActivityFactor = std::max(0.0f, std::min(1.0f, systemActivityFactor));
    
    for (size_t i = 0; i < memoryInfo_.modules.size(); i++) {
        MemoryModuleInfo& module = memoryInfo_.modules[i];
        
        // 实时带宽 = 最大带宽 * 系统活动因子
        // 每个内存条共享系统负载，但根据通道可能略有差异
        float channelFactor = 1.0f;
        if (memoryInfo_.modules.size() > 1) {
            // 双通道时，负载可能略有不同
            channelFactor = (module.channel == 0) ? 1.05f : 0.95f; // 通道0略高，通道1略低
        }
        
        float activityFactor = systemActivityFactor * channelFactor;
        activityFactor = std::max(0.0f, std::min(1.0f, activityFactor));
        
        module.realTimeBandwidth = module.maxBandwidth * activityFactor;
        module.utilization = activityFactor * 100.0f;
        
        // 更新历史数据
        module.bandwidthHistory.push_back(module.realTimeBandwidth);
        if (module.bandwidthHistory.size() > MemoryModuleInfo::MAX_HISTORY) {
            module.bandwidthHistory.erase(module.bandwidthHistory.begin());
        }
    }
}

void HardwareMonitor::UpdateDisks() {
    // 初始化磁盘查询（如果尚未初始化）
    if (diskQuery_ == nullptr) {
        PdhOpenQuery(nullptr, NULL, &diskQuery_);
        
        // 枚举所有逻辑磁盘
        DWORD bufferSize = 0;
        GetLogicalDriveStrings(bufferSize, nullptr);
        bufferSize += 1;
        std::vector<char> driveBuffer(bufferSize);
        GetLogicalDriveStrings(bufferSize, driveBuffer.data());
        
        diskInfos_.clear();
        diskCounters_.clear();
        
        for (char* drive = driveBuffer.data(); *drive; drive += strlen(drive) + 1) {
            UINT driveType = GetDriveTypeA(drive);
            if (driveType == DRIVE_FIXED) { // 只监控固定磁盘
                DiskInfo disk;
                disk.name = std::string(1, drive[0]) + ":";
                
                // 获取磁盘容量
                ULARGE_INTEGER freeBytes, totalBytes;
                if (GetDiskFreeSpaceExA(drive, nullptr, &totalBytes, &freeBytes)) {
                    disk.totalSize = static_cast<float>(totalBytes.QuadPart) / (1024.0f * 1024.0f * 1024.0f); // GB
                }
                
                // 估算磁盘类型和最大带宽
                if (disk.totalSize > 1000.0f) {
                    // 大容量，可能是HDD
                    disk.type = "HDD";
                    disk.model = "机械硬盘";
                    disk.maxReadBandwidth = 0.2f;  // GB/s
                    disk.maxWriteBandwidth = 0.15f; // GB/s
                } else {
                    // 较小容量，可能是SSD
                    disk.type = "SSD";
                    disk.model = "固态硬盘";
                    disk.maxReadBandwidth = 3.5f;  // GB/s (NVMe PCIe 3.0估算)
                    disk.maxWriteBandwidth = 2.5f; // GB/s
                }
                
                diskInfos_.push_back(disk);
                
                // 创建PDH计数器 - 使用正确的路径格式
                DiskCounter counter;
                counter.diskName = disk.name;
                
                // PDH路径格式: \PhysicalDisk(0 C:)\Disk Read Bytes/sec
                char driveLetter = drive[0];
                std::string readPath = "\\PhysicalDisk(" + std::string(1, driveLetter) + ":)\\Disk Read Bytes/sec";
                std::string writePath = "\\PhysicalDisk(" + std::string(1, driveLetter) + ":)\\Disk Write Bytes/sec";
                
                // 尝试添加计数器，如果失败则跳过
                PDH_STATUS status1 = PdhAddCounterA(diskQuery_, readPath.c_str(), NULL, &counter.readCounter);
                PDH_STATUS status2 = PdhAddCounterA(diskQuery_, writePath.c_str(), NULL, &counter.writeCounter);
                
                if (status1 == ERROR_SUCCESS && status2 == ERROR_SUCCESS) {
                    diskCounters_.push_back(counter);
                } else {
                    // 如果PDH路径失败，尝试使用逻辑磁盘路径
                    readPath = "\\LogicalDisk(" + std::string(1, driveLetter) + ":)\\Disk Read Bytes/sec";
                    writePath = "\\LogicalDisk(" + std::string(1, driveLetter) + ":)\\Disk Write Bytes/sec";
                    
                    counter.readCounter = nullptr;
                    counter.writeCounter = nullptr;
                    status1 = PdhAddCounterA(diskQuery_, readPath.c_str(), NULL, &counter.readCounter);
                    status2 = PdhAddCounterA(diskQuery_, writePath.c_str(), NULL, &counter.writeCounter);
                    
                    if (status1 == ERROR_SUCCESS && status2 == ERROR_SUCCESS) {
                        diskCounters_.push_back(counter);
                    }
                }
            }
        }
        
        if (diskQuery_ != nullptr && !diskCounters_.empty()) {
            PdhCollectQueryData(diskQuery_);
        }
    }
    
    // 更新磁盘IO数据
    if (diskQuery_ != nullptr && !diskCounters_.empty()) {
        PdhCollectQueryData(diskQuery_);
        
        for (size_t i = 0; i < diskInfos_.size() && i < diskCounters_.size(); i++) {
            DiskInfo& disk = diskInfos_[i];
            DiskCounter& counter = diskCounters_[i];
            
            // 读取读取速度
            if (counter.readCounter != nullptr) {
                PDH_FMT_COUNTERVALUE readValue;
                if (PdhGetFormattedCounterValue(counter.readCounter, PDH_FMT_DOUBLE, NULL, &readValue) == ERROR_SUCCESS) {
                    // 转换为GB/s
                    disk.realTimeReadBandwidth = static_cast<float>(readValue.doubleValue) / (1024.0f * 1024.0f * 1024.0f);
                    disk.readUtilization = (disk.maxReadBandwidth > 0.0f) ? 
                        (disk.realTimeReadBandwidth / disk.maxReadBandwidth * 100.0f) : 0.0f;
                }
            }
            
            // 读取写入速度
            if (counter.writeCounter != nullptr) {
                PDH_FMT_COUNTERVALUE writeValue;
                if (PdhGetFormattedCounterValue(counter.writeCounter, PDH_FMT_DOUBLE, NULL, &writeValue) == ERROR_SUCCESS) {
                    // 转换为GB/s
                    disk.realTimeWriteBandwidth = static_cast<float>(writeValue.doubleValue) / (1024.0f * 1024.0f * 1024.0f);
                    disk.writeUtilization = (disk.maxWriteBandwidth > 0.0f) ? 
                        (disk.realTimeWriteBandwidth / disk.maxWriteBandwidth * 100.0f) : 0.0f;
                }
            }
            
            // 更新历史数据
            disk.readBandwidthHistory.push_back(disk.realTimeReadBandwidth);
            disk.writeBandwidthHistory.push_back(disk.realTimeWriteBandwidth);
            
            if (disk.readBandwidthHistory.size() > DiskInfo::MAX_HISTORY) {
                disk.readBandwidthHistory.erase(disk.readBandwidthHistory.begin());
            }
            if (disk.writeBandwidthHistory.size() > DiskInfo::MAX_HISTORY) {
                disk.writeBandwidthHistory.erase(disk.writeBandwidthHistory.begin());
            }
        }
    }
}

void HardwareMonitor::UpdateSystemBandwidth() {
    // ========== 1. PCIe 总线带宽（CPU 与 GPU 的桥梁）==========
    float totalPcieMaxBandwidth = 0.0f;
    float totalPcieRealTimeBandwidth = 0.0f;
    
    for (const auto& gpu : gpuInfos_) {
        if (gpu.available) {
            // 最大带宽 = 理论PCIe带宽
            totalPcieMaxBandwidth += gpu.pcieBandwidth;
            // 实时带宽 = 实际吞吐量（接收 + 发送）
            totalPcieRealTimeBandwidth += (gpu.pcieRxThroughput + gpu.pcieTxThroughput) / 1024.0f; // 转换为GB/s
        }
    }
    
    systemBandwidthInfo_.pcieMaxBandwidth = totalPcieMaxBandwidth;
    systemBandwidthInfo_.pcieRealTimeBandwidth = totalPcieRealTimeBandwidth;
    systemBandwidthInfo_.pcieUtilization = (totalPcieMaxBandwidth > 0.0f) ? 
        (totalPcieRealTimeBandwidth / totalPcieMaxBandwidth * 100.0f) : 0.0f;
    
    // 兼容性字段
    systemBandwidthInfo_.pcieTotalBandwidth = totalPcieMaxBandwidth;

    // ========== 2. 内存带宽（CPU 与 RAM 的桥梁）==========
    MEMORYSTATUSEX memInfo;
    memInfo.dwLength = sizeof(MEMORYSTATUSEX);
    GlobalMemoryStatusEx(&memInfo);
    
    float totalMemGB = static_cast<float>(memInfo.ullTotalPhys) / (1024.0f * 1024.0f * 1024.0f);
    
    // 估算最大内存带宽（基于内存类型和速度）
    // DDR4-3200 双通道 ≈ 51.2 GB/s, DDR5-4800 双通道 ≈ 76.8 GB/s
    float memoryMaxBW = 0.0f;
    if (totalMemGB >= 64.0f) {
        // 大内存系统，可能是DDR5或高端DDR4
        memoryMaxBW = 60.0f; // GB/s
        systemBandwidthInfo_.memoryType = "DDR5/DDR4";
        systemBandwidthInfo_.memorySpeed = 4800;
    } else if (totalMemGB >= 32.0f) {
        // 中等内存系统，DDR4
        memoryMaxBW = 50.0f; // GB/s
        systemBandwidthInfo_.memoryType = "DDR4";
        systemBandwidthInfo_.memorySpeed = 3200;
    } else {
        // 较小内存系统
        memoryMaxBW = 25.0f; // GB/s
        systemBandwidthInfo_.memoryType = "DDR3/DDR4";
        systemBandwidthInfo_.memorySpeed = 2400;
    }
    
    systemBandwidthInfo_.memoryMaxBandwidth = memoryMaxBW;
    
    // 估算实时内存带宽（基于内存使用率和CPU活动）
    // 内存带宽利用率与内存使用率和CPU利用率相关
    float memPercent = memoryInfo_.percent; // 已在UpdateMemory中更新
    float cpuUtil = cpuInfo_.utilization;   // 已在UpdateCPU中更新
    float memoryActivityFactor = (memPercent / 100.0f) * 0.6f + 
                                 (cpuUtil / 100.0f) * 0.4f;
    systemBandwidthInfo_.memoryRealTimeBandwidth = memoryMaxBW * memoryActivityFactor;
    systemBandwidthInfo_.memoryUtilization = memoryActivityFactor * 100.0f;
    
    // 兼容性字段
    systemBandwidthInfo_.memoryBandwidth = memoryMaxBW;
    systemBandwidthInfo_.cpuBandwidth = memoryMaxBW * 1.2f; // CPU带宽估算

    // ========== 3. 存储/IO 带宽（硬盘与内存的桥梁）==========
    // 估算存储最大带宽（基于常见存储接口）
    // SATA3 ≈ 0.6 GB/s, NVMe PCIe 3.0 x4 ≈ 3.5 GB/s, NVMe PCIe 4.0 x4 ≈ 7.0 GB/s
    // 这里使用保守估算，假设是NVMe PCIe 3.0
    systemBandwidthInfo_.storageMaxBandwidth = 3.5f; // GB/s (保守估算)
    
    // 估算实时存储带宽（基于内存活动）
    // 存储带宽利用率与内存使用率相关（内存不足时会频繁读写磁盘）
      memPercent = memoryInfo_.percent; // 已在UpdateMemory中更新
    float storageActivityFactor = 0.0f;
    if (memPercent > 90.0f) {
        // 内存接近满载，可能有频繁的Swap操作
        storageActivityFactor = 0.3f + (memPercent - 90.0f) / 10.0f * 0.4f; // 30%-70%
    } else if (memPercent > 80.0f) {
        // 内存使用较高，可能有少量IO
        storageActivityFactor = 0.1f + (memPercent - 80.0f) / 10.0f * 0.2f; // 10%-30%
    } else {
        // 内存充足，IO活动较少
        storageActivityFactor = memPercent / 80.0f * 0.1f; // 0%-10%
    }
    
    systemBandwidthInfo_.storageRealTimeBandwidth = systemBandwidthInfo_.storageMaxBandwidth * storageActivityFactor;
    systemBandwidthInfo_.storageUtilization = storageActivityFactor * 100.0f;

    // ========== 4. 显存带宽（GPU 内部带宽 - 极重要）==========
    float totalVramMaxBandwidth = 0.0f;
    float totalVramRealTimeBandwidth = 0.0f;
    
    for (const auto& gpu : gpuInfos_) {
        if (gpu.available && gpu.memoryClock > 0) {
            // 显存带宽计算公式：带宽(GB/s) = 显存时钟(MHz) * 位宽(bits) / 8 / 1000
            // 常见GPU显存位宽：128bit, 192bit, 256bit, 320bit, 384bit
            // 这里使用估算值，基于显存时钟频率
            // 假设位宽为256bit（常见的中高端GPU）
            unsigned int memoryBusWidth = 256; // bits (估算值)
            float vramMaxBW = (static_cast<float>(gpu.memoryClock) * memoryBusWidth) / 8.0f / 1000.0f; // GB/s
            
            totalVramMaxBandwidth += vramMaxBW;
            
            // 实时显存带宽 = 最大带宽 * 显存控制器负载
            float vramActivityFactor = gpu.memoryControllerLoad / 100.0f;
            totalVramRealTimeBandwidth += vramMaxBW * vramActivityFactor;
        }
    }
    
    systemBandwidthInfo_.vramMaxBandwidth = totalVramMaxBandwidth;
    systemBandwidthInfo_.vramRealTimeBandwidth = totalVramRealTimeBandwidth;
    systemBandwidthInfo_.vramUtilization = (totalVramMaxBandwidth > 0.0f) ? 
        (totalVramRealTimeBandwidth / totalVramMaxBandwidth * 100.0f) : 0.0f;

    // ========== 总系统带宽（主板总带宽）==========
    // 总系统带宽 = PCIe最大带宽 + 内存最大带宽 + 存储最大带宽 + 显存最大带宽
    systemBandwidthInfo_.totalSystemBandwidth = 
        systemBandwidthInfo_.pcieMaxBandwidth + 
        systemBandwidthInfo_.memoryMaxBandwidth + 
        systemBandwidthInfo_.storageMaxBandwidth + 
        systemBandwidthInfo_.vramMaxBandwidth;

    // ========== 更新历史数据 ==========
    systemBandwidthInfo_.totalBandwidthHistory.push_back(systemBandwidthInfo_.totalSystemBandwidth);
    systemBandwidthInfo_.cpuBandwidthHistory.push_back(systemBandwidthInfo_.cpuBandwidth);
    systemBandwidthInfo_.memoryBandwidthHistory.push_back(systemBandwidthInfo_.memoryMaxBandwidth);
    systemBandwidthInfo_.pcieBandwidthHistory.push_back(systemBandwidthInfo_.pcieRealTimeBandwidth);
    systemBandwidthInfo_.storageBandwidthHistory.push_back(systemBandwidthInfo_.storageRealTimeBandwidth);
    systemBandwidthInfo_.vramBandwidthHistory.push_back(systemBandwidthInfo_.vramRealTimeBandwidth);
    
    // 限制历史数据大小
    if (systemBandwidthInfo_.totalBandwidthHistory.size() > SystemBandwidthInfo::MAX_HISTORY) {
        systemBandwidthInfo_.totalBandwidthHistory.erase(systemBandwidthInfo_.totalBandwidthHistory.begin());
    }
    if (systemBandwidthInfo_.cpuBandwidthHistory.size() > SystemBandwidthInfo::MAX_HISTORY) {
        systemBandwidthInfo_.cpuBandwidthHistory.erase(systemBandwidthInfo_.cpuBandwidthHistory.begin());
    }
    if (systemBandwidthInfo_.memoryBandwidthHistory.size() > SystemBandwidthInfo::MAX_HISTORY) {
        systemBandwidthInfo_.memoryBandwidthHistory.erase(systemBandwidthInfo_.memoryBandwidthHistory.begin());
    }
    if (systemBandwidthInfo_.pcieBandwidthHistory.size() > SystemBandwidthInfo::MAX_HISTORY) {
        systemBandwidthInfo_.pcieBandwidthHistory.erase(systemBandwidthInfo_.pcieBandwidthHistory.begin());
    }
    if (systemBandwidthInfo_.storageBandwidthHistory.size() > SystemBandwidthInfo::MAX_HISTORY) {
        systemBandwidthInfo_.storageBandwidthHistory.erase(systemBandwidthInfo_.storageBandwidthHistory.begin());
    }
    if (systemBandwidthInfo_.vramBandwidthHistory.size() > SystemBandwidthInfo::MAX_HISTORY) {
        systemBandwidthInfo_.vramBandwidthHistory.erase(systemBandwidthInfo_.vramBandwidthHistory.begin());
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
    
    if (diskQuery_) {
        // 关闭所有磁盘计数器
        for (auto& counter : diskCounters_) {
            if (counter.readCounter) {
                PdhRemoveCounter(counter.readCounter);
            }
            if (counter.writeCounter) {
                PdhRemoveCounter(counter.writeCounter);
            }
        }
        diskCounters_.clear();
        PdhCloseQuery(diskQuery_);
        diskQuery_ = nullptr;
    }
}

const GPUInfo& HardwareMonitor::GetGPUInfo(int index) const {
    static GPUInfo empty;
    if (index >= 0 && index < static_cast<int>(gpuInfos_.size())) {
        return gpuInfos_[index];
    }
    return empty;
}

const DiskInfo& HardwareMonitor::GetDiskInfo(size_t index) const {
    static DiskInfo empty;
    if (index < diskInfos_.size()) {
        return diskInfos_[index];
    }
    return empty;
}

