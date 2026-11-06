# 当前交接：OmniMorph native

更新：2026-11-07。目标仍 active：按计划继续 M2–M6，每阶段自动验收后提交、推送、确认、查询额度，再进入下一阶段。

## 当前阶段与 Git

- 分支 main，上游 origin/main。M0 已推送 5ed8bfc；M1 已推送 a81e288。
- M2 实施和自动验收已通过，当前准备阶段提交与推送；准确提交状态以 git log/status 和远端为准。
- M3–M6 尚未开始。没有把整个目标标记完成。
- 保留 .codegraph/、.omc/；两项不提交。build/dist/logs/refs 不提交。

## M2 已完成

- raw/ZIP/YSGP v1/v2/v3 与当前 Asset Container 解码及规范化、C ABI/JNI 资产/资源/section 接口，schema 1。
- 当前容器支持 container 0.1.0-unstable、mixel/character 0.1.1-unstable，raw/zstd/BLAKE3/protobuf；Portable Program 保留 source/bytecode，不求值。
- worker_count / workerCount：0=逻辑核心数，1=串行，显式线程总上限包含调用线程。单模型文件及 clip 并行，嵌套共享预算，输入顺序合并/报错。
- UTF-8 有效连续片段批量复制；AES GF 表优化。raw 几何重复名称替换后线性拒绝 parent 环，C ABI 自环/双骨骼环 RED→GREEN。
- runtime 累计资产/section 保留容量预算与释放核算；不是 RSS/临时峰值硬限。
- 四项 vendored 依赖版本/源码哈希及许可证安装；Sparkle-Morpher MIT notice 保留。

## 验证证据

- 最终 Release/Debug 构建，各 CTest 6/6；Java 17/21/25 加载/解析/current fixture PASS。
- 日志 build-m2-parent-release.log、build-m2-parent-debug.log；详细子日志 build/windows-x64-{Release,Debug}/logs/。
- 最终差分 build-m2-parent-differential.log：18 加密样本摘要 PASS；53 YSM serial/auto 规范化精确一致（tolerated=0）；6 负样本拒绝。
- 六个公开 section 的 schema 校验 PASS：build-m2-schema-final.log。全部 vendor 文件哈希核对一致。
- 最终 Release DLL SHA-256：0984307bee500fdf81e28a61a141d5d4aaca92cc12777b1a89e7620639425131。
- 基准 docs/m2/parser-benchmark.md：4 模型，每模型 H2+独立表格；同 JVM 预热5/测量20，Java1/native1/8/16。原始摘要 build-m2-benchmark.log。基准受测 DLL 是报告内的 577fc8…，测于最终 raw 环检测补充之前；不得换成最终产物哈希冒充重测。
- native16 parse p50 相对 Java：20.74→16.14、56.94→55.93、36.50→22.23、1093.97→738.68ms。整体含桥接/绑定代理 -20% 尚未普遍达到。报告注明同期 MSBuild nodeReuse 节点可能影响结果，未干预其他进程。
- 旧 bridge 在三版 Java 的兼容加载此前 PASS。v1/第三方 current 真实样本 UNVERIFIED；游戏/GUI/跨平台运行 MANUAL/PENDING。未启动游戏或使用 Qt offscreen。

## 下一步

1. 完成 M2 git diff/status/diff --check、精确暂存、提交、推送 main，确认远端 HEAD 后查询额度。
2. 检查计划 M3 与 API：共用模型、Figura mesh/YSM asset 接入、pose/normal/排序及标量/SSE2/AVX2/NEON；先核对现有接口与参考行为，完成该阶段后独立检查点。
3. 继续 M4–M6，按原计划顺序。M2 基准已经完成，不需重复整组计时。

## 文档和约束

- docs/OmniMorph-native-plan.md；docs/OmniMorph-native-api.md；docs/m2/M2-ysmparser.md；docs/m2/parser-benchmark.md。
- 源码集中 src/ysmparser、src/abi/parallel.hpp、src/jni；测试 tests/cpp、tests/java、tests/support、tools/m2。
- 编译/测试必须前台 PowerShell，经 2>&1 | Tee-Object，立即检查 LASTEXITCODE，失败读日志。
- 不扩大范围、不覆盖既有修改；真实 Windows 可见窗口才是 GUI 验证依据，严禁 Qt offscreen。
- 人工项记 MANUAL/PENDING，不作为 blocked 条件。不要上传第三方模型或解密内容。
- 最近可靠额度：周窗口已用19%，剩余81%，未达阈值。剩余≤10%预警，≤2%安全交接且仅手册编写委派 gpt-6-luna；不可猜测额度或擅自切换开发模型。
