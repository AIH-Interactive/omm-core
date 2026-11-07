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
