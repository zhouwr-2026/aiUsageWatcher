cmake_minimum_required(VERSION 3.20)

project(aiUsageWatcher VERSION 0.2.0 LANGUAGES CXX)

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

plasma_install_package(package aiUsageWatcher)

kcoreaddons_add_plugin(aiUsageWatcher
    SOURCES
        src/aiusagewatcherapplet.cpp
        src/aiusagewatcherapplet.h
        src/codexloginoutputparser.cpp
        src/codexloginoutputparser.h
        src/customusageclient.cpp
        src/customusageclient.h
        src/javascripthighlighter.cpp
        src/javascripthighlighter.h
        src/minimaxclient.cpp
        src/minimaxclient.h
        src/minimaxresponseparser.cpp
        src/minimaxresponseparser.h
        src/codexzhclient.cpp
        src/codexzhclient.h
        src/codexzhresponseparser.cpp
        src/codexzhresponseparser.h
    INSTALL_NAMESPACE "plasma/applets"
)
target_link_libraries(aiUsageWatcher
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
set_target_properties(aiUsageWatcher PROPERTIES PREFIX "")
target_compile_definitions(aiUsageWatcher
    PRIVATE
        QUOTA_PILOT_SCRIPT_WORKER_PATH="${CMAKE_INSTALL_FULL_LIBEXECDIR}/quota-pilot-script-worker"
)

add_executable(quota-pilot-script-worker src/usagescriptworker.cpp)
target_link_libraries(quota-pilot-script-worker PRIVATE Qt6::Core Qt6::Qml)
install(TARGETS quota-pilot-script-worker RUNTIME DESTINATION "${CMAKE_INSTALL_LIBEXECDIR}")

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
