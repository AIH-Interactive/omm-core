cmake_minimum_required(VERSION 3.21)

project(omm_core
    VERSION 0.1.0
    DESCRIPTION "OmniMorph cross-version native core"
    LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_VISIBILITY_PRESET hidden)
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "omm-core supports 64-bit targets only")
endif()

option(OMM_BUILD_JNI "Build the JNI bridge into omm_core" ON)
option(OMM_BUILD_TESTS "Build native tests" ON)
set(OMM_BUILD_ID "" CACHE STRING "Build identifier embedded in omm_version.build_id")

if(OMM_BUILD_ID STREQUAL "")
    execute_process(
        COMMAND git rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE omm_git_revision
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET)
    if(omm_git_revision STREQUAL "")
        set(omm_git_revision "unknown")
    endif()
    set(OMM_BUILD_ID "${PROJECT_VERSION}+${omm_git_revision}")
endif()

if(MSVC)
    # Static CRT: JVMs ship their own (often older) msvcp140.dll, and a newer STL built
    # against the shared CRT crashes in std::mutex when that older DLL is already loaded.
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    set(omm_warning_flags /W4 /permissive- /utf-8)
else()
    set(omm_warning_flags -Wall -Wextra -Wpedantic)
endif()

add_library(omm_core SHARED
    src/abi/error.cpp
    src/abi/cpu_features.cpp
    src/abi/runtime.cpp)

target_include_directories(omm_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(omm_core PRIVATE OMM_BUILDING_LIBRARY "OMM_BUILD_ID=\"${OMM_BUILD_ID}\"")
target_compile_options(omm_core PRIVATE ${omm_warning_flags})

if(OMM_BUILD_JNI)
    target_sources(omm_core PRIVATE src/jni/jni_bridge.cpp)
    if(NOT ANDROID)
        # Only the JNI headers are needed; the library never links libjvm.
        if(DEFINED ENV{JAVA_HOME})
            set(omm_java_home "$ENV{JAVA_HOME}")
        endif()
        find_path(OMM_JNI_INCLUDE_DIR jni.h
            HINTS "${omm_java_home}/include"
            REQUIRED)
        if(WIN32)
            set(omm_jni_platform win32)
        elseif(APPLE)
            set(omm_jni_platform darwin)
        else()
            set(omm_jni_platform linux)
        endif()
        target_include_directories(omm_core PRIVATE
            "${OMM_JNI_INCLUDE_DIR}"
            "${OMM_JNI_INCLUDE_DIR}/${omm_jni_platform}")
    endif()
endif()

if(APPLE)
    set_target_properties(omm_core PROPERTIES INSTALL_RPATH "@loader_path")
elseif(UNIX)
    target_link_options(omm_core PRIVATE "LINKER:--no-undefined")
endif()
if(ANDROID)
    # Android 15+ devices may use 16 KB pages (docs/OmniMorph-native-plan.md section 10).
    target_link_options(omm_core PRIVATE "LINKER:-z,max-page-size=16384")
endif()

include(GNUInstallDirs)
install(TARGETS omm_core
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
install(DIRECTORY include/omm DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}")
install(FILES LICENSE.md DESTINATION "${CMAKE_INSTALL_DATADIR}/omm_core")

if(OMM_BUILD_TESTS)
    enable_testing()

    add_executable(omm_abi_test tests/c/abi_test.c)
    target_link_libraries(omm_abi_test PRIVATE omm_core)
    target_compile_options(omm_abi_test PRIVATE ${omm_warning_flags})
    add_test(NAME omm_abi_test COMMAND omm_abi_test)

    add_executable(omm_header_cpp_test tests/cpp/header_cpp_test.cpp)
    target_link_libraries(omm_header_cpp_test PRIVATE omm_core)
    target_compile_options(omm_header_cpp_test PRIVATE ${omm_warning_flags})
    add_test(NAME omm_header_cpp_test COMMAND omm_header_cpp_test)

    if(WIN32)
        # Test executables find omm_core.dll next to themselves.
        set_target_properties(omm_core omm_abi_test omm_header_cpp_test PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endif()
endif()
