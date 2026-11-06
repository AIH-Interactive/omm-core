# M2 native ysmparser

日期：2026-11-07。状态：实施与自动验收完成，Git 阶段检查点收尾中。退出条件见[实现计划](../OmniMorph-native-plan.md)第 8 节。

## 实现与来源

ABI 0.2 提供 `omm_ysm_probe/parse/parse_entries` 与 `omm_asset_get_info/read_section/read_resource/release`。JNI 保持 base 方法签名，parser 方法逐个可选注册。几何留在 native asset，供 M3 直接消费。

| 输入 | 实现及证据 |
|---|---|
| raw 当前/旧目录 | 35 个本地目录与 OMP 规范化差分 |
| ZIP | 内存读取；递归选择最浅元数据根目录，同深度多根拒绝；自有 stored/deflate、嵌套根、歧义及 JNI fixtures |
| YSGP v1 | 按 OMP decoder 移植；无真实 v1 样本，`UNVERIFIED` |
| YSGP v2 | `old.ysm` 的 10 个资源摘要一致 |
| YSGP v3，model format 1–32 | 17 个样本的 payload 与 binary 深度摘要一致；模型/动画/controller 规范化差分 |
| 当前 Asset Container | container `0.1.0-unstable`、`mixel/character` `0.1.1-unstable`；BLAKE3、raw/zstd、protobuf；自有 C ABI/JNI 端到端、单多线程一致及错误拒绝；第三方真实样本 `UNVERIFIED` |

`source_profile=0` 按格式选择来源；显式 OMP/current profile 不混用，current profile 当前只接收 Asset Container。ED25519 和其他容器/model schema 返回 unsupported。

两个有意差异：按用户要求，ZIP 不再固定 `/` 为根；v3 图片保留 `.encoded` 原编码，不在 parser 转 PNG。媒体仍由宿主解码。非 ASCII 路径大小写必须一致；ASCII 支持大小写不敏感查询，冲突拒绝。

## 数据、线程和限额

几何保留 bone/pivot/rotation 与 quad position/UV/normal；动画保留 pre/post、loop、blend、首人称标记和稳定事件 ordinal；controller 保留状态/条件/entry/exit/blend。当前 protobuf 的 tick 转为秒、几何 radians 转为 asset degrees。

Molang、Lua 和 Portable Program 不在 parser 求值。当前程序以 `program: {format, source}` 或 `program: {format, bytecode_hex}` 交给宿主；存在 program 时不得把数值 fallback 当作求值结果。原始 protobuf manifest/definition/blob 仍是可读取资源，保留未展开字段与来源身份。

主场景是一次解析一个模型。复用 C ABI `worker_count`/JNI `workerCount`：0 默认逻辑核心数，1 串行，非零值指定总执行线程上限，调用线程计入。移除原先默认最多 8、显式最多 64 的限制。单模型内部按动画/controller 文件拆分，单个动画文件还按独立 clip 拆分；当前容器也并行处理独立几何。文件与 clip 共享同一线程预算，嵌套不会各自创建一整套线程。任务少时不创建空闲线程；按输入顺序合并/报错，坏 clip 保留原先的截断语义；同步返回前 join。多个同时发起的 API 调用分别受其每次调用上限约束。

解码限制输入/展开/单条目/条目数/字符串/深度/骨骼/面数，发布前检查可配置模型计数。Runtime 原子核算已保留资产和 section 缓存的累计容量，超限拒绝，release 归还计数。计入 vector/string、模型、资源和保守索引开销；不是进程 RSS、分配器元数据或临时峰值的硬限制。失败不发布半成品。raw/legacy JSON 骨骼在重复名称替换后以线性遍历拒绝 parent 环；C ABI 自环/双骨骼环用例经历 RED→GREEN，错误时输出句柄为 0。

## 可重复验证

各命令按项目规则在前台追加 `2>&1 | Tee-Object -FilePath build.log`，立即检查退出码。入口内部也逐步记录日志。

```powershell
./tools/build-native.ps1 -Target windows-x64 -Configuration Release -OutputDirectory dist/windows-x64 -RequireAllJdks
./tools/build-native.ps1 -Target windows-x64 -Configuration Debug -OutputDirectory dist/windows-x64-debug -RequireAllJdks
./tools/m2/Build-OmpHarness.ps1
./tools/m2/Test-OmpDifferential.ps1
./tools/m2/Measure-Parser.ps1
```

Release 已验证：CTest 6/6、12 exports、Java 17/21/25 的加载/解析/当前程序传输；旧 bridge 的三版 Java 加载通过。六个公开 section 已经 JSON Schema 校验。最后 current/ProgramRef 版本的本地差分：18 个加密样本摘要通过；53 个 YSM 的 serial/auto 两轮均一致，tolerated=0；6 个负样本拒绝。最终 Debug 亦通过 CTest 6/6 及 Java 17/21/25 加载/解析验证。

详细差分数据只写系统临时目录；终端/build 日志留摘要，不提交第三方样本及其解密内容。自有 current fixture 为 `tests/support/current_fixture.hpp`，测试工具可用 `--write-fixture` 导出供 JNI 验证。

SDK 安装 [section JSON Schema](../../schemas/asset-section-v1.schema.json) 与四项 vendor 许可证。Schema 测试依赖仅安装在 `build/m2-python`，不进入 native 产物：

```powershell
D:/ProgramData/miniconda3/python.exe -I -m venv build/m2-python
./build/m2-python/Scripts/python.exe -I -m pip install -r tools/m2/requirements.txt
./build/m2-python/Scripts/python.exe -I tools/m2/Validate-SectionSchema.py dist/windows-x64/bin/omm_core.dll build/windows-x64-Release/java/current-fixture.ysm schemas/asset-section-v1.schema.json
```

## 性能与未验证项

基准在同一 JVM 内依次运行 Java 单线程、native 1/8/16 线程，每次只解析同一个模型，模型之间也串行。从已在内存的输入开始，排除文件读取；每路径预热 5 次、测量 20 次，报告 p50/p95/min。Java 解析入口只使用调用线程，GC/JIT 设置对所有路径保持一致。`native` 包含 JNI parse/release，`native+jni` 加 metadata/animations/controllers 传输，`native+jni+bind` 再由 Gson 解析动画/controller。最后一项是绑定代理，未覆盖正式 Molang 绑定或游戏加载。

同时报告调用线程的 Java 分配字节/次，以及一次额外非计时解析的 native retained 容量。容量查询不计入解析耗时。它们不表示进程 RSS 或临时峰值内存；Java 路径的 native retained=0 表示不适用。

多线程已降低若干样本的解析时延，但 **parser 冷解析+规范化+桥接 p95 -20% 的整体目标尚未达成**。v2、section 序列化/传输和绑定代理仍有开销。完整结果见[单模型 benchmark](parser-benchmark.md)：每个模型一个 H2 与独立表格，包含 Java 单线程、native 1/8/16 线程。16 线程 native parse p50 相对 Java：raw 20.74→16.14 ms，v2 56.94→55.93 ms，new v3 36.50→22.23 ms，大模型 1093.97→738.68 ms。UTF-8 连续有效片段批量复制降低逐字节追加成本；文件及 clip 并行改善单模型解析。桥接/绑定代理仍未普遍达到整体目标。同期发现 MSBuild 复用节点，报告已单独标记可能影响结果。

真实游戏绑定、帧性能和跨平台执行属于后续阶段，`MANUAL/PENDING`。v1 及当前容器第三方样本证据缺口为 `UNVERIFIED`。本阶段没有启动游戏、创建 GPU 对象或作 GUI 正确性结论。
