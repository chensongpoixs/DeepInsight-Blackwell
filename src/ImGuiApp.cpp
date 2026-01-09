#include "ImGuiApp.h"
#include <GLFW/glfw3.h>
#define GL_SILENCE_DEPRECATION
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdio>
#include <functional>
#include <cmath>
#ifdef _WIN32
#include <io.h>
#include <fcntl.h>
#endif

ImGuiApp::ImGuiApp(const std::string& title, int width, int height)
    : title_(title), width_(width), height_(height) {
}

ImGuiApp::~ImGuiApp() {
    Shutdown();
}

bool ImGuiApp::Initialize() {
    // 初始化GLFW
    if (!glfwInit()) {
        std::cerr << "GLFW初始化失败！" << std::endl;
        return false;
    }

    // 设置OpenGL版本
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // 创建窗口
    window_ = glfwCreateWindow(width_, height_, title_.c_str(), nullptr, nullptr);
    if (!window_) {
        std::cerr << "窗口创建失败！" << std::endl;
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // 启用垂直同步

    // 初始化ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    
    // 设置中文字体支持
    // 尝试使用 Windows 系统字体
    const char* fontPaths[] = {
        "C:/Windows/Fonts/msyh.ttc",           // 微软雅黑
        "C:/Windows/Fonts/simhei.ttf",          // 黑体
        "C:/Windows/Fonts/simsun.ttc",          // 宋体
        "C:/Windows/Fonts/msyhbd.ttc",          // 微软雅黑 Bold
        nullptr
    };
    
    ImFont* font = nullptr;
    for (int i = 0; fontPaths[i] != nullptr; i++) {
        // 检查文件是否存在
        FILE* testFile = nullptr;
        if (fopen_s(&testFile, fontPaths[i], "r") == 0 && testFile != nullptr) {
            fclose(testFile);
            // 加载字体，包含中文字符范围
            font = io.Fonts->AddFontFromFileTTF(fontPaths[i], 16.0f, nullptr, 
                                                 io.Fonts->GetGlyphRangesChineseFull());
            if (font != nullptr) {
                std::cout << "成功加载字体: " << fontPaths[i] << std::endl;
                break;
            }
        }
    }
    
    // 如果系统字体加载失败，使用默认字体
    if (font == nullptr) {
        io.Fonts->AddFontDefault();
        std::cout << "使用默认字体（可能不支持中文）" << std::endl;
    }
    
    // 注意：新版本的 ImGui 后端会自动构建字体纹理，不需要手动调用 Build()
    // 字体会在第一次渲染时自动构建

    // 设置美化颜色主题
    ImGui::StyleColorsDark();
    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.ScrollbarRounding = 8.0f;
    style.GrabRounding = 4.0f;
    style.WindowPadding = ImVec2(12.0f, 12.0f);
    style.FramePadding = ImVec2(8.0f, 6.0f);
    style.ItemSpacing = ImVec2(8.0f, 6.0f);
    style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
    style.WindowBorderSize = 0.0f;
    style.FrameBorderSize = 0.0f;
    
    // 自定义颜色
    ImVec4* colors = ImGui::GetStyle().Colors;
    colors[ImGuiCol_WindowBg] = ImVec4(0.08f, 0.08f, 0.10f, 1.00f);
    colors[ImGuiCol_ChildBg] = ImVec4(0.12f, 0.12f, 0.15f, 1.00f);
    colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.20f, 0.25f, 1.00f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.00f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.35f, 1.00f);
    colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.00f);
    colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.15f, 0.18f, 1.00f);

    // 初始化ImGui平台/渲染器绑定
    ImGui_ImplGlfw_InitForOpenGL(window_, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    return true;
}

void ImGuiApp::Shutdown() {
    if (window_) {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window_);
        glfwTerminate();
        window_ = nullptr;
    }
}

void ImGuiApp::BeginFrame() {
    glfwPollEvents();

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
}

void ImGuiApp::EndFrame() {
    ImGui::Render();
    
    int display_w, display_h;
    glfwGetFramebufferSize(window_, &display_w, &display_h);
    glViewport(0, 0, display_w, display_h);
    
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    
    glfwSwapBuffers(window_);
}

void ImGuiApp::Render(const HardwareMonitor& monitor) {
    RenderMainWindow(monitor);
}

void ImGuiApp::RenderMainWindow(const HardwareMonitor& monitor) {
    ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);
    
    // 使用 ## 创建隐藏的唯一ID，避免空ID错误
    ImGui::Begin("DeepInsight Blackwell - 硬件资源监控##MainWindow", nullptr, 
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | 
                 ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // 紧凑标题栏
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 0.8f, 1.0f, 1.0f));
    ImGui::SetWindowFontScale(1.1f);
    ImGui::Text("🚀 DeepInsight Blackwell");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    ImGui::SameLine(ImGui::GetWindowWidth() - 150);
    ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "实时监控");
    ImGui::Separator();
    ImGui::Spacing();

    // 获取数据
    size_t gpuCount = monitor.GetGPUCount();
    const GPUInfo& gpu = gpuCount > 0 ? monitor.GetGPUInfo(0) : GPUInfo();
    const CPUInfo& cpu = monitor.GetCPUInfo();
    const MemoryInfo& mem = monitor.GetMemoryInfo();
    const SystemBandwidthInfo& bandwidth = monitor.GetSystemBandwidthInfo();
    
    // 计算自适应网格大小（根据窗口宽度）
    float windowWidth = ImGui::GetWindowWidth();
    bool showWaitTime = (gpuCount > 0 && gpu.available && gpu.dataTransferWaitTime > 0.0f);
    
    // 计算圆形大小（自适应窗口宽度，第一行3个，第二行2个）
    float circleSize = (windowWidth / 3.0f) * 0.5f; // 基于3列计算，让圆形更紧凑
    circleSize = std::max(100.0f, std::min(180.0f, circleSize)); // 限制在100-180之间

    // 顶部圆形指标 - 两行布局：第一行2个，第二行3个
    if (gpuCount > 0 && gpu.available) {
        // 第一行：2个圆形（居中显示）
        ImGui::Columns(4, "TopMetricsRow1", false);
        ImGui::SetColumnWidth(0, windowWidth / 4.0f); // 左边留空，用于居中
        ImGui::SetColumnWidth(1, windowWidth / 4.0f); // 中间列，放置第一个圆形
        ImGui::SetColumnWidth(2, windowWidth / 4.0f); // 中间列，放置第二个圆形
        ImGui::SetColumnWidth(3, windowWidth / 4.0f); // 右边留空，用于居中
        
        ImGui::NextColumn(); // 跳过第一列（留空）
        
        // GPU利用率 - 圆形显示（标明数据类型）
        DrawCircularProgress("GPU利用率", gpu.utilization, 0.0f, 100.0f, 
                           ImVec2(circleSize, circleSize),
                           GetStatusColor(gpu.utilization, 85.0f, 100.0f), "%");
        ImGui::NextColumn();
        
        // 显存占用 - 圆形显示（标明数据类型）
        DrawCircularProgress("显存占用", gpu.memoryPercent, 0.0f, 100.0f,
                           ImVec2(circleSize, circleSize),
                           GetStatusColor(gpu.memoryPercent, 80.0f, 95.0f, true), "%");
        ImGui::NextColumn();
        
        ImGui::Columns(1);
        ImGui::Spacing();
        
        // 第二行：3个圆形
        ImGui::Columns(3, "TopMetricsRow2", false);
        for (int i = 0; i < 3; i++) {
            ImGui::SetColumnWidth(i, windowWidth / 3.0f);
        }
        
        // CPU利用率 - 圆形显示（标明数据类型）
        DrawCircularProgress("CPU利用率", cpu.utilization, 0.0f, 100.0f,
                           ImVec2(circleSize, circleSize),
                           GetStatusColor(cpu.utilization, 30.0f, 70.0f), "%");
        ImGui::NextColumn();
        
        // 内存使用 - 圆形显示（标明数据类型）
        DrawCircularProgress("内存使用", mem.percent, 0.0f, 100.0f,
                           ImVec2(circleSize, circleSize),
                           GetStatusColor(mem.percent, 0.0f, 80.0f, true), "%");
        ImGui::NextColumn();
        
        // CPU同步到GPU等待时长 - 圆形显示
        //if (showWaitTime) 
        //{
            // 将等待时长转换为百分比显示（0-10ms对应0-100%）
            float maxWaitTime = 10.0f; // 最大参考值10ms
            float waitPercent = (gpu.dataTransferWaitTime / maxWaitTime) * 100.0f;
            waitPercent = std::min(100.0f, waitPercent);
            
            // 根据等待时长选择颜色（反向：等待时间越短越好）
            ImVec4 waitColor;
            if (gpu.dataTransferWaitTime > 5.0f) {
                waitColor = ImVec4(1.0f, 0.3f, 0.3f, 1.0f); // 红色 - 警告
            } else if (gpu.dataTransferWaitTime > 2.0f) {
                waitColor = ImVec4(1.0f, 0.7f, 0.3f, 1.0f); // 橙色 - 注意
            } else {
                waitColor = ImVec4(0.3f, 1.0f, 0.3f, 1.0f); // 绿色 - 正常
            }
            
            DrawCircularProgress("CPU同步到GPU等待时长", waitPercent, 0.0f, 100.0f,
                               ImVec2(circleSize, circleSize),
                               waitColor, "%");
            ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "实际: %.2f ms", gpu.dataTransferWaitTime);
        //} else {
            // 如果没有等待时长数据，显示占位或隐藏
        //    ImGui::Text("");
        //}
        ImGui::NextColumn();
        
        ImGui::Columns(1);
        ImGui::Spacing();
    }

    // 详细信息 - 使用表格布局压缩显示
    if (gpuCount > 0 && gpu.available) {
        // GPU详细信息 - 紧凑表格
        ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.15f, 0.5f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
        if (ImGui::BeginChild("GPUDetails", ImVec2(0, 0), true)) {
            ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "🎮 GPU 详细信息");
            ImGui::Separator();
            
            // 使用表格布局
            if (ImGui::BeginTable("GPUTable", 4, ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp)) {
                ImGui::TableSetupColumn("指标", ImGuiTableColumnFlags_WidthFixed, 100);
                ImGui::TableSetupColumn("数值", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("状态", ImGuiTableColumnFlags_WidthFixed, 80);
                ImGui::TableSetupColumn("历史", ImGuiTableColumnFlags_WidthStretch);
                
                // GPU利用率行
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("计算性能");
                ImGui::TableNextColumn();
                ImGui::TextColored(GetStatusColor(gpu.utilization, 85.0f, 100.0f), "%.1f%%", gpu.utilization);
                ImGui::TableNextColumn();
                ImGui::TextColored(GetStatusColor(gpu.utilization, 85.0f, 100.0f), "%s", 
                                  GetStatusIcon(gpu.utilization, 85.0f, 100.0f));
                ImGui::TableNextColumn();
                if (!gpu.utilizationHistory.empty()) {
                    float maxUtil = *std::max_element(gpu.utilizationHistory.begin(), gpu.utilizationHistory.end());
                    ImGui::PlotLines("##gpu_util_hist", gpu.utilizationHistory.data(), 
                                   static_cast<int>(gpu.utilizationHistory.size()),
                                   0, nullptr, 0.0f, 100.0f, ImVec2(-1, 30));
                }
                
                // 显存行
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("显存占用");
                ImGui::TableNextColumn();
                ImGui::TextColored(GetStatusColor(gpu.memoryPercent, 80.0f, 95.0f, true),
                                  "%.0f MB / %.0f MB", gpu.memoryUsed, gpu.memoryTotal);
                ImGui::TableNextColumn();
                ImGui::TextColored(GetStatusColor(gpu.memoryPercent, 80.0f, 95.0f, true), "%.1f%%", gpu.memoryPercent);
                ImGui::TableNextColumn();
                if (!gpu.memoryHistory.empty()) {
                    ImGui::PlotLines("##gpu_mem_hist", gpu.memoryHistory.data(),
                                   static_cast<int>(gpu.memoryHistory.size()),
                                   0, nullptr, 0.0f, 100.0f, ImVec2(-1, 30));
                }
                
                // 温度行
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("温度");
                ImGui::TableNextColumn();
                ImVec4 tempColor = gpu.temperature > 80.0f ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : 
                                  gpu.temperature > 70.0f ? ImVec4(1.0f, 0.7f, 0.3f, 1.0f) : 
                                  ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
                ImGui::TextColored(tempColor, "%.1f °C", gpu.temperature);
                ImGui::TableNextColumn();
                if (gpu.temperature > 80.0f) ImGui::TextColored(tempColor, "🔥");
                ImGui::TableNextColumn();
                if (!gpu.temperatureHistory.empty()) {
                    float maxTemp = *std::max_element(gpu.temperatureHistory.begin(), gpu.temperatureHistory.end());
                    ImGui::PlotLines("##gpu_temp_hist", gpu.temperatureHistory.data(),
                                   static_cast<int>(gpu.temperatureHistory.size()),
                                   0, nullptr, 0.0f, maxTemp * 1.2f, ImVec2(-1, 30));
                }
                
                // PCIe带宽行
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("PCIe带宽");
                ImGui::TableNextColumn();
                // 计算实时带宽大小（GB/s）
                float realTimeBandwidth = (gpu.pcieRxThroughput + gpu.pcieTxThroughput) / 1024.0f; // 转换为GB/s
                float pcieUtil = 0.0f;
                if (gpu.pcieBandwidth > 0.0f) {
                    pcieUtil = (realTimeBandwidth / gpu.pcieBandwidth) * 100.0f;
                }
                // 显示：理论带宽 | 实时带宽 | 利用率
                ImGui::Text("理论: %.2f GB/s | 实时: %.2f GB/s", gpu.pcieBandwidth, realTimeBandwidth);
                ImGui::SameLine();
                ImGui::TextColored(GetStatusColor(pcieUtil, 0.0f, 80.0f, true), "(%.1f%%)", pcieUtil);
                ImGui::TableNextColumn();
                ImGui::TextColored(GetStatusColor(pcieUtil, 0.0f, 80.0f, true), "%.1f%%", pcieUtil);
                ImGui::TableNextColumn();
                if (!gpu.pcieRxHistory.empty() && !gpu.pcieTxHistory.empty()) {
                    std::vector<float> combined;
                    size_t maxSize = std::max(gpu.pcieRxHistory.size(), gpu.pcieTxHistory.size());
                    for (size_t i = 0; i < maxSize; i++) {
                        float rx = (i < gpu.pcieRxHistory.size()) ? gpu.pcieRxHistory[i] : 0.0f;
                        float tx = (i < gpu.pcieTxHistory.size()) ? gpu.pcieTxHistory[i] : 0.0f;
                        combined.push_back(rx + tx);
                    }
                    if (!combined.empty()) {
                        float maxThroughput = *std::max_element(combined.begin(), combined.end());
                        ImGui::PlotLines("##pcie_hist", combined.data(), static_cast<int>(combined.size()),
                                       0, nullptr, 0.0f, maxThroughput * 1.2f, ImVec2(-1, 30));
                    }
                }
                
                // 电压行
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("电压");
                ImGui::TableNextColumn();
                if (gpu.currentVoltage > 0.0f && gpu.maxVoltage > 0.0f) {
                    // 显示：最大电压 | 实时电压 | 百分比
                    ImGui::Text("最大: %.0f mV | 实时: %.0f mV", gpu.maxVoltage, gpu.currentVoltage);
                    ImGui::SameLine();
                    ImVec4 voltageColor = GetStatusColor(gpu.voltagePercent, 0.0f, 90.0f, true);
                    ImGui::TextColored(voltageColor, "(%.1f%%)", gpu.voltagePercent);
                } else {
                    ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "不可用");
                }
                ImGui::TableNextColumn();
                if (gpu.voltagePercent > 0.0f) {
                    ImVec4 voltageColor = GetStatusColor(gpu.voltagePercent, 0.0f, 90.0f, true);
                    ImGui::TextColored(voltageColor, "%.1f%%", gpu.voltagePercent);
                } else {
                    ImGui::Text("-");
                }
                ImGui::TableNextColumn();
                // 电压历史图表（如果有历史数据的话，可以添加）
                ImGui::Text("-");
                
                ImGui::EndTable();
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    // CPU和内存详细信息 - 并排显示
    ImGui::Columns(2, "CPUMem", false);
    
    // CPU详细信息
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.15f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    if (ImGui::BeginChild("CPUInfo", ImVec2(0, 0), true)) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "⚡ CPU 详细信息");
        ImGui::Separator();
        
        ImVec4 cpuColor = GetStatusColor(cpu.utilization, 30.0f, 70.0f);
        ImGui::Text("CPU 利用率: ");
        ImGui::SameLine();
        ImGui::TextColored(cpuColor, "%.1f%%", cpu.utilization);
        ImGui::SameLine();
        ImGui::TextColored(cpuColor, " %s", GetStatusIcon(cpu.utilization, 30.0f, 70.0f));
        
        // 状态提示
        ImGui::Spacing();
        ImGui::Separator();
        if (cpu.utilization > 90.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "⚠️ 警告: CPU 预处理压力大，建议增加 num_workers");
        } else if (cpu.utilization > 30.0f && cpu.utilization < 70.0f) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ CPU 使用正常");
        } else if (cpu.utilization < 30.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "💡 提示: CPU 使用率较低");
        }
        
        // CPU利用率历史图表
        ImGui::Spacing();
        if (!cpu.utilizationHistory.empty()) {
            DrawHistoryChart("CPU利用率历史", cpu.utilizationHistory, 0.0f, 100.0f, "%");
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    
    ImGui::NextColumn();
    
    // 内存详细信息
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.15f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    if (ImGui::BeginChild("MemInfo", ImVec2(0, 0), true)) {
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 0.4f, 1.0f), "💾 内存详细信息");
        ImGui::Separator();
        
        ImVec4 memColor = GetStatusColor(mem.percent, 0.0f, 80.0f, true);
        ImGui::Text("内存使用: ");
        ImGui::SameLine();
        ImGui::TextColored(memColor, "%.2f GB / %.2f GB (%.1f%%)", 
                          mem.used, mem.total, mem.percent);
        ImGui::SameLine();
        ImGui::TextColored(memColor, " %s", GetStatusIcon(mem.percent, 0.0f, 80.0f));
        
        ImGui::Spacing();
        ImGui::Text("💾 可用内存: %.2f GB", mem.available);
        
        // 状态提示
        ImGui::Spacing();
        if (mem.percent > 95.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 
                              "🚨 警告: 内存使用过高，可能出现 Swap，导致性能断崖式下跌！");
        } else if (mem.percent > 80.0f) {
            ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "💡 提示: 内存使用较高，请注意监控");
        } else {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ 内存使用正常");
        }
        
        // 内存使用历史图表
        ImGui::Spacing();
        if (!mem.percentHistory.empty()) {
            DrawHistoryChart("内存使用历史", mem.percentHistory, 0.0f, 100.0f, "%");
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    
    ImGui::Columns(1);
    ImGui::Spacing();

    // 主机带宽模块 - 使用圆形图表显示
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.15f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    if (ImGui::BeginChild("Bandwidth", ImVec2(0, 0), true)) {
        RenderSystemBandwidthInfo(bandwidth);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // 诊断建议详细信息
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.15f, 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    if (ImGui::BeginChild("Diagnosis", ImVec2(0, 0), true)) {
        RenderDiagnosis(monitor);
    }
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImGui::End();
}

void ImGuiApp::RenderGPUInfo(const GPUInfo& gpu, int index) {
    if (!gpu.available) {
        ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "❌ GPU %d: 不可用", index);
        return;
    }

    // GPU利用率 - 美化显示
    ImVec4 utilColor = GetStatusColor(gpu.utilization, 85.0f, 100.0f);
    ImGui::Text("计算性能: ");
    ImGui::SameLine();
    ImGui::TextColored(utilColor, "%.1f%%", gpu.utilization);
    ImGui::SameLine();
    ImGui::TextColored(utilColor, " %s", GetStatusIcon(gpu.utilization, 85.0f, 100.0f));
    DrawProgressBar("##gpu_util", gpu.utilization, 0.0f, 100.0f, "%",
                   gpu.utilization > 85.0f ? IM_COL32(0, 255, 0, 255) : 
                   gpu.utilization < 70.0f ? IM_COL32(255, 0, 0, 255) : 
                   IM_COL32(255, 255, 0, 255));
    
    // 状态提示
    if (gpu.utilization > 85.0f) {
        ImGui::SameLine();
        ImGui::Text("  ✓ 充分利用");
    } else if (gpu.utilization < 70.0f) {
        ImGui::SameLine();
        ImGui::Text("  ✗ 负载不足");
    }

    // 显存信息 - 美化显示
    ImVec4 memColor = GetStatusColor(gpu.memoryPercent, 80.0f, 95.0f, true);
    ImGui::Text("显存状态: ");
    ImGui::SameLine();
    ImGui::TextColored(memColor, "%.0f MB / %.0f MB (%.1f%%)", 
                      gpu.memoryUsed, gpu.memoryTotal, gpu.memoryPercent);
    ImGui::SameLine();
    ImGui::TextColored(memColor, " %s", GetStatusIcon(gpu.memoryPercent, 80.0f, 95.0f));
    DrawProgressBar("##gpu_mem", gpu.memoryPercent, 0.0f, 100.0f, "%",
                   IM_COL32(memColor.x * 255, memColor.y * 255, memColor.z * 255, 255));

    // 显存状态提示
    ImGui::Spacing();
    if (gpu.memoryPercent < 50.0f) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "💡 提示: 显存占用过低，建议增大 batch_size");
    } else if (gpu.memoryPercent > 95.0f) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "⚠️ 警告: 显存接近满载，可能出现 OOM");
    } else if (gpu.memoryPercent > 80.0f) {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ 显存使用合理");
    }

    // 温度 - 美化显示
    ImGui::Spacing();
    ImVec4 tempColor = gpu.temperature > 80.0f ? ImVec4(1.0f, 0.3f, 0.3f, 1.0f) : 
                      gpu.temperature > 70.0f ? ImVec4(1.0f, 0.7f, 0.3f, 1.0f) : 
                      ImVec4(0.3f, 1.0f, 0.3f, 1.0f);
    ImGui::Text("🌡️ 温度: ");
    ImGui::SameLine();
    ImGui::TextColored(tempColor, "%.1f °C", gpu.temperature);
    if (gpu.temperature > 80.0f) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), " 🔥 高温警告");
    }

    // PCIe 带宽信息
    ImGui::Separator();
    ImGui::Text("PCIe 带宽信息:");
    ImGui::Text("  链路宽度: x%d", gpu.pcieLinkWidth);
    ImGui::Text("  链路速度: PCIe %d.0 (%.1f GT/s)", gpu.pcieLinkSpeed, 
                static_cast<float>(gpu.pcieLinkSpeed) * 2.5f);
    ImGui::Text("  理论带宽: %.2f GB/s", gpu.pcieBandwidth);
    ImGui::Text("  接收吞吐量: %.2f MB/s", gpu.pcieRxThroughput);
    ImGui::Text("  发送吞吐量: %.2f MB/s", gpu.pcieTxThroughput);
    
    // PCIe 吞吐量利用率
    if (gpu.pcieBandwidth > 0.0f) {
        float pcieUtilPercent = ((gpu.pcieRxThroughput + gpu.pcieTxThroughput) / 1024.0f) / gpu.pcieBandwidth * 100.0f;
        ImGui::Text("  PCIe 利用率: %.1f%%", pcieUtilPercent);
        DrawProgressBar("##pcie_util", pcieUtilPercent, 0.0f, 100.0f, "%",
                       pcieUtilPercent > 80.0f ? IM_COL32(255, 0, 0, 255) :
                       pcieUtilPercent > 50.0f ? IM_COL32(255, 165, 0, 255) :
                       IM_COL32(0, 255, 0, 255));
    }
    
    // 数据传输等待时间
    if (gpu.dataTransferWaitTime > 0.0f) {
        ImGui::Text("  CPU→GPU 等待时间: %.2f ms", gpu.dataTransferWaitTime);
        if (gpu.dataTransferWaitTime > 5.0f) {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), "  (较高)");
        }
    } else {
        ImGui::Text("  CPU→GPU 等待时间: < 0.1 ms");
    }

    // PCIe 吞吐量历史图表
    if (!gpu.pcieRxHistory.empty() || !gpu.pcieTxHistory.empty()) {
        ImGui::Text("PCIe 吞吐量历史:");
        // 合并显示接收和发送
        std::vector<float> combinedThroughput;
        size_t maxSize = std::max(gpu.pcieRxHistory.size(), gpu.pcieTxHistory.size());
        combinedThroughput.reserve(maxSize);
        for (size_t i = 0; i < maxSize; i++) {
            float rx = (i < gpu.pcieRxHistory.size()) ? gpu.pcieRxHistory[i] : 0.0f;
            float tx = (i < gpu.pcieTxHistory.size()) ? gpu.pcieTxHistory[i] : 0.0f;
            combinedThroughput.push_back(rx + tx);
        }
        if (!combinedThroughput.empty()) {
            float maxThroughput = *std::max_element(combinedThroughput.begin(), combinedThroughput.end());
            DrawHistoryChart("PCIe吞吐量", combinedThroughput, 0.0f, 
                           maxThroughput > 0 ? maxThroughput * 1.2f : 1000.0f, "MB/s");
        }
    }

    // 利用率历史图表
    if (!gpu.utilizationHistory.empty()) {
        DrawHistoryChart("GPU利用率历史", gpu.utilizationHistory, 0.0f, 100.0f, "%");
    }

    // 显存使用历史图表
    if (!gpu.memoryHistory.empty()) {
        DrawHistoryChart("显存使用历史", gpu.memoryHistory, 0.0f, 100.0f, "%");
    }
}

void ImGuiApp::RenderCPUInfo(const CPUInfo& cpu) {
    ImGui::Text("CPU 利用率: %.1f%%", cpu.utilization);
    DrawProgressBar("##cpu_util", cpu.utilization, 0.0f, 100.0f, "%",
                   cpu.utilization > 90.0f ? IM_COL32(255, 0, 0, 255) :
                   cpu.utilization > 70.0f ? IM_COL32(255, 165, 0, 255) :
                   cpu.utilization > 30.0f ? IM_COL32(0, 255, 0, 255) :
                   IM_COL32(200, 200, 200, 255));

    // 状态提示
    if (cpu.utilization > 90.0f) {
        ImGui::Text("  警告: CPU 预处理压力大，建议增加 num_workers");
    } else if (cpu.utilization > 30.0f && cpu.utilization < 70.0f) {
        ImGui::Text("  ✓ CPU 使用正常");
    } else if (cpu.utilization < 30.0f) {
        ImGui::Text("  提示: CPU 使用率较低");
    }

    // CPU利用率历史图表
    if (!cpu.utilizationHistory.empty()) {
        DrawHistoryChart("CPU利用率历史", cpu.utilizationHistory, 0.0f, 100.0f, "%");
    }
}

void ImGuiApp::RenderMemoryInfo(const MemoryInfo& memory) {
    // 内存使用 - 美化显示
    ImVec4 memColor = GetStatusColor(memory.percent, 0.0f, 80.0f, true);
    ImGui::Text("使用情况: ");
    ImGui::SameLine();
    ImGui::TextColored(memColor, "%.2f GB / %.2f GB (%.1f%%)", 
                      memory.used, memory.total, memory.percent);
    ImGui::SameLine();
    ImGui::TextColored(memColor, " %s", GetStatusIcon(memory.percent, 0.0f, 80.0f));
    DrawProgressBar("##mem_util", memory.percent, 0.0f, 100.0f, "%",
                   IM_COL32(memColor.x * 255, memColor.y * 255, memColor.z * 255, 255));

    ImGui::Spacing();
    ImGui::Text("💾 可用内存: %.2f GB", memory.available);

    // 状态提示
    ImGui::Spacing();
    if (memory.percent > 95.0f) {
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), 
                          "🚨 警告: 内存使用过高，可能出现 Swap，导致性能断崖式下跌！");
    } else if (memory.percent > 80.0f) {
        ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.0f, 1.0f), "💡 提示: 内存使用较高，请注意监控");
    } else {
        ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ 内存使用正常");
    }

    // 内存使用历史图表
    if (!memory.percentHistory.empty()) {
        DrawHistoryChart("内存使用历史", memory.percentHistory, 0.0f, 100.0f, "%");
    }
}

void ImGuiApp::RenderSystemBandwidthInfo(const SystemBandwidthInfo& bandwidth) {
    ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "🌐 主机带宽模块");
    ImGui::Separator();
    
    ImGui::Text("总系统带宽: ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%.2f GB/s", bandwidth.totalSystemBandwidth);
    
    ImGui::Separator();
    ImGui::Text("内存带宽:");
    ImGui::Text("  带宽: %.2f GB/s", bandwidth.memoryBandwidth);
    ImGui::Text("  类型: %s", bandwidth.memoryType.c_str());
    ImGui::Text("  速度: %d MHz", bandwidth.memorySpeed);
    
    ImGui::Separator();
    ImGui::Text("PCIe 总带宽: ");
    ImGui::SameLine();
    ImGui::TextColored(ImVec4(0.2f, 0.6f, 1.0f, 1.0f), "%.2f GB/s", bandwidth.pcieTotalBandwidth);
    
    // 总带宽历史图表
    if (!bandwidth.totalBandwidthHistory.empty()) {
        float maxBandwidth = *std::max_element(bandwidth.totalBandwidthHistory.begin(), 
                                               bandwidth.totalBandwidthHistory.end());
        DrawHistoryChart("总系统带宽历史", bandwidth.totalBandwidthHistory, 0.0f, 
                        maxBandwidth > 0 ? maxBandwidth * 1.2f : 100.0f, "GB/s");
    }
}

void ImGuiApp::RenderDiagnosis(const HardwareMonitor& monitor) {
    size_t gpuCount = monitor.GetGPUCount();
    bool hasIssue = false;

    if (gpuCount > 0) {
        for (size_t i = 0; i < gpuCount; i++) {
            const GPUInfo& gpu = monitor.GetGPUInfo(i);
            if (!gpu.available) continue;

            const CPUInfo& cpu = monitor.GetCPUInfo();

            // 诊断逻辑
            if (gpu.utilization < 70.0f && cpu.utilization > 90.0f) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
                                  "【GPU %d】检测到 CPU 瓶颈:", i);
                ImGui::Text("  GPU 在等数据，请增加 DataLoader 的 num_workers 或优化数据增强代码。");
                hasIssue = true;
            } else if (gpu.utilization < 70.0f && gpu.memoryPercent < 50.0f) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
                                  "【GPU %d】检测到计算未饱和:", i);
                ImGui::Text("  请尝试增大 batch_size 以提升并行度。");
                hasIssue = true;
            } else if (gpu.utilization < 70.0f && gpu.memoryPercent > 90.0f) {
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.0f, 1.0f), 
                                  "【GPU %d】显存已满但利用率低:", i);
                ImGui::Text("  可能是复杂的循环计算、频繁的数据拷贝或小尺寸数据的频繁计算。");
                hasIssue = true;
            }
        }
    }

    if (!hasIssue && gpuCount > 0) {
        const GPUInfo& gpu = monitor.GetGPUInfo(0);
        if (gpu.available && gpu.utilization > 85.0f && 
            gpu.memoryPercent > 80.0f && gpu.memoryPercent < 95.0f &&
            monitor.GetCPUInfo().utilization > 30.0f && 
            monitor.GetCPUInfo().utilization < 70.0f) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "✓ 硬件资源使用状态良好！");
        }
    }

    // 通用建议
    ImGui::Spacing();
    ImGui::Text("建议:");
    ImGui::BulletText("GPU 利用率应保持在 85%% - 100%%");
    ImGui::BulletText("显存占用应保持在 80%% - 95%%");
    ImGui::BulletText("CPU 利用率应在 30%% - 70%%");
    ImGui::BulletText("内存使用应保持充足余量（< 95%%）");
}

void ImGuiApp::DrawProgressBar(const char* label, float value, float min, float max, 
                               const char* suffix, unsigned int  color) {
    float normalized = (value - min) / (max - min);
    normalized = std::max(0.0f, std::min(1.0f, normalized));

    // 使用 label 作为 ProgressBar 的 ID
    ImGui::ProgressBar(normalized, ImVec2(-1, 0), label);
    ImGui::SameLine(0, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::Text("%.1f%s", value, suffix);
    
    // 绘制颜色条（需要手动绘制）
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 pos = ImGui::GetItemRectMin();
    ImVec2 size = ImGui::GetItemRectSize();
    ImVec2 p_min = ImVec2(pos.x, pos.y - ImGui::GetFrameHeight());
    ImVec2 p_max = ImVec2(pos.x + size.x * normalized, pos.y);
    draw_list->AddRectFilled(p_min, p_max, color);
}

// 绘制圆形进度条
void ImGuiApp::DrawCircularProgress(const char* label, float value, float min, float max,
                                   const ImVec2& size, const ImVec4& color, const char* unit) {
    // 定义 PI 常量（如果未定义）
    #ifndef M_PI
    #define M_PI 3.14159265358979323846f
    #endif
    
    float normalized = (value - min) / (max - min);
    normalized = std::max(0.0f, std::min(1.0f, normalized));
    
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();
    ImVec2 center = ImVec2(canvas_pos.x + size.x * 0.5f, canvas_pos.y + size.y * 0.5f);
    float radius = std::min(size.x, size.y) * 0.35f;
    float thickness = radius * 0.2f;
    
    // 绘制背景圆环
    draw_list->AddCircle(center, radius, IM_COL32(40, 40, 50, 255), 64, thickness);
    
    // 绘制进度圆环（使用Arc）
    float start_angle = -(M_PI / 2.0f); // 从顶部开始 (-90度)
    float end_angle = start_angle + normalized * 2.0f * M_PI;
    
    // 使用状态颜色
    ImU32 progress_color = IM_COL32(color.x * 255, color.y * 255, color.z * 255, 255);
    
    // 绘制进度弧
    if (normalized > 0.0f) {
        int num_segments = (int)(64 * normalized);
        for (int i = 0; i < num_segments; i++) {
            float a1 = start_angle + (end_angle - start_angle) * (float(i) / num_segments);
            float a2 = start_angle + (end_angle - start_angle) * (float(i + 1) / num_segments);
            
            ImVec2 p1 = ImVec2(
                center.x + radius * cosf(a1),
                center.y + radius * sinf(a1)
            );
            ImVec2 p2 = ImVec2(
                center.x + radius * cosf(a2),
                center.y + radius * sinf(a2)
            );
            
            draw_list->AddLine(p1, p2, progress_color, thickness);
        }
    }
    
    // 绘制中心文本区域
    ImVec2 text_pos = ImVec2(canvas_pos.x + size.x * 0.5f, canvas_pos.y + size.y * 0.5f - 8);
    ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x, text_pos.y));
    
    // 标签
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(color.x, color.y, color.z, 0.8f));
    ImGui::SetWindowFontScale(0.7f);
    float label_width = ImGui::CalcTextSize(label).x;
    ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x + (size.x - label_width) * 0.5f, text_pos.y));
    ImGui::Text("%s", label);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    
    // 数值
    text_pos.y += 18;
    ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x, text_pos.y));
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::SetWindowFontScale(1.3f);
    char value_text[32];
    snprintf(value_text, sizeof(value_text), "%.0f%s", value, unit);
    float value_width = ImGui::CalcTextSize(value_text).x;
    ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x + (size.x - value_width) * 0.5f, text_pos.y));
    ImGui::Text("%s", value_text);
    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleColor();
    
    // 设置下一个控件位置
    ImGui::SetCursorScreenPos(ImVec2(canvas_pos.x, canvas_pos.y + size.y + ImGui::GetStyle().ItemSpacing.y));
}

void ImGuiApp::DrawHistoryChart(const char* label, const std::vector<float>& history, 
                                float scaleMin, float scaleMax, const char* unit) {
    if (history.empty()) return;

    ImGui::Text("%s", label);
    
    // 使用 label 作为 PlotLines 的 ID，确保每个图表都有唯一的 ID
    // ImGui 要求每个控件都必须有唯一的 ID，不能使用空字符串
    std::string plotId = std::string(label) + "##Plot";
    ImGui::PlotLines(plotId.c_str(), history.data(), static_cast<int>(history.size()), 
                     0, nullptr, scaleMin, scaleMax, 
                     ImVec2(ImGui::GetContentRegionAvail().x, 60.0f));
    
    // 显示当前值
    float current = history.back();
    ImGui::Text("当前: %.1f%s  最小: %.1f%s  最大: %.1f%s", 
                current, unit,
                *std::min_element(history.begin(), history.end()), unit,
                *std::max_element(history.begin(), history.end()), unit);
}

bool ImGuiApp::ShouldClose() const {
    return window_ ? glfwWindowShouldClose(window_) : true;
}

// 绘制卡片容器（使用函数指针而不是 std::function 以避免头文件依赖）
void ImGuiApp::DrawCard(const char* title, const ImVec4& color, std::function<void()> content) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(color.x * 0.15f, color.y * 0.15f, color.z * 0.15f, 0.3f));
    ImGui::PushStyleColor(ImGuiCol_Border, color);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.5f);
    
    std::string childId = std::string(title) + "##Card";
    if (ImGui::BeginChild(childId.c_str(), ImVec2(0, 0), true, ImGuiWindowFlags_None)) {
        // 标题
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s", title);
        ImGui::PopStyleColor();
        ImGui::Separator();
        
        // 内容
        content();
    }
    ImGui::EndChild();
    
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

// 绘制指标卡片
void ImGuiApp::DrawMetricCard(const char* icon, const char* label, float value, const char* unit, 
                              const ImVec4& color, float minVal, float maxVal) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(color.x * 0.1f, color.y * 0.1f, color.z * 0.1f, 0.2f));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 8.0f));
    
    std::string childId = std::string(label) + "##Metric";
    if (ImGui::BeginChild(childId.c_str(), ImVec2(0, 80), true)) {
        // 图标和标签
        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::Text("%s %s", icon, label);
        ImGui::PopStyleColor();
        
        // 数值
        ImGui::PushFont(ImGui::GetIO().Fonts->Fonts[0]);
        ImGui::SetWindowFontScale(1.5f);
        ImGui::TextColored(color, "%.1f %s", value, unit);
        ImGui::SetWindowFontScale(1.0f);
        ImGui::PopFont();
        
        // 进度条
        float normalized = (value - minVal) / (maxVal - minVal);
        normalized = std::max(0.0f, std::min(1.0f, normalized));
        ImGui::ProgressBar(normalized, ImVec2(-1, 4), "");
    }
    ImGui::EndChild();
    
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(1);
}

// 获取状态颜色
ImVec4 ImGuiApp::GetStatusColor(float value, float goodMin, float goodMax, bool reverse) {
    if (reverse) {
        if (value >= goodMin && value <= goodMax) return ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // 绿色-好
        if (value < goodMin) return ImVec4(0.8f, 0.2f, 0.2f, 1.0f); // 红色-差
        return ImVec4(1.0f, 0.6f, 0.0f, 1.0f); // 橙色-警告
    } else {
        if (value >= goodMin && value <= goodMax) return ImVec4(0.2f, 0.8f, 0.2f, 1.0f); // 绿色-好
        if (value > goodMax) return ImVec4(0.8f, 0.2f, 0.2f, 1.0f); // 红色-差
        return ImVec4(1.0f, 0.6f, 0.0f, 1.0f); // 橙色-警告
    }
}

// 获取状态图标
const char* ImGuiApp::GetStatusIcon(float value, float goodMin, float goodMax) {
    if (value >= goodMin && value <= goodMax) return "✓";
    if (value < goodMin) return "⚠";
    return "✗";
}

