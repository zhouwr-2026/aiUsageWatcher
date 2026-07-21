cmake_minimum_required(VERSION 3.20)

project(aiUsageWatcher VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(BUILD_SHARED_LIBS ON)

include(GNUInstallDirs)

# This project only needs the standard Plasma package and Qt plugin locations.
# Defining them directly keeps local builds independent of extra-cmake-modules.
set(KDE_INSTALL_DATADIR "${CMAKE_INSTALL_DATAROOTDIR}")
set(KDE_INSTALL_METAINFODIR "${CMAKE_INSTALL_DATAROOTDIR}/metainfo")
set(KDE_INSTALL_PLUGINDIR "${CMAKE_INSTALL_LIBDIR}/qt6/plugins")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Network Test)
find_package(KF6CoreAddons 6.0 REQUIRED)
find_package(KF6Config 6.0 REQUIRED)
find_package(KF6Wallet 6.0 REQUIRED)
find_package(Plasma 6.0 REQUIRED)

plasma_install_package(package aiUsageWatcher)

kcoreaddons_add_plugin(aiUsageWatcher
    SOURCES
        src/aiusagewatcherapplet.cpp
        src/aiusagewatcherapplet.h
        src/minimaxclient.cpp
        src/minimaxclient.h
        src/minimaxresponseparser.cpp
        src/minimaxresponseparser.h
    INSTALL_NAMESPACE "plasma/applets"
)
target_link_libraries(aiUsageWatcher
    PRIVATE
        Qt6::Core
        Qt6::Network
        KF6::CoreAddons
        KF6::Wallet
        Plasma::Plasma
)
set_target_properties(aiUsageWatcher PROPERTIES PREFIX "")

include(CTest)

if(BUILD_TESTING)
    add_executable(tst_minimaxresponseparser
        tests/cpp/tst_minimaxresponseparser.cpp
        src/minimaxresponseparser.cpp
        src/minimaxresponseparser.h
    )
    target_include_directories(tst_minimaxresponseparser PRIVATE src)
    target_link_libraries(tst_minimaxresponseparser PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME minimax-response-parser COMMAND tst_minimaxresponseparser)

    add_executable(tst_minimaxclient
        tests/cpp/tst_minimaxclient.cpp
        src/minimaxclient.cpp
        src/minimaxclient.h
        src/minimaxresponseparser.cpp
        src/minimaxresponseparser.h
    )
    target_include_directories(tst_minimaxclient PRIVATE src)
    target_link_libraries(tst_minimaxclient PRIVATE Qt6::Core Qt6::Network Qt6::Test KF6::Wallet)
    add_test(NAME minimax-client-contract COMMAND tst_minimaxclient)
endif()
