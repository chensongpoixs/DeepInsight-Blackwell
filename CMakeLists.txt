cmake_minimum_required(VERSION 3.15)
project(DeepInsightBlackwell)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 设置路径
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/imgui)
set(GLFW_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/glfw)



add_definitions(-DNOMINMAX)


# GLFW
if(EXISTS ${GLFW_DIR}/CMakeLists.txt)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    add_subdirectory(${GLFW_DIR} ${CMAKE_BINARY_DIR}/glfw)
else()
    message(FATAL_ERROR "GLFW not found at ${GLFW_DIR}. Please download GLFW to third_party/glfw")
endif()

# ImGui源文件
file(GLOB IMGUI_SOURCES
    "${IMGUI_DIR}/*.cpp"
)

# ImGui后端绑定文件
if(EXISTS ${IMGUI_DIR}/backends)
    list(APPEND IMGUI_SOURCES
        "${IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
        "${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
    )
    set(IMGUI_BACKENDS_DIR ${IMGUI_DIR}/backends)
elseif(EXISTS ${IMGUI_DIR}/examples)
    list(APPEND IMGUI_SOURCES
        "${IMGUI_DIR}/examples/imgui_impl_glfw.cpp"
        "${IMGUI_DIR}/examples/imgui_impl_opengl3.cpp"
    )
    set(IMGUI_BACKENDS_DIR ${IMGUI_DIR}/examples)
else()
    message(FATAL_ERROR "ImGui backends not found. Please check ImGui installation.")
endif()

# 创建ImGui静态库
add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC
    ${IMGUI_DIR}
    ${IMGUI_BACKENDS_DIR}
)

target_link_libraries(imgui PUBLIC glfw)

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${IMGUI_DIR}
    ${IMGUI_BACKENDS_DIR}
    ${GLFW_DIR}/include
)

# 应用源文件
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
    "src/*.h"
    "src/*.hpp"
)

# 可执行文件
add_executable(${PROJECT_NAME} ${SOURCES})

# 链接库
target_link_libraries(${PROJECT_NAME}
    imgui
    glfw
    opengl32
)

# Windows特定设置
if(WIN32)
    add_definitions(-DWIN32_LEAN_AND_MEAN)
    
    # NVML (NVIDIA Management Library) - 通过CUDA环境变量查找
    # 优先查找CUDA_PATH环境变量（CUDA安装路径）
    if(NOT DEFINED CUDA_PATH)
        set(CUDA_PATH $ENV{CUDA_PATH})
    endif()
    
    # 如果CUDA_PATH未设置，尝试查找常见CUDA路径
    if(NOT CUDA_PATH)
        set(POSSIBLE_CUDA_PATHS
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.0"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.7"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.6"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.5"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.4"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.3"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.2"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.1"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.0"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v10.2"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v10.1"
            "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v10.0"
        )
        foreach(CUDA_PATH_CANDIDATE ${POSSIBLE_CUDA_PATHS})
            if(EXISTS ${CUDA_PATH_CANDIDATE})
                set(CUDA_PATH ${CUDA_PATH_CANDIDATE})
                break()
            endif()
        endforeach()
    endif()
    
    # 如果找到CUDA路径，使用其中的NVML
    if(CUDA_PATH AND EXISTS ${CUDA_PATH})
        message(STATUS "Found CUDA installation: ${CUDA_PATH}")
        
        # NVML头文件路径
        set(NVML_INCLUDE_DIR "${CUDA_PATH}/include")
        if(EXISTS ${NVML_INCLUDE_DIR}/nvml.h)
            target_include_directories(${PROJECT_NAME} PRIVATE ${NVML_INCLUDE_DIR})
            message(STATUS "Found NVML headers: ${NVML_INCLUDE_DIR}")
        else()
            message(WARNING "NVML header not found at ${NVML_INCLUDE_DIR}/nvml.h")
        endif()
        
        # NVML库文件路径（Windows x64优先）
        set(NVML_LIB_DIR "${CUDA_PATH}/lib/x64")
        if(NOT EXISTS ${NVML_LIB_DIR})
            set(NVML_LIB_DIR "${CUDA_PATH}/lib/Win32")
        endif()
        
        # 查找NVML库
        find_library(NVML_LIB 
            NAMES nvml
            PATHS 
            "${CUDA_PATH}/lib/x64"
            "${CUDA_PATH}/lib/Win32"
            NO_DEFAULT_PATH
        )
        
        if(NVML_LIB)
            target_link_libraries(${PROJECT_NAME} ${NVML_LIB})
            message(STATUS "Found NVML library: ${NVML_LIB}")
        else()
            message(WARNING "NVML library not found in ${NVML_LIB_DIR}. Attempting system paths...")
            # 尝试通过系统路径查找
            find_library(NVML_LIB nvml
                PATHS
                "C:/Program Files/NVIDIA Corporation/NVSMI"
                "C:/Windows/System32"
            )
            if(NVML_LIB)
                target_link_libraries(${PROJECT_NAME} ${NVML_LIB})
                message(STATUS "Found NVML library in system path: ${NVML_LIB}")
            else()
                message(WARNING "NVML library not found. GPU monitoring may not work.")
                # 尝试链接标准NVML库名（可能会在链接时找到）
                target_link_libraries(${PROJECT_NAME} nvml)
            endif()
        endif()
    else()
        message(WARNING "CUDA_PATH not found. Attempting to find NVML in system paths...")
        # 回退到系统路径查找
        find_library(NVML_LIB nvml
            PATHS
            "C:/Program Files/NVIDIA Corporation/NVSMI"
            "C:/Windows/System32"
            ENV PATH
        )
        if(NVML_LIB)
            target_link_libraries(${PROJECT_NAME} ${NVML_LIB})
            message(STATUS "Found NVML library: ${NVML_LIB}")
            # 尝试从库路径推断头文件位置
            get_filename_component(NVML_DIR ${NVML_LIB} DIRECTORY)
            get_filename_component(NVML_PARENT_DIR ${NVML_DIR} DIRECTORY)
            set(NVML_INCLUDE_CANDIDATE "${NVML_PARENT_DIR}/include")
            if(EXISTS ${NVML_INCLUDE_CANDIDATE}/nvml.h)
                target_include_directories(${PROJECT_NAME} PRIVATE ${NVML_INCLUDE_CANDIDATE})
                message(STATUS "Found NVML headers: ${NVML_INCLUDE_CANDIDATE}")
            else()
                # 尝试在CUDA标准路径查找头文件
                set(CUDA_INCLUDE_CANDIDATES
                    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v12.0/include"
                    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.8/include"
                    "C:/Program Files/NVIDIA GPU Computing Toolkit/CUDA/v11.7/include"
                )
                foreach(INCLUDE_CANDIDATE ${CUDA_INCLUDE_CANDIDATES})
                    if(EXISTS ${INCLUDE_CANDIDATE}/nvml.h)
                        target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_CANDIDATE})
                        message(STATUS "Found NVML headers: ${INCLUDE_CANDIDATE}")
                        break()
                    endif()
                endforeach()
            endif()
        else()
            message(WARNING "NVML library not found. GPU monitoring may not work.")
            # 尝试链接标准NVML库名（可能会在链接时找到）
            target_link_libraries(${PROJECT_NAME} nvml)
        endif()
    endif()
    
    # PDH (Performance Data Helper) - Windows自带
    target_link_libraries(${PROJECT_NAME} pdh)
    
    # Windows系统库
    target_link_libraries(${PROJECT_NAME} 
        kernel32
        user32
        gdi32
        winspool
        shell32
        ole32
        oleaut32
        uuid
        comdlg32
        advapi32
        psapi
    )
endif()
