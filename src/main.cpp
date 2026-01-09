#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <sstream>
#include <iomanip>
#include "HardwareMonitor.h"
#include "ImGuiApp.h"

int main() {
    try {
        // 初始化硬件监控
        HardwareMonitor monitor;
        if (!monitor.Initialize()) {
            std::cerr << "硬件监控初始化失败！" << std::endl;
            return -1;
        }

        // 初始化图形界面
        ImGuiApp app("DeepInsight Blackwell - 硬件资源监控", 1280, 720);
        if (!app.Initialize()) {
            std::cerr << "图形界面初始化失败！" << std::endl;
            return -1;
        }

        // 主循环
        while (!app.ShouldClose()) {
            // 更新硬件信息
            monitor.Update();

            // 渲染界面
            app.BeginFrame();
            app.Render(monitor);
            app.EndFrame();

            // 控制更新频率（约60 FPS）
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }

        app.Shutdown();
        monitor.Shutdown();
    }
    catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return -1;
    }

    return 0;
}

