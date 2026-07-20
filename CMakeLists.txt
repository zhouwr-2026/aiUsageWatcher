cmake_minimum_required(VERSION 3.16)

project(aiUsageWatcher LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

# aiUsageWatcher 主体是 Plasma 6 小部件（QML-only）
# Plasma 6 加载小部件时不需要编译此 CMakeLists——kpackagetool6 直接读取 metadata.json + contents/
# 但 kdesrc-build / distro 包需要 CMake 入口，所以保留这个最小工程文件以便包管理器识别。
# 真正构建流程：plasmawindowed aiUsageWatcher （开发期）或 kpackagetool6 --install aiUsageWatcher （打包）

# 仅添加一个非构建目标，让 cmake configure 阶段不报错
add_custom_target(aiUsageWatcher-help
    COMMAND ${CMAKE_COMMAND} -E echo "Plasma 6 小部件，无需 C++ 编译。请直接用 plasmawindowed aiUsageWatcher 运行。"
)