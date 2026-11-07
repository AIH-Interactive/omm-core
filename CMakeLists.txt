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

set(CMAKE_POSITION_INDEPENDENT_CODE ON)
find_package(Threads REQUIRED)

# ---- Pinned third-party sources (deps/dependencies.lock.json) ----
add_library(omm_blake3 STATIC third_party/blake3/blake3.c third_party/blake3/blake3_dispatch.c
    third_party/blake3/blake3_portable.c)
target_include_directories(omm_blake3 PUBLIC third_party/blake3)
target_compile_definitions(omm_blake3 PUBLIC BLAKE3_API= BLAKE3_PRIVATE= PRIVATE BLAKE3_NO_SSE2 BLAKE3_NO_SSE41
    BLAKE3_NO_AVX2 BLAKE3_NO_AVX512 BLAKE3_USE_NEON=0)

add_library(omm_zstd STATIC
    third_party/zstd/lib/common/debug.c
    third_party/zstd/lib/common/entropy_common.c
    third_party/zstd/lib/common/error_private.c
    third_party/zstd/lib/common/fse_decompress.c
    third_party/zstd/lib/common/xxhash.c
    third_party/zstd/lib/common/zstd_common.c
    third_party/zstd/lib/decompress/huf_decompress.c
    third_party/zstd/lib/decompress/zstd_ddict.c
    third_party/zstd/lib/decompress/zstd_decompress.c
    third_party/zstd/lib/decompress/zstd_decompress_block.c)
target_include_directories(omm_zstd PUBLIC third_party/zstd/lib PRIVATE third_party/zstd/lib/common)
# Portable C decoder only: no hand-written assembly, no legacy formats, no threads.
target_compile_definitions(omm_zstd PRIVATE ZSTD_DISABLE_ASM ZSTD_LEGACY_SUPPORT=0 ZSTD_MULTITHREAD=0)

add_library(omm_miniz STATIC third_party/miniz/miniz.c)
target_include_directories(omm_miniz PUBLIC third_party/miniz)
target_compile_definitions(omm_miniz PUBLIC MINIZ_NO_STDIO MINIZ_NO_TIME MINIZ_NO_ARCHIVE_WRITING_APIS)

add_library(omm_yyjson STATIC third_party/yyjson/yyjson.c)
target_include_directories(omm_yyjson PUBLIC third_party/yyjson)

# ---- omm-core implementation (shared by the library and the white-box tests) ----
add_library(omm_core_impl OBJECT
    src/abi/error.cpp
    src/abi/cpu_features.cpp
    src/abi/runtime.cpp
    src/abi/cpu_executor.cpp
    src/model/math.cpp
    src/model/model.cpp
    src/model/bake.cpp
    src/model/api.cpp
    src/model/kernels.cpp
    src/model/sort.cpp
    src/model/kernel_scalar.cpp
    src/ysmparser/decode.cpp
    src/ysmparser/crypto.cpp
    src/ysmparser/current_container.cpp
    src/ysmparser/current_mapper.cpp
    src/ysmparser/resource_set.cpp
    src/ysmparser/ysgp.cpp
    src/ysmparser/binary_reader.cpp
    src/ysmparser/json.cpp
    src/ysmparser/normalize.cpp
    src/ysmparser/manifest.cpp
    src/ysmparser/binary_mapper.cpp
    src/ysmparser/parse.cpp
    src/ysmparser/api.cpp)

if(CMAKE_GENERATOR_PLATFORM MATCHES "^(ARM64|arm64)$" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
    target_sources(omm_core_impl PRIVATE src/model/kernel_neon.cpp)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|X86_64)$" OR CMAKE_GENERATOR_PLATFORM STREQUAL "x64")
    target_sources(omm_core_impl PRIVATE src/model/kernel_sse2.cpp src/model/kernel_avx2.cpp)
    if(MSVC)
        set_source_files_properties(src/model/kernel_avx2.cpp PROPERTIES COMPILE_OPTIONS /arch:AVX2)
    else()
        set_source_files_properties(src/model/kernel_sse2.cpp PROPERTIES COMPILE_OPTIONS -msse2)
        set_source_files_properties(src/model/kernel_avx2.cpp PROPERTIES COMPILE_OPTIONS -mavx2)
    endif()
endif()
target_include_directories(omm_core_impl
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>")
target_compile_definitions(omm_core_impl PUBLIC OMM_BUILDING_LIBRARY PRIVATE "OMM_BUILD_ID=\"${OMM_BUILD_ID}\"")
target_compile_options(omm_core_impl PRIVATE ${omm_warning_flags})
if(NOT MSVC)
    # Bit-exact parity with the Java reference needs explicit fma() calls only: Clang otherwise
    # contracts a*b+c into FMA on its own. MSVC /fp:precise does not contract (VS 2022+).
    target_compile_options(omm_core_impl PRIVATE -ffp-contract=off)
endif()
target_link_libraries(omm_core_impl PUBLIC omm_zstd omm_miniz omm_yyjson omm_blake3 Threads::Threads)

add_library(omm_core SHARED $<TARGET_OBJECTS:omm_core_impl>)
target_include_directories(omm_core
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
        "$<INSTALL_INTERFACE:include>"
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_definitions(omm_core PRIVATE OMM_BUILDING_LIBRARY)
target_compile_options(omm_core PRIVATE ${omm_warning_flags})
target_link_libraries(omm_core PRIVATE omm_zstd omm_miniz omm_yyjson omm_blake3 Threads::Threads)

if(OMM_BUILD_JNI)
    target_sources(omm_core PRIVATE src/jni/jni_bridge.cpp src/jni/model_jni.cpp
        tools/preview/native/preview_vertices_jni.cpp tools/preview/native/preview_asset_jni.cpp)
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
install(FILES schemas/asset-section-v1.schema.json DESTINATION "${CMAKE_INSTALL_DATADIR}/omm_core/schemas")
foreach(omm_dependency IN ITEMS zstd miniz yyjson)
    install(FILES "third_party/${omm_dependency}/LICENSE"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/omm_core/licenses/${omm_dependency}")
endforeach()
install(FILES third_party/blake3/LICENSE_CC0 third_party/blake3/LICENSE_A2
    DESTINATION "${CMAKE_INSTALL_DATADIR}/omm_core/licenses/blake3")
install(FILES third_party/notices/LICENSE-Sparkle-Morpher.txt
    DESTINATION "${CMAKE_INSTALL_DATADIR}/omm_core/licenses/sparkle-morpher")

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

    add_executable(omm_model_math_test tests/cpp/model_math_test.cpp)
    target_link_libraries(omm_model_math_test PRIVATE omm_core_impl)
    target_compile_options(omm_model_math_test PRIVATE ${omm_warning_flags})
    add_test(NAME omm_model_math_test COMMAND omm_model_math_test)

    add_executable(omm_model_test tests/cpp/model_test.cpp)
    target_link_libraries(omm_model_test PRIVATE omm_core_impl)
    target_compile_options(omm_model_test PRIVATE ${omm_warning_flags})
    add_test(NAME omm_model_test COMMAND omm_model_test)

    add_executable(omm_cpu_executor_test tests/cpp/cpu_executor_test.cpp)
    target_link_libraries(omm_cpu_executor_test PRIVATE omm_core_impl)
    target_compile_options(omm_cpu_executor_test PRIVATE ${omm_warning_flags})
    add_test(NAME omm_cpu_executor_test COMMAND omm_cpu_executor_test)
    set_tests_properties(omm_cpu_executor_test PROPERTIES TIMEOUT 15)

    add_executable(omm_parser_test tests/cpp/parser_test.cpp)
    target_link_libraries(omm_parser_test PRIVATE omm_core omm_yyjson)
    target_compile_options(omm_parser_test PRIVATE ${omm_warning_flags})
    add_test(NAME omm_parser_test COMMAND omm_parser_test)

    # White-box tests link the implementation objects directly.
    add_executable(omm_crypto_test tests/cpp/crypto_test.cpp)
    target_link_libraries(omm_crypto_test PRIVATE omm_core_impl)
    target_compile_options(omm_crypto_test PRIVATE ${omm_warning_flags})
    add_test(NAME omm_crypto_test COMMAND omm_crypto_test)

    add_executable(omm_parallel_test tests/cpp/parallel_test.cpp)
    target_link_libraries(omm_parallel_test PRIVATE omm_core_impl)
    target_compile_options(omm_parallel_test PRIVATE ${omm_warning_flags})
    add_test(NAME omm_parallel_test COMMAND omm_parallel_test)
    set_tests_properties(omm_parallel_test PROPERTIES TIMEOUT 15)

    add_executable(omm_current_parser_test tests/cpp/current_parser_test.cpp)
    target_link_libraries(omm_current_parser_test PRIVATE omm_core_impl)
    target_compile_options(omm_current_parser_test PRIVATE ${omm_warning_flags})
    add_test(NAME omm_current_parser_test COMMAND omm_current_parser_test)

    # Differential probe (prints digests only); run by tools/m2/Test-OmpDifferential.ps1.
    add_executable(omm_ysm_decode_probe tests/tools/ysm_decode_probe.cpp)
    target_link_libraries(omm_ysm_decode_probe PRIVATE omm_core_impl)
    target_compile_options(omm_ysm_decode_probe PRIVATE ${omm_warning_flags})

    add_executable(omm_ysm_stage_probe tests/tools/ysm_stage_probe.cpp)
    target_link_libraries(omm_ysm_stage_probe PRIVATE omm_core_impl)
    target_compile_options(omm_ysm_stage_probe PRIVATE ${omm_warning_flags})

    # Normalization differential, native side (prints the NormalizedDump line format).
    add_executable(omm_ysm_normalized_dump tests/tools/ysm_normalized_dump.cpp)
    target_link_libraries(omm_ysm_normalized_dump PRIVATE omm_core_impl)
    target_compile_options(omm_ysm_normalized_dump PRIVATE ${omm_warning_flags})

    # Public-ABI sample probe (summary only); run by tools/m2/Test-OmpDifferential.ps1.
    add_executable(omm_ysm_asset_probe tests/tools/ysm_asset_probe.cpp)
    target_link_libraries(omm_ysm_asset_probe PRIVATE omm_core)
    target_compile_options(omm_ysm_asset_probe PRIVATE ${omm_warning_flags})

    if(WIN32)
        # Test executables find omm_core.dll next to themselves.
        set_target_properties(omm_core omm_abi_test omm_header_cpp_test omm_crypto_test omm_ysm_decode_probe omm_ysm_asset_probe
            omm_ysm_normalized_dump omm_parser_test omm_parallel_test omm_current_parser_test omm_ysm_stage_probe omm_model_math_test omm_model_test omm_cpu_executor_test
            PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
    endif()
endif()
