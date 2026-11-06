# M1 构建与 ABI

日期：2026-11-06。阶段：[实现计划](../OmniMorph-native-plan.md) 第 8 节 M1。

## 交付

| 项目 | 位置 |
|---|---|
| 公共 C ABI（C11/C++，`omm_` 前缀，ABI 0.1） | [`include/omm/omm.h`](../../include/omm/omm.h) |
| 错误转换、带 generation 的句柄表、CPU 特性探测、runtime | [`src/abi`](../../src/abi) |
| JNI 桥接（`JNI_OnLoad` + RegisterNatives，绑定 `top.aihmc.omm.NativeApi`） | [`src/jni/jni_bridge.cpp`](../../src/jni/jni_bridge.cpp) |
| Java bridge（`--release 17`） | [`bindings/java`](../../bindings/java) |
| CMake 工程与依赖锁 | [`CMakeLists.txt`](../../CMakeLists.txt)、[`deps/dependencies.lock.json`](../../deps/dependencies.lock.json) |
| 构建入口 | [`tools/build-native.ps1`](../../tools/build-native.ps1) |
| 手动 CI（新增 JDK 17/21/25 和 NDK r28c 安装） | [`.github/workflows/native-manual.yml`](../../.github/workflows/native-manual.yml) |
| 测试 | [`tests/c/abi_test.c`](../../tests/c/abi_test.c)、[`tests/cpp/header_cpp_test.cpp`](../../tests/cpp/header_cpp_test.cpp)、[`tests/java/JniLoadTest.java`](../../tests/java/JniLoadTest.java) |

M1 的 ABI 只包含版本、runtime create/caps/destroy。parser、model、device 等入口按 API 文档在 M2–M4 追加，作为 minor 扩展。

## 决定与发现

- **Windows 静态 CRT（`/MT`）**：首次 JNI 测试在 Java 17 上崩溃，故障帧为 `msvcp140.dll+0x13020`。原因是 JDK `bin` 目录自带较旧的 `msvcp140.dll`，新版 MSVC STL 的 `std::mutex` 在它上面读到空指针。现改为静态链接运行库，构建入口用 `dumpbin /dependents` 检查，禁止导入 `msvcp*`、`vcruntime*`、`ucrtbase*`，防止回归。
- 句柄布局为 `[kind:8][generation:24][slot:32]`。槽位复用时 generation 递增并跳过 0，确保旧句柄值不会被重新签发。
- 显式指定当前不可用的 CPU kernel 时返回 `omm_error_unsupported`，不会悄悄降级。
- 构建日志按步骤分别写入 `build/<target>-<config>/logs/<step>.log`。

## 验收

| M1 退出条件 | 结果 | 证据 |
|---|---|---|
| Windows Debug/Release | PASS | `build-native.ps1 -Target windows-x64 -Configuration Debug` 与 `Release` 均退出码 0 |
| C 头文件编译 | PASS | `omm_abi_test`（C11，含 sizeof/offsetof 静态断言）与 `omm_header_cpp_test`（C++20）均通过 CTest |
| Java 17/21/25 同一 DLL 加载 | PASS | `-RequireAllJdks`：Dragonwell 17.0.18、21.0.9、Oracle 25.0.2 加载同一 `dist/windows-x64/bin/omm_core.dll` |
| 导出与错误路径测试 | PASS | 导出仅 `JNI_OnLoad` + 4 个 `omm_*`；空指针、结构尺寸、ABI major/minor、未知/不可用 cpu_mode、空/伪造/过期/错误类型句柄、Java 负 uint32 均返回约定状态 |
| 手动 CI 四平台 | NOT_RUN | 配置已更新；按计划只手动触发，未执行远程 workflow，全矩阵结果在 M6 记录 |

Linux、Android、macOS 的 CMake 分支和导出检查（nm/llvm-nm）尚未在对应主机上执行，是证据缺口，不计为 PASS。
