# OmniMorph 纯 native 渲染与解析库实现计划

日期：2026-11-06。状态：设计交付，尚未开始 native 实施。

## 1. 目标与范围

构建独立 `omm-core`，让 Figura 和 YSM 模型共用 native 几何处理、SIMD 运算、GPU 资源管理和实际绘制。将 ysmparser 同时迁入 native，减少容器解码、JSON 解析、几何规范化与 Java 对象构造成本。公开 API 的函数、类型、常量和回调 typedef 统一使用 `omm_` 前缀，详见 [API 设计](OmniMorph-native-api.md)。

“纯 native 渲染”指模型几何的变换、可见面处理、排序、GPU 上传与 draw 均在 native 内完成。Java 继续处理游戏事件、模型选择、Lua/Molang 宿主语义、资源授权及版本适配。它不意味着 Minecraft、Figura Lua VM 或所有动画逻辑都重写成 C++。宿主物品、文字等 RenderTask 仍在游戏侧执行，其与 native 模型绘制间有显式顺序屏障。

SIMD 与 GPU **同时属于必需交付**，不能只交付其中之一：

- SIMD：CPU 几何烘焙、矩阵/法线处理、姿态整理、包围盒和 CPU 顶点生成；支持 x64 SSE2、可用时 AVX2，以及 ARM64 NEON，保留标量参考实现。
- GPU：静态几何常驻、姿态增量上传、native shader/pipeline 管理、索引绘制与批次合并。SIMD 可先准备姿态，GPU 再执行顶点变换，两者协同。
- Native CPU 模式仍由 native 上传并 draw，不通过 Java 逐顶点提交。compute/SSBO 是后续同一阶段内的能力增强，不是四平台基础功能的前提。

本次仅交付计划、API 设计和手动 CI 配置；不修改 `refs`、不实现 native、不启动游戏。后续本地只编译/测试 Windows；Linux、Android、macOS 通过 GitHub Actions 编译。各平台真实运行结果独立记录。

## 2. 参考证据与复用边界

以下为本地参考快照，不把参考项目的计划当成已实现能力。

| 参考 | 已核对事实 | 采用方式 |
|---|---|---|
| `refs/YesSteveModel-Native/docs/infrastructure/gfx_cn.md` | BakeModel → BakedModel → ModelState::Extract → Render；Render 写调用方 buffer | 复用 bake/extract/execute 分工、所有权与状态验证思路；新增 native GPU 提交 |
| `refs/YesSteveModel/docs/architecture/rendering/overview.md` | Java 控制宿主提交；含负尺寸、透明、normal/tangent 和第一人称语义 | 作为 YSM 行为对照，不能只复制矩阵算法 |
| `refs/YesSteveModel/docs/future/gpu-compute-renderer.md` | 明确 GPU compute 尚未实现，CPU 布局不是 GPU ABI | GPU 数据单独设计，不宣称直接移植现成 YSM GPU renderer |
| `refs/YesSteveModel-Native/docs/platform-porting_cn.md` | JNI、SIMD、依赖、目标 runtime 均需分别核验 | 借鉴跨平台清单，不照搬定制工具链及全部依赖 |
| `refs/YesSteveModel/docs/architecture/asset-pipeline/model-compatibility.md` | raw/legacy v1/v2/v3 转换到当前容器，转换有明确语义 | 借鉴历史格式解码与规范化；兼容身份单独保存 |
| `refs/Figura_Orig/common/src/main/java/org/figuramc/figura/model/rendering/ImmediateAvatarRenderer.java` | 部件遍历中有 pre/mid/post 回调、复杂度预算、RenderTask 与纹理更新 | 保留执行顺序，不能把整棵树无条件合并 |
| `refs/OMP/OMP_1.20.1/common/src/main/java/top/aihmc/omnimorph/render/acceleration/NativeSimdBackend.java` | JNI 变换后读回 Java double 数组 | 作为对照路径；新主路径不做每帧完整顶点回传 |
| `refs/OMP/OMP_26.3/common/src/main/java/top/aihmc/omnimorph/render/acceleration/MinecraftGpuDeviceAdapter.java` | 使用 GpuDevice、RenderPass、宿主 pipeline、fence | 版本适配必须处理真实后端与宿主状态，不能把对象地址当 GL 名称 |
| OMP 的 `YsmGeometryParser`、`YsmAnimationParser`、`YsmAnimationControllerParser`、`YsmPackage` | 当前解析、表达式编译和运行对象创建存在交织 | 拆开“数据解析”和“宿主求值”，保持默认值、事件顺序及诊断语义 |
| `refs/avatars` | 同时含 Figura 的 bbmodel/Lua/avatar.json、raw YSM、`.ysm` 和 ZIP | 本地差分与性能样本；扩展名不作为格式识别依据 |

OMP 版本目录有 1.20.1、1.21.1、26.1.2、26.2、26.3；配置中 Java 基线分别为 17、21、25、25、25。源码已重点核对 1.20.1 和 26.3；其余版本适配细节在 M0 核对，不假设完全相同。

## 3. 总体架构

```text
YSM 文件/目录条目 ─ native ysmparser ─ 不可变 asset ─ native bake ─ model
Figura 宿主解析结果 ───── 公共 mesh/part 描述 ────────────────────┘
Lua/Molang/游戏状态 ─ 宿主有序执行 ─ 局部姿态与材质增量 ─ instance
instance + 宿主 pass/context ─ native SIMD prepare ─ prepared
prepared ─ native GPU 顶点路径 或 native CPU SIMD 顶点路径 ─ native draw
```

拟建模块：

| 模块 | 职责 | 不承担 |
|---|---|---|
| `include/omm/omm.h`、`src/abi` | 稳定 C ABI、错误码、尺寸/版本校验、句柄 | Minecraft 类、STL 跨库布局 |
| `src/jni`、`bindings/java` | 固定包名桥接、DirectByteBuffer 批量输入、加载与握手 | 每个 MC 版本一份 JNI 二进制 |
| `src/ysmparser` | 探测、容器解码、解压、JSON/二进制解析、规范化及资源索引 | 下载、授权、执行脚本、播放音频 |
| `src/model`、`src/math` | 不可变 mesh、part/bone 层级、bake、SIMD kernel | UI 与游戏状态查询 |
| `src/render` | prepare、排序、批次、GPU/CPU 路径选择 | 跨语义屏障重排 |
| `src/gpu` | 上下文附着、shader、buffer、draw、fence、回收 | 创建或交换游戏窗口 |
| `tests`、`benchmarks` | ABI/解析差分、SIMD 差分、真实窗口渲染与计时 | 用模拟结果替代实机结论 |

实现采用 C++20、CMake、固定版本依赖清单；公开边界是 C11 可用头文件。参考 YSM 的实现方法，不为复用其 C++23 内部类型而直接引入整个运行时。

## 4. 跨版本共用同一 native

同一 OS/CPU/runtime 目标只发布一个 `omm_core` 二进制，所有受支持 MC 版本与 loader 使用同一文件和 SHA-256。跨 OS 或 CPU 不能共用同一机器码文件；共用源码、API、版本号与发布清单。

此要求长期适用于**未来 OMP 版本**，不限于参考目录的五个版本。新增 Minecraft/loader/OMP 版本只增加或修改宿主适配器，必须能加载既有稳定 native 并完成其已定义的 Figura、YSM、SIMD、GPU 与解析功能；不得因版本号变化而要求重新编译、复制分叉或发布专属 native。

采用长期稳定的基础能力合同：未来 OMP 保留对最低稳定 ABI 的调用路径，新 native 保留旧 ABI 与语义。新增可选功能先查询 capability，旧 native 不具备该功能时保留既有完整功能，明确标示新增功能不可用。不能把现有 GPU 渲染降为仅 CPU，或跳过 YSM/Figura 特性来通过兼容验收。缓存与内部布局升级不得强迫模组更换公共 ABI。

确有旧二进制未实现的新格式、图形后端或必需功能时，单独评估并审批统一 native 的能力升级；升级产物仍服务所有 OMP 版本，不产生按 MC/OMP 版本划分的库。新底层能力不由 ABI 协商凭空产生，此类需求不得伪装为普通版本适配。

每个未来 OMP 发布都加入兼容门禁：使用归档的最低稳定 native（固定 SHA-256）运行新适配器，再用最新统一 native 运行历史适配器。覆盖双引擎解析/渲染、SIMD 与 GPU、JNI 加载、回调顺序及资源回收；测试过程中禁止重编 native。该门禁持续扩展版本集合，不冻结为当前五版。

- native 不链接 Minecraft、Forge、NeoForge、Fabric、Iris 或 LWJGL Java 类。
- Java 桥接基线为 Java 17，面向 17/21/25 做同一二进制 JNI 加载验证；游戏适配器分别按项目的 Java 版本构建。
- 版本适配器只负责宿主资源、pass、矩阵、纹理、shader 语义映射；通过数据描述与后端互操作契约接入。
- C ABI major 不兼容则拒绝加载；minor 只追加字段/可选能力。结构体有 size/version；缓存、解析 schema、GPU 上传布局各自版本化。
- JNI 使用 RegisterNatives 注册 `omm_` 方法，系统要求的 `JNI_OnLoad` 是唯一命名例外；业务导出不得暴露 C++ 符号。
- 一个 JVM 由公共 bridge classloader 持有库；禁止同一 native 在多个隔离 classloader 重复加载。每个游戏上下文有独立 device generation。

## 5. 渲染路线与宿主互操作

### 5.1 基础 GPU 与 SIMD

先实现基础 OpenGL core 与 GLES 路径；基础路径不要求 compute 或 SSBO。顶点保存静态 position/normal/UV、part 索引及必要材质属性；姿态由 UBO 或能力允许的 texture buffer 访问。按设备上限分块，超限不能裁掉骨骼。GPU 无相应资源能力时选 native SIMD 生成顶点后上传的路径，仍由 native 绘制。

SIMD 以 SoA/AoSoA 为内部布局，公开 API 不暴露 SIMD 宽度。x64 的 loader/dispatch 保持基础 ISA，AVX2 独立编译并核验 CPU 与 OS 保存寄存器的能力；ARM64 用 NEON。不得全库启用 AVX2、`-march=native` 或不经对照的 fast-math。非均匀缩放、镜像、法线、切线 handedness 和 NaN 拒绝分别测试。

GPU 增强路径在已通过基础路径的设备启用 SSBO/compute/indirect；同一 native 仍保留基础 GPU 与 SIMD 路径。每次记录 requested/selected 路径和原因，不把 GPU 不支持悄悄计为 GPU PASS。

### 5.2 真正接入游戏渲染

附着宿主当前图形上下文，使用宿主提供的 proc resolver 和带 generation 的纹理/FBO/program 描述。不得创建第二个独立上下文代替游戏绘制。首个里程碑核实各 OMP 版本实际图形后端：高层 `GpuDevice` 的存在不证明可安全取得底层 GL 资源。

宿主在合法 pass 插入点 flush 先前命令，导出该 pass 的绑定、viewport/scissor、blend/depth/cull、light/overlay、fog、纹理与 shader 语义。Native 保存并恢复自己改动的 GL 状态；适配器同时处理宿主 Java 状态缓存的失效或重绑定。只还原实际 GL 状态而留下缓存失真不算通过。

26.x 若能合法导出当前 GL 设备与 pass，则走同一 native GL 后端；若实际为其他图形 API，须在 M0 记录具体证据与缺口。不得用 Java `drawIndexed` 转发冒称纯 native。新增 Vulkan/Metal 后端属于需明确评估的扩展，不默认纳入本计划。该问题不会影响 parser、ABI、SIMD 等独立实施，但未接入版本不能宣称完整支持。

### 5.3 Figura 与 YSM 行为

- Figura：cube 与任意 mesh、层级、pivot、parent type、可见性、UV 变换、主/次材质、emissive、复杂度预算和动态纹理均进入兼容表。
- pre/mid/post 回调按原有时间点执行，回调前需要的矩阵通过小型查询取回；回调后写入的状态影响相应后续 draw。每个可观察修改生成独立快照/有序段，不能整帧只上传最终姿态。RenderTask、附着物和透明层形成屏障。
- YSM：cube/pivot/mirror/inflate、负尺寸轮廓、透明/双面分类、normal/tangent、locator、第一人称双手、投射物/载具及已有材质行为纳入验收。
- 两引擎可共享几何与 GPU 资源，但实例、动画推进权、事件触发和脚本状态分别持有；多 pass 不重复推进 tick/事件。
- 原版、阴影、GUI、人称、Iris/shader pack 逐项申报 capability。不能绕过宿主光照、雾、阴影与透明排序来获得更高帧率。

### 5.4 提交与寿命

GPU 操作只在绑定的渲染线程；解析和独立模型 bake 可在 worker。prepare 输入同步借用，返回后不持有 Java buffer 地址；prepared 持有不可变快照及资源引用。

热重载先停止接纳旧 generation，再等待已提交 fence 后释放资源。上下文丢失使旧 GPU 对象不可用，CPU asset/model 可用于重建。显式释放为主，Cleaner 仅兜底且不直接从 GC 线程调用 GL。

draw 前预检所有输入，区分 `not_submitted`、`submitted`、`partial`。已经部分提交不能整体重放另一条路径，否则会重复透明绘制；只报告并终止该段。选择 CPU/GPU 必须发生在提交之前。

## 6. Native ysmparser

### 6.1 输入与输出

支持目标：raw 目录条目集、ZIP、历史 YSM v1/v2/v3 及当前 YSM Asset Container。按魔数/结构识别，不依赖文件后缀。M0 为每类建立具体可读样本及预期；已有受支持格式迁移不能少掉。某格式缺样本时单列 `UNVERIFIED`，不靠版本名称宣称覆盖。

目录由宿主枚举为 UTF-8 相对路径+bytes 条目集传入；native 不自行遍历任意目录。容器尽量在内存解析，不解压到工作目录。条目路径规范化、重复/大小写碰撞语义按来源 profile 固定，防止 Windows/Linux 表现不同。

输出是 native 拥有的不可变 asset：metadata、geometry、bone/locator、动画关键帧、controller 数据、表达式原文/位置、纹理/音频资源索引。geometry 直接交 bake；Java 只按需读取动画/metadata，不把全量模型转 JSON 再交回 native。

### 6.2 解析与求值边界

Native 负责 JSON/二进制结构、数值、默认值、关键帧排序、pre/post、loop、controller 转移数据和表达式字段提取。Molang 编译/执行、query 查询、Lua 回调与游戏事件继续使用宿主已有实现。不能将表达式直接当 0，也不能用“解析成功”代替动画生效。

解析 profile 标明来源语义（OMP 兼容或当前 YSM 格式），保存原始版本与来源身份。遇到 OMP 与 YSM 主仓行为不同，先用实际样本与产品行为确定，不能混用默认值。无 `animation_length`、空动画、轮盘默认值、同时间事件顺序均需差分用例。

历史解码参考 `YesSteveModel-Native/modules` 中的 legacy 实现；只移植实际所需 decoder 和依赖，不恢复旧联网、缓存或授权体系。无法解码、未知版本、损坏与不支持必须区分；不静默当作空模型。

### 6.3 性能与资源约束

选用支持目标架构的 JSON 解析库，优先评估 SIMD JSON 方案；最终由相同语义差分与基准决定，不预先承诺倍率。压缩依赖按实际格式引入 zlib/minizip、zstd；历史算法依赖按解码需求加入。GL loader、JNI、测试库与以上解析依赖固定版本/哈希，生产依赖尽量静态链接在单一动态库内。YSM 的音频、追踪及全套通用基础设施不整包引入。

输入来自用户模型，必须有输入字节、解压总量、单条目、条目数、嵌套深度、骨骼/顶点/关键帧数和总内存预算，检查尺寸运算溢出。拒绝路径穿越、环形骨骼与越界索引；输出发布前验证完成。失败销毁临时 asset，不发布半成品。

比较旧 Java parser 与 native parser：文件读取、解码、结构解析、规范化、JNI 输出、bake 分开计时；总加载时间必须包括 JNI 与宿主动画绑定。缓存冷热分开，统计 p50/p95、峰值内存、Java 分配和 native retained bytes。小文件若 JNI 成本占优如实记录，不能只报告 JSON kernel 吞吐。

## 7. 平台与构建

首批目标（计划基线，必须在 M0/M1 用工具链验证）：

| 平台 | 架构 | 拟定基线 | 验证地点 |
|---|---|---|---|
| Windows | x64 | Windows 10；MSVC 2022；SSE2/AVX2 | 本地与手动 CI |
| Linux | x64 | Ubuntu 22.04 构建环境；glibc 符号基线写入产物 | 手动 CI 编译、CPU/JNI 测试 |
| Android | arm64-v8a | API 26；NDK r28c；Bionic；NEON | Linux CI 交叉编译；目标运行另记 |
| macOS | arm64、x64 | deployment target 13.0；AppleClang；NEON/SSE2 | 各架构 CI 编译、CPU/JNI 测试 |

Windows ARM64、Linux ARM64、32 位平台不自动包含；后续需求单独批准。macOS 基础 GPU 路径不依赖 GL 4.3 compute。Android 需核对启动器实际 JVM、EGL/GLES 或桌面 GL 翻译层，导出的函数必须属于同一个宿主上下文，不能把 Bionic `.so` 当 Linux glibc `.so`。

CI 配置见 [`../.github/workflows/native-manual.yml`](../.github/workflows/native-manual.yml)，只允许 `workflow_dispatch`，无 push/PR/schedule 触发。本次提供的是实施骨架，M1 需要交付其中约定的 `tools/build-native.ps1`；缺少入口时明确失败，不制造空产物成功。配置不打包 `refs/avatars`。macOS runner 使用 `macos-15`（ARM64）与 `macos-15-intel`，依据 [GitHub runner 矩阵](https://docs.github.com/en/actions/reference/runners/github-hosted-runners)，构建主机版本不代替 deployment target。

构建入口约定：`tools/build-native.ps1 -Target <target> -Configuration Release -OutputDirectory <dir>`。负责固定依赖、CMake configure/build、匹配宿主的 CPU/JNI tests、安装、导出/依赖检查和 manifest。Android 只交叉构建，不在 Linux 执行 ARM64 测试；CI GPU/真实游戏验收另记 `MANUAL/PENDING`。

全部构建、打包、测试使用前台 PowerShell，命令经 `2>&1 | Tee-Object -FilePath build.log`，立即保存 `$LASTEXITCODE`，失败读取日志并返回原退出码。各步骤保留独立日志，避免后一条覆盖前一条。不得后台编译或静默丢弃错误。

输出包含库、公共头文件、bridge JAR、许可证、manifest 和测试报告；manifest 包含源码提交、ABI/schema/cache 版本、OS/arch、最低 runtime、编译器/依赖版本、SHA-256、实际验证状态。所有模组消费相同目标的同一 artifact，不各自重新编译 native。

## 8. 有序实施里程碑

以下是未来实施计划，不是本次已完成阶段。

| 阶段 | 实施内容 | 自动验收与退出条件 |
|---|---|---|
| M0 证据与契约冻结 | 五版 OMP 的 pass/后端互操作；格式样本表；性能基线；冻结 API 结构与平台工具链 | 所有目标映射有来源；parser 与双引擎样本清单可重复；不支持的后端明确记录 |
| M1 构建与 ABI | CMake、依赖锁、`omm_` C ABI/JNI、句柄/错误、Windows 构建入口和手动 CI | Windows Debug/Release；C 头文件编译；Java 17/21/25 同一 DLL 加载；导出与错误路径测试 |
| M2 native ysmparser | raw/ZIP/legacy/current 解码、规范化 asset、资源读取与解析诊断 | 格式差分、动画字段/事件顺序、损坏/限额/清理测试；解析基准含 JNI 成本 |
| M3 模型与 SIMD | Figura mesh 与 YSM asset 共用 model；标量/SSE2/AVX2/NEON；pose/normal/排序 | 两引擎同样本数值与拓扑差分；边界/生命周期测试；Windows 实际执行 x64 kernels；NEON 运行证据另记 |
| M4 native GPU | 上下文附着、基础 GPU 路径、SIMD→GPU 协同、CPU native draw、fence/回收 | Windows 可见窗口自动绘制与像素/状态检查；两个引擎都有 native draw 计数；GPU/CPU 路径强制测试 |
| M5 双引擎与五版本集成 | 逐版适配；Figura 回调屏障；YSM 动画结果；GUI/第一人称/透明/阴影/热重载 | 同一 native 哈希下适配器契约测试；两引擎与混合场景；可自主运行的集成检查完成 |
| M6 跨平台产物与性能交付 | 手动触发矩阵、产物归一、性能复测、文档与已验证支持表 | 全矩阵构建结果可追溯；同平台五版本同哈希；基准达到冻结目标或明确未达标，不虚报完成 |

每阶段自动验收通过后更新进度，检查 `git status`/`git diff`/`git diff --check`，提交并推送当前分支，确认成功后才进入下一阶段。保留用户既有改动；分支不添加 `codex/` 前缀。人工/实机待验项记录 `MANUAL/PENDING`，不得因此标记任务 blocked 或停掉可自主实施工作；跳过只按用户决定记录 `SKIPPED`。

## 9. 验收与性能目标

### 正确性

建立自己拥有分发权的最小 fixtures；`refs/avatars/小符玄` 作为 Figura 脚本样本、`01_taisho_maid` 等作为 raw YSM 候选，`.ysm`/ZIP 经探测归类后选择历史样本。记录 SHA-256、实际解析格式和覆盖特性，不按文件名推断复杂度或可解码性。第三方完整模型只做本地验收，不上传 CI artifact。

每种引擎覆盖静态、动画、透明重叠、镜像/非均匀缩放、动态纹理、回调改姿态、第一人称、GUI、重载和混合穿搭。数值结果检查有限性、矩阵/normal 容差；拓扑、索引、事件顺序严格一致。截图比较固定相机/时间/光照并另查阴影、透明和边缘，避免单一像素阈值掩盖缺面。

真实 GPU 验证必须有实际可见窗口；游戏启动遵守用户授权。测试窗口稳定后再捕获，完成后关闭。若涉及 Qt，严禁 offscreen。编译、CPU 测试、模拟 replay 和真实游戏验收分别记录，不能互相冒充。

### 性能

以 OMP 当前 Java/SIMD/GPU 路径分别作基线，而非只对比最慢模式。固定设备、驱动、分辨率、场景、模型哈希、相机、VSync 与预热，1/10/50 个实例、Figura/YSM/混合各测至少 5 轮。GPU 时间使用异步 timer query，报告 CPU frame p50/p95/p99、GPU 时间、draw/JNI 次数、上传字节、GC/native/VRAM 与加载时延。

建议在 M0 冻结的工程目标：高密度场景模型渲染 CPU p95 降低至少 30%；GPU frame p95 不回退超过 5%；native parser 冷解析+规范化+桥接 p95 降低至少 20%；小模型总帧耗时无显著回退。上述是目标不是实测结论；达不到时定位开销并报告，不靠取消脚本、降低画质或跳过模型达标。

## 10. 当前状态与实施入口

进度：M0 已完成，见 [M0 证据与契约](m0/M0-contract.md)。

本次已形成设计：双引擎、SIMD+GPU、native ysmparser、跨版本 ABI、平台矩阵和手动 CI。尚无 native 源码、编译结果、性能倍率或真实窗口 PASS。下一项是 M0 的互操作与格式契约核实，然后按 M1–M6 实施；不得直接跨过 API/语义确认开始大规模替换渲染器。

外部构建依据：GitHub 的 [手动 workflow 说明](https://docs.github.com/en/actions/how-tos/manage-workflow-runs/manually-run-a-workflow?tool=webui)；Android 的 [16 KB 页支持](https://developer.android.com/guide/practices/page-sizes)（r28+ 默认 16 KB 对齐，仍检查最终 ELF）；GLFW 的 [平台上下文约束](https://www.glfw.org/docs/latest/compat_guide.html)。手动入口需 workflow 存在于默认分支；本次不触发远程构建。
