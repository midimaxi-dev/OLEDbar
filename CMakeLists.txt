cmake_minimum_required(VERSION 3.25)

project(oledbar
    VERSION 0.2.1
    DESCRIPTION "OLEDbar: dims the Windows 11 taskbar to prevent OLED burn-in"
    LANGUAGES CXX RC)

if(NOT MSVC)
    message(FATAL_ERROR "OLEDbar requires the MSVC toolchain.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

option(OLEDBAR_BUILD_TESTS "Build unit tests" ON)
option(OLEDBAR_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)

find_package(wil CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

# Single source of truth for the version: project(VERSION) -> version.h -> resources and code.
configure_file(src/app/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/version.h @ONLY)

function(oledbar_configure_target target)
    target_compile_options(${target} PRIVATE
        $<$<COMPILE_LANGUAGE:CXX>:/W4 /permissive- /sdl /guard:cf /Qspectre /utf-8 /Zc:__cplusplus /external:W0>)
    if(OLEDBAR_WARNINGS_AS_ERRORS)
        target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:/WX>)
    endif()
    target_compile_definitions(${target} PRIVATE
        UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
endfunction()

# core: pure C++, no Win32 headers.
add_library(oledbar_core STATIC
    src/core/clock.h
    src/core/dim_state_machine.h
    src/core/dim_state_machine.cpp
    src/core/hotkey.h
    src/core/hotkey.cpp
    src/core/settings.h
    src/core/settings.cpp)
target_include_directories(oledbar_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(oledbar_core PRIVATE nlohmann_json::nlohmann_json)
oledbar_configure_target(oledbar_core)

add_library(oledbar_platform STATIC
    src/platform/log.h
    src/platform/log.cpp
    src/platform/win_util.h
    src/platform/win_util.cpp
    src/platform/desktop_icons.h
    src/platform/desktop_icons.cpp
    src/platform/taskbar_locator.h
    src/platform/taskbar_locator.cpp
    src/platform/overlay_window.h
    src/platform/overlay_window.cpp
    src/platform/pointer_monitor.h
    src/platform/pointer_monitor.cpp
    src/platform/settings_store.h
    src/platform/settings_store.cpp
    src/platform/shell_event_monitor.h
    src/platform/shell_event_monitor.cpp
    src/platform/startup.h
    src/platform/startup.cpp)
target_link_libraries(oledbar_platform PUBLIC oledbar_core WIL::WIL user32 shell32)
oledbar_configure_target(oledbar_platform)

add_executable(oledbar WIN32
    src/app/main.cpp
    src/app/app_host.h
    src/app/app_host.cpp
    src/app/tray_icon.h
    src/app/tray_icon.cpp
    src/app/settings_dialog.h
    src/app/settings_dialog.cpp
    src/app/resource.h
    src/app/version.h.in
    src/app/oledbar.rc
    src/app/oledbar.manifest)
set_target_properties(oledbar PROPERTIES OUTPUT_NAME OLEDbar)
target_include_directories(oledbar PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_link_libraries(oledbar PRIVATE oledbar_platform wtsapi32 comctl32 delayimp)
target_link_options(oledbar PRIVATE /guard:cf /CETCOMPAT /DYNAMICBASE /HIGHENTROPYVA /DELAYLOAD:comctl32.dll)
oledbar_configure_target(oledbar)

if(OLEDBAR_BUILD_TESTS)
    enable_testing()
    find_package(GTest CONFIG REQUIRED)
    add_executable(core_tests
        tests/core_tests/dim_state_machine_test.cpp
        tests/core_tests/hotkey_test.cpp
        tests/core_tests/settings_test.cpp)
    target_link_libraries(core_tests PRIVATE oledbar_core GTest::gtest GTest::gtest_main)
    oledbar_configure_target(core_tests)
    include(GoogleTest)
    # First launch of a new unsigned exe can be slow while Defender/Smart App Control scans it.
    gtest_discover_tests(core_tests DISCOVERY_MODE PRE_TEST DISCOVERY_TIMEOUT 60)
endif()
