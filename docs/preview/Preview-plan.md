# 独立实窗预览器实施计划

日期：2026-11-07。状态：计划已建立，优先实施。用户明确调整顺序：暂停当前 M3 收尾流程，先交付便于后续测试的独立预览器。M3 的未提交改动、日志和报告完整保留；不把 M3/M4/M5 提前记为完成。

补齐预览器缺口前，以[当前最小缺口清单](Minimum-gaps.md)固定范围：仅实施已确认项；每类模型只抽样、加载一次，停止逐项重复加载。该清单不替代原阶段交付要求。

## 当前主要持续性目标：专项性能优化

用户指定专项优化优先，其他开发阶段暂缓。主线为固定真实样本的单模型 CPU/GPU 与解析专项：实窗基线、一次一个热点、同配置实窗复测、逐项报告 FPS/百分比、阶段提交推送及额度检查。覆盖原三类样本，并增加旧版 YSM 独立类别；最终多模型测试等待全部路径优化完成，不以“全绿”目标更改画面或隐藏下降成绩。

旧版容器按文件头识别，不根据名称或修改时间猜版本。新增抽样记录见 [旧版样本清单](legacy-sample.json)，保留原三类选样不变；实际内部binary版本和动画可用性由后续实窗加载记录确认。

## 最新测试顺序（用户要求，2026-11-07）

优化期间只用单模型：固定原抽样YSM目录、加密YSM、Figura，分别覆盖Java/native单线程的CPU/GPU路径及各自必要解析测量。先完成所有路径优化与验证，再统一进行最终多模型same-model/mixed五档测试。已有多模型成绩保留为旧基线，优化期间不新增多模型测量，不为每个小改动重复多模型矩阵。

## 用户授权的性能实验（2026-11-07）

用户要求依据 benchmark 同时分析 CPU/GPU，参考 OpenYSM/Sparkle-Morpher，找到可能原因后尝试优化并复测，目标提高真实成绩。已完成的 [CPU/GPU 分项分析](Benchmark-analysis.md)记录实际数据、源码差异及可证伪的实验顺序。

保留全部剩余正式矩阵，先进行一次只改一个热点的单线程性能实验；不将未完成矩阵或主M3提前标为完成，不因目标“全绿”改变模型、画面或采样口径。每个实验通过数值/真实窗口验证后独立提交推送并查询额度。

## 用户调整的代码检查点（2026-11-07）

用户要求预览器暂时没有必改项时直接提交推送。当前 G01–G08 已确认缺口均已补齐，先提交可用的预览器及必要 native 模型/执行器依赖，不等待剩余正式测量。不将此代码检查点等同于完整 benchmark 或主 M3 完成。

正式测量继续逐例记录；50 实例正在运行，100 实例及 Figura 完整运行仍待验证。测量分析后，按实际热点对照 refs/OpenYSM 与 refs/Sparkle-Morpher，优先定位单线程顶点变换、跨语言调用、缓冲复制和提交开销，再决定最小优化；参考项目不自动扩大实施范围。

## 最新随机测量检查点（2026-11-07）

- 固定 seed42 原样本不变：YSM 目录 `08_sta`、加密霜狐、Figura `sickle.zip`。
- 两个 YSM 单模型各 Java/native 单线程 × CPU/GPU 的 8 个正式案例已完成，结果见 [随机 CPU](random-single/Benchmark-CPU.md) / [随机 GPU](random-single/Benchmark-GPU.md)。此前手选数据仍为辅助基线。
- Figura 两次正式运行失败不计成绩；确认 ARMOR 和根级 getChildren 两项公共缺口，已按表记录后做最小修复及无模型专项测试。修复后的完整 Figura 行为未验证，不关闭脚本或换样本。
- 单线程多模型正式测量继续；8/16 后续测量保持 SKIPPED（用户决定）。不得将旧实窗正确性短测当作完整正式矩阵。

## 当前范围核对（2026-11-07，用户询问进度后）

当前仍是独立预览器交付阶段；主M3暂停，未完成预览器提交/推送。下方逐次“增量”仅是实施记录，不是用户批准的新增需求，也不能自动增加完成门槛。

| 用户要求的交付项 | 当前状态 |
|---|---|
| tools/preview独立JAR、配置启动、五类模型输入 | Java五类输入已实现；native YSM目录/ZIP/加密输入已接并验证 |
| 实窗、FPS记录、草地/天空/assets、皮肤缓存 | 已实现并有实窗证据 |
| 动画/脚本、轮盘/模型设置、附件隐藏及实例隔离 | 已接大量原实现并验证；不等于全部Minecraft宿主API兼容 |
| Java/native与CPU/GPU组合、线程预算 | Java CPU/GPU与native CPU/GPU1/8/16/auto已接，native骨骼/实际Draw与解析已验证；单线程正式矩阵继续执行；后续8/16线程按用户要求暂缓 |
| 同模型/混合模型1/5/20/50/100、共享/隔离、完整指标 | 已接场景/共享隔离/可见提交/峰值内存；正式矩阵报告待完成 |
| 正式分模型H2 Markdown结果、阶段提交推送、额度检查 | 已生成解析及单模型CPU/GPU报告；多模型正式矩阵执行中，尚未提交推送 |

范围审计结论：原计划第3/5节要求真实解析、播放和脚本效果，但未要求重建完整Minecraft运行时或穷举所有模型/宿主分支。此前把持续发现的宿主兼容细节直接加入交付前置条件，造成实施膨胀。额外内建状态机代码已按用户要求撤回，不继续追求全API/全模型兼容。

后续按原验收主线推进。额外宿主API/设备pending脉冲/程序化手臂细节，只有已证实阻止明确要求的功能或真实抽样用例时，才实施最小必要修复；其余列为后续事项，不能再自动升级为交付门槛。不得关闭样本必需脚本、改用静态姿态或只选简单fixture冒充真实模型benchmark。CPU/GPU/线程、多模型、指标和正式报告等用户明确要求全部保留，不因收敛兼容工作而删减。

撤回后的必要回归正常收尾；后续只在产品变更、失败或新证据需要时重复相应验证，不为更新文字重跑全套。

## 1. 交付物与目录

用户新增验收（2026-11-07）：Lua未隐藏原版模型时，绘制带皮肤的原版玩家；允许URL皮肤。首次成功请求持久缓存，同URL不重复请求，后续可省略URL复用最近皮肤。默认按原始PNG窄臂空白透明像素检测Steve/Alex，同时提供显式覆盖。

实现增量：vanilla已合并实际顶点/皮肤/部件palette，Figura及开启Lua的YSM使用实例隔离的vanilla_model可见性，全部隐藏会实际跳过该Draw Call。缓存、auto/steve/alex及强制覆盖、部位/组隐藏、实例隔离、YSM/Figura和单次HTTP请求测试通过。用户563皮肤真实下载、识别Alex、窗口轮盘隐藏/恢复已目视确认（Draw Call3→2→3），省略URL后90帧缓存启动成功，哈希一致。关闭顺序调整为先完成GLFW原生清理、最后释放Swing后，完整16窗口回归PASS并正常退出（build-preview-test-close-order.log）；之前AWT等待的失败日志保留，不能算PASS。皮肤特性不等同整体预览器完成。中立站姿、64×64PNG为本轮范围；完整原版动作/装备仍在原宿主接入工作中。

2026-11-07 世界时钟增量：`world.getTime/getTimeOfDay`已接实际20Hz脚本计数，保留原tick+delta及累计dayTime语义；初始world.time/dayTime可配置。原math.lua已原样入JAR并执行。数值/模型矩阵/seek/实例隔离测试及完整15次实窗回归通过。真实AI Hobbyist Figura配置玩家状态后，init/t=.25/t=5脚本回放成功，.25秒88个drawable owner矩阵改变；真实JAR运行120帧正常退出并记录animation/bones/GPU，run见build/preview/runs/real-figura-clock/20261107-111659-731-29992。这是当前状态动画路径证据，不是粒子/声音、所有模型分支或正式benchmark通过；主计划其余验收不变。

2026-11-07 按键增量：原FiguraKeybind/KeybindAPI已接实际GLFW键盘/鼠标/scancode队列与配置重放、失焦释放、初始state/vanilla映射和状态快照。回调→实际模型矩阵、重复边沿、modifier/override、GUI/禁用门控、记录重放均通过，完整回归14次真实窗口PASS。测试classpath改为复用构建精确依赖，避免旧LWJGL JAR被wildcard选入。真实AI Hobbyist模型提供显式玩家值后已完成初始化/零时刻渲染；0.25秒tick在world.getTime缺失处失败，下一步补实际模拟时钟。原生键盘人工操作MANUAL/PENDING，不将自动配置重放等同人工键盘验收。

2026-11-07 向量与玩家模拟增量：已复用原Figura二维至四维向量/矩阵与VectorsAPI/MatricesAPI、纯参数/颜色数学；部件变换可接原向量并返回独立getter值。`states.player`显式值驱动常用只读查询，缺失/错类型报键名；local pings是真实实例内Lua回调。自有端到端矩阵验证及13次真实窗口通过；随后named-color补齐的最终构建和向量测试通过。真实`AI Hobbyist Founder Public Server`脚本探测在`keybinds:newKeybind`缺失处失败，不能报告其完整脚本通过。下一步接真实按键宿主，再处理其后实际依赖；相机转换、粒子/声音、world/entity等宿主与native/多模型基准仍待完成。

2026-11-07 Figura关键帧增量：已复用原Animation/AnimationPlayer/Interpolation/TransformType/AnimationAPI，保留原bbmodel动画metadata与部件channels；实际Lua表达式、时间轴、速度/循环/暂停/反向/优先级/混合接GPU palette。固定seek按60Hz重放，目录/嵌套ZIP数值测试通过；loop/hold/once、多循环事件与实例隔离均有测试。完整Test-Preview通过12次真实窗口。此处覆盖前文“关键帧尚未接”的历史状态；动态bbmodel Molang属性、完整向量/部件/模拟player及weighted armature仍未完成，完整模型benchmark仍不开放。

2026-11-07 Figura增量：原bbmodel解析输出现保留部件层级、绑定矩阵、默认可见性和逐顶点owner；隐藏几何仍保留以便脚本恢复。`animation.mode=figura`配合`scripts.enabled=true`可运行原LuaJ事件、模型设置和轮盘，`models.<模型>.<部件>`的标量setPos/setRot/setScale/setPivot/setVisible实际更新GPU矩阵。自有测试覆盖已知点、轮盘设置回调、父子隐藏、实例隔离、seek重建和旋转/缩放；完整Test-Preview通过11次真实窗口。Figura关键帧动画/完整向量与模型API仍待接入，不将该子集视为完整Figura模型播放验收。

最新增量（2026-11-07）：YSM附件默认隐藏、原声明动作/设置schema、自动轮盘分页与本地化已连接实际骨骼播放。`animation.actions=true`启用；hold释放可交互/重放/记录。目录及嵌套ZIP动作姿态测试通过；53个真实YSM注册检查通过；真实加密new.ysm完成120帧运行。最终Test-Preview包含10次真实窗口，全部通过。Figura完整模型动画/Lua宿主、设置持久化、完整控制器宿主及native/CPU/GPU/多模型矩阵仍未完成；当前不算完整预览器交付。

- 源码、构建/启动脚本、配置样例和使用说明：`tools/preview/`。
- 可执行程序：`build/preview/preview.jar`，通过 `java -jar ... --config scene.json` 启动；依赖和平台 native 库按发行目录部署，说明所需 Java 版本。
- 默认生成数据：`build/preview/runs/<run-id>/`，包含最终解析配置、来源/模型哈希、日志、逐帧记录、汇总和必要截图。
- 独立报告：CPU、GPU 分开写 Markdown；一个抽样模型/场景一个 H2 标题和一张表格，交付时返回路径。
- 第三方完整模型、解密内容、缓存与大体积输出不提交到 Git 或 CI artifact。

初始实现采用 Java + LWJGL/GLFW 创建正常 Windows 可见 OpenGL 窗口，通过 JNI 调用 C++ native 核心。优先复用系统现有 JDK、Gradle 缓存依赖及参考模组代码；具体依赖版本在构建脚本中固定并保留许可证声明。

## 2. 当前流程暂停与恢复

1. 已保存 M3 现状：模型/实例/bake/SIMD/排序/JNI、runtime 工作线程复用、replay 和 CPU 基准；M2 已推送 `f51df7d`，M3 尚未提交。
2. 暂停 M3 的剩余审计、阶段提交与后续顺序推进，优先实施本预览器；允许仅为预览器接通必需的 native 能力，不借机清理无关模块。
3. 预览器作为当前独立交付单元，完成下述验收后检查 diff/status、提交、推送、确认远端并查询额度。若交付依赖现有未提交的 native 支持，明确列入其最小必要依赖，不把其他未完阶段混记为完成。
4. 用户后续明确的顺序：预览器完成并提交推送 → 查询剩余额度 → 更新主仓库根目录AGENTS.md，加入经过验证的预览器构建/配置/启动/记录及测试使用说明 → 恢复原M3–M6主开发阶段。更新AGENTS时使用writing-for-agents技能；不提前把未实现选项写成可用能力。
5. 预览器源码、测试、配置、构建脚本及复用代码全部放在tools/preview/；生成的JAR、依赖和运行输出放在build/preview/。总目标保持active；这里只暂停原先的实施顺序。

## 3. 输入与解析复用

必须覆盖以下五类输入，按内容识别并使用原有资源语义：

| 输入 | 引擎 | 验收 |
|---|---|---|
| YSM 文件夹 | YSM | 几何、贴图、动画/controller 加载并实际绘制 |
| Figura 文件夹 | Figura | avatar、bbmodel、贴图、动画和脚本加载并实际绘制 |
| YSM ZIP | YSM | 资源根目录及相对路径正确，不要求先手动解包 |
| Figura ZIP | Figura | 与对应文件夹的几何、脚本、动画结果一致 |
| 加密 `.ysm` | YSM | Java/native 解析路径可选，模型及资源正确进入预览 |

用户已授权直接复用任意参考版本模组的 Molang、`.ysm`、`.bbmodel`、Lua 相关实现。以真实调用路径确定最小依赖，保留来源、修改说明和许可证，不重写已有解析器。不得以现有 M2 harness 的 Molang/图片 stub 冒充预览器实际解析/执行能力。

native parser 继续只解析，不执行脚本；Java 宿主执行动画、Molang/Lua、事件及 controller，再提交姿态。脚本回调/求值不因 benchmark 被取消。未支持的宿主 API 明确诊断并使对应 case 保持未通过，不能静默跳过后计为有效性能结果。

## 4. 配置与命令行

以配置文件为主，命令行仅提供帮助、配置路径和少量显式覆盖项；冻结实际参数名后写入 README。

```text
java -jar build/preview/preview.jar --config tools/preview/configs/single.json
java -jar build/preview/preview.jar --config scene.json --state running=true
java -jar build/preview/preview.jar --help
```

配置至少包含：native 库路径、模型列表/引擎探测、场景类型、实例数或数量分档、随机种子、Java/native、CPU/GPU、线程数、动画选择/固定时间/时间偏移、模拟宿主状态、相机、分辨率、VSync、预热帧数、采样帧数、轮数、记录及报告路径。

校验错误需包含配置项位置和原因。每次运行保存最终解析配置及命令行覆盖结果；相对路径基准和覆盖优先级必须明确。允许一次配置驱动完整组合矩阵，自动切换前重置场景与固定动画时钟。

## 5. 脚本与动画宿主

用户新增要求：复用OMP已有YSM实现，支持附件默认隐藏；并支持YSM与Figura的动画轮盘及模型自带设置。默认隐藏需按基础动画/初始化控制器与原资源语义计算，不能硬编码模型名或删除附件mesh；后续轮盘/设置/脚本必须能恢复显示，父子可见性与实例状态隔离须验证。轮盘使用各引擎原动作/脚本定义，支持实际选择/播放/停止及原回调；模型设置使用原控制定义、默认值和绑定，改变动画、姿态、可见性或脚本值，而非仅显示选项。轮盘与设置的最终值/操作需可配置复现并记录，YSM/Figura分别做自有fixture及实窗交互验证。

- 不启动 Minecraft。用户已明确当前所有测试均不需要游戏进程。
- 必要的 Minecraft 状态由配置/命令行模拟，例如 `running: true/false`；提供类型、名称及实际脚本 API 的映射说明。
- 使用固定模拟时钟与种子；实例的 Lua/Molang 环境、controller、事件进度、骨骼及可见性互相隔离。
- 同一模型的多个实例可以播放不同动画或使用不同时间偏移；混合模型分别使用各自动画。
- 正确性测试应证明 running 状态切换、动画关键帧/controller 和脚本改姿态真正影响可见模型，不只证明解析未抛异常。

## 6. 执行路径与组合矩阵

| 路径 | CPU 准备 | 实际绘制 |
|---|---|---|
| Java CPU，1线程 | Java 动画/骨骼/顶点/排序 | Java GL 提交 CPU 变换结果 |
| native CPU，1/8/16线程 | 宿主动画 + native 骨骼/顶点/排序 | native CPU 准备后的真实 GL draw |
| Java GPU，1线程 | Java 动画及 GPU 所需姿态 | Java GPU 基线 |
| native GPU，1/8/16线程 | 宿主动画 + 对应线程预算的 native 准备 | native GPU draw |

GPU 组合中的线程数控制 CPU 准备，不冒充 GPU 并行度。CPU/GPU 使用同模型、贴图、动画时钟、相机和画质。显式选择的路径不可静默降级；能力不支持时记录原因，不填入替代路径的成绩。Windows 实际执行的 native draw 必须有可核查计数。

## 7. 场景矩阵和可见性

- 单模型：一次解析/播放/预览一个模型，保持与 parse 的 Java1/native1/native8/native16 比较一致。
- 相同模型多实例：共享不可变 mesh、纹理等资源，分别持有脚本/动画/骨骼状态。
- 不同模型混合渲染：YSM、Figura 及混合模型集分别验证，随机位置与不同动画/时间偏移固定。
- 多模型数量档位统一为 **1/5/20/50/100**，替代早期 1/10/50。
- 一个大空间，固定随机种子、相机、分辨率和动画时间。布局/相机应确保测试模型进入可见范围，并在全部组合间保持相同。
- 每帧记录配置模型数、实际可见数、已提交数。视锥剔除导致工作量变化时明确报告，不用配置数量冒充提交数量。
- 验证多个实例确实绘制而不是重复一份计数；验证共享资源数量、独立骨骼/脚本状态和不同动画结果。

## 8. 实窗显示、记录与计时

窗口显示模型/动画、当前路径/线程/场景、FPS、CPU帧耗时与可用 GPU 时间。实现方便观察和测试的最小交互，不扩展为无关编辑器。

用户补充：默认使用无限延伸的Minecraft草方块地面，材质从本机游戏JAR读取，不启动Minecraft。地面实现及配置仍位于tools/preview/；提取的第三方纹理仅存build/preview/。地面和模型使用同一相机及正确深度遮挡，支持配置关闭和草地染色；记录原JAR/纹理来源及哈希。模型与地面的Draw Call、CPU提交和GPU计时分列，避免把环境绘制误计为模型本身的开销。

用户进一步要求：背景天空也参考Minecraft，并把预览器使用的内置贴图放进JAR内assets目录。草地、云、太阳、满月按原路径打包到assets/minecraft/...，运行时通过classpath读取；原JAR/贴图哈希清单位于assets/omm_preview/texture-sources.json。天空采用蓝天渐变和实际云/日/月PNG，环境方向/高度固定可配置；天空的Draw Call、CPU提交和GPU query同样独立统计。无需外部环境PNG目录，不启动Minecraft。

可启停逐帧记录，保存 CSV/TSV，至少包含 run/case/frame ID、时间戳、场景/模型、路径/线程、动画时间、配置/可见/提交数，以及：

- CPU 动画更新、骨骼变换、顶点变换、排序/准备、GPU 提交的各阶段耗时；GPU路径没有CPU顶点变换时明确标为不适用。
- 实际 GPU 绘制耗时，使用异步 timer query，按原 frame ID 关联，避免错配当前帧；不可用值单独标识。
- Present 等待：记录实际 swap/present 调用的 CPU 阻塞耗时，注明 VSync，不混作 GPU 时间。
- 模型 Draw Call、上传字节、端到端帧耗时、FPS、峰值进程内存及其采样口径；可用时分列 Java/native/VRAM 数据。

预热后采样，每个 case 至少5轮。报告平均FPS及p50/p95/p99帧耗时，保留卡顿帧和原始数据。CPU/GPU分别输出；模型/场景/数量/线程组合不混合统计。同期出现编译或测试进程时单独注明“可能影响结果”，不终止其他会话进程。

## 9. 验证与完成条件

- JAR可从命令行/配置文件启动；`--help`、无效配置及资源错误有明确诊断。
- 五类输入均有自有可分发 fixture；本地真实模型单独验证，不上传其内容。
- 实际可见Windows窗口稳定显示模型和动画，观察FPS/HUD，必要截图检查纹理、透明、镜像/缩放及脚本变化；完成后正常关闭窗口和资源。
- Java/native、CPU/GPU、1/8/16线程组合有实际执行证据与输出一致性检查；不支持项如实列明，不冒称通过。
- 两种多模型场景及1/5/20/50/100数量档位可配置复跑，可见/提交/Draw Call与实际绘制吻合，资源共享和实例状态隔离测试通过。
- YSM附件默认隐藏与恢复符合OMP基础动画和初始化controller语义；YSM/Figura动画轮盘及模型自带设置均有实际可见交互、原动作/脚本回调执行及状态变化证据，配置/记录可复现。
- 逐帧日志可解析，GPU query关联正确，汇总可从原始记录重建；每模型/场景独立表格、CPU/GPU独立报告，返回路径。
- 所有编译/测试前台PowerShell + Tee日志，立即检查退出码；readability/formatter/diff --check通过。
- 不用Qt offscreen，不启动Minecraft。真实窗口预览结果与Minecraft实机性能分开；后者为 `MANUAL/PENDING`，不阻塞当前工作。

本计划是一项优先交付，不意味着其余native GPU/跨平台/五版本集成已完成。阶段验收按实际证据更新，不用规划文字代替实现。

## 10. 当前实施证据（2026-11-07）

### 新增进展：实际轮盘/设置窗口与可重放操作

- Java/Swing伴随控制窗口已接入GLFW预览：圆形轮盘、模型设置页、中文标题；原ActionWheelAPI/Page/Action的点击/切换/滚动/全局拦截/分组语义连接同实例Lua与Molang，不在EDT执行脚本。`interaction.enabled`开启窗口，固定frame actions及JSONL replayFile使用同一渲染线程命令队列。
- 外部操作记录frame/动画时间/请求/操作后的实际状态到interaction.jsonl，关闭时保存interaction-state.json；回放逐帧GPU palette点及最终模型状态一致PASS。Core wheel测试覆盖左右键、toggle/untoggle、scroll/action/page、global interception、keepSlots与实例隔离。
- Computer Use真实操作已验证中文轮盘“向右移动/恢复位置”、从轮盘进入设置、“水平偏移”应用2→轮盘1→重置0，实际窗口输入日志位于build/preview/fixtures/runs/20261107-095921-123-29900/interaction.jsonl；模型/FPS/草地/天空窗口已观察，两个窗口正常关闭，process exit0，无Minecraft。F1/R/Escape快捷键仍MANUAL/PENDING，不当成blocked。
- 最新Build/Test-Preview PASS（build-preview-build.log、build-preview-test.log），9个真实预览run含伴随控制窗口自动清理；最后交互run ...100609-410-27692。UI目前验证上述数值/重置/页面与轮盘操作，未据此声明所有控件/模型均验收。
- 仍需原YSM声明动作资源/自动轮盘及完整Figura模型部件/动画和host API；当前YSM declarative trigger明确拒绝，贴图/物品图标和完整设置持久化/live hydration未接。完整asset benchmark继续拒绝，preview整体不记完成，不提前阶段提交/更新根AGENTS/恢复M3。

### 新增进展：真实 LuaJ 与模型控制链路

- 已使用FiguraMC LuaJ core/jse 3.0.8-figura，复用原events队列、whitelist/metatable转换、sandbox、PathUtils及AvatarControlsAPI/Definition/Type。目录和嵌套ZIP脚本、autoScripts、相对require/cache、init/tick/render/post回调、seed和实例隔离已有实际执行测试。设置→原Molang VariableStorage→GPU palette已连接；controller回放中实际插入20Hz Lua tick，post事件在真实场景提交后调用。
- scripts.enabled可选开启，当前要求YSM animation.enabled；scripts.controls保存可复现初始覆盖。自有固定0.125秒已知点(0,5,0.5)与配置/init/tick/render/回退/实例隔离及controller转场测试PASS，8真实窗口及Lua窗口CSV/GPU记录PASS（build-preview-test.log），Lua输出...094533-519-27868；59资源最终加载回归PASS（build-preview-corpus.log），嵌套反射控制回调预算及后续恢复测试PASS（test-lua.log）。没有启动Minecraft。
- 本轮实际RED为Lua table无法传入模型Enum.options(List)，保留test-lua-red.log；对原binder补实际List转换后GREEN。脚本预算和反射传播同时验证，pcall不能吞掉宿主限额，未加载io/os/luajava。
- 仍未完成：完整Figura模型部件/动画与player等host API、原动作轮盘、可见设置UI、持久化/live hydration、controller声音/函数/builtin、native和multi矩阵。完整benchmark继续拒绝，预览器不记完成。

### 新增进展：声明 controller 实际回放

- `animation.mode=controller`复用原YsmControllerRuntime/YsmControllerPoseLayer，真实entry/exit、初次进入、状态时钟、完成转场、子控制器、层顺序、权重与过渡snapshot已连接GPU palette。默认mode=clip保持原显式命名路径；每实例独占状态，共享解析定义。固定时间首次从0按60Hz重放，回退重置。
- 自有控制器数值验收已通过：半程姿态、父子/轴向、entry/exit实际变量、子控制器、parallel阶段、0.5权重、once完成转场、与基础隐藏同时存在时恢复附件及子级、共享AST/实例隔离。控制器窗口记录真实animation/bones/GPU和0.3秒姿态。
- 当前控制器声音/函数/builtin宿主、Lua、YSM/Figura轮盘与模型设置仍未完成，相关输入明确诊断，完整资产benchmark保持未开放；不将该子集计作完整预览器交付。
- 最新Build/Test-Preview通过，7个实际可见窗口（build-preview-build.log、build-preview-test.log）；控制器输出build/preview/fixtures/runs/20261107-093002-222-13172；59资源最终加载回归PASS（build-preview-corpus.log）。没有启动Minecraft。

### 新增进展：YSM 命名动画与基础附件隐藏

- 原YsmAnimationParser/Sampler/Clip/Keyframe/Easing/BonePose/Event类已复用；明确注入真实PreviewMolang，解析/求值异常传播，不能以空clip或默认数值冒充执行成功。二进制动画经原YsmBinaryAnimationMapper进入同一解析路径。
- PreviewAnimation资源与每实例State分离：真实命名clip采样、Molang timeline排序/执行、循环、时钟回退重置、实例变量/随机/骨骼隔离；遵循原YsmRenderer位移X反向、旋转X/Y反向与Z同向及父级矩阵组合。GPU通过顶点owner和texture-buffer矩阵palette变换绑定姿态顶点。
- configs/ysm-animation.json可播放真实01_taisho_maid的idle，提供显式states.ysm.food_level=20；也验证new.ysm与old.ysm的idle各60实窗帧。自有模型两次真实窗口观察到子骨骼姿态变化，真实模型观察到头部动画；正常关闭，无Minecraft/offscreen。
- animation_ms与bones_ms分别记录实际采样及骨骼/矩阵打包。当前仍明确只支持named clip；controller、Lua、非Molang事件等未接入，完整资产benchmark继续拒绝。
- 已知GPU palette堆FloatBuffer测试曾触发JOML Unsafe写空地址崩溃，崩溃栈定位writePalette→Matrix4f.get(FloatBuffer)；改为显式NIO按列写入，原RED用例已GREEN，测试含真实已知姿态与矩阵布局。未用JVM flag绕过；日志hs_err_pid14240.log与build-preview-animation-green.log。
- 附件基础默认隐藏按OMP base slot和恒零scale规则实现：pre_parallel/parallel0..7、标量/向量/关键帧pre/post、动态排除、后资源覆盖、第一人称资源隔离、父子隐藏和非base显式scale恢复；保留mesh并按可见部分取景。自有fixture全部通过，真实模型原先显示的蓝色辅助框在实窗中已确认按默认规则隐藏（build-preview-hidden-attachment-window.log，run ...090717-676-23112）。
- 默认隐藏现已补齐初始controller引用：文件夹/ZIP manifest与加密binary controller映射均保留源，复用原controller parser与DTO，初始状态及回退、数组/对象定义、后资源覆盖、条件真假与实例隔离均有数值测试。只分析恒零scale引用，不在静态分析阶段执行转场/entry/exit；完整controller播放尚未接。YSM/Figura轮盘和模型设置已加入强制验收，尚未实现，不能据此宣布预览器交付完成。
- 本轮最终Build/Test-Preview通过，6个真实窗口输出 ...091933-753-9144、...091936-057-2352、...091937-986-10692、...091940-208-15304、...091941-952-17700、...091943-772-27108；59资源静态加载PASS。日志build-preview-build.log、build-preview-test.log、build-preview-corpus.log。
- 最新build及Test-Preview PASS，6真实窗口；输出 ...090659-579-18824、...090701-638-15212、...090703-476-20484、...090705-258-27888、...090706-828-21756、...090708-537-22128。59个本地YSM/Figura资源加载回归PASS，日志build-preview-corpus.log。后续以最终实际检查结果为准。

### 新增进展：Figura、Molang基础与无限草地

- 环境后续更新：四张真实Minecraft PNG已打入JAR的assets/minecraft/...，PackagedTexturesTest核对资源路径、可解码与来源哈希。此前外部assets目录的描述为历史状态；当前只需JAR+lib，纹理暂存build/preview/resources/不提交。
- 天空已加入原游戏云/太阳/满月PNG及蓝天渐变，默认固定方向/高度；和草地/模型正确叠加，HUD增加深色底板保持FPS可读。真实窗口已目视复核模型正面、草地、云、太阳和HUD，并正常关闭；未使用offscreen，未启动Minecraft。
- 最终build与Test-Preview均PASS：YSM/Figura资源一致性、真实Molang宿主、JAR内4贴图及哈希、5个真实窗口、三个独立GPU query、环境关闭时零提交/NA计时。Figura两材质样本每帧模型2+草地1+天空1=场景4次绘制；CSV按frame ID记录，HUD不计入场景Draw Call。最新输出 ...083232-669-22188、...083237-008-14048、...083238-918-15920、...083240-649-15032、...083242-297-2480；目视输出以build-preview-sky-final.log为准。

- 预览器实现、测试、构建、配置及复用代码均在tools/preview/；产物仍在build/preview/。用户再次确认此目录要求。
- 原OMP26.3 bbmodel v4/v5 parser已接入独立JAR；NBT依赖改为实际保留数值宽度/数组的内存数据树，文件、UUID、日志和ParentType元数据用独立宿主适配，来源/改动已写vendor/omp/NOTICE.md。
- 自有Figura v4/v5文件夹和嵌套ZIP均通过：立方体/三角网格、两材质、父级旋转、坐标单位、顶点/索引/绘制范围一致。本地全59个YSM/Figura抽样资产静态加载通过；小符玄实际窗口正面贴图/FPS/HUD已目视确认并正常关闭。
- 相机yaw/pitch/FOV/distanceScale可配置并保存。加权armature蒙皮仍明确拒绝；模型脚本/动画尚未运行，仍是静态绑定姿态。
- 复用了真实Molang lexer/parser/AST/evaluator/预算与math函数，独立宿主支持模拟query、时钟、变量和固定种子。测试已验证实际三角函数/条件、running=true/false、共享AST而实例变量隔离、临时变量、随机序列和预算耗尽；尚未连接模型播放，不据此宣称动画验收通过。
- 无限草地使用解析射线/平面相交和Minecraft草方块顶部纹理，一格一贴图，正确写入深度并向远处衰减。纹理来自本机26.3 JAR，源JAR与纹理哈希保存在assets/grass-source.json及运行记录。实际草地+Figura窗口已检查，无有限地板边缘，脚下遮挡和HUD正常；未启动Minecraft。
- 地面和模型有独立Draw Call/CPU提交/GPU query，原frame ID关联；total_draw_calls只合计场景模型与地面，不含HUD。地面可配置关闭/替换PNG/染色，纹理不提交Git。最终自动验证结果以build-preview-test.log及build/preview/test-*.log为准。
- 预览器完整验收仍未通过，不提前提交推送、不提前更新主仓库AGENTS.md或恢复原主开发。接下来连接真实模型动画/controller和Lua宿主，再完成native路径、单/多模型矩阵、资源/状态隔离及峰值内存与正式报告。

### 新增进展：YSM 静态模型资源接入

- 已复用OMP26.3的YsmPackage、manifest/geometry解析、加密source/crypto、binary reader/geometry mapper、bounded texture decoder及AVIF生命周期adapter；来源、最小适配与许可证保存在tools/preview/vendor/omp/。没有沿用M2的Molang或图片stub。
- YSM文件夹/嵌套ZIP/加密.ysm可在Java GPU实窗加载主模型及默认贴图，按实际绑定姿态边界取景。窗口、model.txt、报告明确标记静态绑定姿态；真实资源benchmark暂时拒绝，避免把缺失动画计入成绩。
- 本地抽样53个YSM主模型加载通过（build-preview-corpus.log）；自有两骨骼fixture的文件夹与嵌套ZIP顶点、索引、贴图完全一致。静态fixture与真实加密new.ysm已目视确认图形/贴图/FPS HUD并正常关闭；old.ysm另运行60帧正常结束。未启动Minecraft。
- 构建/运行要求改为JDK25：已核对本机缓存aircompressor-v3 3.10字节码major69；不能继续宣传整个JAR支持Java17。其余原bootstrap测试证据保留为当时状态。
- Figura、动画/controller/Molang/Lua、半透明排序、native执行、多模型矩阵、峰值内存仍未实现。当前交付单元未完成，不提交阶段检查点，不恢复原M3收尾。

### 此前 bootstrap 记录（历史状态）

- 独立目录、配置主入口、--help/--state/--frames/--no-record与可执行JAR已实现。产物build/preview/preview.jar，依赖同目录lib/；目前仅支持明确标记的builtin:cube / Java GPU基础窗口。
- 真正可见GLFW/OpenGL窗口已在Windows运行并由Computer Use检查图形/FPS/帧耗时/GPU HUD；实际GL3.3，RTX5060 Laptop GPU。正常关闭后进程退出0、窗口已消失。未使用offscreen，未启动Minecraft。
- 逐帧CSV及GPU摘要已生成，GPU query按原frame ID关联；一次实窗记录核对14125条唯一有序ID及14125个GPU结果。每次运行新增run-id目录，避免覆盖此前数据。
- 自动smoke验证70帧/60采样和两轮一致动画时间；Test-Preview.ps1验证非法state拒绝、两个真实窗口自动结束、running=false配置覆盖、GPU结果、关闭记录不写帧行。Java25已验证；Java17是编译目标，其他JDK实窗运行尚未执行。
- R/Escape的自动化按键测试未观察到回调，快捷键MANUAL/PENDING；不据此阻塞后续模型/脚本实施。
- 五类真实模型输入、贴图、模型自带动画/controller、真实Molang/Lua、native CPU/GPU、多模型矩阵、可见/提交数真实统计及峰值内存尚未完成。当前不能据fixture窗口宣布本计划验收通过。
- 当前未提交预览器检查点；主分支仍为M2的f51df7d，M3依赖改动和预览器改动全部保留。额度最近可靠查询周窗口剩余79%。

### 2026-11-07 部件世界矩阵增量验证

- partToWorldMatrix已接入原FiguraMat4，返回最近实际提交的枢轴相对矩阵副本；数值测试核对层级/单位/实际顶点/实例隔离。
- build-preview-test.log完整回归PASS：17真实GLFW窗口及伴随窗正常退出，最终60帧run为build/preview/fixtures/runs/20261107-120624-385-15108。未启动Minecraft，未使用offscreen。
- 实际AI Hobbyist模型显式开启global/aways和8尾开关后，失败点为lib/particle:52缺少particles宿主（build/preview/host-probe-particles.log）；不是完整效果PASS。下一步接入实际粒子宿主及绘制，不使用空实现。
- 仍未达到整个预览器交付标准，无本阶段提交/推送；主M3继续暂停。可靠周额度剩余71%。

### 2026-11-07 独立粒子宿主增量

- 复用原ParticleAPI/LuaParticle，实际20Hz状态更新、实例隔离、寿命/运动/平面碰撞/配置水环境及真实贴图合批绘制已接入。独立模拟的固定风/平面/全局水语义在README明确，不当作Minecraft完整粒子引擎等价验证。
- 原始六类JSON和41张粒子PNG由本机游戏JAR打入assets，哈希与47张总PNG验证通过。CSV独立记录粒子提交数/draw/CPU提交/GPU query，计入total_draw_calls。
- 真实目录/ZIP 5秒都生成40粒子，姿态SHA256=884bb225f68ce5bc2dca53cf3cb8e63d70cb4cbc5905387400b89dc08f2c92b6，粒子状态SHA256=d0846951e6d0edfd74398e74e238714ce015a02474076ff01c17a4b8d65328e9。moving/gliding/swimming各5秒生成500粒子并通过；对应日志build/preview/host-probe-*.log。
- Computer Use已目视六类自有精灵及真实ZIP轮盘全开彩色尾部粒子。真实ZIP run build/preview/runs/real-figura-particles/20261107-121852-453-31604：4426帧，4311帧实际提交粒子，最后164粒子/1粒子draw，总draw9。Escape正常退出0，未启动Minecraft。
- 自有测试覆盖原方法重载/向量/颜色/power/重复spawn/清空/种子/环境/seek；补正seek重建时地面配置丢失，环境现于createLua初始化。
- DYING状态明确失败在lib/particle:113 sounds:playSound缺失（host-probe-dying.log），不是PASS；声音宿主为下一已证实依赖。预览器整体验收仍未完成，主M3仍暂停，无阶段提交推送。

最终验证补记：地面配置已移入PreviewAnimation.createLua，正常/seek重建路径共用。最终build-preview-build.log构建成功，build-preview-test.log完整18真实窗口及伴随窗回归PASS、正常exit0（session91907），最后六类精灵run build/preview/fixtures/runs/20261107-122402-334-14024。原方法重载、nil isPresent、power和seek后地面碰撞回归通过；资源六JSON+47PNG及引用完整性通过。修改文件格式及差分空白检查通过。

### 2026-11-07 真实OpenAL声音宿主增量

- 原SoundAPI/LuaSound已复用并接真实OpenAL播放与STB Vorbis PCM，不用模拟isPlaying标记。目录/嵌套ZIP自定义OGG使用原点分ID；base64/字节表新增声音、位置/音量/pitch/attenuation/loop、pause/resume/stop、ON_PLAY_SOUND本实例回调取消已接。未知/未打包声音明确失败。
- Build-Preview从现有Loom asset index/object缓存打包默认squid hurt/death/squirt三事件及10OGG，读本机资源、不下载不启动Minecraft。增加-MinecraftAssets/-AssetIndex/-SoundEvents和精确OpenAL3.4.1 Java/native依赖，保存音频来源hash。构建使用PowerShell7。
- 音频内容缓冲共享、实例source隔离，seek停止旧声音、最终释放OpenAL context/device。窗口输出audio.json的真实play/source/buffer计数和closed状态。
- PreviewSoundTest通过真实PCM字节数、原生游标推进/暂停不动/继续/自然结束、Lua接口、回调取消、实例隔离、内容共享和seek/退出清理。探测静音但执行真实默认设备路径，不冒充人工听音。听音MANUAL/PENDING，不阻塞自主交付。
- 首次完整Test-Preview PASS19实窗及伴随窗正常退出，音频ZIP 60帧run build/preview/fixtures/runs/20261107-123647-893-6308，audio.json: playCalls=1/decodedBufferLoads=1/activeSources=0/closed=true。
- 本地真实模型301次姿态提交/5秒模拟回放：目录DYING生成800粒子、实际play100/分配100source、3解码buffer；ZIP swimming+配置“游泳喷墨(按住)”初始true生成600粒子、实际play50/分配50source。日志real-sound-dying-green.log、real-sound-ink-green.log；使用真实设备、listener gain0，正常exit0，原脚本未修改。声音为墙钟播放，未把模拟5秒等同声卡5秒。
- 音频source清理轮询放在PreviewLua.render，覆盖Figura/YSM命名/actions/controller路径；无完整controller声音事件接线完成声明。完整preview的设置持久化、YSM宿主、native/CPU/GPU、多模型/内存/报告仍待完成，主M3暂停，无本阶段提交/推送。可靠周额度remaining70%。

最终声音检查点：canonical preview.jar已重新构建，包含最新来源NOTICE；最终完整build-preview-test.log PASS19实窗及伴随窗正常exit0（session41438），最后音频ZIP run build/preview/fixtures/runs/20261107-124058-610-28388。补充字节表newSound测试通过；最终JAR再次60帧实窗正常exit0（session90515），run .../20261107-124214-670-29812，audio.json closed=true/activeSources=0/playCalls=1。最终原版资源hash/来源NOTICE/播放清理核对PASS（test-final-packaged-assets.log、test-final-audio-artifact.log）。全部修改文件formatter及diff空白检查通过。当前无运行中测试句柄；预览器整体仍未完成，不提前执行阶段提交推送。

### 2026-11-07 模型设置持久化增量

- 按原AvatarControlValueStore规则接入persistent控件选择/typed值/恢复及保存顺序；JSON按规范源路径+引擎隔离并hash文件/控件ID，支持settings.directory/modelKey。默认enabled，benchmark强制读写关闭，显式scripts.controls优先保存值。
- 注册时恢复以支持Lua立即读取及后续range/options归一化；初始化再应用已存值，优先初始live binding。原Definition只增加setPersistent(false)撤销pending restored value，避免链式非持久化声明误用旧缓存。按钮/标签/分隔线不保存，全局拦截不保存，重复值不回写，重置持久化。
- PreviewSettingsTest通过Figura/YSM保存→新运行时→实际palette、早期范围、Figura枚举文本/YSM枚举序号及私有表达式不落盘、实例隔离、非持久化、拦截、重置、配置覆盖、seek和benchmark不读不写。既有16个配置型fixture明确settings.enabled=false，避免测试污染用户偏好。
- build-preview-build.log构建成功。完整build-preview-test.log PASS23真实GLFW窗口及伴随窗清理，session48843 exit0。新增独立JVM保存/恢复：Figura write .../20261107-125552-725-16256 →ZIP read .../20261107-125555-134-31520（offset40）；YSM write .../20261107-125557-492-14416 →read .../20261107-125559-812-2036（width_scale2.5）。各60帧，写入至少一次，读取0次回写、settings文件相同。目录前缀build/preview/fixtures/runs/。
- 源码仍tools/preview，缓存/JSON输出build/preview，不提交模型/用户偏好。当前preview整体验收尚未完成，原M3暂停，无新阶段提交推送。下一步既定YSM controller声音/函数/builtin宿主，再推进CPU/native/GPU多模型与报告。可靠周额度剩余70%。
退出补正：新增held-key释放回调实窗RED，原关闭顺序在释放回调playSound时抛Audio device already closed；调整为GLFW清理→interaction释放回调/最终状态→audio关闭及settings/audio最终记录。首次GREEN复用了RED偏好而触发初值断言，日志保留test-settings-release-stale-fixture.log；重新生成隔离目录后30帧真实窗口exit0（test-settings-release-green.log，run .../20261107-130057-532-20312）。新增24号实窗回归验证release保存offset24、真实play1次、按键全释放、source0/closed=true及最终writes=1。

最终检查点：退出回调修复后，build-preview-test.log完整24真实窗口及伴随窗回归PASS，session14030正常exit0；最终held-key run build/preview/fixtures/runs/20261107-130300-245-30868。关闭回调保存offset24，settings.json writes1，audio.json playCalls1/activeSources0/closed=true，最终按键全释放。canonical preview.jar已构建并含最新来源NOTICE。所有修改Java已按范围格式化，第三方Definition保留原版布局只改6行；新增/修改代码、测试及文档差分空白检查通过。可靠周额度剩余70%，无Luna交接；本轮无活动测试句柄，不需清理JVM。预览器整体仍未完成，无阶段提交/推送，主M3保持暂停。

### 2026-11-07 YSM 音频宿主增量

- 接原 YsmAnimationEventWindow/YsmControllerSoundBank、状态 entry/exit、命名 clip 与声明 action 的实际 OpenAL 声音，独立 Lua 开关。循环严格大于结束点才跨界；timeline 使用原播放步时钟。
- 原 YsmAudioDecoder Opus 路径复用，Vorbis 的 Minecraft JOrbis 流替换为有界 STB 分块解码；保留模型音频单声道、预算和取消检查。新增本机已有 vorbis-java-core 0.8 / Concentus 1.0.2，未启动游戏。
- 53 个真实 YSM 静态扫描保留 162 音频：158 Vorbis、4 Opus（audio-corpus.log）。解码/实际 OpenAL 上传测试通过 5 个原验证样本、单声道/48k Opus/预算拒绝/缓存复用（test-model-audio.log）。控制器/动作所有权、两个实例、循环/退出/seek、嵌套 ZIP 和二进制资源存活测试已通过。
- 函数/builtin/其他事件、CPU/native、多模型完整 benchmark 仍待实现；整体预览器尚未完成，主 M3 暂停，不进行阶段提交。额度安全查询周剩余69%。实窗最终结果在本节后追加，人工听音 MANUAL/PENDING。

最终检查点：build-preview-test.log完整25真实窗口及伴随窗回归PASS（session10262 exit0），最后.../20261107-132918-159-29328。最终NOTICE/格式化后重编成功并独立60帧实窗PASS（test-final-ysm-sound.log、session94757 exit0），run.../20261107-133017-043-9748：play2/decode1/active0/closedtrue。无残留测试JVM；人工听音MANUAL/PENDING。预览器整体未完成，主M3暂停，无新阶段提交。

### 2026-11-07 YSM 函数宿主增量

- 复用原 YsmMolangFunctionName、YsmFunctionEventBus、YsmControllerSlotBinder；PreviewYsmFunctions 从原 YsmMolangFunctionRuntime 接入 named call、args、返回值、临时作用域、controller stage/bus 和上下文恢复。资源字符串代替 NBT/live Avatar；未知函数及执行失败明确传播，不用0掩盖。
- functions/*.molang 从目录/ZIP与加密binary保留到模型资源；每实例建立独立变量/args/temp/call/event状态。控制器标准阶段事件恢复原调用顺序（state处理之前），缺JSON的标准阶段用原binder补齐。原32递归上限及执行预算继续生效。TempVariableStorage只增加clearFrame重置数据/作用域并复用分配空间。
- 新PreviewYsmFunctionTest已验证嵌套函数参数和temp隔离/return、多个manual事件顺序、recursion/unknown/执行错误后恢复、stage事件→实际child palette、两个实例和seek、嵌套ZIP及binary关闭后源数据存活。原ControllerTest的函数拒绝负测更新为实际调用副作用断言。
- 初次新增测试因Float配置写成int而编译失败，改0f后通过。完整26个真实窗口及伴随控件回归PASS（session33284 exit0，最后build/preview/fixtures/runs/20261107-133917-278-4644）。此后仅增加temp复用清理和help/NOTICE，最终验证结果随后追加。
- builtin、deferred/network相关函数及完整Lua API依然明确未支持；CPU/native、多模型和正式报告仍待实现。整体preview未交付，无stage commit/push，主M3暂停。可靠周剩余69%；人工视觉MANUAL/PENDING。

最终函数检查点：包含temp存储复用的canonical JAR构建成功（session50486），完整26实窗回归最终PASS（session20067 exit0，最后.../20261107-134149-505-14452）；tracked及本轮范围空白PASS，无活动JVM需清理。真实53模型的46函数静态编译探测明确4模型失败，首个未接绑定依次ysm.play_sound、ysm.second_order、ctrl.use、ysm.bone_scale/abs_particle（function-corpus-detail.log）；这不是完整真实模型函数通过。下一步依这些证据接原宿主，无阶段提交推送，整体目标active。

### 2026-11-07 YSM 函数声音与程序滤波增量

- PreviewYsmSoundFunctions复用原YsmSoundRuntime的ID转换、anonymous/indexed banks、replace/global/loop flags、controller/local/global键、拥有者清理及transfer逻辑。配置位置和显式preview生命周期代替live Entity位置/删除状态；真实PreviewSounds/OpenAL创建/播放，独立预览器按用户要求允许声音（去除模组自己的preview静音门控）。
- 恢复函数嵌套client-effect scope；timeline拥有者、controller stage、state entry/exit作用域接线，保留原执行预算。非效果位置调用返回false，不意外在骨骼求值中播放。stopController/stopPlayback/tick/close联动真实声音清理，包括global ID且有播放拥有者的声音。
- YsmFilterRuntime原样复用，first_order/second_order接原参数默认与PREVIEW域。每模型渲染frame按单调模拟秒*20更新一次，早于本帧采样；clip loop和controller子步不重新更新。Sampler恢复原bone scope，仅以PreviewMolang替换Avatar上下文；保留既有异常传播。
- 新PreviewYsmSoundFunctionTest已PASS真实native source计数：同ID拒绝/替换、本地/全局/匿名、flags/无效参数、嵌套owner恢复、state entry/stage、两实例/退出/seek、timeline全局owner跨loop和seek清理。新PreviewYsmFilterTest已PASS120帧原算法对照→实际骨骼palette，一/二阶、同名不同骨骼、循环单调时钟、重复frame、实例/seek隔离。人工听音/视觉MANUAL/PENDING。
- 新来源NOTICE和README已写入最终canonical JAR，build-preview-build.log构建成功（session50253 exit0）；Test-Preview纳入两测试和两个60帧真实窗口，完整回归目标28窗，最终结果后追加。tracked及本轮范围空白检查PASS。
- 再次真实53模型/46函数编译探测（function-corpus-sound-filter.log，session20079 exit0）证明原先play_sound/second_order位置已越过；仍4模型失败：15_kluonoa下一缺ysm.sync（并使用ysm.keyboard），18_wedding/21_saint下一缺ctrl.use，绫樱.ysm下一缺ysm.bone_scale/abs_particle。探测不是完整模型回放/benchmark，通过与失败分开记录。
- 完整预览器尚未验收，无stage commit/push，主M3暂停；可靠周额度69%，未达Luna阈值。下一最小任务继续真实明确绑定及builtin，再做既定CPU/native/GPU、多模型与正式报告。

退出回归更正：第一次完整28窗回归session8794停在Figura scripted窗口PID21424（run.../20261107-135129-729-21424），60帧/最终JSON写完、audio closedtrue/active0，但JVM仍在AWT-Shutdown/DestroyJavaVM。只读agent确认frame non-displayable/visiblefalse、peerMap空、busyThreadSet空、toolkitThreadBusyfalse，Swing TimerQueue空；不能再认定先前关闭顺序已彻底修复退出。线程/agent日志exit-21424-*保留。用户既有授权覆盖本会话清理，核对CLI和输出后仅清理21424，session8794 exit-1（失败，不PASS）。build-preview-test-awt-21424.log与test-figura-scripted-awt-21424.log保留。未改内部AWT标记、未用System.exit掩盖；同源码一次完整复验结果后追加。该偶发退出问题为预览器交付前需解决事项，不能把重跑PASS等同根因已修复。

最终复验：同源码完整28真实窗口及伴随控制窗PASS（session43319 exit0，build-preview-test.log），最后filter run.../20261107-135845-856-12072，function sounds.../20261107-135843-252-3616。失败8794/21424仍按上述记录保留，AWT偶发退出未根除。功能增量通过，整体preview未完成，无新阶段提交。

### 2026-11-07 装备模拟与本地 @sync 增量

- PreviewYsmEquipment按原CtrlBinding的handItem/armor/parseSlot/itemMatches/参数验证接显式states.equipment；use/hold/swing/armor、手部别名、空/精确ID/tag/type匹配与armor分类限制保留。Minecraft注册表/物品类型和动作状态由配置提供，未按ID子串猜测。状态快照每实例隔离，缺必要字段报states.equipment路径。
- PreviewYsmFunctions.recordSync复用原本地分支：client-effect gate、最多16个转换后finite float、最近16条记录、立即按顺序运行@sync且隔离args/temp。无网络捕获/传输/远端重放。本地回调能调用named函数并实际改变模型变量/骨骼，不是空sync接口。
- PreviewYsmEquipmentTest先在ctrl.use解析处RED（test-ysm-equipment-red.log），实现后GREEN。新增PreviewYsmSyncTest通过effect gate、infinity拒绝、嵌套函数和scope清理、记录上限；using true/false→controller→sync参数→实际palette y9/y8、两个实例及seek重建通过。初测误用sqrt(-1)/NaN当拒绝样本，原Molang约定NaN→0，应接受；改用infinity验证原finite检查。args.length点访问亦不属于原parser支持，改为直接验证实际参数栈为空，未修改生产parser迎合测试。
- 来源NOTICE/README与最终canonical JAR已构建（session45353 exit0，build-preview-build.log）。Test-Preview加入两测试及60帧装备/sync实窗（目标29窗）；最终结果后追加。tracked及本轮范围空白检查PASS。人工视觉MANUAL/PENDING。
- 真实53模型/46函数静态探测（function-corpus-equipment-sync.log，session71035 exit0）现在15_kluonoa的4函数、21_saint的1函数全部编译；仍2模型失败：18_wedding首先ctrl.indicate_reload，绫樱.ysm首先ysm.bone_scale/abs_particle。这是编译证据，不是这些模型完整播放/benchmark。
- 整体preview仍未验收，无stage commit/push，主M3继续暂停；AWT偶发退出历史问题仍未修复（见前节失败21424），本轮不能用新回归PASS将其抹掉。可靠周额度69%，无Luna触发。下一步按真实缺口接原controller函数指令、input/bone/particle/builtin与既定CPU/native/GPU及多模型报告。

最终检查点：最新canonical JAR构建PASS（45353），完整29真实窗口及伴随控件回归PASS（46230 exit0，build-preview-test.log），最后装备/local sync run.../20261107-140807-972-20380。所有本轮范围格式与diff空白检查PASS，无运行中测试句柄；人工视觉MANUAL/PENDING。AWT前轮偶发退出缺陷仍未修复，整体preview未验收，无阶段提交/推送。

### 2026-11-07 控制器函数动画槽增量

- 新PreviewYsmControllerFunctions接原CtrlBinding的set_animation/reset/set_beginning_transition_length/indicate_reload、参数个数、controller上下文门控与loop/state常量。PreviewControllerPlayer接原function槽身份、每controller选中播放/transition长度/reload请求、退休/owner清理和开始时钟/初始事件延迟；ControllerRuntime补原function播放参与clock/completion统计。
- 同动画refresh保留实例/时钟/声音；reload下一次set实际重建owner并退休旧槽；reset停止所属声音并退出当前槽。正开始时长冻结function clock/events，跨界只推进剩余时间；controller帧内新建播放延后本步timeline advance；LOOP严格跨过端点才wrap。别名按实际解析clip统一槽身份，控制器及模型实例独立。
- 新PreviewControllerFunctionTest已PASS实际palette、原生声音源、refresh/reload/reset、.5秒开始延迟、准确边界和cancel、loop/once/hold、slot别名、双controller、实例/seek。真实原源码进一步确认退休槽应不再采样；补断言在旧binary RED（test-controller-retirement-red.log，-.1696001!=-.124），修正采样跳过retired后GREEN（test-controller-functions.log）。旧姿态只由原pose layer快照处理，不把已退休clip再次叠入；reset未写通道立即回到下层。
- 最终canonical JAR构建PASS（session24050 exit0，build-preview-build.log），含最新来源NOTICE。Test-Preview加入该测试及60帧函数槽实窗（目标30窗），最终结果后追加；tracked及本轮范围空白PASS，人工视觉/听音MANUAL/PENDING。旧declared槽正开始时长时钟冻结不在本增量宣称修复范围，交付前继续按原语义核对。
- 第一次30窗回归session35268在interactive-ysm PID32244完成180帧/最终JSON/audio closed后AWT-Shutdown/DestroyJavaVM等待。exit-32244-threads.log及run.../20261107-141959-761-32244保留；按用户持续授权核对CLI/输出/线程证据后仅清理本会话32244，exit-1（失败）。日志build-preview-test-awt-32244.log、test-interactive-awt-32244.log保留，不冒称PASS。没有内部AWT标记修改/System.exit或启动Minecraft。
- 真实53模型/46函数编译探测session22021 exit0（function-corpus-controller-commands.log）：15_kluonoa4和21_saint1仍全部compiled，18_wedding已越过indicate_reload，下一缺query.relative_block_has_any_tag；绫樱.ysm仍缺bone_scale/abs_particle。编译不是完整模型运行/benchmark。
- 整体preview未验收，主M3暂停，无stage commit/push；最新可靠周used32%/remaining68%，未达Luna阈值。下一步继续明确input/相对方块query/bone/particle/builtin，随后CPU/native/GPU多模型与正式Markdown报告，保留AWT退出未解决项。

JAR生命周期修正（覆盖此前仅依赖AWT自动退出的方案）：退休采样修正后的复验session59630仍在interactive-ysm PID31280 main返回后AWT等待，180帧/最终JSON/audio closed已完成，线程证据exit-31280-threads.log。按持续授权核对后仅清理31280，session59630 exit-1，失败日志build-preview-test-awt-31280.log/test-interactive-awt-31280.log保留。由此不再反复将关闭顺序当可靠进程结束机制。

独立PreviewMain.main改为应用入口拥有进程生命周期：调用可复用run，成功完成其所有finally/窗口/音频/输入回调及最终写入后System.exit(0)请求正常JVM关闭（含hooks）；异常打印并返回1。没有内部AWT标记修改、Runtime.halt或终止其他进程。渲染/解析宿主类不终止调用方进程。该方案是对独立JAR预期“结束运行即退出”的显式应用策略，不宣称修复JDK自动AWT退出内部原因，也不以强制结束掩盖未完成写入。

最终Test-Preview追加一个真实窗口内故意触发Lua render错误的case，要求原始错误、退出码1、audio closed/active0及interaction最终状态已写入。30个正常窗口仍分别要求成功退出；所有案例验证完成后脚本显式exit0，避免最后预期失败污染测试脚本返回码。最终canonical JAR含最新NOTICE构建成功（session49708 exit0），最终完整结果后追加。范围格式和diff空白检查PASS。

最终检查点：显式CLI正常关闭策略及最终controller源码构建PASS（49708）；完整回归PASS（31298 exit0，build-preview-test.log）：30成功真实窗口和1预期render失败实窗均退出，成功0/失败1及最终清理记录已验证。正常function run.../20261107-143130-556-29732 play1/decode1/active0/closedtrue；负测.../expected-render-failure/20261107-143133-319-31992。外部CIM确认32244/31280已清理。此前35268/59630仍作为失败保留，未以重跑抹掉。范围空白检查PASS，周剩余68%，无Luna；整体preview未验收，无阶段提交/推送。

### 2026-11-07 相对方块标签查询增量

- 新PreviewBlockQueries按原QueryVariables.worldTagFunction(true,false)/relativeQueryPosition接query.relative_block_has_any_tag：各轴偏移<=5、原finite检查、round(entity+offset-0.5)、世界轴不旋转、ANY短路及原标识符命名空间语义。显式states.world.blocks/defaultBlockTags代替Minecraft世界/标签注册表；位置states.player.pos省略时取预览模型原点，未知采样块明确报配置缺口，未从草地材质推断tags。
- 私有snapshot保存整数坐标和不可变tag集合，重复坐标后项覆盖，列出单元覆盖显式默认tag。配置错误包含新增字段路径。未添加Minecraft依赖或启动游戏。
- PreviewBlockQueryTest在旧binary解析缺绑定处RED（test-block-query-red.log），实现后GREEN（test-block-query.log）。覆盖负/分数坐标、namespace、ANY/非法tag及短路、偏移范围、缺状态诊断、原配置变更不污染已有host；function controller query实际令child palette y=1.25，seek重建正确。新60帧实窗夹具偏移保持可见模型范围。
- 最终canonical JAR构建PASS（session17662 exit0，build-preview-build.log），含最新来源NOTICE。Test-Preview加入该测试及实窗，目标31成功窗口+1预期render错误窗口；最终回归结果后追加。tracked及本轮范围formatter/diff空白检查PASS，人工视觉MANUAL/PENDING。
- 真实53模型/46函数编译探测session55247 exit0（function-corpus-block-query.log）：15_kluonoa4、18_wedding9、21_saint1全部compiled；仅绫樱.ysm32函数组仍在bone_scale/abs_particle处失败。静态编译不等于模型完整运行或benchmark。
- 整体preview未完成、主M3暂停、无新阶段提交/推送；显式CLI生命周期策略保持，JDK自动退出原因没有宣称修复。可靠周额度剩余68%，无Luna。下一步明确bone/particle/input/builtin与declared开始延迟，之后既定CPU/native/GPU多模型与正式Markdown报告不变。


### 2026-11-07 相对方块、骨骼查询与声明式开始延迟

- PreviewBlockQueries数值与实际controller palette/seek验证通过；明确states.world.blocks/defaultBlockTags模拟输入，保持原坐标取整、范围、ANY标签规则。缺方块元数据明确报错，不从地面贴图推断。
- PreviewYsmBones接实际position/rotation/scale，只读live Struct保留引用语义；求值期间使用前一完整帧快照，同frame ID不刷新输入，成功采样后才发布完整输出。附加原版玩家骨骼不纳入YSM查询，实例与seek重建隔离。原getPart也采用exact/lowercase查找，未添加角色别名。绝对跟踪pivot、第一人称和直接YSM Lua部件修改仍未接入。
- 骨骼数值测试PASS（test-bone-query.log），含真实GPU palette、raw单位/旋转符号、保存引用、只读写入、重复帧、实例和seek0。此前完整回归已输出32正常实窗+1预期错误实窗PASS，无残留preview JVM；骨骼实窗run为20261107-144939-791-35052。该次终端句柄因上下文截断未保留，已用最终PASS及外部进程检查确认结束，未伪造句柄退出记录。
- 真实函数语料再次静态编译PASS执行（session42556 exit0，function-corpus-bone-query.log）：53模型/46函数资源，1模型仍失败（绫樱.ysm abs_particle_body的abs_particle调用）；15_kluonoa、18_wedding、21_saint编译通过。此项仅为编译覆盖，不是完整运行或性能数据。
- PreviewDeclaredTransitionTest在旧实现RED（test-declared-red.log，零时刻事件1!=0）。原playInSlot/advance源码确认declared与function都记录beginning delay并延后创建帧advance；当前适配遗漏declared路径。最小修复为declared创建时保存延迟/创建帧标志，advance共享既有冻结与剩余delta逻辑。GREEN验证实际controller、0/.49/.51/.7时钟与零/中间事件、实际palette、实例和seek。
- 构建session8409 exit0；完整回归session98229运行中，数值部分已通过。最终结果后追加。未完成整体preview，无stage commit/push，原M3继续暂停；周额度remaining68%，未触发Luna。人工视觉/听音MANUAL/PENDING；CPU/native/GPU矩阵和正式分模型Markdown表格仍待后续阶段。


最终检查点：完整回归session98229 exit0，build-preview-test.log：32正常真实窗口+1预期错误窗口，含本轮新增声明式延迟数值回归，所有清理/退出码通过。最终canonical JAR含最新NOTICE重新构建通过（session57857 exit0），同session随后声明式延迟真实60帧窗口通过，run.../20261107-145523-691-11004，frames.csv=60、audio closedtrue/active0。数值证据覆盖真实时钟/事件/palette；未把实窗启动检查称为人工视觉验收。tracked和本轮范围空白检查通过。整体preview仍未完成，无阶段提交推送，下一最小实施为YSM粒子宿主（原MolangBindings.particleFunction仅转给YsmParticleRuntime.execute；须复用实际分发规则，不能空返回使编译通过）。


### 2026-11-07 YSM Molang 粒子宿主

- 新PreviewYsmParticles按原YsmParticleRequest.parse/emit与YsmParticleRuntime.execute的客户端效果门控适配；不新建Minecraft依赖。保留默认参数、count0单粒子与正数Gaussian批量、1024上限、tick寿命、单粒子bodyYaw/批量entityYaw、Vec3.yRot符号、abs仍加实体位置但不旋转、速度不旋转。
- 显式states.player.pos/yaw/bodyYaw模拟YSM玩家。Gaussian与粒子内部随机均由每实例seed固定；不声称非玩家实体或Minecraft完整registry/参数化粒子。当前支持现有六种真实独立粒子，其余明确报错。渲染主循环读取State.particles，不再要求存在Lua。Lua存在时与YSM共用列表，由Lua的20Hz tick单独推进；无Lua时由独立宿主推进，controller虚拟步内同步推进。seek关闭旧owner并重建。
- PreviewYsmParticleTest已PASS实际controller timeline无Lua发射/20Hz推进、单/批朝向/abs位置/速度/寿命、effect gate/count预算、未知类型诊断、共享Lua列表、实例/seek。新增Gaussian同seed重复性断言后将在完整套件结束补跑，避免并行覆盖其测试classes。
- canonical build session14907 exit0（build-preview-build.log），包含README/NOTICE来源更新。完整回归session81445运行中，目标33正常实窗+1预期错误；新增窗口要求60帧每帧particles>0及particle_draw_calls>0，而非仅窗口启动。
- 原53模型/46函数资源静态编译probe session28870 exit0（function-corpus-particles.log）：failedModels=0。仅证明函数静态编译，不作为全部模型播放或benchmark结果。
- 本轮tracked与scoped空白检查PASS；整体preview未完成，无stage commit/push，原M3继续暂停。尚需键鼠/pivot/builtin/直接Lua、CPU/native/GPU及多模型正式报告，人工视觉MANUAL/PENDING。


粒子最终检查点：Test-Preview session81445 exit0，33正常真实GLFW窗口+1预期错误窗口全部通过（build-preview-test.log）。新增YSM无Lua粒子实窗run.../20261107-150236-512-31472，60帧每帧particles>0、particle_draw_calls>0；不是仅创建了模拟粒子而未提交绘制。最终增加的同seed Gaussian重复性/非零速度断言已独立重新编译并PASS（test-ysm-particle-compile.log、test-ysm-particle.log），产品代码未改变，无须重复整套。可靠额度周剩余68%；没有活动测试句柄。下一项接原YsmInputState键鼠规则，继续整个preview，不将当前粒子子增量冒充完整阶段完成。


### 2026-11-07 YSM 键鼠与无Lua输入回放

- PreviewYsmInput适配原YsmInputState：GLFW数字keyboard32..348、mouse0..7、keyboard ANY语义；原inputFunction arity为keyboard>=1/mouse=1。没有把Figura修饰键组合或scancode误当YSM规则。
- states.input提供owner/ready/keys/mouse离线快照；guiOpen时默认未就绪。未就绪不接受按下并清空键鼠，恢复不重按；owner=false始终查询false。明确替代Minecraft实体identity/overlay/screen/mouse-grab，不声称游戏设备状态。手部装备动作仍用现有显式模拟，不从按键猜测。
- PreviewInteraction新增带Molang host的update；原Lua调用继续委托同路径。真实GLFW输入/现有回放可驱动无Lua模型；input-ready布尔操作记录就绪变化，焦点变化也接同队列。记录逐条ysmInput状态，关闭清空后写最终JSON；Lua轮盘/控件路径保持原逻辑，缺Lua时相关操作明确报错。
- PreviewYsmInputTest PASS ANY/边界/scancode隔离、owner与GUI、失去ready清空、no-Lua骨骼palette、JSONL record/replay与最终快照、实际输入队列/释放及模型隔离。首次测试javac失败仅因Float字段赋int0（已改0f），最终javac/java通过，见test-ysm-input-compile.log/test-ysm-input.log。
- canonical构建PASS，build-preview-build.log包含最新README/NOTICE；Test-Preview完整session15600运行中（34正常真实窗口+1预期错误），加入no-Lua YSM窗口60帧和日志held keys/mouse/ready/final cleanup验证。最终结果后追加。tracked/scoped空白及新Java/修改Interaction格式检查PASS。
- 整体preview仍未完成，无阶段提交/推送，主M3继续暂停；下一项绝对pivot与builtin/直接Lua，再按既定矩阵执行正式benchmark。人工视觉MANUAL/PENDING。


补充与真实失败修复：原keyEvent只处理press/release，忽略GLFW_REPEAT。记录Operation增加repeat，Main保留原action信息，Figura仍沿既有set路径，YSM跳过repeat；数值测试追加自动重复不重新按下。首轮实窗session15600 exit1：旧PreviewConfig.validate拒绝无Lua interaction（build-preview-test-input-config-failed.log），不是渲染/退出失败。最小更新为交互要求animation.enabled（非key操作仍由Interaction要求Lua/control host），新增正式PreviewConfig.load覆盖。最终build+完整回归session3322运行中，结束后追加实际结果；不将失败轮计PASS。


键鼠最终检查点：最终canonical JAR构建及完整回归session3322 exit0（build-preview-build.log/build-preview-test.log）。34正常真实GLFW窗口+1预期render错误窗口全部通过；新增无Lua YSM输入窗口60帧，JSONL实际记录键75、mouse0、ready=false及最后清空状态。数值测试涵盖原查询、正式Config.load、repeat不重按、实际palette和record/replay。没有Minecraft/offscreen；人工视觉MANUAL/PENDING。最新可靠额度周used33%/remaining67%，未达Luna阈值；无活动工具句柄。preview整体仍未完成，无阶段提交推送，下一项按原模型矩阵接绝对pivot，不伪造坐标。


### 2026-11-07 已绘制骨骼 pivot

- 源码澄清：原bone_pivot_abs不是实体世界坐标。YsmRenderer.updateTrackedPivots从identity局部根遍历，stopAtPivot后record(-m30*16,m31*16,m32*16)，不含applyModelScale/实体/相机。原read对PIVOT_ABS直接取tracked向量，不走普通通道previous-frame inputs。初次track为0，隐藏或任意轴零缩放的分支不更新并保留旧值。这覆盖交接中曾将其称为世界pivot的错误推测。
- PreviewYsmBones接独立tracked map，注册only-on-query、只读live view/有限值更新/隐藏祖先传播。Main在实际model绘制提交边界调用State.recordRenderedPivots，使用同一已算world矩阵transform骨骼pivot；去掉根width/height控制后按原模型像素符号输出，不从局部动画通道假造结果。只sample/updateBones不会刷新pivot，seek重建后初值0，第一人称仍未接入。
- PreviewYsmPivotTest数值PASS：父级Rz90与非等比scale、动画位置、子级自旋转/缩放、注册晚于render的0初值、渲染边界、保存Struct实时更新/只读、隐藏父级任一轴0保留旧值、模型尺寸排除、实例/seek。第一次期望符号失败88!=56，核对原YsmGeometryParser第47行镜像pivot X后更正测试手算；产品代码未为匹配错误预期更改。失败证据test-ysm-pivot-expected-sign-failed.log；最终test-ysm-pivot.log通过。
- canonical build session56122 exit0含最终README/NOTICE。完整回归session45390运行中，目标35正常真实窗口+1预期错误；新增pivot fixture窗口60帧。tracked/scoped空白检查PASS，最终结果后追加；人工视觉MANUAL/PENDING。整体preview未完成，无阶段提交推送，原M3仍暂停。


pivot最终检查点：完整Test-Preview session45390 exit0（build-preview-test.log），35正常真实GLFW窗口+1预期render错误窗口全部通过；新增pivot实窗60帧及数值测试通过。canonical JAR与当前产品源码/NOTICE一致；人工视觉MANUAL/PENDING。无活动工具句柄，无需清理本会话测试JVM；原M3仍暂停，preview整体未验收，不提前提交/推送。下一项复用原YsmAnimationPlayer内建controller播放，保留明确失败直至真实宿主接通。


### 2026-11-07 撤回无关状态机扩展

按用户明确要求，删除自动Minecraft主动作/locomotion选择、MAIN/ARM默认动作模板、基础parallel自动播放、持物/使用/挥手状态机及其专项测试/实窗配置入口；清除JAR中的遗留class/模板，删除相关可用能力说明和额外验收门槛。

只执行模型自身声明的动画/controller/Lua；宿主模拟值直接来自配置：states.query、states.ysm、states.ctrl及已有states.player/world/equipment等显式快照。速度和物品不再自动推导Minecraft动作状态。不以完整Minecraft状态机/全API兼容为交付要求。用户要求的五类输入、动画/轮盘/设置/附件隐藏/皮肤缓存以及性能矩阵全部保留，M3改动未回滚。

撤回后完整回归session30595 exit0：35正常实窗+1预期错误实窗及全部保留功能数值测试通过。最后仅删除无用集合字段，再次canonical构建及显式Molang配置专项通过；最终JAR无撤回类/模板。日志build-preview-build.log/build-preview-test.log，详细终态见交接手册最新检查点。后续优先CPU/native/GPU、线程、多模型与正式报告。


### 2026-11-07 CPU 顶点路径与线程预算

Java CPU路径按同一实际palette变换xyz，保持UV/绑定颜色，动态VBO进入真实GL绘制；Java GPU仍在原着色器变换。native CPU通过tools/preview/native的专用JNI桥复用核心immutable mesh、SIMD transform_vertices与持久CpuExecutor；不改公共法线/可逆矩阵合同，位置路径支持附件零缩放。桥接随核心JNI构建，不新增公共C ABI。

execution.nativeLibrary显式指定（相对配置文件或绝对路径），native threads=0/省略按CPU逻辑核心总预算，1/8/16按指定预算；Java默认归一为1。HUD/CSV/resolved-config记录实际预算、native-build.txt保留核心build ID。vertices_ms含完整native调用/转换开销，CPU/GPU分别摘要；GPU路径vertices_ms为NA。动画/Lua和骨骼palette目前仍为同一Java宿主，不能称native解析或native骨骼准备已完成。

canonical native/JAR构建通过；原沙箱MSBuild子节点通信等待经CIM确认、原session14442 Ctrl+C正常结束并确认子进程退出，改沙箱外单节点/nr:false构建后通过。期间缺stage参数编译错误已修正。没有重叠启动构建或强杀其他会话。

Test-PreviewCpu session32975 exit0：真实GPU shader对照、YSM/Figura共12个60帧实窗，覆盖Java CPU/GPU及native CPU1/8/16/auto，标签/耗时/摘要/清理通过。最终新增12,288顶点单模型（48块）的1/8/16/auto专项再次编译运行PASS，覆盖足够并行工作；最后只改测试/README，产品JAR未改。日志build-preview-cpu-test.log、test-cpu-vertices.log、native-build.log。不是正式性能成绩，不以小fixture速度作加速结论。

仍需native骨骼/GPU路径、解析选择、多模型1/5/20/50/100、可见/提交和共享隔离、峰值内存及正式报告；不恢复已撤回状态机。整体preview未完成，主M3仍暂停，无阶段提交推送。


### 2026-11-07 native骨骼、GPU提交与模拟参数原则

native路径现在由Java产生模型表达式/局部TRS输入，C++按父子深度并行组合world，独立并行生成inverse-bind palette和继承可见性；结果回填原world供模型查询/partToWorld使用，原版附加部件使用相应metadata范围。零缩放不要求法线逆矩阵，不改变公共pose合同。

native GPU复用同一着色器、GLFW/context及资源，在C++通过当前GL函数指针实际绑定VAO/纹理/Uniform并DrawElements。CSV追加native_draw_calls，记录实际循环发出的调用数；GPU路径vertices_ms为NA。此为预览器所需GL桥接，不将其称为原M4完整GPU设备API完成。

canonical核心/JAR构建PASS。Test-PreviewCpu session38582 exit0：六类现有动画/脚本/vanilla fixture的1/8/16骨骼/palette数值对照及12,288顶点用例通过；YSM/Figura20个60帧实窗覆盖Java CPU/GPU与native CPU/GPU的1/8/16/auto，native_draw_calls/线程/摘要/退出通过。随后原Test-Preview session64313 exit0：35正常实窗+1预期错误及原功能全部数值回归通过。日志build-preview-cpu-test.log/build-preview-test.log/native-build.log。

用户再次明确query.*/ctrl.*/player:isXxx等均可模拟传参。现states.query/ysm/ctrl继续直接注入，states.player按配置额外注册isXxx布尔/getXxx标量或三维向量，不需要为每个状态搬Minecraft API。新增任意方法名与既有向量/参数隔离专项PASS（test-vector.log/test-molang.log）；最后canonical JAR已重建。此参数增量后未重跑20+35实窗，未改渲染/矩阵产品代码。

native解析选择、多模型/共享隔离/可见提交数量、峰值内存及正式CPU/GPU分模型Markdown仍待完成，主M3暂停，无阶段提交推送，不恢复已撤回状态机。


### 2026-11-07 多模型与峰值内存

Windows K32GetProcessMemoryInfo/PeakWorkingSetSize经JDK25 FFM读取，每帧写peak_rss_bytes，摘要MiB。口径为进程生命期驻留高水位，含JVM/native/host driver分配，不含独立显存；不以Java堆替代。32MiB触页/单调性测试及Java/native两实窗CSV/摘要PASS。

same-model/mixed两类真实窗口场景已接，最大100。相同路径共享geometry/纹理/原生mesh；每实例独立动画/Lua/Molang/骨骼，clips与timeOffsetStepSeconds支持不同动作/相位。混合模型的modelOptions允许animation/scripts/states/clips覆盖，YSM/Figura同时渲染；所有native模型共享一个总线程池，不按模型数倍增线程。固定seed/100槽位布局与完整源集合相机范围保持1/5/20/50/100档位相同布局前缀与相机。

每帧按实际pose palette变换每骨骼绑定边界后做视锥测试，configured/visible/submitted单列。GPU逐模型Draw区间query相加，避免把CPU上传间隙算GPU绘制；限制4096个未回收query，覆盖不全记NA。骨骼准备含边界准备/视锥CPU开销。scene.json保留共享资源数/布局/实例标识，instance-palettes.json保存最终实际骨骼；audio.json保存各播放定义关闭状态。

Test-PreviewMulti session54660 exit0：两类×五档JavaGPU，加100实例JavaCPU/nativeCPU8/nativeGPU8，共16个12帧真实窗口PASS；跨路径100实例逐bone palette误差<=0.0005，不同clip/phase生效，资源共享/单native runtime/实际提交/进程内存通过。固定布局前缀复核PASS。新增剔除负例单独实窗12帧PASS：配置5、实际可见/提交3（run...171805-003-6104）；正式普通用例全部进入视锥。不是性能成绩。

后续只增加音频关闭快照及help/README，最终canonical重新构建；混合100 nativeGPU再次12帧实窗PASS（...172035-884-9616），100可见/提交、native Draw精确匹配，三个播放定义音频closed且active0。此前最终single JavaGPU12帧也PASS（...171933-616-35324）。没有把最后小改后的专项称为重跑完整16窗。无Minecraft/offscreen/活动工具句柄或强杀。未提交推送，整体preview还需native解析选择及真实抽样正式五轮矩阵/独立H2表格报告后交付。


### 2026-11-07 native解析与正式单模型基准

PreviewNativeAsset直接消费native已解析几何、动画/controller与资源；JNI只导出解析后几何供宿主绑定，不重新解析原容器。execution.parser=java/native、parserThreads=0(auto)/1/8/16，与渲染implementation/path独立；Figura继续原Java parser，混合场景按modelOptions.parser指定。parse.json区分native核心parse和端到端load，后者还含I/O、纹理、绑定和网格构建。

Test-PreviewParser通过10类输入×native1/8/16的几何、逐像素贴图、层级、默认隐藏、资源与真实动画姿态对照；new.ysm attacked片段在0/.125/.5姿态通过，并完成三个30帧真实Windows窗口，实际native Draw及parse.json预算/时间正确。额外目录01_taisho_maid真实idle姿态通过，所需ysm.food_level=20直接写配置，没有新增状态机。

差分发现并最小修复：YsmVisibilityDefaults.literalScale误把binary映射出的bezier_linked:false编辑器元数据当动态缩放，导致Java漏隐藏Mood1等附件。最小metadata测试RED→GREEN；原Java解析器本就忽略此标记，修复仅使预览器默认隐藏判断一致。NativeAssetTest联合断言已拆为可定位字段，临时输出动画JSON的调试代码已删除。

已新增PreviewParseBenchmark（无窗口的单模型暖缓存load测量）、Measure-Preview.ps1（八种实窗CPU/GPU组合及前后编译/测试进程记录）、Write-PreviewReport.py（CPU/GPU分开、各模型H2表格）。真实new.ysm、01_taisho_maid、AI Hobbyist Founder Public Server三种样本的单模型正式矩阵已全部完成：每例1280×720、VSync0、120预热帧+5轮×600采样帧。原始CSV/config/log在build/preview/benchmark；docs/preview/Benchmark-Parse.md、Benchmark-CPU.md、Benchmark-GPU.md已生成，目前CPU/GPU报告只含已完成的单模型行，需随多模型矩阵完成后重新生成。

解析测量每种组合独立JVM，3次预热10次采样。native当前端到端load未优于Java，不能用更低核心parse时间冒充整体提速。硬件经CIM读取AMD Ryzen7 260、8物理核/16逻辑处理器；自动预算按16逻辑处理器。解析同期进程快照不完整，报告明确标注；实窗每例前后快照留存，不清理其他会话。

本增量canonical JAR与native核心构建通过，parser专项实窗退出正常；未重复无关全套旧窗口。人工视觉/听音MANUAL/PENDING，不作为受阻。无阶段commit/push，主M3仍暂停；继续既定多实例/混合1/5/20/50/100及正式报告、最终审计/预览器提交推送，不新增宿主兼容要求。


### 2026-11-07 用户调整：单线程优先，暂停后续多线程测量

用户明确“优先测试单线程渲染，多线程如果多次低于单线程，可以暂时停止多线程测试相关，主要优化单线程”。实际六组CPU场景均满足：new-single native1/8/16平均FPS=730.54/692.98/630.74；maid-single=724.37/687.01/493.64；figura-single=824.15/647.08/593.59；new-same-5=384.41/300.92/258.41；figura-same-5=273.46/188.14/136.50；new-same-20=90.44/70.77/53.44。原始CSV保留，不替换既有结果。

因此停止后续8/16线程测量，缺少组合记SKIPPED（用户决定，未执行），不再作为预览器交付前置条件。正执行figura-same-20-native-gpu-8时，已通过命令行/当前case确认PID5140属于本会话并正常WM_CLOSE，JVM清理退出0；因帧数未完成，其metadata标stopped，部分采样不计入性能表。没有强杀、没有影响其他会话。

Measure-Preview.ps1默认Threads=1（Java1/native1×CPU/GPU），可显式-Threads 1,8,16恢复多线程比较；-SkipCompleted只复用已有complete案例ID，修改配置或程序后应使用新Name/输出目录。首次新参数因PowerShell变量大小写不敏感，Threads参数被threads循环变量覆盖而失败，未启动任何测量；已改为caseThreads，当前单线程续跑正常。runtime的threads=0/CPU核心数默认行为未改。

当前继续同模型YSM/Figura及混合场景20/50/100的单线程测量，保留1/5已完成矩阵；完成后报告以单线程为主要比较。后续性能实施优先缩小单模型单线程瓶颈，保持模型逻辑和输出正确性，不继续追加多线程门槛或Minecraft宿主状态机。

用户另授权增加优化参考库，已浅克隆完整工作树并生成独立CodeGraph，未编译或启动其Minecraft程序：

| 本地目录 | 来源提交 | CodeGraph验证 |
|---|---|---|
| refs/OpenYSM | a515d44686af77155a311b2a592327ca5d45a658 | 1,136 files / 21,933 nodes / 50,382 edges；status与projectPath查询成功 |
| refs/Sparkle-Morpher | 2e9dddadb09f74ae1456714fcfa118bc1d8c965e | 1,939 files / 42,046 nodes / 110,164 edges；status与projectPath查询成功 |

两目录被主仓库忽略，不提交上游源码/索引；来源分别为https://github.com/OpenYSM/OpenYSM.git、https://github.com/AIH-Interactive/Sparkle-Morpher.git。拉取和建图在benchmark暂停期间完成，没有与正式计时并行。日志build/preview/clone-*.log与codegraph-*.log。

### 2026-11-07 旧版基线检查点

old.ysm 四组合已完成，各 3000 聚焦采样帧、可见/提交 1。CPU native 相对同轮 Java -2.19%，GPU -8.50%，完整成绩保存在 unlit-v1 报告；不重复旧基线。下一步 P2 仅单线程骨骼准备优化，主 M3–M6 暂停。

### 2026-11-07 P2检查点与最新抽样要求

P2单线程骨骼顺序执行已构建并通过60帧实窗数值/shader对照；四类16组合正式实窗均完整，但仅目录CPU、FiguraGPU相对Java提升，仍未总体全绿。结果与下一热点见 Benchmark-analysis.md / bones-v1。最终多模型尚未开始，主M3–M6继续暂停。

用户最新明确每次模型重新随机抽样：从下一轮起四类各随机抽样一次，记录种子与清单，同轮Java/native CPU/GPU复用同一样本。此要求覆盖上文和旧交接中的固定样本约束。汇报用表格，只列相对Java差值/百分比。可以参考OpenYSM及Sparkle-Morpher实现进行最小热点优化。

### 2026-11-07 bones-v2随机轮次完成

矩阵float输入直读/double累积实验及实窗正确性通过；新随机四类16组合全部完成，仍未总体全绿。完整记录在Benchmark-analysis.md和bones-v2，失败保留。下一步先细分骨骼桥接阶段实窗耗时，再优化；不提前最终多模型或恢复主M3–M6。

### 2026-11-07 bones-v3诊断与复用实验完成

真实窗口细分确认JNI内部占骨骼桥接主要耗时；最小复用预览私有instance的double world存储，移除全部临时插桩后构建及60帧实窗对照PASS。新随机四类16组合完整，仍未全绿，结果与失败见Benchmark-analysis.md/bones-v3。下一步分析骨骼实际调用频次及Java直接父world TRS与native local+compose的结构差异；最终多模型和主M3–M6尚未开始。
