# CPU/GPU Benchmark 分项分析与最小优化顺序

日期：2026-11-07。仅依据已完成的独立实窗 benchmark 与实际源码；不是 Minecraft 实机成绩。完整矩阵仍在执行。计时单位 ms，FPS 由采样总帧数/总耗时计算。下表只使用聚焦帧；Figura 各组均3000帧，YSM50 GPU亦各3000帧。Present 是 SwapBuffers 的CPU等待，不等同于GPU绘制；各分项不强行相加为GPU时间。

## CPU 路径

| 模型 / 数量 | 实现 | 动画 | 骨骼 | CPU顶点 | 提交 | GPU绘制 | Present | 平均帧耗时 | FPS |
|---|---|---:|---:|---:|---:|---:|---:|---:|---:|
| sickle / 1 | Java | 0.2141 | 0.0220 | 0.3559 | 0.1238 | 0.0175 | 0.5820 | 1.5496 | 645.34 |
| sickle / 1 | native1 | 0.2146 | 0.0479 | 1.3490 | 0.1207 | 0.0179 | 0.5705 | 2.5801 | 387.58 |
| sickle / 5 | Java | 0.6436 | 0.0964 | 1.6944 | 0.8207 | 0.0749 | 0.8003 | 4.4040 | 227.07 |
| sickle / 5 | native1 | 0.5597 | 0.1632 | 6.5448 | 2.2521 | 0.0872 | 0.0883 | 10.8455 | 92.20 |

单模型帧耗时增加约1.0305，顶点阶段增加0.9931；已测的主要差值落在CPU顶点。5实例还出现提交阶段差值，不能只看总FPS。

已确认源码差异：Java `PreviewCpuVertices.transform` 只做float位置变换并复制UV。native预览桥每模型一次bulk JNI（不是逐顶点FFM），创建pose向量，调用通用 `transform_vertices` 分配完整顶点结果、计算法线及UV并校验，再复制为5float位置/UV缓冲。当前native测量包含无光照预览不使用的工作，不能直接将差值归因于SIMD算法本身。

## GPU 路径

| 模型 / 数量 | 实现 | 动画 | 骨骼CPU | 提交CPU | GPU绘制 | Present | 平均帧耗时 | FPS |
|---|---|---:|---:|---:|---:|---:|---:|---:|
| sickle / 1 | Java | 0.2143 | 0.0219 | 0.0331 | 0.0191 | 0.6050 | 1.1347 | 881.32 |
| sickle / 1 | native1 | 0.1759 | 0.0364 | 0.0240 | 0.0194 | 0.4700 | 0.9043 | 1105.80 |
| sickle / 5 | Java | 0.5900 | 0.0735 | 0.0491 | 0.0671 | 0.5911 | 1.5681 | 637.73 |
| sickle / 5 | native1 | 0.6320 | 0.1613 | 0.0545 | 0.0702 | 0.6795 | 1.8249 | 547.96 |
| 08_sta / 50 | Java | 0.9684 | 2.2784 | 0.2809 | 0.8288 | 0.6018 | 4.5959 | 217.58 |
| 08_sta / 50 | native1 | 1.0185 | 4.5813 | 1.3000 | 0.8432 | 0.0677 | 8.2558 | 121.13 |

单Figura native GPU的FPS更高，但GPU Draw几乎相同，主要下降的已测项是Present和CPU动画；不能据此宣称shader更快。5Figura的骨骼与Present各增加约0.088。50YSM中骨骼增加2.3029、提交增加1.0191，GPU Draw只增加0.0144；当前优先查CPU侧准备/提交，而不是扩大GPU计算或改画质。

预览器native骨骼compose每次重建depth/seen/levels和world临时数据，并按层调用执行器；该结构与单线程热点相关，但其独立耗时尚未隔离。提交时间包含上传及驱动调用，需进一步分离后再修改。

## 单模型解析

| 模型 | Java端到端均值 | native端到端均值 | native核心parse均值 | native非核心部分差额 |
|---|---:|---:|---:|---:|
| 08_sta | 138.713 | 571.908 | 89.040 | 482.868 |
| 加密霜狐 | 112.840 | 425.494 | 125.644 | 299.850 |

差额包含输入准备、JSON/字符串导出与Java绑定、纹理解码、网格构建及清理，不能全部称为JSON时间。`PreviewNativeAsset.load` 在core parse之后读取metadata/resources/geometry/动画/controller记录并绑定为Java预览网格，后续必须分离这些阶段；不能只优化C++核心然后宣称端到端提速。Figura当前仍Java解析。

## 参考源码核对

| 参考 | 已观察实现 | 本项目可验证方向 |
|---|---|---|
| OpenYSM a515d44：NativeModelRenderer | 整模型JNI调用，矩阵批量传输 | 当前也已bulk JNI，不能把“改为批量调用”当作未实施优化 |
| OpenYSM：rip/ysm/gpu/GpuRenderPath | 缓存gpuMeshHandle，复用perFrameBoneBuffer，每帧上传骨骼缓冲后绘制 | 先保持共享静态mesh，检查骨骼准备/上传与提交边界；不照搬Minecraft光照/RenderSystem |
| Sparkle-Morpher 2e9ddda：ModelRendererBridge | RenderScratch复用矩阵和数组、已有骨骼顺序线性计算 | 对照本项目重复临时分配和拓扑处理；保留实例状态隔离 |
| Sparkle-Morpher：native dllmain.cpp | 缓存骨骼矩阵、thread_local输出缓存、四顶点quad SIMD数据 | 优先验证输出缓存/布局成本；不假设本项目所有几何都是quad或直接复制参考代码 |
| Sparkle-Morpher：GpuRenderPath | mesh注册表、复用骨骼scratch/缓冲，GPU mesh保留，按帧更新骨骼数据 | 优先减少CPU侧重复准备；不能据源码推断本机GPU已提速 |

参考源码仅用于比较，未运行其Minecraft路径，也未引用其性能数字。源码位置在本地 `refs/OpenYSM`、`refs/Sparkle-Morpher`；对应提交与索引已登记原计划。

## 已授权优化实验

1. CPU无光照输出等价化：若额外属性/中转是主因，直接写位置/UV并保持同一实际变换语义应显著降低CPU顶点时间。生产完整顶点/法线/UV接口保持原语义。
2. GPU单线程骨骼准备：若拓扑重建/临时分配及分层调度是主因，复用固定结构或避免单线程重复层调度后，bones_ms应下降而最终palette不变。
3. GPU提交：若CPU提交/上传是主因，GPU query近似不变但submit_ms应下降；先细分再改，不以降低绘制模型数或画质换FPS。
4. 解析宿主绑定：若core之后的转换是主因，减少中间表示后load_ms下降、core_parse_ms近似不变；单独立项验证，不与渲染实验混改。

每次先保留旧基线，再改一个变量，运行针对性数值对照及真实窗口同配置重测，逐例输出红绿比较。目标是实际提升；不删除失败/失焦/较慢成绩，不把微基准等同于真实模型FPS。

## CPU实验 P1：无光照输出等价化（本轮完成）

基线反馈命令：PreviewVertexPerformanceTest，10万自有顶点，实际JNI单线程，200轮预热后5轮×100调用，Java/native交替测量次序；校验反射/非均匀及零缩放位置、UV。旧路径median Java=0.323779、native=1.630494，慢5.036倍并触发RED；删除未使用法线/完整中间顶点数组后median Java=0.314886、native=0.269921，ratio=0.857，GREEN。微基准只证明该固定负载，不能替代模型FPS。

实现仅修改预览器无光照JNI桥，直接使用传入float palette写位置/原UV，保留有限仿射与溢出检查以及原线程预算；生产完整法线/UV变换接口未修改。10万顶点真实窗口GPU transform-feedback对照通过（反射/非均匀与零缩放），日志test-vertex-unlit-gpu-parity.log。当前再测原sickle单模型四组合，独立结果目录unlit-v1，保留原基线不覆盖。

### P1 三类单模型实窗结果

12组各120预热+5×600聚焦采样帧，可见/提交均1；原图像、动画、模型、相机与采样参数保留。目录与加密样本分别沿用原seed42抽样，不换模型。结果独立保存在 [P1 CPU](unlit-v1/Benchmark-CPU.md) / [P1 GPU](unlit-v1/Benchmark-GPU.md)。相对同轮Java：目录CPU -6.00%、GPU -3.93%；加密CPU -5.04%、GPU -13.60%；Figura CPU +10.79%、GPU +11.89%。本轮尚未全绿，后续继续GPU骨骼/提交实验。

三骨骼、10万不同owner顶点的真实窗口shader输出对照通过，涵盖反射、非均匀缩放、零缩放及原UV，日志test-unlit-visible-window.log。源无窗口微基准只保留为历史证据；用户要求测试仅用实窗后，已将其源移到build历史目录，新增的正式测试入口PreviewUnlitVertexWindowTest只创建真实窗口执行，没有无窗口模式。后续不再运行无窗口测试/微基准。

## P2：单线程骨骼顺序执行实验（2026-11-07）

仅修改预览 JNI compose：单线程沿已校验拓扑顺序组合 world/palette，省去 depth/levels 分配和逐层执行器调度；保留 double 中间矩阵、范围/顺序/有限仿射校验及多线程路径。生产 pose ABI 未变。

Release 构建 PASS（build/preview/build-bones-v1.log）。新增 PreviewBoneWindowTest 修改前后各 60 帧真实可见窗口 PASS，覆盖 96 骨骼两条反向索引链、非零子范围、反射/非均匀/零缩放、world/palette/继承可见性和实际 GPU 输出对照；test-bones-before.log / test-bones-after.log。

四类共16组合实窗完成，各120预热+3000聚焦采样帧，可见/提交1。首次目录 Java 启动因外层与脚本同时写 build.log 失败，已保留 failed metadata；重试改用独立外层日志，内部 build.log 保持前台 Tee。失败不计成绩。结果见 [CPU](bones-v1/Benchmark-CPU.md)、[GPU](bones-v1/Benchmark-GPU.md)。

| 样本 | 路径 | Java FPS | Native FPS | 相对 Java ΔFPS | 相对 Java |
|---|---|---:|---:|---:|---:|
| 08_sta | CPU | 504.25 | 742.48 | +238.23 | 🟢 +47.24% |
| 08_sta | GPU | 1084.30 | 984.67 | -99.63 | 🔴 -9.19% |
| 加密霜狐 | CPU | 1430.63 | 1238.99 | -191.64 | 🔴 -13.40% |
| 加密霜狐 | GPU | 1581.01 | 1461.68 | -119.33 | 🔴 -7.55% |
| Figura | CPU | 661.20 | 570.19 | -91.01 | 🔴 -13.76% |
| Figura | GPU | 972.18 | 1055.60 | +83.42 | 🟢 +8.58% |
| old.ysm | CPU | 957.63 | 891.87 | -65.76 | 🔴 -6.87% |
| old.ysm | GPU | 1338.75 | 1186.04 | -152.70 | 🔴 -11.41% |

正确性通过，但总体性能未达标。GPU 路径 native 骨骼均仍慢于本轮 Java：目录0.109/0.054、加密0.025/0.014、Figura0.030/0.021、旧版0.047/0.025 ms。提交差距仅约0.001–0.002 ms（Figura native 更低），当前优先继续检查骨骼的 JNI 数据转换、矩阵计算及缓冲复用。Present/动画亦有轮次波动，不把总FPS差异全部归为骨骼。用户授权参考 OpenYSM / Sparkle-Morpher 后尝试最小优化。

用户最新更正：后续汇报使用表格，只记录相对同条件 Java 的 FPS 差值和百分比；不再列相对旧 native 的提升。从下一轮起每轮四类各重新随机抽样并保存种子/清单；同轮 Java/native × CPU/GPU 共用对应样本以保证比较，历史结果不删除。该要求覆盖先前固定样本不重抽规则。

## P3：矩阵输入转换实验与重新随机抽样（bones-v2）

2026-11-07。参考 Sparkle-Morpher `common/src/main/native/openysm.cpp/dllmain.cpp` 的按列矩阵计算，以及 `ModelRendererBridge` 的 scratch/既有顺序复用；OpenYSM `NativeModelRenderer` 已核对为批量调用。本轮独立实现仅让预览 JNI 矩阵乘法直接读取 float 右矩阵，保留 double 累积、完整4×4和原加法顺序；不照搬参考库的float/FMA精度或游戏状态机，也未修改公共数学内核。剩余world临时分配与Java打包/回填未改。

Release构建、60帧实窗world/palette/可见性/GPU对照PASS，日志build-bones-v2.log / test-bones-v2.log。采样工具默认使用新随机种子并记录，增加YSGP v1/v2旧版独立类别；本轮种子3193744283785998159、完整候选池与SHA256见[bones-v2/sample.json](bones-v2/sample.json)。旧版池仅old.ysm一个候选，未因重复选中换样。

16个组合均完成，各3000聚焦采样帧、可见/提交1，原脚本/画面参数保留。目录首次测量工具宿主断连，原JVM22296已退出且CSV在第1975帧截断，保留failed记录并同样本重试；不计入成绩。人工视觉MANUAL/PENDING，无活动测试。

| 样本 | CPU 相对Java ΔFPS | CPU 相对Java | GPU 相对Java ΔFPS | GPU 相对Java |
|---|---:|---:|---:|---:|
| YSM_OLD_Model | -75.25 | 🔴 -6.76% | -154.11 | 🔴 -10.77% |
| 芙宁娜v3.14中文配音 | -53.33 | 🔴 -8.45% | +171.29 | 🟢 +15.54% |
| wangshu.zip | -10.97 | 🔴 -1.18% | -276.15 | 🔴 -17.14% |
| old.ysm | +88.21 | 🟢 +9.75% | -8.36 | 🔴 -0.57% |

结果见[CPU](bones-v2/Benchmark-CPU.md)/[GPU](bones-v2/Benchmark-GPU.md)。总体仍未达标；新随机样本不能用于证明相对旧版本native的因果收益。四类native GPU骨骼CPU仍高于同轮Java（目录0.027/0.017、加密0.093/0.062、Figura0.034/0.016、旧版0.041/0.024 ms）；GPU Draw几乎一致，Present波动也影响总FPS。下一步先在真实模型窗口细分Java输入打包、JNI计算、Java输出回填耗时，再决定最小优化，避免仅凭总FPS继续猜测数学热点。最终多模型尚未开始。

## P4：真实窗口分段诊断及world存储复用（bones-v3）

bones-v2检查点20c383afc22075ce677115ed57fa8f59e643e0ed已推送。用户已授权后续提交默认执行，不再重复询问。新随机样本见[bones-v3/sample.json](bones-v3/sample.json)：ZZZ_Ellen_1、银狼Lv.999v1.2、小符玄、old.ysm。目录没有idle，静态检查原动画声明后配置walk；缺clip失败日志保留，未换样或修改资产。

先用目录134骨骼真实可见窗口诊断3000次调用：Java pack0.001117 / JNI0.013829 / read0.002218 ms。第二次独立插桩把JNI分为buffers0.002554 / topology+world准备0.004358 / matrices0.008429 ms。诊断带计时开销，不作为正式FPS；日志profile-bones-directory-walk.log和profile-bones-native-walk.log。首次PowerShell未引用-D属性导致JVM参数错误，日志亦保留。

最小改动仅复用预览私有instance已存在的double world矩阵，避免每帧world vector分配/初始化；保持实例mutex、独立存储、父先子后校验及子范围offset。没有新增缓存生命周期或公共API。所有临时Java/native计时已移除，JAR/DLL重新构建PASS；60帧实窗反向链/子范围/world/palette/可见性/实际GPU对照PASS（test-bones-v3.log）。

| 样本 | CPU ΔFPS | CPU相对Java | GPU ΔFPS | GPU相对Java |
|---|---:|---:|---:|---:|
| ZZZ_Ellen_1 | -0.31 | 🔴 -0.03% | -184.34 | 🔴 -12.67% |
| 银狼Lv.999v1.2 | +45.94 | 🟢 +5.54% | -179.39 | 🔴 -13.32% |
| 小符玄 | -14.20 | 🔴 -4.00% | -22.39 | 🔴 -5.19% |
| old.ysm | -46.90 | 🔴 -5.12% | -164.27 | 🔴 -13.34% |

16组合全部完成，各3000聚焦采样帧、可见/提交1。完整[CPU](bones-v3/Benchmark-CPU.md)/[GPU](bones-v3/Benchmark-GPU.md)保留下降。仍未总体全绿。

五轮原始数据分析保存build/preview/benchmark/bones-v3/round-analysis.json。目录Java GPU骨骼round0=0.0356、round4=0.0133 ms；native=0.0422/0.0313。加密Java=0.0547/0.0219、native=0.0845/0.0499。早期采样仍有热身变化，但后轮native差距未消失，不能仅归因JIT；所有帧保留，不追删慢帧。小符玄骨骼整体Java0.621/native0.775 ms，需继续检查脚本相关调用与组合次数。

源码已确认YSM Java路径直接在父world上应用TRS，native路径先在Java生成local，再JNI做通用world组合；native多出一次矩阵组合和桥接。下一步针对实际调用频次与该结构差异定位，不再只从world分配推断总FPS收益。参考项目可以用于最小实现，但不移植游戏状态机、不改画质、不提前多模型。

## 参考架构与独立候选验证（2026-11-07）

用户更新顺序：先在独立测试中验证候选组合，随后性能筛选，确认后才集成omm-core。下面是本地源码的具体机制对比，不是四项目整体性能排名。

| 项目及源码 | 已确认机制 | 当前候选用途 |
|---|---|---|
| OMP_26.3 `render/acceleration/GpuPoseArena.java` | 按帧暂存对齐的姿态块，只上传尚未flush部分；GPU fence完成后回收缓冲 | 借鉴输出所有权与生命周期；暂不引入多模型arena |
| OpenYSM `rip/ysm/gpu/GpuRenderPath.java` | native骨骼计算直接填充perFrameBoneBuffer，然后上传SSBO；mesh缓存 | 评估直接生成上传布局，减少Java回填后再次打包 |
| SPM `core/gpu/GpuRenderPath.java` | native写输出缓冲；Java路径复用骨骼scratch并使用预烘焙顺序 | 作为同语义候选的对照；不移植静默fallback |
| YSM-native `modules/gfx/src/renderer/model_state_impl.inc` | 直接消费BoneAttribute，在native PoseStack应用TRS；复用pose存储及适用的调度结果 | 评估消除Java local矩阵构建与native通用组合之间的额外工作 |

当前预览器`PreviewMultiWindow`先更新全部实例再绘制，多个实例共享模型renderer/native桥。直接把桥的最后输出给所有实例使用会串姿态，必须保留每实例结果。Figura的`partToWorldMatrix`读取缓存savedPartToWorld，不会自行递归重新组合；世界矩阵查询语义仍需保留。

首个独立候选测试`tools/preview/tests/PreviewPoseCandidateWindowTest.java`使用实际单线程native compose，测试输出发布边界：两实例先更新、后消费；手算矩阵常量作为预期。共享输出原型在真实窗口RED：实例A位移应为-0.25，实际被B覆盖为0.5（`build/preview/test-pose-candidate-red.log`）。改为每实例输出后GREEN，30组双实例更新覆盖隐藏/恢复、反射/非均匀缩放、世界矩阵读取及真实GPU shader与CPU顶点一致性（`test-pose-candidate-green.log`）。这是小型合成正确性测试，不是提前运行最终多模型性能矩阵；未加载模型资产。

生产运行路径、JAR和DLL均未改变。该候选仍通过Java矩阵打包，因此尚未证明消除往返，也没有FPS收益结论。下一步在同一输出边界增加独立native直写/TRS候选，继续实窗正确性与性能筛选；四类正式模型每轮重新随机抽样，同轮Java/native路径复用样本，最终报告仅相对Java。当前最优组合与总目标均未完成。
