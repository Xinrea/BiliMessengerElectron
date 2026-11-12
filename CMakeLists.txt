cmake_minimum_required(VERSION 3.16)
project(BiliMessenger VERSION 2.0.0 LANGUAGES C)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(BM_BUILD_TESTS "Build the unit tests" ON)

# --- CUI (git submodule) -----------------------------------------------------
if(NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/third_party/CUI/CMakeLists.txt)
    message(FATAL_ERROR "third_party/CUI is missing. Run: git submodule update --init --recursive")
endif()
set(CUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/CUI EXCLUDE_FROM_ALL)
include(third_party/CUI/cmake/CuiApp.cmake)

# --- libcurl -----------------------------------------------------------------
find_package(CURL REQUIRED)
find_package(Threads REQUIRED)

# --- core library: everything except the window, so the tests can link it ----
add_library(bm_core STATIC
    src/core/util.c
    src/core/version.c
    src/core/md5.c
    src/core/http.c
    src/core/diagnostics.c
    src/core/feedback.c
    src/core/jobs.c
    src/core/store.c
    src/core/bili.c
    src/core/guards.c
    third_party/cjson/cJSON.c
    third_party/qrcodegen/qrcodegen.c
)
target_include_directories(bm_core PUBLIC
    src
    third_party/cjson
    third_party/qrcodegen
)
target_compile_definitions(bm_core PUBLIC BM_VERSION="${PROJECT_VERSION}")
target_link_libraries(bm_core PUBLIC CURL::libcurl Threads::Threads)
if(WIN32)
    target_compile_definitions(bm_core PUBLIC _CRT_SECURE_NO_WARNINGS WIN32_LEAN_AND_MEAN NOMINMAX)
endif()

# --- the app -----------------------------------------------------------------
set(BM_APP_SOURCES
    src/main.c
    src/ui/images.c
    src/ui/fonts.c
    src/ui/common.c
    src/pages/summary.c
    src/pages/guard_list.c
    src/pages/template.c
    src/pages/codes.c
    src/pages/message.c
    src/pages/setting.c
    src/pages/about.c
)

if(APPLE)
    enable_language(OBJC)
    list(APPEND BM_APP_SOURCES src/ui/window_mac.m)
    set_source_files_properties(src/ui/window_mac.m PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    set(MACOSX_BUNDLE_ICON_FILE icon.icns)
    set(BM_ICON ${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/icon.icns)
    set_source_files_properties(${BM_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
    add_executable(BiliMessenger MACOSX_BUNDLE ${BM_APP_SOURCES} ${BM_ICON})
    set_target_properties(BiliMessenger PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "BiliMessenger"
        MACOSX_BUNDLE_GUI_IDENTIFIER "cn.joi-club.bilimessenger"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_CURRENT_SOURCE_DIR}/assets/Info.plist.in
    )
elseif(WIN32)
    list(APPEND BM_APP_SOURCES src/ui/window_win.c)
    add_executable(BiliMessenger ${BM_APP_SOURCES} assets/app.rc)
    configure_file(assets/windows-installer.iss.in BiliMessenger.iss @ONLY)
else()
    list(APPEND BM_APP_SOURCES src/ui/window_stub.c)
    add_executable(BiliMessenger ${BM_APP_SOURCES})
endif()

target_include_directories(BiliMessenger PRIVATE third_party/CUI/third_party)
target_link_libraries(BiliMessenger PRIVATE bm_core cui::cui)
cui_app_target(BiliMessenger)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(bm_core PRIVATE -Wall -Wextra -Wno-unused-parameter)
    target_compile_options(BiliMessenger PRIVATE -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers)
    set_source_files_properties(third_party/cjson/cJSON.c third_party/qrcodegen/qrcodegen.c PROPERTIES COMPILE_OPTIONS "-w")
endif()

# --- tests -------------------------------------------------------------------
if(BM_BUILD_TESTS)
    enable_testing()
    add_executable(bm_tests tests/test_core.c)
    target_link_libraries(bm_tests PRIVATE bm_core)
    add_executable(bm_feedback_tests tests/test_feedback.c)
    target_link_libraries(bm_feedback_tests PRIVATE bm_core)
    add_executable(bm_feedback_ui_tests tests/test_feedback_ui.c src/pages/about.c)
    target_link_libraries(bm_feedback_ui_tests PRIVATE bm_core cui::cui)
    add_executable(bm_guard_import_ui_tests tests/test_guard_import_ui.c src/ui/fonts.c)
    target_link_libraries(bm_guard_import_ui_tests PRIVATE bm_core cui::cui)
    target_include_directories(bm_guard_import_ui_tests PRIVATE third_party/CUI/src)
    # Test assertions (including calls under test) must run in Release too.
    if(MSVC)
        target_compile_options(bm_tests PRIVATE /UNDEBUG)
        target_compile_options(bm_feedback_tests PRIVATE /UNDEBUG)
        target_compile_options(bm_feedback_ui_tests PRIVATE /UNDEBUG)
        target_compile_options(bm_guard_import_ui_tests PRIVATE /UNDEBUG)
    else()
        target_compile_options(bm_tests PRIVATE -UNDEBUG)
        target_compile_options(bm_feedback_tests PRIVATE -UNDEBUG)
        target_compile_options(bm_feedback_ui_tests PRIVATE -UNDEBUG)
        target_compile_options(bm_guard_import_ui_tests PRIVATE -UNDEBUG)
    endif()
    add_test(NAME bm_tests COMMAND bm_tests)
    add_test(NAME bm_feedback_tests COMMAND bm_feedback_tests)
    add_test(NAME bm_feedback_ui_tests COMMAND bm_feedback_ui_tests)
    add_test(NAME bm_guard_import_ui_tests COMMAND bm_guard_import_ui_tests)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME bm_feedback_http
            COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_feedback_http.py
                    $<TARGET_FILE:bm_feedback_tests>)
        set_tests_properties(bm_feedback_http PROPERTIES TIMEOUT 20)
    endif()
endif()
