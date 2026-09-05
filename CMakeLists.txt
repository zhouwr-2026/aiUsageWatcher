cmake_minimum_required(VERSION 3.20)

project(AIQuotaPilot VERSION 0.2.0 LANGUAGES CXX)

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

find_package(Qt6 6.6 REQUIRED COMPONENTS Core DBus Gui Network Qml Quick)
find_package(KF6CoreAddons 6.0 REQUIRED)
find_package(KF6Config 6.0 REQUIRED)
find_package(KF6Wallet 6.0 REQUIRED)
find_package(Plasma 6.0 REQUIRED)

plasma_install_package(package org.kde.plasma.AIQuotaPilot)

kcoreaddons_add_plugin(org.kde.plasma.AIQuotaPilot
    SOURCES
        src/agnesclient.cpp
        src/agnesclient.h
        src/agnesresponseparser.cpp
        src/agnesresponseparser.h
        src/aiusagewatcherapplet.cpp
        src/aiusagewatcherapplet.h
        src/codexloginoutputparser.cpp
        src/codexloginoutputparser.h
        src/commandcodeclient.cpp
        src/commandcodeclient.h
        src/customusageclient.cpp
        src/customusageclient.h
        src/javascripthighlighter.cpp
        src/javascripthighlighter.h
        src/kwalletdispatcher.cpp
        src/kwalletdispatcher.h
        src/minimaxclient.cpp
        src/minimaxclient.h
        src/minimaxresponseparser.cpp
        src/minimaxresponseparser.h
        src/codexzhclient.cpp
        src/codexzhclient.h
        src/codexzhresponseparser.cpp
        src/codexzhresponseparser.h
        src/deepseekclient.cpp
        src/deepseekclient.h
        src/deepseekresponseparser.cpp
        src/deepseekresponseparser.h
        src/opencodegoclient.cpp
        src/opencodegoclient.h
        src/resilientnetworkrequest.cpp
        src/resilientnetworkrequest.h
        src/sharedproviderconfig.cpp
        src/sharedproviderconfig.h
    INSTALL_NAMESPACE "plasma/applets"
)
target_link_libraries(org.kde.plasma.AIQuotaPilot
    PRIVATE
        Qt6::Core
        Qt6::DBus
        Qt6::Gui
        Qt6::Network
        Qt6::Qml
        Qt6::Quick
        KF6::CoreAddons
        KF6::Wallet
        Plasma::Plasma
)
set_target_properties(org.kde.plasma.AIQuotaPilot PROPERTIES PREFIX "")
target_compile_definitions(org.kde.plasma.AIQuotaPilot
    PRIVATE
        QUOTA_PILOT_SCRIPT_WORKER_PATH="${CMAKE_INSTALL_FULL_LIBEXECDIR}/quota-pilot-script-worker"
        QUOTA_PILOT_KWALLET_WORKER_PATH="${CMAKE_INSTALL_FULL_LIBEXECDIR}/quota-pilot-kwallet-worker"
)

add_executable(quota-pilot-script-worker src/usagescriptworker.cpp)
target_link_libraries(quota-pilot-script-worker PRIVATE Qt6::Core Qt6::Qml)
install(TARGETS quota-pilot-script-worker RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")

# 独立钱包 worker：跑在 plasmashell 之外，同步调用 KWallet，不阻塞插件进程。
add_executable(quota-pilot-kwallet-worker src/kwalletworker.cpp)
target_link_libraries(quota-pilot-kwallet-worker PRIVATE Qt6::Core KF6::Wallet)
install(TARGETS quota-pilot-kwallet-worker RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")

include(CTest)

if(BUILD_TESTING)
    find_package(Qt6 6.6 REQUIRED COMPONENTS Test)

    add_executable(tst_minimaxresponseparser
        tests/cpp/tst_minimaxresponseparser.cpp
        src/minimaxresponseparser.cpp
        src/minimaxresponseparser.h
    )
    target_include_directories(tst_minimaxresponseparser PRIVATE src)
    target_link_libraries(tst_minimaxresponseparser PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME minimax-response-parser COMMAND tst_minimaxresponseparser)

    add_executable(tst_codexzhresponseparser
        tests/cpp/tst_codexzhresponseparser.cpp
        src/codexzhresponseparser.cpp
        src/codexzhresponseparser.h
    )
    target_include_directories(tst_codexzhresponseparser PRIVATE src)
    target_link_libraries(tst_codexzhresponseparser PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME codexzh-response-parser COMMAND tst_codexzhresponseparser)

    add_executable(tst_codexzhclient
        tests/cpp/tst_codexzhclient.cpp
        src/codexzhclient.cpp
        src/codexzhclient.h
        src/codexzhresponseparser.cpp
        src/codexzhresponseparser.h
        src/kwalletdispatcher.cpp
        src/kwalletdispatcher.h
        src/resilientnetworkrequest.cpp
        src/resilientnetworkrequest.h
    )
    target_include_directories(tst_codexzhclient PRIVATE src)
    target_link_libraries(tst_codexzhclient PRIVATE Qt6::Core Qt6::DBus Qt6::Network Qt6::Test KF6::Wallet)
    add_test(NAME codexzh-client-contract COMMAND tst_codexzhclient)

    add_executable(tst_sharedproviderconfig
        tests/cpp/tst_sharedproviderconfig.cpp
        src/sharedproviderconfig.cpp
        src/sharedproviderconfig.h
    )
    target_include_directories(tst_sharedproviderconfig PRIVATE src)
    target_link_libraries(tst_sharedproviderconfig PRIVATE Qt6::Core Qt6::Test KF6::ConfigCore)
    add_test(NAME shared-provider-config COMMAND tst_sharedproviderconfig)

    add_executable(tst_minimaxclient
        tests/cpp/tst_minimaxclient.cpp
        src/minimaxclient.cpp
        src/minimaxclient.h
        src/minimaxresponseparser.cpp
        src/minimaxresponseparser.h
        src/kwalletdispatcher.cpp
        src/kwalletdispatcher.h
        src/resilientnetworkrequest.cpp
        src/resilientnetworkrequest.h
    )
    target_include_directories(tst_minimaxclient PRIVATE src)
    target_link_libraries(tst_minimaxclient PRIVATE Qt6::Core Qt6::DBus Qt6::Network Qt6::Test KF6::Wallet)
    add_test(NAME minimax-client-contract COMMAND tst_minimaxclient)

    add_executable(tst_deepseekresponseparser
        tests/cpp/tst_deepseekresponseparser.cpp
        src/deepseekresponseparser.cpp
        src/deepseekresponseparser.h
    )
    target_include_directories(tst_deepseekresponseparser PRIVATE src)
    target_link_libraries(tst_deepseekresponseparser PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME deepseek-response-parser COMMAND tst_deepseekresponseparser)

    add_executable(tst_deepseekclient
        tests/cpp/tst_deepseekclient.cpp
        src/deepseekclient.cpp
        src/deepseekclient.h
        src/deepseekresponseparser.cpp
        src/deepseekresponseparser.h
        src/kwalletdispatcher.cpp
        src/kwalletdispatcher.h
    )
    target_include_directories(tst_deepseekclient PRIVATE src)
    target_link_libraries(tst_deepseekclient PRIVATE Qt6::Core Qt6::DBus Qt6::Network Qt6::Test KF6::Wallet)
    add_test(NAME deepseek-client-contract COMMAND tst_deepseekclient)

    add_executable(tst_opencodegoclient
        tests/cpp/tst_opencodegoclient.cpp
        src/opencodegoclient.cpp
        src/opencodegoclient.h
        src/kwalletdispatcher.cpp
        src/kwalletdispatcher.h
    )
    target_include_directories(tst_opencodegoclient PRIVATE src)
    target_link_libraries(tst_opencodegoclient PRIVATE Qt6::Core Qt6::DBus Qt6::Network Qt6::Test KF6::Wallet)
    add_test(NAME opencodego-client-contract COMMAND tst_opencodegoclient)




    add_executable(tst_javascripthighlighter
        tests/cpp/tst_javascripthighlighter.cpp
        src/javascripthighlighter.cpp
        src/javascripthighlighter.h
    )
    target_include_directories(tst_javascripthighlighter PRIVATE src)
    target_link_libraries(tst_javascripthighlighter PRIVATE Qt6::Gui Qt6::Test)
    add_test(NAME javascript-highlighter COMMAND tst_javascripthighlighter)

    add_executable(tst_codexloginoutputparser
        tests/cpp/tst_codexloginoutputparser.cpp
        src/codexloginoutputparser.cpp
        src/codexloginoutputparser.h
    )
    target_include_directories(tst_codexloginoutputparser PRIVATE src)
    target_link_libraries(tst_codexloginoutputparser PRIVATE Qt6::Core Qt6::Test)
    add_test(NAME codex-login-output-parser COMMAND tst_codexloginoutputparser)


    add_executable(tst_customusageclient
        tests/cpp/tst_customusageclient.cpp
        src/customusageclient.cpp
        src/customusageclient.h
    )
    target_include_directories(tst_customusageclient PRIVATE src)
    target_compile_definitions(tst_customusageclient
        PRIVATE
            QUOTA_PILOT_SCRIPT_WORKER_PATH="$<TARGET_FILE:quota-pilot-script-worker>"
    )
    target_link_libraries(tst_customusageclient PRIVATE Qt6::Core Qt6::Network Qt6::Test)
    add_test(NAME custom-usage-client COMMAND tst_customusageclient)

    add_executable(tst_usagescriptworker tests/cpp/tst_usagescriptworker.cpp)
    target_compile_definitions(tst_usagescriptworker
        PRIVATE
            USAGE_SCRIPT_WORKER_TEST_PATH="$<TARGET_FILE:quota-pilot-script-worker>"
    )
    target_link_libraries(tst_usagescriptworker PRIVATE Qt6::Core Qt6::Test)
    add_dependencies(tst_usagescriptworker quota-pilot-script-worker)
    add_test(NAME usage-script-worker COMMAND tst_usagescriptworker)

endif()
