#pragma once

#include <string>
#include "HardwareMonitor.h"
#include <functional>
#include "imgui.h"



struct GLFWwindow;

class ImGuiApp {
public:
    ImGuiApp(const std::string& title, int width, int height);
    ~ImGuiApp();

    bool Initialize();
    void Shutdown();
    
    void BeginFrame();
    void EndFrame();
    void Render(const HardwareMonitor& monitor);
    
    bool ShouldClose() const;

private:
    void RenderMainWindow(const HardwareMonitor& monitor);
    void RenderGPUInfo(const GPUInfo& gpu, int index);
    void RenderCPUInfo(const CPUInfo& cpu);
    void RenderMemoryInfo(const MemoryInfo& memory);
    void RenderSystemBandwidthInfo(const SystemBandwidthInfo& bandwidth);
    void RenderDiagnosis(const HardwareMonitor& monitor);
    void DrawProgressBar(const char* label, float value, float min, float max, 
                        const char* suffix = "%", unsigned int  color = 0);
    void DrawCircularProgress(const char* label, float value, float min, float max, 
                             const ImVec2& size, const ImVec4& color, const char* unit = "%");
    void DrawHistoryChart(const char* label, const std::vector<float>& history, 
                         float scaleMin, float scaleMax, const char* unit = "%");
    void DrawCard(const char* title, const ImVec4& color, std::function<void()> content);
    void DrawMetricCard(const char* icon, const char* label, float value, const char* unit, 
                       const ImVec4& color, float minVal = 0.0f, float maxVal = 100.0f);
    void DrawCompactMetric(const char* icon, const char* label, float value, const char* unit,
                          const ImVec4& color, float minVal = 0.0f, float maxVal = 100.0f);
    ImVec4 GetStatusColor(float value, float goodMin, float goodMax, bool reverse = false);
    const char* GetStatusIcon(float value, float goodMin, float goodMax);

    std::string title_;
    int width_;
    int height_;
    GLFWwindow* window_ = nullptr;
};

