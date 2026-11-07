## 当前有效交接（2026-11-07）：独立候选测试先行

- 最新用户更正覆盖下面测试方式：当前候选单元测试仅测对应操作耗时，允许真实模型，暂不需要实窗。后续候选筛选采用无窗口操作计时，分别记录骨骼/JNI/回填/打包；不能把操作耗时当作最终FPS。下述实窗RED/GREEN保留为已完成历史，不再据旧规则启动候选实窗。

- 用户最新顺序：对比OMP/OpenYSM/SPM/YSM-native，先在独立单元测试确认候选，再实窗性能筛选，通过后集成omm-core。生产路径未改，主M3–M6仍暂停，整体未全绿。
- 新增tools/preview/tests/PreviewPoseCandidateWindowTest.java：实际native单线程compose、两实例先更新后消费、手算预期。共享输出原型真实窗口RED（A位移-0.25被覆盖为0.5），每实例输出GREEN；30组覆盖隐藏恢复、反射/非均匀缩放、世界矩阵值、实际GPU/CPU一致性。日志build/preview/test-pose-candidate-red.log和test-pose-candidate-green.log。不含Lua API端到端或真实资产性能结论。
- 四参考机制与源码位置记录docs/preview/Benchmark-analysis.md末尾。已排除共享桥最后输出直接供全部实例绘制的方案；目前候选仍走Java矩阵打包，尚非最终直写优化。
- 下一步：在独立测试输出边界加入native直写/TRS候选，验证offset、实例隔离及脚本所需world语义，然后实窗性能筛选；不要直接改生产路径或宣称已找到最优组合。正式新轮四类重新随机抽样，同轮路径共用样本。
- 当前main基于277f05f；本轮仅测试/分析/交接记录待检查点提交推送，其他M3修改保留。生产JAR/DLL不变，无活动测试；人工视觉MANUAL/PENDING。可靠额度周剩56%，banked reset可用2次，未消耗。

## 历史交接（2026-11-07）：bones-v3 world复用实验完成，未全绿

- 用户明确后续所有提交默认授权、不再询问；按阶段规则提交推送当前专项，保留其他M3修改。bones-v2检查点20c383afc22075ce677115ed57fa8f59e643e0ed已推送并核对远端main。
- 新轮随机manifest/config位于build/preview/benchmark/bones-v3，manifest另存docs/preview/bones-v3。选中ZZZ_Ellen_1、银狼Lv.999v1.2.ysm、小符玄、old.ysm；同轮各路径共用样本。目录无idle，配置改用其声明walk；错误日志保留，不换样。
- 临时实窗诊断：目录134骨骼，Java pack0.001117 / JNI0.013829 / read0.002218 ms。进一步JNI buffers0.002554 / topology+world准备0.004358 / matrices0.008429 ms（独立插桩运行，不当正式FPS）。日志profile-bones-directory-walk.log、profile-bones-native-walk.log。
- 已移除所有临时Java/native插桩。唯一生产改动preview_vertices_jni.cpp：复用预览私有instance.pose的double world存储，仍持锁、每实例隔离、保留拓扑校验。build-bones-v3.log / build-bones-v3-java.log构建PASS，test-bones-v3.log的60帧实窗world/palette/可见性/shader对照PASS。JAR/DLL已重建，artifacts.json存哈希。
- 本轮目录5395、加密59833、Figura44847、旧版75406均exit0；16组合完整，各3000聚焦帧、可见/提交1。CPU/GPU相对Java：目录-0.03%/-12.67%，加密+5.54%/-13.32%，Figura-4.00%/-5.19%，旧版-5.12%/-13.34%。当前无活动测试，未全绿，主M3–M6与最终多模型未开始。
- 本轮检查点277f05f621dc17ffc102c5dd93b994bc6fa9ec22已提交推送并核对远端main；本条提交后状态更新尚未单独提交。之后优先核对骨骼调用频次及Java父world直接TRS与native local+compose结构差异；五轮热身分析已保存，不能只把差距归因JIT。最后额度周剩56%，reset2次；<=2%优先使用授权reset。人工视觉MANUAL/PENDING。
## 当前有效交接（2026-11-07）：bones-v2 已完成，性能未达标

- P2 已推送 a2d8629885ca8ad898589dcb19c533421afeaa37；用户已明确授权后续所有提交默认授权、不再询问；本轮八文件提交推送继续按阶段规则执行，保留其他M3修改。本轮未提交改动为 preview_vertices_jni.cpp 的 multiply_preview（直接消费float右矩阵，保留double累积及原运算顺序）、Sample-PreviewModels.py（随机新种子及旧版独立分层）、本轮报告。其他M3修改保留。
- 构建PASS：build/preview/build-bones-v2.log；60帧真实窗口world/palette/可见性/GPU对照PASS：test-bones-v2.log。JAR不变，DLL已重建，artifacts.json保存哈希。
- 本轮seed3193744283785998159，四类抽中YSM_OLD_Model、芙宁娜v3.14中文配音.ysm、wangshu.zip、old.ysm；manifest和配置在build/preview/benchmark/bones-v2，manifest另存docs/preview/bones-v2/sample.json。同轮路径共用样本，不因失败换样。
- 目录四组完成：CPU -6.76%、GPU -10.77%；加密四组完成：CPU -8.45%、GPU +15.54%（相对同轮Java）。都是3000聚焦采样帧，可见/提交1，逐项报告已保存。
- 原目录首次测量因工具宿主断连，JVM22296已消失，CSV在1975帧截断；已标failed保留，不计成绩。重试目录四组session96830 exit0；加密session3289 exit0。
- Figura session68464、旧版 session94575 均exit0，本轮16组合全部完整。Figura CPU -1.18% / GPU -17.14%，旧版CPU +9.75% / GPU -0.57%；各3000聚焦采样帧、可见/提交1。当前无活动测试。
- 未总体全绿，不开始最终多模型或主M3–M6。下一步完成本轮提交推送检查点；之后新随机轮次先细分Java打包/JNI计算/Java回填耗时，再做有依据的最小优化。只实窗，无Minecraft。
- 用户要求表格只比较Java；周剩<=2%优先banked reset，最后可靠余额58%、可用reset2次，尚未使用。
## 当前有效交接（2026-11-07）：P2 实验完成，性能未达标

- 最新用户规则优先于下方历史：下一轮起每轮四类各重新随机抽样，保存种子/清单，同轮 Java/native CPU/GPU 共用对应样本；汇报用表格，只列相对 Java 的 FPS 差值和百分比。允许参考 refs/OpenYSM、refs/Sparkle-Morpher 做最小优化。
- main；旧版基线检查点 2c9a28cf4c9b0e14860b8d03c7b2910f40468596 已推送确认。P2 检查点 a2d8629885ca8ad898589dcb19c533421afeaa37 已提交推送并核对远端 main；其他 M3 修改必须保留，主 M3–M6 暂停。
- P2：tools/preview/native/preview_vertices_jni.cpp 单线程跳过深度分桶和分层调度，保持 double 中间矩阵及校验；tools/preview/tests/PreviewBoneWindowTest.java 新增真实窗口对照。Release 构建、修改前后各60帧world/palette/可见性/实际GPU对照 PASS。
- bones-v1 四类16组合完整，各3000聚焦帧、可见/提交1；相对Java：目录CPU +47.24% / GPU -9.19%，加密CPU -13.40% / GPU -7.55%，FiguraCPU -13.76% / GPU +8.58%，旧版CPU -6.87% / GPU -11.41%。未总体全绿。首次日志占用失败保留并排除；当前无活动测试。
- 日志 build/preview/build-bones-v1.log、test-bones-before.log、test-bones-after.log；正式原始数据 build/preview/benchmark/bones-v1，报告 docs/preview/bones-v1；构建产物与路径见下方。没有改Java生产源码，JAR未重建，DLL已重建。
- 下一最小任务：参考两项目，优先定位 JNI 骨骼矩阵转换/计算/缓冲复用，因为四类GPU骨骼耗时仍高于Java。不要恢复其他功能或提前最终多模型。人工视觉 MANUAL/PENDING。
- 周额度最近剩59%，banked reset 可用2次。用户授权周剩<=2%时优先自动使用可用reset，成功后查询继续；不可用/无法执行则按原规则安全交接。目前未使用reset。
## 当前有效交接（2026-11-07）：专项优化已接续

- 当前主要持续性目标为固定四类样本单线程专项优化；原主 M3–M6 保持暂停。下方历史状态如有冲突以本节为准。
- main / ac8b1f50844b94d75a6f035ee0c7cf188c353d73，P1 已推送。旧版 old.ysm 四组合 metadata 均 complete / exitCode 0，结果已保存，无需重测旧基线。各 3000 聚焦采样帧、可见/提交 1；CPU native 881.64 FPS，相对 Java -19.70 / -2.19%；GPU native 1377.27 FPS，相对 Java -127.89 / -8.50%。
- 旧版基线检查点 2c9a28cf4c9b0e14860b8d03c7b2910f40468596 已提交推送，远端 main 已核对。P2 已最小修改单线程 compose，跳过深度分桶与逐层调度；构建及修改前后 60 帧实窗骨骼/shader 对照 PASS。正在执行 bones-v1 四类单模型复测，尚未全绿。原计划下一步描述为历史：已核对 compose 每帧建立 depth/seen/levels/world 并按层调度；下一步仅优化单线程骨骼准备，保持 Java/native world、palette、可见性语义及真实窗口验证。其他未提交 M3 修改全部保留。
- 固定样本与启动方式见 tools/preview/README.md、docs/preview/Preview-plan.md、docs/preview/Benchmark-analysis.md；配置位于 build/preview/benchmark/random，旧版为 legacy-single.json。JAR 为 build/preview/preview.jar，DLL 为 build/windows-x64-Release/bin/Release/omm_core.dll。
- 仅真实可见 Windows 窗口测试；不运行无窗口测试/微基准，不启动 Minecraft。优化期间仅四类单模型 Java/native1 × CPU/GPU，所有路径完成后再最终多模型矩阵。逐组合更新报告，保留下降；人工视觉 MANUAL/PENDING。
- 可靠额度接口：周已用 41%，剩余 59%，可用 banked reset 2 次。用户新增授权：周剩余 <=2% 时优先自动使用可用 banked reset，成功后重新查询并继续；不可用或无法执行时按原规则安全交接。尚未使用重置。当前尚未总体全绿，P2 与最终矩阵未完成。
> 新会话发送状态：用户要求重试后已成功创建并发送到“单线程专项优化：实窗总体全绿”，threadId=01a1b5c4-0a4d-7830-939e-601f4ebd740e，hostId=local。原会话总目标保持paused。

## 当前主要持续性目标：专项优化，覆盖旧版YSM

- 用户最新明确专项优化作为当前主要持续性目标，其他开发阶段暂缓。保留总目标与阶段提交推送/额度规则，但下一任务只围绕已定位CPU/GPU/解析热点及实窗复测，不恢复其他功能开发。
- ac8b1f50844b94d75a6f035ee0c7cf188c353d73已推送origin/main，P1源优化/12例实窗/仅实窗AGENTS规则均已提交；推送后remaining60%。当前没有活动会话。
- 原目录/当前加密/Figura三样本不重抽。已只读扫描.ysm文件头，新增legacy stratum按seed42固定选样，manifest在docs/preview/legacy-sample.json及build/preview/benchmark/random/legacy-sample.json；old.ysm容器YSGP v2已确认，内部binary版本尚未解析确认，不混为当前v3。
- 下一步先用当前P1程序对新增旧版样本作单模型四路径实窗基线，随后P2骨骼准备优化覆盖四类单模型。所有路径后再最终多模型；不再运行无窗口测试或微基准。最小运行配置legacy-single.json已生成，尚未启动模型。

## P1本轮单模型实窗验证完成，准备提交

- session74175 exit0，三类合计12例实窗完成，所有采样均3000聚焦帧，可见/提交1；数据在unlit-v1，未覆盖原random基线。相对同轮Java FPS：目录CPU619.25(-39.52/-6.00%) GPU1280.54(-52.36/-3.93%)；加密CPU1135.78(-60.32/-5.04%) GPU1055.35(-166.09/-13.60%)；FiguraCPU685.23(+66.75/+10.79%) GPU1035.49(+110.04/+11.89%)。仍未全绿。
- 用户要求仅实窗后，已把新无窗口性能测试源码移入build/preview历史文件；正式新增PreviewUnlitVertexWindowTest无headless模式，真实可见窗口100k/3骨骼owner反射/非均匀/零缩放和UV对照PASS，test-unlit-visible-window.log。不再执行无窗口测试或微基准。
- P1改动仅无光照CPU JNI桥，直接float位置/原UV输出，无生产完整法线/UV变换语义改变；GPU骨骼路径未修改。下一步提交推送本阶段/查询额度，然后单模型GPU骨骼准备实验；最终多模型等待全部路径优化完成。
- 当前无活动测量/构建会话。根AGENTS实窗规则及测试顺序已更新。主M3未完文件保留，不纳入本提交。

## 最新强制规则：测试仅用实窗测试（已写入根AGENTS）

- 用户明确要求“写入agents，测试仅用实窗测试”。根AGENTS新增原意规则：所有后续测试/benchmark仅真实可见Windows窗口，不再运行无窗口测试或无窗口微基准；构建/编译不属于测试。历史无窗口记录保留，不作为最终实窗结论。
- 优化期只单模型，原三类样本和CPU/GPU/Java/native1路径齐全后再最终多模型。每例立即记录并汇报相对Java FPS/百分比，优化前后native另列基线。
- 当前无活动会话。P1三类12例实窗已完成，聚焦样本均3000且可见/提交1；native相对Java：目录CPU-6.00%/GPU-3.93%，加密CPU-5.04%/GPU-13.60%，FiguraCPU+10.79%/GPU+11.89%。需把本轮结果和规则纳入P1提交检查点，尚未提交优化代码。
- 原100k窗口GPU对照最终PASS，覆盖三骨骼不同owner、反射/非均匀/零缩放；首次新增测试fixture因跨part三角形被真实API拒绝，修正fixture为每三顶点同part后PASS，未改生产模型校验。

## 最新强制顺序：优化只测单模型，全部路径后再最终多模型

- 用户明确：优化期间只用单模型，完成所有路径后再最终多模型。停止新增same-model/mixed中间测量，原数据保留为基线；单模型仍覆盖原YSM目录/加密YSM/Figura及CPU/GPU，单线程优先。
- 当前session74175正依次补unlit-v1目录/加密样本的单模型四组合；不涉及多模型。继续前轮询同一会话，不并行构建/其他性能测试。Figura四组已完成，本轮三类合计目标12例。
- P1全部三类实窗完成后记录并提交推送/额度；后续GPU骨骼/提交及解析优化也只用单模型，所有路径完成后再做最终1/5/20/50/100同模型及含Figura混合场景。

## P1实窗Figura已完成，用户要求三类样本全部复测

- session55965 exit0：unlit-v1 Figura单模型四组合各120预热+5×600聚焦帧完成。CPU Java618.47/native685.23，native相对同轮Java+66.75FPS/+10.79%；GPU Java925.45/native1035.49，相对+110.04/+11.89%。每例即时报告。只说明本样本，不宣称所有矩阵全绿。
- 用户要求每轮实窗后汇报相对FPS和百分比，并明确YSM目录、加密YSM、Figura都测。接下来补同一原样本sample0/1的四组合，不重新抽样；输出仍unlit-v1，与原random基线分开。
- CPU桥代码、性能测试、分析增量尚未提交；当前没有活动测量。三类实窗完成及必要检查后做P1检查点提交推送/额度，再继续GPU骨骼实验。其余20/50/100矩阵仍保留。

## 性能实验P1：CPU无光照桥已优化，等待真实模型复测

- 分析阶段已推送4b976325dd6b822b6be27ea9d3c7c61439b3dc53；推送后额度remaining60%。用户授权CPU/GPU都按benchmark分析、找到原因后优化再测，目标实际全绿。
- 新PreviewVertexPerformanceTest不加载模型：旧实际JNI单线程10万顶点RED（Java0.323779/native1.630494ms）；最小改为float位置+原UV直接写出后GREEN（Java0.314886/native0.269921ms）。同位姿反射/非均匀/零缩放与UV对照通过。100k真实窗口GPU transform feedback对照也PASS。
- 只改preview_vertices_jni.cpp的无光照transform桥：避免临时pose/full顶点结果和未使用法线/UV工作，保留有限仿射/溢出检查与执行器预算。生产完整C ABI变换未改。native单进程前台Release重建PASS，build-native-unlit.log。新源码/测试尚未提交。
- 原混合69070已exit0，无其他活动测量。即将原sickle单模型四路径复测，输出build/preview/benchmark/unlit-v1，报告docs/preview/unlit-v1；原random基线保留。artifacts.json记录当前JAR/DLL SHA256。报告生成器在无解析数据时不生成空Parse报告。

## 当前阶段：CPU/GPU分析完成，用户授权最小优化再测

- session69070 exit0，含Figura三来源mixed1/5八例完成；随机总60 complete，3失败保留排除。test-figura-ysm-mixed-records.log验证真实mixed5：资源3、五个独立状态、native按来源java/native/native parser；CPU/GPU最终palette对照max_abs=5.06e-7。无活动测量会话。
- docs/preview/Benchmark-analysis.md已按真实聚焦帧分别分析CPU/GPU及解析，并核对OpenYSM NativeModelRenderer/GpuRenderPath、Sparkle ModelRendererBridge/GpuRenderPath/native dllmain源码。没有运行Minecraft，也没有改参考库。
- 用户明确授权找到原因后尝试优化再测，目标全绿；保留模型/画质/采样及所有慢成绩。分析首选CPU无光照路径多余完整属性/中转，其次GPU骨骼重复准备和CPU提交。完整矩阵20/50/100仍保留，当前先做单热点实验。
- 下一步先保存本分析与60例报告检查点，推送/额度后开始CPU紧凑反馈：旧native/Java同输入微基准→只改变已证实的无光照额外工作/中转→数值对照→原真实模型同配置重测。生产完整法线/UV接口语义保持不变。不能把微基准当真实FPS或把GPU总FPS归为GPU算力。

## 当前活动会话：69070（含Figura混合1/5）

- session69070前台执行含Figura三来源mixed1/5四组合，mixed1 JavaCPU已完成并自动更新报告；继续前先轮询，不重复启动。最新额度周used40%、remaining60%。

## G11检查点已推送 f3d2810，下一步含Figura混合场景

- f3d28107bb4c86cfa2feddb0d1e67fa596f6f1fe 已推送origin/main，远端与本地一致。修复限两个states深拷贝语句及回归/结果记录；主M3仍未完成，已有修改保留。
- 准备了figura-ysm-mixed五档配置，来源池是原sickle+原目录YSM+原加密YSM，不重新抽样。Figura保持原脚本/figura模式，两个YSM通过modelOptions明确clip idle/scripts=false；Measure脚本按各来源选择解析器，Java/native单线程CPU/GPU分开。
- 本轮先测含Figura混合1/5；其后Figura同模型20/50/100和混合20/50/100及YSM100，仍逐例即时记录。不能把只含YSM的mixed成绩代替含Figura场景。

## 当前有效结果：Figura same-model1/5八组完成，G11检查点收尾

- session88800已exit0，Figura same-model1/5八组各120预热+5×600采样完成，逐例自动写表；随机总52 complete，3 failed保留排除。当前无活动测量会话。
- test-figura-multi-records.log核对真实实窗输出：5实例共享geometry1/纹理上传4、五个独立状态/相位、native runtime1；CPU/GPU两条路径最终Java/native palette一致，max_abs=2.38e-7。没有重新解析或加载模型。
- G11两个复制点各新增states深拷贝，ConfigNullTest RED→GREEN、构建PASS、上述实窗及数值验证PASS；缺口表和集中分析文档已更新。本检查点仅含这两行代码、对应测试及证据报告，不夹带主M3修改。
- 下一最小任务：Figura20/50/100与含Figura混合场景，剩余YSM100；性能分项初查发现native顶点路径是每模型一次bulk JNI，并非逐顶点FFM调用。JNI适配每次建pose/full顶点结果并复制为5float输出，后续应读内核和OpenYSM/Sparkle-Morpher确认实际成本，再优化，不据此先改代码。

## 当前测量会话：88800

- session88800前台执行Figura same-model1/5四路径，id含nullfix；same-model1 JavaCPU已完成并立即报告。必须先轮询88800，不启动重复测量或并行编译测试。G11源改动尚未提交，等当前实窗证据后检查点提交推送。

## G11已修：Figura多实例配置克隆保留显式null

- 第一轮random-figura-same-model1 JavaCPU失败于EZAnims384，原生成配置确有getVehicle:null；静态路径确认sourceConfig和Source.newState的Gson克隆丢null。已先登记G11，PreviewConfigNullTest RED→GREEN，两个复制点仅增加states深拷贝，未增加宿主功能。
- canonical Build PASS（build-config-null.log），JAR已更新；最小缺口表已立即记录，配置深拷贝隔离专项PASS。新增源改动2行+测试，尚未提交，不将专项视为实际多实例通过。
- 接下来运行same-model1/5新id带nullfix，保留失败case且不计成绩，原脚本/样本/显式配置不变。上轮无活动进程；记录新session后从同一会话继续。

## 已推送 Figura 公共补充检查点 c544673

- c54467325c322319232b0f4ba46f6cc1183ff658 已提交推送origin/main，ls-remote与本地一致；14文件347新增行，限G09/G10、接口/原库测试、分析说明和44例已完成报告。主M3未完成文档/工具修改保留。
- 推送后额度周used39%，remaining61%，未达交接阈值。根AGENTS使用规范已在d11e380推送。
- 下一优先测量Figura same-model1/5（保留原脚本和显式状态、Java/native1×CPU/GPU）；其后20/50/100与含Figura混合场景。100档YSM仍未执行。不把解析或单模型通过当全部矩阵完成。

## 当前有效结果：Figura四组正式单模型完成，公共补充验收通过

- session52891 exit0，random-figura-single-g08 Java/native1×CPU/GPU四组各120warmup+5×600sample完整通过，原sickle脚本开启，可见/提交均1；每组已立即写表。随机累计44 complete，之前2失败保留排除。当前无活动测量进程。
- G09/G10各接口RED→GREEN，原完整动画库内存接线/60帧实际idle PASS；Files/Client/Metatables/ModelRoots四项相关回归全部PASS（test-figura-query-regression-*.log），不再重复加载模型。最后命令退出1仅因为读取不存在的figura-animation示例路径，不是测试失败；已改在README写实际配置说明。
- 集中分析文档已记录原库版本/哈希、必经/条件分支及真实四组结果。README说明配置返回常量、null/对象及非Minecraft宿主边界。用户要求公共最小修复已完成，没有关闭脚本/改模型/搬状态机。
- 本次检查点提交范围仅公共查询适配、根矩阵入口、相关测试/说明和已完成报告；主M3未完工作区保留。下一步优先Figura same-model五档与含Figura的混合场景，仍需100档等剩余正式矩阵；分析性能后再参考OpenYSM/Sparkle-Morpher做单线程最小优化。

## G09/G10及完整原动画库接线已通过，优先Figura正式测量

- G09配置常量查询接线已完成（host、player null/对象、world固定方块），保持原world时钟/已有typed player查询；G10根矩阵1/16模型单位与现有部件一致。两个接口分别RED→GREEN，每项通过即更新Minimum-gaps及集中分析文档。
- canonical Build PASS，JAR已更新。原EZAnims/GSAnimBlend完整文本+实际内存Animation对象的初始化/60帧tick/render/postRender+idle:isPlaying PASS，test-animation-libraries.log；未加载模型。
- sample-2及figura-same-model五档配置已显式声明host=false、无载具、双手空气/NONE/空tag、固定空气方块isSolidBlock=false。不增加游戏状态机，不处理当前条件不触发的目标/载具/原版姿态回调。
- 下一步正式random-figura-single-g08四组合CPU/GPU×Java/native1，保留脚本；须记录实际session句柄与终态，不把内存原库测试当实窗模型PASS。新增修复尚未提交；先取得实窗证据后做最小回归与检查点。

## 最新用户要求：集中分析动画库并记录缺口

- 已完成原选样 EZAnims3.0.1（775行）、GSAnimBlend2.4.1（2271行）及8行script的静态宿主/元表/事件/动画调用核对；docs/preview/Figura-script-gaps.md 保存原始SHA256、调用行号、已支持/必经缺口/条件分支和配置边界，并由Minimum-gaps链接。
- 先登记G09：host布尔、null载具、固定方块与空手物品对象的显式配置返回值；G10：根partToWorldMatrix。其余飞行/载具实体/目标实体/原版姿态混合回调均按真实分支记录，不自动实施。没有重新加载模型。
- 已写未接入的PreviewConfiguredQueries草稿和两个无模型接口测试源码（ConfiguredQueries/RootMatrix）；还未运行RED或编译，不能记为完成。正式实现接线在集中分析记录后执行。现产品JAR仍为已推送G08版本。
- session14283已exit0：50档same/mixed共8例完成，随机complete总40；每例已立即刷新报告。当前无活动测量。下一步先RED测试，G09/G10最小接线GREEN，再完整原库内存接线测试，随后优先Figura正式测量，不能只继续YSM。

## 最新用户优先级：必须加入 Figura，不能仅测 YSM

- 用户明确要求Figura加入正式测试。保留原随机sickle.zip及完整Lua，不换简单样本，不将YSM成绩代替Figura。
- session14283在mixed50 nativeCPU运行，等待现有批次终态后优先运行 random-figura-single-g08（最新JAR已含G07根children与G08焦点），四组Java/native1×CPU/GPU；先不继续YSM100档。
- 已准备figura-same-model-{1,5,20,50,100}.json，仍同一选样及显式states，未加载模型、未启动并行测量。Figura模型解析基线已有10次Java1记录，但不等于Lua/动画运行通过。
- 新失败先按实际调用记录缺口再最小修复；不关闭脚本、不扩充Minecraft状态机。当前没有新的已证实必改项。

## 已确认推送：预览器代码检查点 9920235

- 预览器及必要native依赖已提交99202357c616230f271df3dee50e9f7786afcc0d，并推送origin/main；git ls-remote确认远端HEAD与本地一致。用户授权不等待剩余测量先提交代码；不代表所有benchmark或主M3完成。
- 自动审批首次因334文件/约5万行、vendor及未核实远端拒绝；已核实161个vendor源码/许可证共26856行由构建实际使用、无模型/JAR/日志/缓存，并确认远端旧HEAD为M2 f51df7d。随后提交和推送均获批准并成功，不再受此拒绝限制。
- 推送后可靠额度查询：周used38%，remaining62%，未达交接阈值。
- 根AGENTS已按既定后续要求补充预览器使用、配置状态模拟、逐例记录和焦点分组规则；本次文档检查点仅含AGENTS与本交接记录，不夹带暂停M3修改。
- session14283仍在mixed50 JavaCPU；same-model50四组合均已完成并逐项报告。最新报告在代码检查点后继续变化，尚未提交的测量增量保留。100档和Figura完整运行仍待验证。测量进行时不并行编译测试。
- 用户要求分析后参考refs/OpenYSM及refs/Sparkle-Morpher定位瓶颈；先检查已测量顶点变换/数据复制/提交时间，再决定公共最小优化，不扩充预览器宿主API。

## 用户授权立即提交预览器代码检查点

- 用户最新指示：预览器暂时没有需要修改的地方，直接提交推送。当前没有未实施的已确认必改缺口，G01–G08均已实现并验证；代码检查点不等待剩余benchmark，也不标记主M3完成。
- 提交范围：tools/preview、docs/preview、根AGENTS用户规则、交接，以及预览器编译所需CMake/C ABI/JavaJNI/模型内核/执行器/资产读取接口及CMake引用的针对性测试。保留其他暂停M3文档/工具和外部索引/崩溃日志工作区修改，不擅自纳入。
- session14283仍在same-model50 nativeCPU；提交操作不改JAR或重启测量。后续测量分析后参考OpenYSM/Sparkle-Morpher定位单线程热点，优先看顶点变换、跨语言/数据复制及提交，先用证据再优化。
- 本节为提交准备状态，提交SHA与推送结果必须以实际git输出更新，不能预记成功。

## 最新逐项结果：50实例 Java CPU 已记录

- session14283仍live，random-ysm-same-model-50-java-cpu-1 已完成，脚本已立即生成docs/preview/random-single报告；总complete增至33。当前进入same-model50 nativeCPU，不重启，不并行编译测试。
- Java CPU50原始frames.csv已完整到3119（120warmup+3000sample），最后两帧configured/visible/submitted=50/50/50且window_focused=true；完整焦点分组以生成报告为准，不凭尾帧宣称整批聚焦。
- 其余50组合及100档仍未完成，阶段未提交推送；主M3保持暂停。

## 当前前台测量：50实例，逐例立即报告

- session14283 正在依次执行 random-ysm-same-model-50、random-ysm-mixed-50，各Java/native1×CPU/GPU；新JAR含真实焦点列。ReportDirectory显式docs/preview/random-single，每例完成即生成报告；无需旧临时监听器，28965已退出。
- 下一次继续前先轮询14283，不重启或并行编译/其他性能测试。保持真实窗口静止且聚焦；若发生失焦，报告会独立分组，不能混为聚焦成绩。100档尚未开始。

## G08 完整最小验证已通过

- 新JAR在真实单模型及same-model窗口分别跑内置立方体60帧，两者CSV均60帧聚焦记录，正常退出，无抽样模型加载。test-focus-smoke-{single,same-model,records}.log。
- test_report.py 使用合成计时数据验证分组：聚焦native相对Java+100FPS/+100%，失焦-25FPS/-50%，正绿负红，两组各2帧；running案例只在completed-only被排除，默认仍拒绝。PASS，test-report-focus.log。初次测试受沙箱系统temp路径访问失败，已将临时目录放在本仓库build/preview后通过，无产品回退。
- Measure-Preview.ps1 PowerShell AST语法检查PASS；默认输出render并逐例立即报告；README已说明ReportDirectory和焦点。G08表已即时更新。下一步50/100档；原32例均无焦点字段，报告如实标未记录，不将其当作新聚焦验证。

## 最新有效检查点：32例批次结束，G08 焦点记录完成

- 测量session36808和临时报告监听28965均exit0；随机共32 complete案例，2失败Figura排除。逐项监听确实记录30→31→32，原始日志保留。当前无活动测试/测量进程。
- 用户观察失焦降帧后，已先登记G08再最小补充：FrameLog增加window_focused，单/多模型每帧首尾读取实际GLFW_FOCUSED；报告按聚焦/失焦/旧日志未记录分组，比较key包含focus，不混比不同焦点组。
- canonical Build PASS，session97746 exit0；PreviewWindowFocusTest 两个可见GLFW窗口实际转移焦点并核对CSV true/false PASS（test-window-focus.log）。没有重新加载抽样模型；JAR已更新。
- G08行已立即记录。焦点只证明首尾快照，不覆盖帧内丢失再恢复；旧数据没有焦点证据，不能标为前台固定测量。
- 后续最小：核对报告分组与脚本语法，重建32例报告保留窗口干扰及未知焦点标注，再推进50/100档。Figura完整行为仍未验证；主M3暂停，preview阶段未提交推送。

## 最新用户规则：测完一个立即记录一次

- Measure-Preview.ps1 在每例采样/可见提交检查通过并保存 complete 后立即调用报告生成器；新增 ReportDirectory，默认 <OutputDirectory>/reports。默认 OutputDirectory 修正为 build/preview/benchmark/render，与报告既有目录布局一致；现有显式 random/render 不变。
- Write-PreviewReport.py 新增 --completed-only：只发布 complete，显式列出 running/validating，不影响默认严格拒绝未完成案例；读取 window-movement-observation.json，保留用户窗口干扰说明；native有色比较继续保留。
- 已启动临时前台文档监听 session28965（build/preview/benchmark/random/watch-reports.py），仅在 complete 集合变化时重建 docs/preview/random-single；首次记录29个完成案例。它不启动模型，不重测，目标mixed20 nativeGPU终态后退出。原测量session36808仍在进行mixed20 nativeCPU，不能重启或视为结束。
- 继续前分别轮询36808与28965。批次终结后做必要脚本语法/实际报告核对与diff检查；本次没有新产品JAR修改。阶段尚未完成/提交推送。

## 最新环境观察：移动窗口影响帧率

- 用户报告移动窗口会影响FPS；已响应并记录原始 observation 到 build/preview/benchmark/random/window-movement-observation.json，记录当时 running 案例，但没有证据证明移动发生于哪一案例，不得将全批直接判失败或凭空排除某帧。
- 随机 CPU/GPU 报告已添加条件缺口：未记录移动事件，具体受影响案例未确定；正式采样保持窗口静止，确认发生移动的成绩应单独标记并必要时重测。不能以该类干扰推断实现性能差异。
- session36808仍 live，继续轮询，不重启；native CPU20的帧记录确实持续增长。未擅自中止批次、删帧、缩短采样或启动额外复测。

## 最新用户要求：native 相对 Java 的有色比较

- Write-PreviewReport.py 已在 CPU/GPU 表末尾添加“相对Java ΔFPS”“相对Java变化”，用未四舍五入的总采样FPS计算。基线按 metadata.name 与 CPU/GPU path 匹配（同配置批次/场景/模型数量），不跨路径或不同批次混比；Java标基线，缺基线NA。正值绿 span+🟢，负值红 span+🔴，不支持HTML样式时仍有彩色标记和正负号。
- docs/preview/Benchmark-CPU/GPU.md 辅助基线已重建。random-single三报告从终态metadata快照更新，进行中case排除；snapshot在build/preview/benchmark/random/report-snapshot，原始run/config路径仍引用canonical文件。文档明确仅已完成案例，不宣称矩阵完成。
- session36808仍在运行，已完成same/mixed的1实例8例，正在same-model20 JavaCPU；继续前轮询同一会话。不要因报告已更新而把批次视为结束，不并行编译测试。

## 当前执行中：随机解析完成，1/20实例正式批次

- session21630 exit0：sample0/1 Java/native1及sample2 Java1，5组，每组3预热+10采样，共50条有效解析成绩。docs/preview/random-single/Benchmark-Parse.md 已生成，按实际模型H2；两个YSM native端到端均慢于Java，native核心时间单列；Figura解析不执行Lua，不等于其脚本通过。
- Write-PreviewReport.py 修正输入根路径、解析线程描述及随机模型标题；不再把单线程随机数据写成1/8/16。随机decision文件保留后续多线程SKIPPED。未新增产品逻辑。
- 正在foreground session36808：same-model和mixed的1、20实例，各Java/native1×CPU/GPU。启动后已确认same-model1 JavaCPU完成，后续仍在运行；继续前先轮询同一会话，不重启、不并行编译或其他性能测试。config文件均在benchmark/random，命名random-ysm-{same-model|mixed}-{1|20}。
- 批次结束后再生成报告，报告生成器拒绝running案例。50/100档及Figura完整行为仍待完成，阶段未提交推送。最后额度remaining62%。

## 最新正式测量结果：随机案例16个完成

- session40397 已exit0：目录样本 same-model/5、目录+加密两YSM mixed/5，各Java/native单线程×CPU/GPU，新增8例，连同单模型共16例完成。每例120预热+5×600帧，脚本检查完整采样和可见/提交数量；scene.json保留共享资源和实例状态身份/动画相位。
- docs/preview/random-single/Benchmark-CPU.md、Benchmark-GPU.md 已更新，每来源独立H2表，失败Figura两次不计成绩；不存在随机解析成绩，未输出空Parse报告。虽然目录名保留random-single，现含已完成5实例数据。
- 后续最小任务：保持已选样本，完成其余同模型/混合1/20/50/100档及解析测量；Figura完整行为仍未验证。G07修复后没有重新加载Figura。不得为逐项补缺反复加载。没有活动测试/测量会话。周额度最近remaining62%。

## 最新有效检查点：G07 已补齐

- 修复后 Figura 正式测量 random-figura-single-g06 在 ENTITY_INIT 的 EZAnims:668 失败：models:getChildren 缺失；失败不计成绩。ARMOR 初始化已越过，不宣称 Figura 完整通过。
- G07 已先登记后实施，PreviewFiguraParts 根表通过元表提供 getChildren，返回实际根部件的 1-based 新 Lua 表；保持命名访问及 pairs(models) 只枚举根。参考 OMP FiguraModelPart.getChildren，无模型特判。
- PreviewModelRootsTest RED→GREEN，ArmorVisibility/Metatables 回归 PASS；canonical Build PASS，session98163 exit0，日志 build-g07.log/test-g07-*.log。JAR 已更新。修复后没有再次加载实际模型。
- Minimum-gaps.md 的 G07 已即时更新；无活动测试会话。仍需剩余正式矩阵/解析报告及预览器检查点。main 未提交推送，主 M3 暂停。保留已有修改。

## 最新有效检查点：G06 已补齐并记录

- G06 ARMOR 无装备可见性适配已完成；只改 PreviewVanillaModel 并新增 PreviewArmorVisibilityTest。OMP VanillaPart nullable override 语义：nil/false/true；独立于 PLAYER，无装备几何或世界状态机。
- RED 复现 ARMOR 缺失；canonical Build PASS，GREEN 测试覆盖链式调用、玩家独立及实例隔离。日志 build/preview/test-armor-red.log、test-armor-green.log、build-g06.log。JAR 已更新。未加载真实模型或重新抽样，Figura 最新端到端仍未验证。
- Minimum-gaps.md 的 G06 行已即时更新。下方“G06 未实施”均为历史记录。随机两个 YSM 单模型8例报告保留，G06 不改变这些 YSM 路径。
- main 尚无预览器阶段提交/推送；剩随机 Figura 实际测量、多模型矩阵、解析报告及最终检查。先完成当前阶段，不恢复主 M3 或扩大宿主 API。最近额度剩62%。

## 最新完成的随机单模型测量

- session86964、23790 均已 exit0：08_sta 与加密霜狐各 Java/native 单线程 × CPU/GPU，合计8个完整实窗案例；1280×720/VSync0/120预热/5×600帧。没有活动测量会话。
- 已生成 docs/preview/random-single/Benchmark-CPU.md 和 Benchmark-GPU.md，每模型独立 H2/表格，记录失败 Figura 不计成绩；无随机解析数据，因此未交付空解析报告。当前 native CPU FPS 均低于 Java，不能宣称提速。同期宿主进程未完整核查，报告已注明可能影响结果。
- G06 仍未实施。已对照 OMP VanillaModelAPI：ARMOR 与 PLAYER 独立，包含四个盔甲子组；预览器无装备，下一步仅适配无装备下的独立可见性状态，禁止把 ARMOR 映射为 PLAYER 或扩大为装备系统。补缺期间不重复加载模型。

## 最新正式测量检查点

- 随机 Figura 正式单线程测试失败：script:7 vanilla_model.ARMOR 为 nil；G06 已先登记 Minimum-gaps.md，未实施，不重复模型探测，不关闭脚本或换样本。日志 random/render/cases/random-figura-single-java-cpu-1.log；失败无成绩。
- 已确认只有 PLAYER/ALL/内外皮肤组，没有盔甲几何；不得把 ARMOR 映射为 PLAYER。下一步先对照 OMP 无装备组语义，再做最小适配与无模型测试。
- 本次可靠额度查询：周 used38%，remaining62%，未达到交接阈值。

## 当前有效检查点：缺口表 G01–G05 已补齐

- 以 docs/preview/Minimum-gaps.md 表格为当前范围与状态依据。G01–G05 均已实现并有专项通过记录；下方历史条目中的“G05 尚未实施”已被本检查点取代。
- 最新构建和 PreviewLuaMetatablesTest、PreviewLuaFilesTest、PreviewLuaClientTest、PreviewWheelTest 均通过（test-g05-*.log）。G05 使用真实元表及 FiguraVec3，未加载模型文件或启动模型窗口；最新真实模型端到端仍未验证。
- 当前 main；预览器阶段尚未提交或推送。保留已有工作区修改，主 M3 暂停。随机单线程正式 benchmark、报告和最终阶段检查仍未完成；候选宿主 API 不自动实施。
- 本次仅核对并交付缺口表；未新增功能、抽样或模型加载。额度未重新读取，不推断当前余额。

## 最新范围控制：先列缺口表，逐项完成即记录

- 用户要求“先总结当前最小所需缺口并记文档，再补充”，随后要求“缺口通过表格列出；完成补齐一个就表格记录一次”。已建立docs/preview/Minimum-gaps.md并从主计划链接，统一G01..G05状态、最小范围和完成/验证记录；新候选另表待确认，不自动扩成必补项。
- G01 listFiles、G02客户端版本读取/真实比较、G03实际模型/动画NBT、G04默认隐藏编辑器标记误判已完成并记录专项证据；不把专项测试写成全部真实模型通过。唯一当前必补是G05公共元表导出及type/pairs/ipairs语义，测试已RED，生产修改尚未实施。每项GREEN后立即更新对应表行再继续。
- PreviewLuaMetatablesTest已创建，以内存中的实际Animation/AnimationAPI对象验证类型、元表拦截、__type/__pairs/__ipairs和实例隔离，不加载任何模型文件。已运行RED：Lua类型断言失败，build/preview/test-metatables-red.log。
- 已读OMP FiguraLuaRuntime现成type/pairs/ipairs封装与实际LuaTypeManager.dumpMetatables/getTypeName；Animation/AnimationAPI已保留LuaTypeDoc，现有dump可直接导出真实表。最小实现拟在PreviewLua安装原基础函数封装并于注册类型后更新真实元表，不改模型/生产运行时。尚未写该生产增量，继续前按G05范围执行。

## 最新用户规则：补缺期间每类模型只抽样、加载一次

- 已按用户最新明确要求追加根AGENTS.md：“补齐可选预览器缺口过程中，每类模型仅可抽样一次、加载一次……复用已选样本及既有加载结果，不得因逐项补缺而重复抽样或重复加载模型。”writing-for-agents已在本会话读取并应用。立即停止为了逐项Lua补缺重复加载真实模型；不重新抽样，不对两个新复杂Figura逐个追加加载。后续以已有样本、已知调用路径和不加载模型文件的最小公共接口测试推进。
- 最新产品增量：PreviewAvatarData已把实际保留的bbmodel模型树和完整动画/timeline metadata转为每次新Lua表，供avatar:getNBT；PreviewFiguraParts保留modelRoots，FiguraLoader接原parsed.modelNbt，PreviewLua.attachFigura注册avatar。对照OMP AvatarAPI/NbtToLua语义，无伪造空animations；NOTICE已记保留数据范围。
- 新PreviewAvatarDataTest在旧JAR RED，修复后GREEN（实际动画数、metadata→animations lookup/长度/timeline、模型树与返回表隔离）。canonical Build PASS，JAR已更新。session14064已exit1：sickle越过getNBT，下一已确认失败为GSAnimBlend:328 figuraMetatables.Animation缺失；脚本实际还访问AnimationAPI表。此真实模型再验证发生在新限制之前，之后不得再为每项修复重复加载样本。
- 已读OMP FiguraLuaRuntime构造里types.dumpMetatables(figuraMetatables)以及loadExtraLibraries的type/pairs/ipairs原封装；本预览器LuaTypeManager已有dumpMetatables/getTypeName。下一最小公共修复是导出现有实际Animation/AnimationAPI元表，并按实际库需要复用原type/遍历语义；用已构造对象的接口测试，不加载模型文件。尚未实现元表增量。
- 当前无已知活动构建/benchmark；宿主采样session47478已确认exit0。随机单线程正式矩阵、报告/最终审计、preview提交推送尚未完成；主M3暂停、目标active。rootAGENTS存在，尚未提交。最近可靠周used37%/remaining63%。

## 最新检查点：公共最小修复与根AGENTS规则已写入

- 用户要求将“独立预览器缺失能力的最小补充原则”写入agents.md，已使用writing-for-agents技能，原样写入根AGENTS.md（此前不存在）。这是最新明确指令，覆盖旧“预览器完成后再写根AGENTS”的顺序限制；这里只写用户规则，预览器正式用法仍待交付后补充。文件内容复核与no-index空白检查无问题；新文件diff返回1是正常差异。尚未提交推送，当前preview仍未完成。
- 用户明确要公共修复：只改共用Lua/可见性/适配接口，不按模型名分支，不改模型资产。脚本修复优先参考OMP现成生产实现，Minecraft环境状态用显式配置值。
- session24778已exit1：listFiles实际测试GREEN，sickle下一失败为GSAnimBlend:26 client.getFiguraVersion缺失。已从OMP_26.3原样复用纯Version.java，新增PreviewClient.getFiguraVersion读取states.client.getFiguraVersion显式字符串、compareVersions真实使用OMP版本解析/比较。PreviewLua注册client。PreviewLuaClientTest RED→GREEN（版本、预发布/构建元数据、非法值和缺配置诊断）。随机sample-2配置已显式0.1.5。canonical JAR已重建，Build PASS。
- session28580已exit1：client专项GREEN，但sickle下一失败为GSAnimBlend:267 avatar:getNBT().animations，见random/figura-window-probe-after-client.log。不得声称随机Figura已完整通过。没有关闭其脚本或换样本。当前没有有效benchmark在运行。
- 已查OMP AvatarAPI.getNBT直接调用NbtToLua.convert(avatar.nbt)，NbtToLua的实际数字/字符串/列表/复合数据转换已读；实际模型只请求getNBT().animations以挂动画timeline。当前PreviewFiguraParts.animations已保留原bbmodel parser的CompoundTag完整动画metadata，FiguraLoader.read也可保留实际parsed.modelNbt。下一最小实施应把实际解析数据提供给共用avatar:getNBT，不能返回空animations或伪造数据；尚未实施此增量。
- 对酒狐系列默认隐藏做OMP参考核对：临时build/preview/VisibilityReferenceProbe.java基于原parser实际scale通道与OMP常零/动态排除规则对照。01_taisho_maid至12_little共12模型的默认隐藏无遗漏（包括08_sta31条）；第13模型的基础scale表达式本身在独立probe编译时Non closed expression，不能把后续模型算PASS。无需因此扩张修复无关表达式，日志visibility-reference.log保留。此前公共bezier_linked元数据误判已修并有RED/GREEN；此轮未发现新的同系列缺隐藏规则，不编造新根因或扩大结论。
- 还未将LuaFiles/Client专项加入Test-Preview.ps1常规入口；后续实际公共接口修复后运行必要回归与真实窗口，再继续随机单线程正式测量/报告，预览器完整交付后提交推送→额度→AGENTS用法→主M3单线程优化。最新可靠额度周used37%/remaining63%，无Luna、目标active。保留所有既有M3及其他工作区修改。

## 最新检查点：用户要求随机抽样、单线程优化（覆盖下方旧批次状态）

- 用户最新：优先单线程；多线程多次慢于单线程可暂停。六组已完成CPU数据明确native8/16均低于native1，已停止后续多线程。PID5140的figura-same-20-native-gpu-8按当前case/命令行确认后正常WM_CLOSE，JVM退出0，采样不足标stopped，不计成绩；session17609 exit1（预期帧数验证拒绝），不是渲染崩溃。缺多线程组合SKIPPED（用户决定）。
- 用户随后要求模型随机抽取，并补了两个复杂Figura：苍梧吟月Pro.zip、wangshu.zip，已在Figura候选池内。旧手选单线程续跑session73687已正常中止：PID17268 new-same-50-native-gpu-1正常WM_CLOSE、退出0，metadata stopped，不计成绩；该session exit1为采样不足。旧数据只作为辅助基线，不冒称随机样本。
- 新tools/preview/Sample-PreviewModels.py：固定seed42，按输入类型分层均匀抽样、casefold排序，完整候选池/选中结果与SHA256保存；识别YSGP及UTF8 BOM前缀、目录/ZIP内容。当前池35 YSM目录、18加密、8 Figura输入（含新增两复杂ZIP），YSM ZIP池0。选中08_sta、_创伤小组_霜狐.ysm、sickle.zip；首次临时识别漏BOM和无avatar.json的输入，已在任何随机测量之前修正，不用失败换模型。
- 清单build/preview/benchmark/random-sample.json，副本及三个配置在build/preview/benchmark/random/sample-{0,1,2}.json。states显式提供常用玩家快照、ysm.food_level/head_yaw/head_pitch等；YSM真实idle，Figura原脚本开启。两随机YSM Java/native单线程几何/贴图/资源/实际idle palette对照PASS（random/parity-*.log）。Figura首次真实窗口在EZAnims:97 listFiles=nil失败，保留figura-window-probe.log；未换样本或关闭脚本。
- 用户明确脚本修复可参考OMP并最小移植。已对照OMP_26.3自有CodeGraph与FiguraLuaRuntime.listFiles（264..294），给PreviewLua接实际Program.sources枚举，保持原PathUtils/递归/相对模块/返回点分名称规则，无MC状态机、无宿主文件系统扫描。新PreviewLuaFilesTest先RED后GREEN。canonical Build已PASS，listFiles专项PASS。正在exec session24778末尾真实sickle30帧再验证：必须先轮询24778确认最终结果。截至最后已见Build、listFiles GREEN，未知真实模型下一结果，不得猜测PASS。
- 新增代码：PreviewLua constructor一个listFiles binding、tests/PreviewLuaFilesTest.java；NOTICE记OMP来源。Sample-PreviewModels.py与Write-PreviewReport.py已python -m black格式化。该listFiles是最新产品增量，JAR已重建；其前已完成的正式数据为旧产品辅助数据，后续随机正式测量使用新JAR。尚未把LuaFiles专项加到Test-Preview.ps1常规入口。
- 两参考库已拉取并建图成功：refs/OpenYSM@a515d44686af77155a311b2a592327ca5d45a658，1136文件/21933节点/50382边；refs/Sparkle-Morpher@2e9dddadb09f74ae1456714fcfa118bc1d8c965e，1939文件/42046节点/110164边。各projectPath查询成功，refs被主仓库忽略，不提交第三方源码/索引。无Minecraft启动。
- Measure-Preview.ps1默认Threads=1（Java1/native1 CPU/GPU），显式-Threads 1,8,16可恢复，-SkipCompleted按complete案例ID复用；曾参数Threads/循环threads大小写冲突已改caseThreads。当前没有有效benchmark batch在运行。宿主只读采样session47478最后active=[]，应自退出30秒后再poll确认；旧session96509已exit0。不要把旧17609/73687当活着重启。
- docs/preview/Benchmark-Parse/CPU/GPU.md已生成旧辅助结果，report生成器支持stopped/failed排除、SKIPPED说明及宿主进程快照；随机正式结果/完整剩余多模型档位尚未完成。既有正确性Test-PreviewMulti已验证两类型五档共享/隔离/可见/剔除等，不冒称其12帧是正式性能成绩。
- 目标active，preview尚未交付/commit/push，主M3暂停。最新可靠周used37%/remaining63%，未达阈值，无Luna。继续最小实际脚本修复与随机单线程基线，不能借新模型重建Minecraft状态机/全API。交付后按顺序提交推送→额度→writing-for-agents写根AGENTS用法→主M3单线程优化；保留所有既有M3/.codegraph/.omc/hs_err修改。

## 最新用户补充：拉取优化参考库并生成CodeGraph

- 用户已授权拉取https://github.com/OpenYSM/OpenYSM.git与https://github.com/AIH-Interactive/Sparkle-Morpher.git并为各库生成CodeGraph，作为优化参考；放到现有refs/OpenYSM和refs/Sparkle-Morpher。已确认两目录尚不存在且被主仓库gitignore忽略；codegraph.cmd可用，路径C:/Users/eryth/AppData/Local/codegraph/current/bin/codegraph.cmd。
- 为避免扰动正式测量，等当前benchmark batch结束后执行clone及索引；必须实际完成，不仅记录计划。记录远端、提交SHA与索引验证结果；不因新增参考库扩大预览器实现或重新测试新模型。
- 当前批次：session36914已exit1，原因mixed-5继承YSM attacked clip给Figura；只需在Figura modelOptions.animation.clip显式设空，已修配置。失败metadata保留未计成绩。session83820重跑mixed-5八组合已exit0。现72有效正式案例及1失败记录。
- 后续72组合正在session17609执行20/50/100×new-same/figura-same/mixed，各八CPU/GPU组合；最后观察figura-same-20-native-cpu-8已完成。先轮询17609，不重启。session96509为获授权的宿主只读进程采样，每15秒写build/preview/benchmark/host-process-samples.jsonl；检测无active案例30秒后自动结束，需最后轮询确认。
- 已确认沙箱Get-Process只见3个进程，所以早期空列表不能说整机无编译测试。报告生成器已改：无宿主样本标“未完整核查，可能影响结果”；有样本排除本case配置路径对应的测量JVM，再标其他进程。最新宿主样本只见本case JVM。生成器按真实配置写采样条件/graphics/CPU，遇running/validating拒绝，保留failed但不入成绩。尚需等batch结束后实际重建报告验证改动。
- README已整理为当前使用说明，删除大量被后续覆盖的旧未实现声明，详细API仍保留；新Python报告脚本已用现有python -m black格式化。NativeAssetTest新增自动parser预算0测试（仅animated fixture），末尾改按实际输入数报PASS；这次测试源码增量尚待benchmark结束后javac/java执行，不与正式测量并行。未改产品渲染逻辑/JAR。
- 最新可靠额度周used37%/remaining63%，未达阈值。无preview checkpoint commit/push，主M3暂停；全部完成后按既定顺序提交推送→额度→根AGENTS→主M3。

## 最新检查点：2026-11-07 native解析已验证，正式benchmark执行中

- 原目标active；预览器尚未整体交付/提交推送，主M3仍暂停。用户刚询问是否完成，已明确说明功能及专项通过，剩多模型正式矩阵、报告/最终检查及checkpoint。main/upstream仍M2 f51df7d，保留全部既有M3修改；不新增Minecraft状态机。
- native parser已完成接入：PreviewNativeAsset/Bindings、preview_asset_jni.cpp、parser/parserThreads、parse.json以及多模型按源parser覆盖。原生返回已解析几何/动画/controller/资源，Java只绑定和执行宿主，不重解密/解析原容器。C++ core/JAR构建PASS。
- 上个真实new.ysm失败已定位并修复：Java默认隐藏literalScale把bezier_linked:false当动态值，漏隐藏Mood1。原parser忽略该编辑器标记，最小修复同样忽略。test-visibility-metadata-red→green，native测试10输入×1/8/16最终PASS（含new attacked 0/.125/.5实际palette/逐像素贴图/层级/资源）。Test-PreviewParser session84309 exit0，三native预算30帧真实加密模型窗口PASS，build-preview-parser-test.log。额外目录maid idle姿态1/8/16 PASS；缺food_level时先真实失败，直接配置states.ysm.food_level=20后通过，未新增API/状态机。
- 新PreviewParseBenchmark：每种parser预算独立JVM，3warmups+10samples暖缓存load，已测new/maid Java1/native1/8/16，Figura Java1。原始build/preview/benchmark/parse-*.csv；native核心与端到端load分列，当前native端到端未快于Java，不宣称提速。CIM硬件AMD Ryzen7 260，8物理核/16逻辑CPU；自动预算16。解析同期进程采样未完整取得，报告明确可能影响结果。
- 新Measure-Preview.ps1正式实窗八组合Java1/native1/8/16×CPU/GPU；新Write-PreviewReport.py分CPU/GPU及Parse输出。1280×720/VSync0/120warmup+5×600sample，固定60Hz动画时钟。每例前后编译测试进程快照，不杀其他会话；所有帧configured=visible=submitted需满足。单模型三真实样本new、maid、AI Hobbyist Founder Public Server共24案例已完成（session86421、21675均exit0）。三个Markdown报告已生成于docs/preview/Benchmark-{Parse,CPU,GPU}.md，当前只是已完成案例，不能称完整矩阵。
- 正在运行：工具exec session36914，六场景new-same-1、figura-same-1、mixed-1、new-same-5、figura-same-5、mixed-5，各八组合顺序执行。最后已观察mixed-1-native-cpu-16完成，进程继续运行。下一步先write_stdin轮询36914，绝不因为上下文/观察超时重启。输入config在build/preview/benchmark，输出render/cases与render/runs。该batch未结束时不要并行编译/跑别的性能例，避免污染。单模型原始cases已留完整。
- 剩余：上述batch完成后new/figura same-model及mixed的20/50/100三档正式案例；maid可只作单模型样本，不把新增全交叉矩阵当门槛。每例八组合，五轮。已有正确性Test-PreviewMulti覆盖五档/共享/状态隔离/两clip/跨路径palette和剔除。正式new场景clips=attacked/death，Figura真实脚本及相位偏移，mixed含new+maid+Figura，各源states/parser独立。源码不再新增宿主兼容。
- 报告生成器读取完成案例，但遇status=running/failed会拒绝：等batch终结后再python tools/preview/Write-PreviewReport.py；期间不要把不完整case标成complete。脚本首次Join-Path拼绝对路径失败已修正为IO.Path.GetFullPath(path,base)；失败metadata在benchmark/cases，正式只读benchmark/render/cases。相对modelOptions键已统一转换绝对路径。
- JAR最新含ParseBenchmark且默认隐藏修复；其后只是test可选config/文档/脚本和JNI格式整理，没有产品逻辑新改动。最近git diff --check通过；新Python/PS脚本仍需最终可读性/空白检查，报告生成器增加按真实config生成采样说明/graphics来源后尚待下一次运行验证。最新可靠周used36%/remaining64%，未达阈值；无Luna。
- 完整preview通过后才检查依赖diff、提交推送确认→额度→writing-for-agents写根AGENTS真实用法→恢复主M3–M6。当前无根AGENTS；不得把未完M3顺便标完成。人工视觉/听音仍MANUAL/PENDING不阻塞；未启动Minecraft/offscreen。需要提交的native最小依赖包含部分未提交M3，最终分清范围。

## 最新检查点：2026-11-07 用户再次确认直接模拟传参

- query.*、ctrl.*、player:isXxx()/getXxx()按配置提供宿主查询值；已核对PreviewMolang和PreviewSimulatedApi实际接线。已删除状态机扩展保持撤回，不新增Minecraft状态机或完整API兼容验收。README中“完整Minecraft动作状态待接入”的过时承诺已删除。
- native解析接入仍在验证，未完成交付：新PreviewNativeAsset/PreviewNativeBindings及preview_asset_jni.cpp已构建，使用native归一化几何/资源/动画/controller，不重新解密或解析原容器；parser/java/native与parserThreads独立配置。
- 最新test-native-asset.log：animated-ysm.json、zip.json、encrypted.json的Java/native 1/8/16对照通过；encrypted-new.json真实new.ysm在geometry/owner/hierarchy/visibility联合断言失败，尚需拆分差异定位。不得声明真实样本native解析等价或正式benchmark完成。新测试PreviewNativeAssetTest.java；日志build/preview/test-native-asset.log。
- main/upstream状态未变，无本次提交推送。主M3暂停，剩余先修当前解析对照，再执行既定性能矩阵和报告。

## 最新检查点：2026-11-07 多模型/峰值内存性能主线

- 目标active，preview未完成，主M3暂停。main/upstream origin/main仍M2 f51df7d，无新增阶段提交推送；所有既有M3改动保留。用户明确宿主query/ctrl/player参数直接配置，不搬Minecraft状态机。已删除的自动动作/模板/手部兼容保持撤回。
- 新PreviewMemory用JDK25 FFM调用Windows Kernel32 K32GetProcessMemoryInfo，PeakWorkingSetSize字节/进程生命周期驻留高水位；含JVM/native/host driver，不含专用VRAM。无核心DLL依赖用于Java基线。FrameLog每帧peak_rss_bytes、摘要MiB，32MiB触页单调测试+Java/native各60帧实窗CSV PASS。Jar当前Windows64/JDK25使用，别做无需求的跨平台fallback。
- 新PreviewMultiWindow/PreviewModelRenderer，Main按scene.type非single路由。same-model models单路径；mixed多路径，各source geometry/纹理/native mesh共享，每实例独立PreviewAnimation.State。scene.clips或modelOptions[path].clips轮换动作，timeOffsetStepSeconds默认0.137；per-source只覆盖animation/scripts/states/clips。多模型轮盘不加额外UI，使用配置控制；单模型原轮盘/设置保留。解析当前仍Java。
- 固定seed与100槽位完整空间相机，数量1/5/20/50/100使用同布局前缀，source集合不变时相机不因count改变。动态palette×owner绑定AABB计算姿态边界/视锥，通过后才Draw；每帧configured/visible/submitted实际数。GPU按每实例Draw区间独立query相加，避免CPU转换/上传空隙；FrameLog modelQueries及4096上限，覆盖不足NA。骨骼计时包含边界/视锥准备。环境/粒子/post_render继续执行，固定动画时钟也用于particle插值/post回调。
- NativeVertices构造新增sharedRuntime重载，混合模型只用一个总CPU预算；逆序关闭sources保证owner runtime最后销毁。Renderer每model一套GL mesh/texture，CPU动态输出buffer重用，GPU不额外复制bindPosition VBO。日志scene.json(uniqueModelResources/geometrySets/textureUploads/nativeRuntimes/布局/stateIdentity)、instance-palettes.json、audio.json(各clip定义关闭快照)。两clip共享同source几何，独立compiled playback定义，不重复GPU资源。
- canonical Build成功。Test-PreviewMulti session54660 exit0：两类×五档JavaGPU + 100实例JavaCPU/nativeCPU8/nativeGPU8 =16个12帧真实Windows窗口；configured=visible=submitted=N、GPU query/peakRSS、同model资源1/mixed2、所有100状态不同、实际多clip/phase变化和跨CPU/GPU逐骨骼palette容差0.0005 PASS。最后布局前缀复核PASS。测试脚本首次count1 ConvertFrom-Json顶层数组被展开，已加-NoEnumerate后全16 PASS；未改弱断言。
- 后补剔除负例独立12帧PASS：cfg5、visible/submitted3（run...171805-003-6104）；Test-PreviewMulti已纳入该第17窗，但本轮未重跑前16，准确证据是16+单独负例。规范正式矩阵必须全部可见；负例只正确性测试。
- 最后仅加audio关闭JSON/help/README，canonical再次Build；mixed100 nativeGPU最终12帧run...172035-884-9616，100实际提交/nativeDraw匹配，3个clip定义audio closedtrue/active0。single JavaGPU最终前一run...171933-616-35324验证新FrameLog/Memory格式；不声称最后小改后全部旧35窗重跑。修改脚本一次断言未匹配clang-format单行for，检查部分写入后补齐；现在源码/JAR一致，无未恢复状态。
- 日志build-preview-build.log、build-preview-multi-test.log、test-multi-*.log、test-memory*.log、test-final-multi-mixed-100-native-gpu.log。当前无活动工具句柄/已知本会话残留进程/强杀，无Minecraft/offscreen；人工视觉/听音MANUAL/PENDING不阻塞。formatter/空白和git diff --check通过。
- 本轮只改tools/preview代码、Test-PreviewMulti.ps1、新MemoryTest、README/计划/交接；NativeRuntime共享只是Java桥重用现有C++runtime，无新C++编译。根CMake等既有M3/preview native变更保留。最新可靠周used35%/remaining65%（本安全点复查），未达Luna阈值。
- 下一最小任务：native解析选择，实际消费已有M2 native parser输出（不能先native解析再Java重解析冒充native路径）；随后真实抽样模型正式parse/CPU/GPU/单/同/混合×五档×Java1/native1/8/16五轮矩阵，保存原始数据、同期编译/测试进程注明可能影响结果（不杀其他会话），独立CPU/GPU Markdown每模型/场景H2表格并返路径。继续只按配置模拟，不新增宿主状态机。Preview完整交付后提交推送确认→额度→writing-for-agents写根AGENTS真实用法→恢复M3；不要因为本轮只是正确性短跑就提前阶段提交。

## 最新检查点：2026-11-07 native骨骼/GPU与直接模拟参数

- 用户最新明确：query.*、ctrl.*、player::isxxx()等都应直接配置模拟，不搬Minecraft状态机。已撤回的自动movement/MAIN/ARM/base/hold/use/swing持续保持删除，禁止恢复。目标active，preview整体未完成，主M3暂停；main/upstream origin/main仍f51df7d，未新增阶段commit/push。
- NativeVertices新增复用direct pose buffers和compose桥：Java只构造局部TRS，C++按topological深度使用现有CpuExecutor组合world，独立并行skin palette/继承visible。Java回填world/palette，YSM骨骼查询/已渲染pivot及Figura savedPartToWorld继续原路径。Figura与vanilla追加按metadata offset/count正确处理，零缩放隐藏通过。不改公共法线/可逆矩阵接口。
- PreviewAnimation.updateBones保留无参Java路径，新增native参数分支；FiguraParts.update与Vanilla/Lua updateVanilla同样透传native compositor。Main按实际native实例调用。没有搬游戏状态；动画/Molang/Lua仍真实执行原模型逻辑。
- 新PreviewNativeDraw：Java维护GLFW/context/shaders/资源，C++用当前GL函数指针实发BindVAO/BindTexture/Uniform/DrawElements。Main nativeGPU真实进入此分支，骨骼native准备、顶点GPU；nativeCPU顶点仍核心SIMD并JavaGL提交。FrameLog末尾追加native_draw_calls并实数校验；线程0为CPU核数总预算，显式1/8/16均可。不是原M4完整设备API完成，不能把此阶段混记主M4。
- core/JAR canonical构建PASS（沙箱外parallel1 /nr:false，build.log/native-build.log）。Test-PreviewCpu session38582 exit0：六个fixture×1/8/16的骨骼/world/palette/隐藏/vanilla数值对照、shader parity、12,288顶点单模型并行通过；20个60帧Windows实窗覆盖YSM/Figura Java CPU/GPU/native CPU/GPU1/8/16/auto，draw/有效线程/计时/清理PASS。最后正常CPU矩阵run范围...165257-768-6872至...165337-279-26272。额外20run CSV/resolved/native_draw一致复核PASS。
- 后续原Test-Preview session64313 exit0：35正常实窗+1预期render错误窗口，所有原模型/轮盘/设置/附件/皮肤/声音/查询/退出测试通过（build-preview-test.log），最后正常run...165556-444-33820。人工视觉仍MANUAL/PENDING，不拿数值捕获当目视验收，无Minecraft/offscreen/强杀。
- 最后响应用户配置原则：PreviewSimulatedApi在既有typed只读player getter外，直接按states.player注册额外isXxx bool/getXxx标量/vec3或属性简写，不再为新状态写宿主类。PreviewVectorTest新增未列在Java白名单的自定义方法/向量断言，MolangTest已验证任意query/ctrl及实例隔离。canonical JAR再次构建与两专项PASS；该纯参数增量之后未重跑55窗，不误称最后增量后全套。相关test-vector.log/test-molang.log/test-simulated-queries-compile.log。当前无活动工具句柄。
- README/计划更新了真实native骨骼/GPU/参数用法与范围。当前parser仍Java：native解析选择、多模型同/混合1/5/20/50/100、共享资源/独立状态/可见与提交数量、峰值内存和正式分模型H2 CPU/GPU报告未完成。下一个最小任务应接native解析选择或多模型/内存主线；不再扩张宿主兼容。正式性能基准尚未执行，不从60帧fixture报加速结论。
- 本轮只改tools/preview的矩阵/GL桥/参数及测试文档，根CMake沿上轮一个preview JNI source接入，无其它M3改动。最后对改动Java formatter/空白与git diff --check检查；周额度最近可靠used34%/remaining66%（本安全点查询），未达Luna阈值。整体preview完成后提交推送确认→额度→经writing-for-agents写根AGENTS已验证用法→恢复M3。

## 最新检查点：2026-11-07 CPU顶点性能主线

- 目标active，preview未完成，主M3暂停，main/upstream origin/main仍f51df7d无本轮提交推送。最新用户要求删除无关Minecraft状态机、用配置模拟，上一检查点删除内容保持撤回，绝不重建自动宿主动作。
- 已新增PreviewCpuVertices Java列主序刚性palette变换，CPU路径动态VBO真实绘制；MODEL_VERTEX统一源码供实际窗口与GPU transform-feedback数值对照，bindPosition独立属性保持fixture颜色，UV不改。GPU路径仍shader变换，CPU不再二次骨骼变换；上传计submit_ms，实际顶点准备计vertices_ms。
- 新PreviewNativeVertices使用已有NativeApi generic model/instance/runtime，canonical mesh按YSM profile（坐标已是block，不是声称源格式YSM）复制一次。新tools/preview/native/preview_vertices_jni.cpp在CMake JNI target仅加一个源，借用已有instance/model及runtime SIMD/持久CpuExecutor，不改公共ABI或pose normal可逆合同；非光照位置palette允许零缩放，UV保持。此为native顶点计算，不是native解析/骨骼/GPU draw。
- PreviewConfig execution.nativeLibrary必填native，路径相对config或绝对；threads默认0，native核心自行逻辑CPU核数总预算，Java0→1；当前Java1/nativeCPU1/8/16/auto、单模型。有效线程回写resolved-config/CSV/HUD，native-build.txt保存core buildId。nativeGPU、多模型仍显式拒绝。Build-Preview将bindings/java源码打入JAR。
- FrameLog按实际implementation/path/threads记录，CPU vertices_ms有值/GPU NA，summary分别cpu-summary.md/gpu-summary.md。概要更新了过期脚本/路径文案；不是正式报告，peak_rss仍NA。README新增实际配置和Test-PreviewCpu.ps1用法，未添加状态机。
- 验证：canonical Java CPU首次shader parity+四个60帧实窗PASS；随后native核心/JAR构建PASS。Test-PreviewCpu session32975 exit0：YSM/Figura12个60帧真实窗口、JavaCPU/GPU和nativeCPU1/8/16/auto、实际shader输出/标签/计时适用/摘要/关闭PASS。末次补12,288顶点48块native1/8/16/auto数值shader parity独立javac/java PASS；产品源码未再改变，不重复12窗。12run CSV与resolved线程一致复核PASS。scoped formatter/空白与git diff --check通过。
- 失败与恢复：首次MSBuild sandbox session14442子节点每30s等待通信，经CIM确认，Ctrl+C结束原handle并确认其子进程退出后，才改sandbox外parallel1/nr:false继续；缺HandleTable.get stage参数编译失败已修复。测试脚本一次null固定time属性需Add-Member修正；一次内外两层Tee同build.log造成文件锁，改外层专用log、内层标准build.log后通过。均无残留句柄/强杀其他会话。MSBuild默认sandbox并行会挂，下次同target用已验证外部单节点命令。
- 日志build-preview-build.log、build/preview/native-build.log、build-preview-cpu-test.log、test-cpu-vertices*.log。最后12窗run范围20261107-164332-315-11756至20261107-164353-514-34236。未启动Minecraft/未用Qt offscreen。人工视觉MANUAL/PENDING，不把数值GPU捕获当UI目视验收。
- 本轮变更：tools/preview/{src/PreviewMain,PreviewConfig,FrameLog,PreviewCpuVertices,PreviewNativeVertices,tests/PreviewCpuVerticesTest,native/preview_vertices_jni.cpp,Test-PreviewCpu.ps1,Build-Preview.ps1,README}；根CMake只为JNI增加该源，保留其已有M3改动。其余M3文件未动。native构建产物Windows Release已更新，Java JAR与当前产品源码一致。
- 下一最小任务：继续native骨骼/GPU路径和native解析选择的必需接入，然后单/同模型/混合1/5/20/50/100与实际可见提交/共享隔离、peak内存及正式CPU/GPU分模型H2报告。脚本/动画继续原模型逻辑与配置模拟，禁止以补全Minecraft兼容为前置。preview完整完成后提交/推送/确认→额度→根AGENTS经writing-for-agents技能写已验证用法→恢复M3。最近可靠周used34%/remaining66%（本轮安全点复查），未达Luna阈值。

## 最新检查点：用户要求撤回无关膨胀（2026-11-07）

- 最新指令优先：用户明确要求无关膨胀全部撤回，并指出可配置模拟传参，不应搬整个Minecraft状态机。停止新增兼容工作，已实际删除额外内建主动作/locomotion自动选择、MAIN/ARM默认模板/Resolver、基础parallel自动播放、持物/使用/挥手状态机；对应5个专项测试、5个实窗入口、格式版本元数据及临时手部配置 API一并撤回。源码净删大量状态机，不只是标注停用。
- PreviewMolang现直接从states.ctrl初始化每实例controller变量；states.query/ysm继续显式模拟标量。删除PreviewBuiltinMovement与自动speed/head/yaw默认推导。PreviewMolangTest改为显式速度/ctrl.run/idle参数，验证无自动推导及实例隔离。模型自身声明controller/动画/Lua继续真实执行；未接builtin明确报错，不伪造动画基准。
- 保留用户明确功能：五类输入、实际动画/Lua、轮盘/设置、附件隐藏、皮肤缓存/vanilla、实窗草地天空/FPS记录，以及已有只读配置模拟适配。原M3改动保留。性能主线仍待CPU/native/GPU、线程、多模型1/5/20/50/100、指标与正式Markdown报告。完整preview完成后才阶段提交推送→额度→根AGENTS→恢复M3。
- 删除11个具体源码/测试/模板文件，清除10个生成class/资源；Build增量classes不能留残余模板在jar。仅本地忽略目录build/preview/rollback-before-scope-removal保存撤回前文件供误删恢复，不进入源码或JAR。NOTICE/README/计划已删已撤回能力说明和检查点，不把旧40窗结果当撤回后结果。
- 撤回后完整Test-Preview session30595 exit0：35正常真实Windows GLFW窗口+1预期render错误窗口及全部数值/资源/退出校验PASS。最后正常run...20261107-163214-707-24896，日志build-preview-test.log。其间最后只删除无用途functionControllers集合字段/写入，随后canonical再次构建和Molang显式配置专项PASS（build-preview-build.log/test-molang.log）。最终JAR检查确认已无删除类与模板文件（空目录不是模板文件）。当前无活动工具句柄。formatter/tracked空白通过。未宣称最后纯字段删除后全套重跑。

- main仍M2 f51df7d/upstream origin/main，无本轮提交推送，所有既有M3修改保留。最近可靠周used34%/remaining66%，未触发Luna；人工视觉/听音MANUAL/PENDING，无Minecraft/offscreen/本轮强杀。后续只围绕用户明确性能主线，不自动恢复本次删除的状态机。

## 最新检查点：2026-11-07 15:21 已跟踪骨骼 pivot

- 本节覆盖下方历史推测。目标active，preview整体未完成，主M3继续暂停；main/upstream仍M2 f51df7d，无本轮stage commit/push。全部M3/preview既有修改保留。最新可靠周used33%/remaining67%，无Luna触发。
- **原接口坐标澄清**：bone_pivot_abs不是实体世界坐标。原YsmRenderer.updateTrackedPivots从identity根遍历，updateTrackedPivot在stopAtPivot后record(-m30*16,m31*16,m32*16)，不含模型整体尺寸/实体/相机变换。PIVOT_ABS read直接取tracked，独立于普通3通道上一完整采样帧输入。首次注册0，只有渲染更新后变成实际值，隐藏/任一scale轴为0的整分支保留旧值。此事实覆盖所有下方称“世界pivot”或“必须前帧snapshot”的错误假设。
- PreviewYsmBones增加PIVOT_ABS独立tracked map，仅查询后跟踪，readonly/live copy保持，hidden祖先/zero-axis传播、有限值更新。Main绘制model前调用State.recordRenderedPivots，复用实际world矩阵transform骨骼pivot（与palette同源），移除根width/height再按原像素轴转换。没有另造骨骼矩阵算法或把局部通道当pivot。仅sample/updateBones不刷新，未注册render后再query也先0，下一render才更新；seek重建。第一人称仍未接入。
- 新PreviewYsmPivotTest数值PASS：原几何X镜像、parent Rz90/nonuniform scale/position、child自身rotation/scale、渲染边界与late registration、saved Struct只读/live（包括evaluation scope内读取最近render）、hidden父级任一轴0保留、恢复更新、模型全局size排除、实例/seek。首次手算遗漏原YsmGeometryParser第47行镜像pivot X，88!=56（test-ysm-pivot-expected-sign-failed.log）；对照源码修正测试预期，未改产品迎合错误预期。最终test-ysm-pivot.log PASS。
- 最终canonical build session56122 exit0（build-preview-build.log），含最新README/NOTICE坐标语义与来源。完整Test-Preview session45390 exit0（build-preview-test.log）：35正常真实Windows GLFW窗口+1预期render错误窗口，清理/退出码通过；新增pivot窗口test-ysm-pivot-window.log，60帧。无Minecraft/offscreen；人工视觉/听音MANUAL/PENDING，不作为受阻项。tracked/scoped空白检查PASS，新Java formatter PASS，当前无活动工具句柄或已知残留本会话JVM。
- 本轮文件：PreviewYsmBones/PreviewMolang/PreviewAnimation/PreviewMain最小接线；新增PreviewYsmPivotTest；Test-Preview/README/NOTICE/plan更新。当前产品源码与构建一致。全preview仍需builtin/controller默认动画、直接YSM Lua部件及既定CPU/native/GPU各组合、多模型1/5/20/50/100与正式分模型Markdown报告。53模型/46函数静态编译0失败是既有证据，不称完整实模播放。
- 下一最小任务：接原YsmAnimationPlayer的builtin state/default clips实际播放。PreviewControllerPlayer.Definition当前显式拒绝state.isBuiltin，不能仅删除guard或伪造空pose；先CodeGraph核对原YsmAnimationPlayer与YsmControllerRuntime的builtin选择、时间、state条件/slots/layers，再复用已有PreviewActionPlayer/ControllerPlayer真实clip采样与模拟状态，添加实际palette/timeline/状态切换/实例隔离和窗口验收。随后直接YSM Lua及既定性能矩阵。完整preview验收后才提交推送确认→查额度→根AGENTS已验证用法→恢复M3，不提前stage checkpoint，不修改Main已验证JVM生命周期策略。


## 最新检查点：2026-11-07 15:13 YSM 数字键鼠与无Lua回放

- 本节覆盖下方历史状态。总目标active，preview整体尚未完成、主M3仍暂停。main/upstream origin/main仍M2 f51df7d，无本轮stage commit/push。保留全部已有M3/preview修改及.codegraph/.omc/hs_err；不擅自清理。
- PreviewYsmInput适配原YsmInputState keys32..348/buttons0..7、ANY-key语义、owner/ready门控与未就绪清空。keyboard arity>=1、mouse=1；不把Figura修饰键或scancode当YSM规则。states.input提供owner/ready/keys/mouse；默认owner/ready=true，guiOpen时默认ready=false，恢复不重按。Minecraft实体identity/overlay/鼠标抓取等由显式模拟门控替代，非游戏真实设备状态。
- PreviewMolang注册keyboard/mouse真实Function；PreviewInteraction保留原Lua update入口并新增Molang host入口，Main通过后者支持无Lua输入。按键、mouse、release-all、input-ready同队列记录/replay，JSONL有ysmInput快照，结束释放后保存最终状态。真实GLFW focus也更新ready。缺Lua时wheel/control操作明确报错，未创建多余LuaVM。原手部动作/equipment模拟保持独立。
- 原YSM忽略GLFW_REPEAT：Operation新增repeat标记，Main保留原action；Figura仍走既有set，YSM不把repeat当新press。测试验证清空后repeat不重按。无Lua配置曾在真实入口被旧Config.validate拒绝，已最小改成interaction需要animation.enabled，非key动作仍要求Lua/control host。测试增加PreviewConfig.load，避免只在绕过配置入口的单元用法通过。
- PreviewYsmInputTest数值PASS：ANY/范围/scancode隔离/owner/gui/ready清空、无Lua实际骨骼palette、JSONL replay与最终快照、物理队列/release/repeat与实例隔离。首次测试javac int0赋Float失败，已改0f；最终compile/test日志通过。首轮完整session15600 exit1为旧配置门槛失败，保存build-preview-test-input-config-failed.log；不得算PASS。
- 最终canonical build+完整回归session3322 exit0，build-preview-build.log/build-preview-test.log：34正常真实Windows GLFW窗口+1预期render错误窗口，清理和退出码全部通过。新增YSM无Lua input窗口test-ysm-input-window.log，60帧记录实际keys75/mouse0/readyfalse及最后清空，脚本有断言。无Minecraft/offscreen，无本会话残留进程需要强杀。人工视觉/听音MANUAL/PENDING，不阻塞自主实施。
- 本轮文件：新PreviewYsmInput.java、PreviewYsmInputTest.java；改PreviewMolang/PreviewInteraction/PreviewMain/PreviewConfig/Test-Preview/README/NOTICE/plan。新Java与修改Interaction格式化；tracked及scoped空白PASS。源码产品与最终JAR一致，NOTICE已包含来源/模拟边界。当前无活动工具句柄。
- 最新可靠周额度used33%/remaining67%，primary10080min/reset1794405624；未达Luna阈值，无Luna。53模型/46函数编译0失败仍为上一轮静态证据，不称全部实模播放。当前整体preview仍需绝对pivot、builtin/直接YSM Lua、CPU/native/GPU各组合和多模型1/5/20/50/100资源共享/隔离与正式分模型Markdown结果。
- 下一最小任务：按原YsmBoneQuery.Channel.PIVOT_ABS/YsmBoneQueryRuntime/YsmModelRuntime.captureBonePivots对照预览器实际矩阵。必须使用已计算的真实骨骼/渲染坐标，保留前帧输入协议，不能拿局部通道或零值冒充世界pivot。先CodeGraph定位原规则及PreviewAnimation.State.updateBones，再加可发现父子/旋转/缩放错误的数值验证和实窗。之后按既定计划推进，不扩大到Minecraft启动。完整preview验收后才提交推送确认→查额度→根AGENTS已验证用法→恢复M3；不提前stage checkpoint，保留Main显式正常JVM关闭方案。


## 最新检查点：2026-11-07 15:03 YSM Molang 粒子

- 本节覆盖下方历史状态。目标active，preview整体未完成，主M3暂停。main/upstream仍M2 f51df7d，无新stage commit/push；全部既有M3及preview修改保留。周used32%/remaining68%，未达Luna阈值。
- 新PreviewYsmParticles按原YsmParticleRequest.parse/emit与YsmParticleRuntime.execute客户端效果门控适配：默认参数、count0单粒子/正数Gaussian、1024上限、寿命tick、单bodyYaw/批量entityYaw、Vec3.yRot符号、abs仍加实体位置但不旋转偏移、速度不旋转、预算consume。原类依赖Minecraft Vec3，故用JOML在独立adapter保留算法，没有添加Minecraft空桩或完整客户端。
- 显式states.player.pos/yaw/bodyYaw，模拟YSM玩家，默认origin/yaw0/bodyYaw沿用yaw。独立每实例seed Gaussian。注册particle/abs_particle真实Function，要求模型effects宿主和原client-effect作用域。支持现有六种真实独立粒子，其余（含Minecraft参数化选项）明确报错；不宣称非玩家或任意registry支持。README/NOTICE已写明。
- PreviewYsmEffects持有particle宿主，Lua存在时复用同一列表；Lua原20Hz单独推进，避免双tick。无Lua的named/controller路径由effects推进，controller虚拟substeps逐步tick。Main通过State.particles提交真实ParticleRenderer而非要求Lua存在。seek/关闭清理旧owner。
- 新PreviewYsmParticleTest PASS真实controller无Lua事件发射/20Hz tick、bodyYaw与entityYaw分歧、abs/速度/寿命/count预算/空ID/未知类型/effect gate、Lua共享列表、实例/seek；之后补同seed Gaussian相同位置速度与非零速度断言，独立javac/java foreground PASS（test-ysm-particle-compile.log/test-ysm-particle.log）。未用仅静态编译冒充渲染。
- canonical构建session14907 exit0含最终README/NOTICE（build-preview-build.log）。完整Test-Preview session81445 exit0，build-preview-test.log：33正常真实GLFW窗口+1预期render失败窗口，退出码和最终清理全通过。新增粒子window run build/preview/fixtures/runs/20261107-150236-512-31472，60帧每帧particles>0/particle_draw_calls>0。补充Gaussian断言未改产品，不再无意义重复全套。人工视觉/听音MANUAL/PENDING，不作为受阻项。无活动工具句柄或已知残留本会话JVM，未需强杀，无Minecraft/offscreen。
- 原真实53模型/46函数资源静态编译probe session28870 exit0（build/preview/function-corpus-particles.log）：failedModels=0，含绫樱.ysm32函数。仅编译覆盖，非所有模型完整播放或benchmark。已有temporary probe仍在build/preview/YsmFunctionCorpusProbe.java，勿提交私有模型字节。
- 本轮修改：PreviewYsmParticles新增；PreviewYsmEffects/PreviewMolang/PreviewLua/PreviewAnimation/PreviewMain最小接线；PreviewYsmParticleTest新增；Test-Preview/README/NOTICE/plan更新。formatter覆盖新Java，tracked与scoped空白检查PASS，所有未相关M3修改保留。
- 下一最小任务：接原YsmInputState keyboard/mouse。原MolangBindings.java:1179 inputFunction：mouse arity1，keyboard arity>=1；须核对原key/modifier/ownership/readiness规则与现有PreviewKeys/PreviewInteraction，再做实际数值及窗口交互/replay测试。其次绝对pivot、builtin、直接YSM Lua及计划内CPU/native/GPU矩阵、多模型1/5/20/50/100共享/隔离/正式独立Markdown结果。完整preview验收后才提交推送确认→查额度→根AGENTS已验证用法→恢复M3。不提前stage checkpoint，不把局部通过当整体完成；保持已验证Main正常System.exit生命周期方案。


## 最新检查点：2026-11-07 14:55 方块/骨骼查询与声明式延迟

- 本节覆盖下方历史状态。总目标active；preview整体未完成，主M3仍暂停。main/upstream origin/main仍M2 f51df7d3d7c6a66b4d571a85ca4649832300214d，无本轮stage commit/push。全部已有M3和preview修改保留，不清理.codegraph/.omc/hs_err文件。
- PreviewBlockQueries已接原relative_block_has_any_tag的有限值/每轴5范围/Math.round(pos+offset-.5)/ANY/namespace规则。显式states.world.blocks与defaultBlockTags，默认模型origin或states.player.pos；缺采样元数据报错，不从草地材质推断。数值、实际controller palette/seek与实窗通过，见test-block-query.log。
- PreviewYsmBones已接原bone_pos/rot/scale：实际采样raw单位/旋转符号、只读live Struct、copy返回live view、求值读取前一完整帧输入、同frame ID冻结、成功完整采样后发布输出。每State独立/seek重建；原版追加骨骼按实际offset排除。名称采用原exact/lowercase，无角色alias。绝对pivot/第一人称/直接YSM Lua部件修改尚未接入；controller虚拟substeps在一个可见帧中维持同一骨骼输入，未宣称任意seek的反馈物理等价。test-bone-query.log和真实窗口通过。
- 新PreviewDeclaredTransitionTest在旧binary RED（test-declared-red.log，零时刻事件1!=0）；原playInSlot与advance确认declared也应冻结。PreviewControllerPlayer为新declared保存beginningTransitionRemaining/deferTimelineAdvance，将已有延迟推进移到function分支外。GREEN覆盖真实runtime、0/.49/.51/.7时钟、零/中间事件、palette与实例/seek；已加入Test-Preview，不改变原模组代码。函数槽现有回归继续通过。
- 最终完整Test-Preview session98229 exit0（build-preview-test.log）：32正常真实GLFW窗口+1预期render错误窗口；成功退出/错误码1及finally最终记录全部通过，无Minecraft/offscreen。此前骨骼完整回归也输出PASS并外部无进程，但其句柄因截断丢失，不伪造exit记录。
- 最终canonical JAR重新构建和声明式真实60帧窗口session57857 exit0（build-preview-build.log、test-declared-transition-window.log），run build/preview/fixtures/runs/20261107-145523-691-11004，frames60、audio closedtrue/active0。NOTICE已包含本轮所有来源与语义说明。人工视觉/听音MANUAL/PENDING；不作为受阻项。tracked和本轮scoped formatter/空白检查PASS。当前无活动工具句柄，无本轮强杀测试JVM；用户持续授权仍只覆盖本会话已确认渲染/最终文件完成、只剩AWT等待的测试JVM，不触碰别的会话。
- 真实语料静态编译session42556 exit0（build/preview/function-corpus-bone-query.log）：53模型/46函数资源，剩1模型失败：绫樱.ysm的abs_particle_body调用ysm.abs_particle。15_kluonoa/18_wedding/21_saint编译通过。仅编译覆盖，非完整播放/benchmark。临时probe build/preview/YsmFunctionCorpusProbe.java可复用，不提交私有模型字节。
- 最新可靠额度周used32%/remaining68%，未触发Luna；正常手册由当前Agent更新。整体preview接受标准尚未完成，不提前commit或恢复M3。
- 下一最小任务：接YSM particle/abs_particle宿主。已查原MolangBindings.java:1161 particleFunction：arity>=1，转owner.getYsmRuntime().particles().execute(context,ctx,args,absolute)；接续先CodeGraph读取YsmParticleRuntime真实dispatch规则和现有PreviewParticles，复用实际粒子状态/渲染，不添加空桩只让编译通过。其后YSM键鼠、绝对pivot、builtin/直接Lua及计划内CPU/native/GPU矩阵、多模型1/5/20/50/100、共享/隔离、正式独立Markdown表格。完整preview验收后按用户顺序提交推送确认→查额度→根AGENTS已验证用法→恢复M3。保留Main入口显式正常System.exit的已验证生命周期策略（下节详述），勿回退到不可靠AWT自动退出。


## 最新检查点：2026-11-07 14:32 控制器函数槽与 JAR 进程生命周期

- 本节覆盖所有下方历史描述。总目标active，preview整体未完成，原M3暂停。main/upstream仍M2 f51df7d，无新stage commit/push；保留全部既有M3/preview修改。最新可靠周used32%/remaining68%，无Luna触发。
- 新PreviewYsmControllerFunctions复用原CtrlBinding上下文门控/arity/loop模式语义，接ctrl.set_animation/reset/set_beginning_transition_length/indicate_reload；loop/play_once/hold常量10/11/12，state continue/break-stop/pause/bypass为2/3/4/5。PreviewMolang host.controllers由每State的PreviewControllerPlayer绑定，seek重建不共享。
- PreviewControllerPlayer接原function槽独立身份（按resolved clip统一alias）、transition长度/reload请求/退休、实际clock/initial-event延迟。重复选择保留播放及voices，reload退休旧owner并重建；正开始时长冻结function clock/events，跨界只推进剩余时间；controller update内创建推迟该步advance；loop严格越界wrap，完成标志跨loop保留。ControllerRuntime按原路径把function播放计入clock/completion。原declared槽时钟冻结差异仍待后续核对，本轮未宣称修复（runtime确实传state.blendTransition，现declaredplay路径仍不使用其开始延迟）。
- 新PreviewControllerFunctionTest GREEN：实际palette/声音、重复选择/重载、reset、开始延迟/准确边界/cancel、别名/loop-once-hold、双controller/实例/seek。源码对照发现retired应不再采样；追加断言在旧binary RED（test-controller-retirement-red.log：-.1696001!=-.124），最终skip retired后GREEN。旧姿态只由原pose layer快照处理；reset新incoming未写通道立即回到下层。不能重新把retired clip叠入新pose。
- **AWT处理策略更新，覆盖旧“不要System.exit掩盖”的笼统交接措辞**：独立JAR入口现在显式拥有自身进程生命周期。PreviewMain.main调用run，成功完成其所有finally/GLFW-Swing窗口/input释放回调/audio关闭及最终文件后System.exit(0)正常关闭JVM（含hooks）；异常打印后exit1。渲染/解析宿主类不退出调用方。没有内部AWT标记修改/Runtime.halt/跳过清理；这修复应用“工作结束却进程残留”的契约，不冒称JDK自动AWT退出内部原因已修复。此选择基于多次main完成后只剩AWT-Shutdown/DestroyJavaVM的实际失败，不是仅为隐藏测试失败。
- 本轮原自动退出两次失败：session35268 PID32244 run.../20261107-141959-761-32244、session59630 PID31280 run.../20261107-142446-710-31280，均interactive-ysm180帧/最终JSON/audio closed写完后AWT等待。各exit-<pid>-threads.log、build-preview-test-awt-<pid>.log和test-interactive-awt-<pid>.log保留，两个session exit-1为失败。按用户持续授权核对CLI/180帧/closed及线程证据后只清理这两个本会话JVM；最终外部CIM已确认两PID不存在。不再重复无证据强调只换关闭顺序能彻底解决。
- 最终canonical JAR构建PASS（session49708 exit0，build-preview-build.log），包含最新NOTICE与实际Main生命周期策略。最终Test-Preview完整PASS（session31298 exit0，build-preview-test.log）：30成功真实GLFW窗口/伴随控件全部退出；另1个真实窗口内故意Lua render错误，确认原始EXPECTED_RENDER_FAILURE、退出码1、audio closed/active0及最终interaction-state。测试脚本最后exit0避免预期负测最后native码污染整套返回码。
- 最后正常function窗口build/preview/fixtures/runs/20261107-143130-556-29732：playCalls1/decode1/active0/closedtrue；负测run build/preview/fixtures/runs/expected-render-failure/20261107-143133-319-31992。无Minecraft/offscreen，人工视觉/听音MANUAL/PENDING；tracked及本轮范围formatter/diff空白PASS。当前无活动编译/测试/探测句柄或已知本会话残留。
- 真实53模型/46函数编译探测session22021 exit0（function-corpus-controller-commands.log）仍2模型失败；15_kluonoa4/21_saint1 compiled，18_wedding已越过indicate_reload下一缺query.relative_block_has_any_tag，绫樱.ysm缺bone_scale/abs_particle。只是编译，不是这些模型完整运行或benchmark。下一相对方块实现参照原QueryVariables.worldTagFunction/relativeQueryPosition（原源码465/515附近），不要引用误命中的BlockTask。
- 下一最小任务：继续明确input/relative-block-query/bone/particle和builtin；核对旧declared槽开始延迟语义；再完成既定CPU/native/GPU、多模型1/5/20/50/100、共享/隔离与正式独立Markdown结果。完整preview验收后才提交推送→查额度→根AGENTS已验证用法→恢复M3。不要提前stage checkpoint或goal complete，也不要把本轮局部验收扩大为全部preview已完成。


## 最新检查点：2026-11-07 14:08 装备模拟与本地 sync

- 本节覆盖下方历史记录。目标active、整体preview未完成、原M3暂停；main/upstream仍M2 f51df7d，无本轮stage commit/push，保留全部M3/preview修改。最新可靠周used31%/remaining69%，无Luna触发。
- 新PreviewYsmEquipment从原CtrlBinding handItem/armor/parseSlot/itemMatches合同接states.equipment快照：ctrl.use/hold/swing/armor、手部别名、empty/$精确ID/#tags/:types、armor限制、2或3参数。Minecraft实体动作/注册表元数据由配置显式提供，不按ID子串猜测，缺必要状态报states.equipment路径。PreviewMolang controller binding保留其他动态变量，只为这4个原保留函数名建立Function绑定。
- PreviewYsmFunctions.recordSync接原本地分支：client-effect gate、最多16个转换后finite float、最近16记录、立即按顺序执行@sync，沿原args/temp/named function机制。没有网络捕获/传输/remote replay；不是无操作sync。PreviewMolang绑定ysm.sync原arity<=16。
- 新PreviewYsmEquipmentTest先在ctrl.use解析失败RED（test-ysm-equipment-red.log），后GREEN：use/swing/hold/armor、别名、ID/tag/type、缺配置诊断和配置快照隔离。PreviewYsmSyncTest GREEN：效果门控、Inf拒绝、嵌套函数/参数作用域、16条历史上限、using true/false→controller→@sync→真实child palette y9/y8、两个实例和seek重建。
- 测试调整事实：sqrt(-1)/NaN原Molang转0，不能用来断言finite拒绝，改为Inf；args.length点访问原parser不支持，改验证实际currentArguments为空。未为了测试修改生产parser或原数值转换。test-ysm-sync-math-assumption.log保留最初失败。
- 最终canonical preview.jar含最新源码与NOTICE/README，build-preview-build.log PASS（session45353 exit0）。Test-Preview增加两测试及1个60帧实窗；最终完整回归build-preview-test.log PASS29真实GLFW窗口及伴随控件清理（session46230 exit0），最后build/preview/fixtures/runs/20261107-140807-972-20380。tracked及本轮范围diff空白PASS。没有Minecraft/offscreen，本轮无残留JVM/清理操作；人工视觉MANUAL/PENDING。
- 真实53模型/46函数静态编译探测session71035 exit0（function-corpus-equipment-sync.log）：15_kluonoa全部4函数、21_saint全部1函数已能编译；剩2模型失败：18_wedding首先ctrl.indicate_reload，绫樱.ysm首先ysm.bone_scale/abs_particle。仅编译证据，尚未执行这些模型全部分支，尤其15还用ysm.keyboard，需要接真实/显式模拟输入；不能据此报告完整模型播放或benchmark。
- AWT退出偶发缺陷仍未修复，前轮PID21424/8794失败和只读诊断继续有效，不能因本轮29窗PASS抹掉。当前无活动编译/测试/探测句柄；不要反复无证据攻击旧问题或加内部标记/System.exit掩盖。
- 下一最小任务：原controller函数指令ctrl.indicate_reload/set_animation/reset/set_beginning_transition_length需要接实际槽位/过渡/重新加载语义，不能只让解析通过；然后按真实代码接ysm.keyboard/input、bone/particle和builtin。之后既定CPU/native/GPU、多模型与正式独立Markdown报告不变。完整preview验收后才提交推送→查额度→根AGENTS已验证使用说明→恢复M3，不提前goal complete。


## 最新检查点：2026-11-07 13:59 函数声音/滤波，AWT 偶发退出仍未修复

- 本节覆盖历史状态。总目标active，preview整体未完成，原M3暂停。main/upstream仍M2 f51df7d，无本轮stage commit/push；保留全部原M3/preview修改。最新可靠周used31%/remaining69%，无Luna触发。
- 新PreviewYsmSoundFunctions改接原YsmSoundRuntime（actual PreviewSounds/OpenAL），保留ID/flags/local/global/anonymous banks及拥有者清理。PreviewYsmFunctions加入嵌套client-effect scope；PreviewMolang接play_sound/stop_sound/stop_all_sounds原参数验证；controller stage/entry/exit和timeline接真实作用域。原budget scope保留，vendor ControllerRuntime execute仅额外打开function scope。scope外声音返回false，global ID依旧按实例隔离，带播放owner的global声音结束时也清理。位置用states.player.pos/模型原点，模型生命周期替代liveEntity移除；未实现联网传输。
- YsmFilterRuntime原样复制；Molang first_order/second_order接原参数默认/PREVIEW域。每模型render frame以elapsed*20更新一次，在参数采样之前；与clip wrap/controller substep时钟分开。Sampler只恢复原beginBone scope并改PreviewMolang类型，保留既有fail-fast采样异常。新filter/source NOTICE已入最终canonical JAR。
- PreviewYsmSoundFunctionTest PASS实际OpenAL计数：ID冲突/replace/local/global/anonymous/flags/缺失声音、scope禁用、嵌套owner、controller stage+entry、双实例/exit/seek、timeline global声音跨loop/seek清理。PreviewYsmFilterTest PASS120帧原算法→实际palette比较、一/二阶、bone键隔离、动画循环单调clock、重复frame和实例/seek。Test-Preview纳入两者及各60帧实窗（共28窗）。
- 最终build-preview-build.log PASS（session50253 exit0），包含新README/NOTICE所描述实际源码；tracked及本轮范围diff空白PASS。最终完整复验build-preview-test.log PASS28真实GLFW窗口及伴随控制窗退出（session43319 exit0），最后filter .../20261107-135845-856-12072；function sounds .../20261107-135843-252-3616（play2、active0、closedtrue）。未启动Minecraft/offscreen。人工听音/视觉MANUAL/PENDING。
- **失败必须保留**：第一次同源码完整回归session8794在PID21424 figura-scripted-parts窗口main返回后AWT退出等待。run.../20261107-135129-729-21424已有60帧/最终JSON，audio closedtrue/active0。exit-21424-threads.log见仅AWT-Shutdown/DestroyJavaVM非守护等待；只读read-agent见frame displayablefalse/visiblefalse、peerMap空、busyThreadSet空、toolkitThreadBusyfalse；timer-agent见Swing TimerQueue空。先前关闭顺序修正未根除此偶发缺陷，不能再声称完全修复，也不能把重跑PASS当根因已修复。
- 按用户持续授权，核对21424命令行、60帧/最终文件和AWT线程证据后只清理该本会话JVM；session8794 exit-1如实为失败。失败日志build-preview-test-awt-21424.log、test-figura-scripted-awt-21424.log及exit-21424-*保留。无System.exit、无内部AWT忙标记修改。诊断代码/JAR只在build/preview与shutdown-diagnostic，不进入发行JAR。diagnosing-bugs技能已用；不反复无证据猜测此问题，后续交付前仍须解决或明确未验证，不能视为已验收。
- 真实函数探测session20079 exit0：53模型/46函数仍4模型编译失败，但已越过原play_sound/second_order位置。新缺口：15_kluonoa的ysm.sync（也用ysm.keyboard），18_wedding/21_saint的ctrl.use，绫樱.ysm的ysm.bone_scale/abs_particle。build/preview/function-corpus-sound-filter.log。只是静态编译探测，不是模型完整播放或benchmark。
- 下一最小任务：按这些明确缺口复用原sync/input/hand/bone/particle宿主（用户允许配置模拟Minecraft依赖），再接builtin、既定CPU/native/GPU/多模型矩阵与正式独立Markdown结果。完整preview验收后才提交推送→查额度→根AGENTS说明→恢复M3，不能提前goal complete。当前无活动编译/测试/探测句柄；本轮结束前外部核对21424是否消失，不以sandbox枚举当证据。


## 最新检查点：2026-11-07 13:42 YSM 函数执行接线

- 本节覆盖下方历史状态；整体目标active、preview未完成、主M3暂停。main/upstream仍M2 f51df7d，无新stage commit/push；保留全部M3/preview未提交修改。周额度最新可靠used31%/remaining69%，无Luna触发。
- 本轮源码：新PreviewYsmFunctions来自原YsmMolangFunctionRuntime named call/args/eventBus/controller scoped dispatch/execute/records，使用真实PreviewMolang而非liveAvatar；原32递归上限与执行预算保留，unknown/解析/执行错误明确传播。复用YsmMolangFunctionName、YsmFunctionEventBus、YsmControllerSlotBinder原文件；TempVariableStorage仅加clearFrame复用数组清理帧数据及作用域。未保留无调用的YsmMolangFunctionParser复制文件（已移除），实际使用原MolangEngine parse。
- PreviewMolang增加fn/args/ysm.event/run_event/trigger_event绑定与动态temp委派；PreviewYsmResources保留ordered function strings（目录/ZIP/binary关闭前拷贝）；PreviewAnimation各State初始化及seek重建functions；原ControllerRuntime恢复state处理前function事件钩子，PreviewControllerPlayer beginFrame/slot/unbound和missingEventControllers原binder接线。PreviewMesh不再拒绝所有函数资源；root functions/action_schemas和builtin等不支持路径仍有原诊断。
- 新PreviewYsmFunctionTest PASS：嵌套参数/临时变量/return、manual handler顺序、recursion/unknown/执行异常后恢复、controller stage→实际child palette、双实例/seek、nestedZIP及binary关闭后function源存活。旧ControllerTest拒绝函数负测改为实际fn.test副作用断言。初次Float固定时间写int导致编译失败，改0f后通过，不冒称最初测试PASS。
- 最终源码/NOTICE/help已重编：build-preview-build.log PASS，session50486 exit0。最终完整build-preview-test.log PASS26真实GLFW窗口及伴随控制窗退出（session20067 exit0），最后build/preview/fixtures/runs/20261107-134149-505-14452。此前同26窗口session33284亦exit0，但未包括最后temp复用修正；以20067为最终。tracked及本轮新增/修改范围diff空白PASS。未启动Minecraft/offscreen，无残留测试JVM，不需清理。人工视觉MANUAL/PENDING。
- 真实函数资源探测：53模型有46个函数，4个含函数的模型均在尚未接线绑定处解析失败，不宣称真实模型函数完整通过。build/preview/function-corpus.log/detail.log为只读加载/编译探测，不是benchmark或播放。15_kluonoa首先缺ysm.play_sound('horn','horn')；18_wedding缺ysm.second_order；21_saint缺ctrl.use('mainhand',':bow')；绫樱.ysm缺ysm.bone_scale/abs_particle。详日志保留原行号，源模型只在refs，不提交。
- 下一最小任务按真实证据：优先复用原YsmSoundRuntime连接ysm.play_sound/stop_sound/stop_all_sounds（现有真实audio底座可用，但原作用域ID与函数effect scope必须保留），然后YsmFilterRuntime first/second_order、ctrl.use等明确可配置宿主及bone/particle。builtin同在计划内，但不要先将所有未接线函数伪装支持。builtin相关原YsmAnimationPlayer.updateNativeStateMachine/usesBuiltinController/reconcileBuiltinSources、YsmPlayerActionState与YsmDefaultAnimationResolver已定位，尚未实现/复制。
- 当前无编译/测试/探测运行句柄。下一阶段仍为既定preview全宿主及CPU/native/GPU、多模型/正式独立Markdown报告，完整preview验收后才提交推送→查额度→根AGENTS说明→恢复M3。没有完成整个preview，不能提前阶段checkpoint或goal complete。


## 最新检查点：2026-11-07 13:30 YSM 音频宿主

- 本节覆盖下方历史状态。目标 active，预览器整体未完成，主 M3 暂停。main/upstream 仍 M2 f51df7d；本轮没有阶段提交/推送，保留所有原 M3 和 preview 修改。
- YSM named clip/controller state/declared action 的 timeline/sound 接真实宿主；原 EventWindow/SoundBank 复用，状态退出/动作停止/循环/seek 按拥有者清理，两个实例共享音频 buffer、播放源独立。sound-only YSM 不创建模型 Lua。时间线实际使用原播放步时钟（0.105 事件跨到 7/60 时验证真实 palette）。嵌套 ZIP/文件夹/二进制音频保留已测试。
- 原 YsmAudioDecoder 复用 Opus 算法；Vorbis 用 bounded STB chunks 替代 Minecraft JOrbis，保留单声道 PCM 语义、输入/PCM 上限及取消检查。Build 加本机缓存 vorbis-java-core0.8、concentus1.0.2。普通 Figura Vorbis 保留 mono/stereo，单独 model buffer cache profile。
- 真实53 YSM资源静态扫描：162声音=158Vorbis+4Opus（build/preview/audio-corpus.log）。这只是保留/格式扫描；未宣称162全部播放。新增 PreviewModelAudioTest：5个原验证样本实际解码/OpenAL上传/mono/48k Opus/PCM上限/缓存 PASS（test-model-audio.log），已加入 Test-Preview。
- 完整回归 session10262 exit0，build-preview-test.log 最终 PASS25真实GLFW窗口及伴随窗退出；最后 .../20261107-132918-159-29328。首次sandbox回归session43576因HttpServer回环权限失败，已获自动审批后重跑，不归为代码失败。期间无残留JVM需要清理。
- 最终 NOTICE 和源码格式化后 canonical preview.jar 重构建成功；session94757最终真实60帧YSM nestedZIP exit0，build/preview/test-final-ysm-sound.log，run build/preview/fixtures/runs/20261107-133017-043-9748。audio.json playCalls2/decodedBufferLoads1/activeSources0/closedtrue。最终25窗用的是同功能源码；新增独立codec测试亦PASS。人工听音/新效果视觉 MANUAL/PENDING，未以自动检查冒充人工验收。
- 最新可靠额度：周 used31%/remaining69%，未达Luna阈值，无Luna。无运行中编译/测试句柄。最终空白检查状态以本节后的追加记录为准。
- 下一最小任务：继续计划内真实 YSM function/builtin 宿主（仍明确拒绝，不添加空实现），再推进既定CPU/native/GPU、多模型1/5/20/50/100及独立Markdown报告/峰值内存/资源共享和隔离验收。完整preview完成后才按用户顺序提交推送、查额度、根AGENTS使用说明，再恢复M3。不要重复皮肤/设置/声音已完成实现。


更新时间：2026-11-07。总目标仍active：完成剩余阶段，完成交付单元后提交/推送/确认/查额度；用户最新明确调整顺序，先写预览器独立计划、暂停原M3收尾流程，优先实现预览器。不得擅自恢复原顺序，不丢失任何未提交修改。

## Git 与当前优先级

- main，上游origin/main；M0已推送5ed8bfc，M1已推送a81e288，M2已推送f51df7d3d7c6a66b4d571a85ca4649832300214d（已核对远端）。
- M3与preview均有未提交修改，当前没有新的阶段提交。保留.codegraph/、.omc/，不提交；build/dist/logs/refs同样不提交。
- 当前第一入口是docs/preview/Preview-plan.md，主计划docs/OmniMorph-native-plan.md已明确用户优先级覆盖原阶段顺序。其余阶段未完成，不标记整个目标complete。

## 预览器当前实际状态（覆盖下方历史描述）

- **最终覆盖：皮肤与退出验证已收尾。** canonical build/preview/preview.jar最新构建成功（build-preview-build.log），Main标题已改实际animation.description。最后完整Test-Preview PASS共16真实窗口及伴随窗正常退出，日志build-preview-test-close-order.log末尾PASS，最后vanilla run `build/preview/fixtures/runs/20261107-115533-052-30216`；90帧draw在0..29/30..59/60..89确认为3/2/3。独立同检查还通过于...114348-625-11940。skin用户563省略URL90帧最终独立JAR验证...skin-563/20261107-114507-090-19036 exit0，缓存hash/Alex一致。默认preview.jar已经恢复可构建；preview-skin.jar/preview-close-order.jar只是调试中避开锁的旧/中间产物，不当最终入口。
- AWT退出诊断事实：旧完整回归在interactive窗口主流程结束后JVM未退出，jcmd见DestroyJavaVM与AWT-Shutdown；只读agent快照显示frame non-displayable、peerMap空、busyThreadSet空、toolkitThreadBusy=true。独立5窗和3次子进程测试均可PASS，不能当成稳定红回归；相关源码已移build/preview/shutdown-diagnostic/（不进入正式tests或JAR）。Main实际生命周期现将PreviewInteraction拥有权移到GLFW外层，先glfwDestroy/Terminate，最后close Swing；随后同一完整16回归正常退出。干预结果支持关闭顺序假设，未虚构原生触发细节或使用System.exit/内部busy标记强制清除。
- 清理授权已完成：两次自动审批先拒绝强杀（第一次未获用户针对AWT清理授权，第二次认为原授权只限2472）；用户之后明确允许本次验证中由我创建、经日志/命令行确认渲染及文件写入已完成仅剩AWT等待的测试JVM清理，不处理其他会话。2472、26120、30960均在再次核对CLI后按授权清理；最终outside-sandbox CIM只读确认三PID不存在。相关失败session7641/30551/31971均以-1如实保留，不标PASS。当前无运行中测试句柄/相关窗口；node_repl已reset，后续Computer Use需重新import sky。
- 原始sandbox Get-Process不可见外部escalated JVM，**不得再把sandbox枚举空当作外部进程已退出的证据**；前文“Java30108不存在/只是wrapper等待”推断证据不足，以此条覆盖。未来诊断用已授权只读外部CIM+jcmd、核对本任务CLI，不触及他人会话。
- 最新可靠额度：周used29%/remaining71%，未达Luna阈值，无Luna。整体preview尚未完成（native CPU/GPU/multi/report/full host等保持原要求），主M3继续暂停；main仍M2提交，无本轮stage commit/push。本轮进展为world/math、真实Figura回放、新用户皮肤/缓存/auto模型完整实施与验证；下一最小任务恢复真实模型其余宿主/分支和既定preview验收，不重复实现皮肤功能。

- 最新用户新增已实施：Lua没隐藏原版模型则真实绘制带皮肤玩家；指定测试URL https://skin.aihmc.top/raw/563；首次请求持久缓存、以后URL可省略；按皮肤像素自动Steve/Alex。PreviewConfig新增vanilla.enabled默认true、skinUrl空、skinCacheDirectory空、model=auto|steve|alex。PreviewMesh.load包装raw load：非fixture的Figura或scripts.enabled的YSM附加vanilla；PreviewVanillaModel生成12个实际内/外层body parts、UV/skin材质，PreviewSkeleton合并元数据，Lua附独立实例vanilla_model，动画完成后覆盖对应palette；Main所有vanilla部位隐藏时真正skipDraw，并按实际循环累计drawCalls。原版姿态当前neutral+Lua变换，不宣称完整Minecraft动作/盔甲/披风。
- PreviewSkinCache按URL SHA作为PNG文件名，JAR相邻skin-cache默认（classes运行亦build/preview/skin-cache）；读取last-skin.json省略URL复用；成功验证PNG后写metadata含内容hash，不重复HTTP。配置可隔离cache目录。皮肤限制64x64PNG、HTTP(S)、4MiB和timeout；没有silent fallback。窄臂4组unused透明strip检测，测试原Steve/Alex通过，model强制覆盖可处理像素不明确情况。Build从同本地MC JAR增加wide Steve/slim Alex两默认PNG，共6贴图并校验源hash，仍不启动Minecraft。
- 新PreviewVanillaTest已PASS：12part真实几何、default/per-part/group/instance状态、YSM/Figura实际palette、auto pixel模型/显式覆盖、local HTTP请求计数1、停止服务器仍可省略URL复用缓存。初次本地HttpServer被sandbox loopback权限拒绝，已用已授权escalation跑过；不是代码问题。Test-Preview已纳入该测试与90帧default→hide30→restore60的DrawCall3/2/3检查；完整16实窗最终结果需核对当前session7641/最新build-preview-test.log。旧15实窗在未加cache最终功能时已PASS，不能混为最终16结果。FiguraLoaderTest与PreviewFiguraPartsTest各自明确关闭vanilla以保持其仅原模型几何/层级测试范围，新增VanillaTest负责默认on要求。
- 用户563真实皮肤：Web工具无法打开二进制URL，sandbox DNS拒绝后escalated Invoke-WebRequest成功，build/preview/test-skin-563.png 1103bytes。实际JAR用skin-563.json首次URL下载并缓存，SHA256 07998a9f752d30c35e84df9564f224c4a16b1c9f01a486a27389d29646bc32e1，model=alex。Computer Use skill及guidance/api/confirmations已重读，node_repl @oai/sky真实主窗口确认贴图/FPS/草地/天空，伴随窗口中文隐藏/显示正常；实点slot1隐藏frame9585使draw3→2、slot2恢复frame14005使draw2→3，见build/preview/runs/skin-563/20261107-113136-311-30108/interaction.jsonl。关闭主窗口后sky列表空、Java PID30108不存在；escalated exec宿主session90517未收尾，确认Java结束后Ctrl-C关闭wrapper（wrapper exit1，不冒称原始session exit0）。
- 第二次明确省略URL、无网络escalation执行skin-563-cached.json 90帧exit0：build/preview/runs/skin-563/20261107-113428-597-17544，model.txt显示同一URL(cached)/同hash/Alex，证明真实缓存恢复。缓存用户URL在build/preview/skin-cache/，不提交PNG/缓存/refs。当前node_repl.sky已有绑定但所有相关窗口已关闭，不用旧截图坐标。最新源另修标题硬编码YSM→实际animation.description（尚需最终重编），不要宣称旧截图标题已正确。
- 原长期目标仍active；本轮用户皮肤特性及world时钟等进展属于progress，整体preview/native/multi/report仍未完成、M3继续暂停，无新stage commit/push。最终收尾需核对完整16回归、重编最后标题文本变更、更新计划/notice与本手册准确状态和额度，保留全部预存M3修改。

- 本轮最终安全检查：周额度used28%/remaining72%，未达Luna阈值；tracked diff和当前preview源码/测试/配置/文档/新math.lua空白检查PASS。无运行中测试句柄、无新阶段提交；当前下一步仍为真实模型其他分支，不是恢复主M3。保留全部现有工作区。

- 最新2026-11-07 11:18：PreviewWorldClock接world.getTime/getTimeOfDay，按owner实际tick计数+显式delta（dot/colon均支持），states.world.time默认0，dayTime默认time且不取模24000；base要求signed64整数。不是从可被动画子步回退的animationClock重新floor。原math.lua从refs/Figura_Orig原样复制到tools/preview/resources/assets/omni-morph/scripts/math.lua，注册原vectors后加载，提供math.lerp/clamp/map/round/angle/sign等。vec别名由原Lua资源定义。无fake world block/entity API。
- 新PreviewWorldClockTest PASS：原tick/render delta、dayTime不wrap、scalar/vector math、实际palette、seek重建、实例隔离、默认clock、fractional base负测。生成figura-world-clock.json固定.125秒。最终Build及Test-Preview PASS，完整15个真实窗口，最后worldclock `build/preview/fixtures/runs/20261107-111619-577-16348`；build-preview-build.log/build-preview-test.log（末尾PASS）。无Minecraft/offscreen，所有run正常退出，无运行中测试句柄。
- 真实Figura进展：host-probe先确认world修复后script:79缺math.lerp，再加载原math.lua后init/t=.25/t=5都成功；.25时88个drawable owner矩阵相对初始改变且finite。build/preview/FiguraHostProbe.java、host-probe.log；states仍host-probe-states.json显式loaded=true/idle玩家值，没有用loaded=false绕过。真实JAR + Swing伴随窗口120帧exit0：`build/preview/runs/real-figura-clock/20261107-111659-731-29992`，build-preview-real-figura-clock.log、配置build/preview/real-figura-clock.json。实际mesh16264vertices/20364indices、195parts、4Lua sources、0关键帧clip（此模型由Lua驱动tails），5drawcalls，CSV120行动画/骨骼/GPU非NA；不是性能benchmark成绩/完整视觉验收。
- 下一最小任务：继续真实Figura其他状态/轮盘分支与ZIP输入，按真实缺口补partToWorldMatrix/particles/sounds或必要host；相机、world/block/entity、weighted armature、动态bbmodel Molang及full controller/settings持久化/native CPU/GPU/多模型报告仍未完成。当前模型120帧没有证明这些分支，人工视觉MANUAL/PENDING。整体preview未验收、M3继续暂停；无新stage commit/push，原M3所有修改保留。目标active，本轮分类progress。

- 本轮最终收尾：安全检查点额度周used27%/remaining73%，未达Luna阈值；最终tracked及本轮源码/测试/配置/构建脚本/文档/新增keybind vendor空白检查PASS。最后仅格式化Main新增回调及去除复制文件行尾空白，未改变运行逻辑；无运行中测试句柄，main仍M2提交，整体验收前不checkpoint。

- 最新2026-11-07 11:12：复用原FiguraKeybind/KeybindAPI，owner→PreviewLua、InputConstants→PreviewKeys真实GLFW标识符；原press/release先回调后更新isDown、重复边沿、modifier/binding参数、override、enable/gui/key别名/字段赋值保留。原MC polling改GLFW keyboard/mouse/focus回调→PreviewInteraction队列（render线程），支持key-down/up/release-all及scancode/modifiers，记录/重放/快照keybinds，失焦/close释放。PreviewLua注册API、states.keybinds初始布尔值、states.guiOpen门控、states.vanillaKeybinds显式映射。预览器Escape/R/F1仍独立快捷键，显示名称为键标识符；未把MC翻译/vanilla控制伪造出来。
- 新PreviewKeybindTest PASS：keyboard/mouse/scancode→实际palette、原callback-before-state/重复按下/释放边沿、modifier与override、禁用和GUI门控、初始held/vanilla映射、name roundtrip、JSONL重放最终snapshot一致。Test-Preview增加keybind测试/60帧实窗，最终整套14真实窗口PASS，最后run `build/preview/fixtures/runs/20261107-111024-867-17968`，build-preview-test.log末尾PASS。build-preview-build.log最终Build PASS；无Minecraft/offscreen。原生物理键盘/鼠标人工操作MANUAL/PENDING，不宣称自动replay验证了OS注入。
- 发现并修正测试依赖选择：build/preview/lib保留旧LWJGL3.3.3和当前3.4.1，原test wildcard触发GLFW静态初始化时误选旧版本并输出JNI不匹配。Build-Preview新增classpath.txt写入实际固定$jars；Test-Preview编译/运行均读精确classpath。旧JAR保留未删除。最后Keybind与整套测试无该错误，实际JAR窗口一直使用manifest当前依赖。
- 真实模型探测进展：build/preview/FiguraHostProbe.java支持可选states JSON，显式player loaded=true/health/maxHealth/pose/velocity/pos/lookDir及状态false（build/preview/host-probe-states.json）；AI Hobbyist Founder Public Server目录现在init及t=0 render成功，t=.25 tick明确缺world.getTime，build/preview/host-probe.log记录。不能宣称完整脚本播放已通过。下一最小任务补实际模拟world时钟，再根据真实探测接下一宿主；勿用loaded=false绕过真实行为。
- 当前goal回合progress；整体preview未验收、主M3继续暂停，无新阶段commit/push，保留所有旧M3与用户修改。完整controller、动态bbmodel Molang、部分part/camera/world/entity/particle/sound宿主、weighted armature、settings persistence、native CPU/GPU/多模型/报告仍待完成。最终安全检查点查额度并按可靠值更新，不触发无依据Luna交接。

- 本轮最终检查：tracked diff、自有源码/测试/配置/文档及本轮新增数学vendor空白检查PASS；最后仅去除复制文件行尾空白，无运行逻辑修改。安全检查点可靠额度：周窗口used27%/remaining73%，未触发Luna。无运行中测试句柄。本轮目标回合分类progress（原向量矩阵/玩家模拟实施、13实窗和真实脚本缺口证据）。

- 最新2026-11-07 11:04：复用原Figura vector/matrix八类及VectorsAPI/MatricesAPI。移除只用于MC BlockPos/Vec3的转换，保留JOML；pure LuaUtils参数抽到PreviewVectorArguments（local Pair），pure MathUtils/ColorUtils到PreviewVectorMath（threadlocal formatter）。原数学/运算符/swizzles/矩阵行为保留，named-color含原Figura及标准颜色；两个camera-dependent向量方法明确Unsupported。PreviewLua注册vectors/matrices/vec，local pings实际Lua回调（不联网）。Part setter改Object+原parseVec规则，getter返回独立原Vec3，新增getChildren。
- 新PreviewSimulatedApi只读player函数按states.player完整方法名或property名读取显式typed配置（例如sprinting/velocity/pose/health/loaded）；无缺省假状态，缺失/错类型/非finite抛明确键名，每次返回fresh向量。PreviewVectorTest验证原向量代数/字段/swizzle、矩阵变换与inverse、player类型和返回隔离；PreviewPlayerAnimationTest验证配置→player/vectors→实际palette、local ping/轮盘、不同配置与实例隔离，生成figura-player-simulation.json；PreviewFiguraPartsTest改用vec参数仍PASS。
- Build/Test-Preview完整13真实窗口PASS（build-preview-test.log末尾PASS；最后player run `build/preview/fixtures/runs/20261107-110158-630-8796`），无Minecraft/offscreen。随后仅补named-color兼容，最终Build及强化PreviewVectorTest再次PASS，build-preview-build.log/build/preview/test-vector.log。初次自动提取转换方法的正则误把FiguraVec3也当MC Vec3删掉导致编译失败，已按词边界修正并重编通过；临时adapt脚本仅在build/preview，不属于常规构建且不要重跑覆盖手工收尾。
- 真实Figura脚本探测：build/preview/FiguraHostProbe.java运行AI Hobbyist Founder Public Server目录，init在script:17缺keybinds.newKeybind失败；build/preview/host-probe.log。这是已确认下一所需依赖，不是完整模型通过。静态扫描其脚本还需player、world时间、partToWorldMatrix、particles/sounds等；禁止用空宿主通过。最小下一步实现真实/配置可重放的keybind宿主，再根据新诊断接实际所需API；不把所有周边MC API无条件加入范围。当前手工视觉项MANUAL/PENDING，不受阻。
- 总目标active，主M3继续暂停；整体preview验收未完成，无新阶段commit/push；原M3及全部用户未提交修改保留。native CPU/GPU、多模型场景/资源共享/隔离/报告及full controller等要求不变。额度查询在本轮最终安全检查点执行；未触发Luna。

- 最终日志收尾：Test-Preview最终PASS消息从Write-Host改为Write-Output，使外层Tee日志也包含最终结论；再次完整12实窗PASS/exit0，最后run `build/preview/fixtures/runs/20261107-105441-246-20516`。此前日志停在预期非法running负测堆栈，是Host输出绕过外层Tee，不能据此误判整套失败。当前build-preview-test.log末尾已明确PASS；无测试遗留运行句柄。

- 最终核实2026-11-07 10:54：包含动画snapshot新增字段的最终Build/Test-Preview均PASS，完整12次真实窗口正常退出；最后Figura关键帧run `build/preview/fixtures/runs/20261107-105315-760-28168`，90帧及interaction-state.json实际main.linear状态已核对。build-preview-build.log/build-preview-test.log为最终结果。随后仅去除本轮新复制vendor文件行尾空白并保留许可证，无运行逻辑变化。tracked diff与自有源码/测试/配置及本轮vendor空白检查PASS。额度最新可靠周used27%、remaining73%，无交接阈值触发。上面/下文“正在最终验证”均为历史状态，以本条覆盖；当前无运行中的测试句柄。

- 最新覆盖2026-11-07 11:00：原Figura Animation/AnimationPlayer/Interpolation/TransformType/AnimationAPI已复用，改为PreviewLua owner与实际Part/JOML；TimeController模拟秒时钟/暂停，Keyframe读取原typed pre/end并真实编译执行数值/Lua表达式或block，错误传播不补零。原MathUtils的Catmull/Bezier公式抽到FiguraAnimationMath。Loader保留animationList与各part anim channels，ID沿原parser全局编号；每实例实际clips/缓存/状态独立。PreviewFiguraParts在60Hz子步重放原player，fixedTime跨多循环保留timeline事件；新输入在目标时间启动时TimeController不会被之前子步倒退。Part合成原position X、rotation XY符号和scale/priority规则；override flags关于vanilla transforms的完整host未接，不虚构父类型状态。
- animation.mode=figura可选animation.clip匹配唯一裸名或modelName.clip；即使scripts.enabled=false也创建真实空源Lua host执行选中关键帧，模型script资源不运行，动画自身Lua表达式/事件仍运行。scripts.enabled=true则原脚本可操纵animations API。description包含实际clips数。Snapshot新增animations记录playstate/time/speed/blend/priority/loop；本轮最后修改该记录字段后正在执行最终验证，须以最终日志核实结果。
- PreviewFiguraAnimationTest已PASS：linear/step/catmullrom/bezier、实际Lua timeline、轮盘pause/resume/reverse、equal/higher priority、loop/hold/once、多轮fixed seek事件、表达式block、目录/嵌套ZIP、显式clip无模型脚本、实例隔离及seek重建。完整Test-Preview已PASS共12真实窗口（最后关键帧run `build/preview/fixtures/runs/20261107-105126-302-29700`，build-preview-test.log），无Minecraft/offscreen。后续snapshot新增的paused状态断言和最终构建/回归结果见最新日志；不要将该自动实窗运行宣称人工视觉检查，新动画视觉项MANUAL/PENDING。
- 下一最小任务：连接真实资源需要的完整vector/part及配置模拟player API、动态bbmodel Molang属性，补实际Figura资源动画回放；weighted armature仍拒绝。完整YSM controller sound/function/builtin、设置持久化、native CPU/GPU、多模型矩阵/峰值内存/独立Markdown报告仍未完成。主M3继续暂停，无新阶段commit/push，保留所有原M3未提交改动。目标保持active；最新本轮属于progress，不触发Luna。

- 最新覆盖2026-11-07 10:45：FiguraLoader不再丢弃parts，保留原parser层级/绑定矩阵/默认可见性/顶点owner及隐藏几何；PreviewFiguraParts共享Node bind/inverseBind、每实例Part状态，标量Lua models层级/setPos/setRot/setScale/setPivot/setVisible接真实GPU palette。原parser多模型材质索引已全局分配，不要再次加textureOffset（本轮核对源码后取消重复offset）。PreviewSkeleton新增Figura构造；PreviewAnimation新增独立mode=figura分支与seek重建；Lua attach在init前安装models；YSM visibility不对Figura重新推断。
- PreviewFiguraPartsTest验证设置/轮盘→实际owner矩阵、父子隐藏、独立实例、回退重建、setRot替换原90度与uniform scale已知点。完整Test-Preview本轮PASS含11真实窗口，最后Figura run `build/preview/fixtures/runs/20261107-104118-923-30924`，build-preview-test.log。之后只将inverseBind从逐帧求逆改共享预计算、更新help并加强旋转测试；最终重新Build、独立数值测试PASS，最终60帧窗口见build/preview/test-figura-scripted-window.log。无Minecraft，无offscreen；新Figura UI人工视觉验收MANUAL/PENDING。当前仍无新stage commit/push，原M3修改完整保留。
- 下一最小任务：接原Figura解析输出中的animation metadata/keyframe sampler与Lua animations API，继续向量重载/配置模拟player等实际host API。现只是实际脚本部件变换，不是完整Figura动画；weighted armature仍明确拒绝。然后完整controller sound/function/builtin、设置持久化、native CPU/GPU及多模型矩阵不变。总体预览器未完成，不提前更新根AGENTS/恢复M3/标goal complete。上一goal turn分类progress；本轮也有实施和自动验证进展。

- 最新覆盖（2026-11-07 10:35）：已接原YsmActionDefinition/Runtime/SchemaParser、AvatarTranslations；PreviewYsmResources保留目录/ZIP schema和语言，binary在关闭解密源前提取原properties映射与语言。PreviewYsmRuntime自动生成原风格设置/动作分页；PreviewActionPlayer执行真实clip、timeline、速度、循环、cooldown、hold generation所有权与once淡出；不是动作列表stub。配置`animation.actions=true`，需animation.enabled；不要求scripts.enabled，实际model Lua仅后者启用。语言`states.language`；width/height确实作用于根矩阵。原Action held-input已恢复，interaction新增wheel-release并记录最终actions状态。设置live hydration已接，enum展示本地化标签并保留原option id/numeric index；磁盘持久化仍待完成。
- 最新验证：build-preview-build.log构建成功；最终Test-Preview以PSNativeCommandUseErrorActionPreference=false运行，build-preview-test.log末尾PASS，10个真实GLFW窗口及Swing伴随窗口全部正常结束，最后declared run `build/preview/fixtures/runs/20261107-103407-231-29556`。非法running负测的异常为预期，不是最终测试失败。新增PreviewYsmActionsTest验证原schema/form、本地化、实际姿态、隐藏附件恢复、timeline/速度/cooldown/hold/fade/实例隔离，并补嵌套ZIP同姿态与语言校验（独立重编/运行PASS，build/preview/test-ysm-actions.log）。53个真实YSM动作/设置注册PASS，build-preview-ysm-actions-corpus.log；仅注册，不是完整动画正确性。
- 实际加密new.ysm启用actions已运行120帧exit0：`build/preview/runs/real-ysm-actions/20261107-102946-852-29972`、build-preview-real-ysm-actions.log。配置`build/preview/real-ysm-actions.json`显式给query.head_x_rotation/head_y_rotation/is_sneaking/ground_speed/is_on_ground及ysm.head_yaw/food_level；缺失项仍报错，没有补零fallback。该新自动YSM布局无本轮人工截图验收（MANUAL/PENDING），此前generic Lua轮盘人工记录仍有效。控制器+direct动作插入stage20已实现，独立数值组合次序测试尚未做。
- 下一最小任务改为Figura实际model parts/骨骼动画与Lua宿主API（当前仍静态Figura），然后完整controller sound/function/builtin、设置持久化和其余明确验收矩阵。上文旧记录中的YSM trigger unsupported/无live hydration已过时；图标/texture/vector APIs仍待接。完整预览器未交付，M3继续暂停，无新commit/push；不要提前写根AGENTS或声称完整benchmark。最新可靠额度周窗口used26%、remaining74%，无Luna阈值触发。

- 2026-11-07本轮最新：原ActionWheelAPI/Page/Action已复用，Page不改；Action/API把Avatar/globalGUI换PreviewLua owner，保留click/right/toggle/untoggle/scroll/全局拦截/page/field语义。Action移除未接的FiguraTexture APIs/TextureData/held-Press，item用真实id字符串元数据、Color用PreviewWheelValues.Color实体供UI显示；ysmAction调用PreviewLua.triggerYsmAction仍明确Unsupported，不能宣称原YSM声明动作已接。Lua core轮盘已实测可驱动原avatar_controls/Molang/GPU palette。
- 新PreviewInteraction（渲染线程队列+Operation/快照+JSONL记录/配置重放）和PreviewInteractionWindow（真正Windows Swing圆形wheel+模型settings页，EDT只收不可变快照）接入Main。config.interaction.enabled启动伴随窗口、F1请求toggle；actions/replayFile可按frame复现wheel-click/scroll、set/press/reset-control、open-page；interaction.jsonl含frame/time/source/请求及操作后state，close另写interaction-state.json最终值。controlsPageRequest计数使同一root页的轮盘Open也切到settings，不只比较页名。Snapshot按100ms更新；设置页各类型widget已接，人工本轮只覆盖number/reset/page，不冒称全部UI控件验收。
- 测试已加入Test-Preview：PreviewWheelTest验证原API回调/左右键/切换/scroll拦截/keepSlots/实例隔离；PreviewInteractionTest基于lua-animated-ysm，fixed0.125时操作使child x=-1/-2/0，逐frame核对palette并记录→重放，最终state JSON等价PASS。生成build/preview/fixtures/interactive-ysm.json。最新Build/Test-Preview PASS共9真实预览run（最后interactive ...100609-410-27692）+Swing companion自动清理；全部exit0。build-preview-build.log/build-preview-test.log为最终结果。
- 本轮Computer Use已真实验证：先读computer-use SKILL及guidance/api/confirmations，用node_repl @oai/sky；窗口中文“模型动作/向右移动/恢复位置/模型设置/水平偏移”正常。实际输入offset2，轮盘toggle设1、slot2打开root设置、Reset设0；4条操作日志frame23023/26721/31183/33217位于build/preview/fixtures/runs/20261107-095921-123-29900/interaction.jsonl。模型/FPS/地面/天空目视检查；点击模型关闭X后session28443 exit0，后续sky.list_windows对该title匹配为空，两窗口均关闭。HUD此前stale PENDING已改实际animation.description。没有Minecraft或offscreen。sky.type_text在Swing字段本轮未观察输入，press_key数字2有效；F1/R/Escape仍MANUAL/PENDING，不为此改生产路径/标blocked。
- 下一最小任务：接原YsmActionRuntime/definitions/schemas与自动轮盘到当前dispatch（不是一直保留trigger异常），再接完整Figura bbmodel parts/animation Lua host、player等配置模拟API。当前common Lua轮盘/设置链路已可用，但原YSM数据轮盘和Figura真实动画不算完成；图标/完整vector/persistence/live hydration也未全接。随后native CPU/GPU/multi/report验收原要求不变。主M3继续暂停；main仍M2 f51df7d，原M3与preview都未提交，整体验收前不checkpoint/写根AGENTS。周额度最新used25%/remaining75%，未达Luna阈值，无Luna。

- 2026-11-07本轮最新：实际FiguraMC LuaJ core/jse 3.0.8-figura已打入build/preview/lib（Guava33.6用于原LuaEvent），原sandbox打入JAR assets/omni-morph/scripts/sandbox.lua。复用LuaTypeManager（去MC Component/改docs名称显示、补实际table→List、反射Error透传）、LuaEvent（去profiler）、EventsAPI（去mod entrypoints）、ReadOnlyLuaTable、PathUtils、LuaWhitelist/NotNil/docs annotations、AvatarControlsAPI（Avatar→PreviewLua、openPage记录独立页状态）、AvatarControlDefinition/Type。来源/改动已更新NOTICE。
- 新PreviewLua共享Program定义、每实例globals/modules/queues/controls/RNG；Program.load支持目录/嵌套ZIP资源根与autoScripts/相对require/cache，原标准库组合与sandbox，实际debug hook500k指令预算，Error信号绕过pcall仅在最外层转LuaError。PreviewControlRuntime保留原value→interceptor→binding/option command→change及press/reset顺序、numeric enum fallback/roaming aliases；Molang variables改用原VariableStorage保留嵌套roaming值，shared AST仍按evaluator.entity绑定。持久化/live hydration尚未做，不冒称完整设置。
- 实际渲染链已接：config.scripts.enabled=true，要求animation.enabled（当前YSM skeleton）；scripts.controls保存初始覆盖，states给只读preview全局。State先init Lua/设置再计算默认隐藏；真实20Hz tick、render partial tick、场景提交后post事件进animation_ms；controller时钟重放内交错Lua tick，最终render后resample；回退重建host/Lua/controller。自有0.125秒子骨骼点(0,5,0.5)证明配置→init→tick/render→Molang→GPU palette；named和controller模式均验证、Lua驱动转场与实例隔离PASS。
- PreviewLuaTest/PreviewLuaAnimationTest已加入Test-Preview。实际RED test-lua-red.log证明Enum.options(List)不能收Lua table，binder最小修复后GREEN；相对require/cache、autoScripts目录/嵌套ZIP、事件、limit/pcall、seed、设置norm/interceptor/reset/command、nestedroaming绑定均PASS。最新Build/Test-Preview PASS含8真实窗口：...094520-050-19944、...094522-051-3820、...094523-807-17124、...094525-735-28340、...094527-420-29712、...094529-207-25784、...094531-310-21792、Lua ...094533-519-27868。日志build-preview-build.log、build-preview-test.log；全部正常退出，无Minecraft。59资源最终加载回归PASS，build-preview-corpus.log末行为Preview corpus load PASS: 59 YSM/Figura assets; bind pose only。另最终增强Lua budget负测：反射嵌套控制回调内的无限循环/pcall不能绕过预算，之后return 2+2仍正常，独立test-lua.log PASS。
- 下一最小工作：继续接原ActionWheelAPI/Action/Page到PreviewLua和实窗交互，再做设置界面/操作记录；Figura原bbmodel的model-part/动画需有实际独立实例状态和Lua API，player等宿主值来自配置，不能空stub。controller sound/function/builtin仍未完整；Lua目前仅已覆盖上述YSM链路，完整asset benchmark仍拒绝。原M3/native/multi/report要求未变。当前main仍M2 f51df7d，全部本轮与M3改动未提交；preview整体未验收，不提前写根AGENTS/恢复M3。最新可靠周额度used24%/remaining76%，未触发Luna。

- 2026-11-07本轮最新：声明controller实际回放已接GPU（`animation.enabled=true, mode=controller`，不要求clip）。PreviewControllerPlayer.Definition共享真实解析clip/controller；State独占host/slots/clocks/finite/startPose/layers/runtime。原YsmControllerRuntime与YsmControllerPoseLayer已复制：runtime把Avatar/YsmAnimationPlayer换PreviewMolang/PreviewControllerPlayer，异常传播，未连接的sound/function dispatch仅在明确拒绝相关定义的子集内省略；PoseLayer不改。ctrl/controller/c/context使用原VariableStorage且按context scope还原，完成条件/状态时钟是真实播放值。
- 最新测试：PreviewControllerTest从PreviewAnimationTest fixture复制geometry/PNG生成controlled-ysm；已知点验证entry/exit、initial entry、半程snapshot、完成层替换、子controller、parallel stage、0.5权重、once完成转场、fixed0.3首次60Hz重放、seek reset、实例隔离、base零scale同时存在时恢复附件/子级。function资源guard负测PASS；目录/legacy ZIP与binary分支均在controller模式检测function资源，不能静默忽略后计benchmark。非Molang事件、state sounds、ysm-builtin、root functions/action_schemas明确拒绝，完整宿主尚未接。
- 最终Build/Test-Preview PASS共7真实窗口，...092951-829-24964、...092953-974-12264、...092955-747-4452、...092957-420-24040、...092958-906-2260、...093000-553-26592、controller ...093002-222-13172；日志build-preview-build.log、build-preview-test.log。全部正常退出，无Minecraft。59资源最终加载回归PASS，build-preview-corpus.log末行为Preview corpus load PASS: 59 YSM/Figura assets; bind pose only。编译/测试均前台Tee。下一步优先接LuaJ、原函数/动作/控制定义，再做YSM/Figura轮盘和模型设置交互；controller sound/builtin仍待完整支持，native/multi/report要求不变。
- 本轮没有阶段提交；预览器整体未验收，不提前提交检查点/写根AGENTS/恢复M3。周额度最新used24%、remaining76%，未触发Luna。原M3修改保留。下文“controller回放尚未接”为历史状态，以本轮最新为准。

- 2026-11-07本轮增量：初始controller附件隐藏已接通，覆盖manifest目录/ZIP及binary映射；原YsmAnimationControllerParser与controller/state/ref/transition/slot/blend DTO复用，去Avatar默认engine，错误传播。YsmVisibilityDefaults只保留初始状态中具有literal恒零scale的引用，静态无host阶段跳过条件，运行实例用实际Molang条件；转场/entry/exit仍未执行，完整controller播放尚未实现。原始controller JSON与动画JSON共享保存在mesh.animationSources，后续解析时按root键区分。
- 本轮RED发现全量编译controller时会在静态加载触发ctrl.*转场依赖；改为仅分析实际初始隐藏引用，不以stub补零。GREEN：59抽样资产加载PASS（build-preview-corpus.log），初始状态/数组fallback/空条件/后资源覆盖/不可达表达式不执行/条件真假测试PASS，真实子骨骼GPU palette隐藏和实例隔离PASS。Build/Test-Preview最终PASS含6真实窗口（...091933-753-9144、...091936-057-2352、...091937-986-10692、...091940-208-15304、...091941-952-17700、...091943-772-27108），无窗口残留/无Minecraft。格式器实际路径C:/Users/eryth/AppData/Roaming/Python/Python313/site-packages/clang_format/data/bin/clang-format.exe（python -m clang_format不可用）。
- 本轮尚无阶段提交；main及M3修改保留。周额度最新used24%、remaining76%，未达交接阈值，无Luna调用。下一最小任务：接完整controller运行与LuaJ/原控制定义，再接YSM/Figura轮盘和模型自带设置的实际状态/回调及交互；其余native/multi/report验收保持原要求。下文“初始controller隐藏待接”为历史状态，以本项为准。

- 用户顺序保持：完整预览器验收 → commit/push并确认远端 → 查额度 → 更新主仓库根AGENTS.md使用说明（届时用writing-for-agents技能）→ 恢复主M3–M6。预览器未完成，不提前提交检查点/写AGENTS已支持能力/恢复M3。分支main仍是M2 f51df7d，全部M3未提交改动保留。
- 用户最新新增：附件默认隐藏参考OMP YSM现有实现；YSM和Figura的动画轮盘、模型自带设置都必须支持，实际绑定原动作/回调/模型状态并可配置/记录。已加入独立计划5/9节，尚未完成轮盘和设置UI，不得删除这些验收要求。
- 全部预览器实现/测试/配置/构建/vendor在tools/preview/；产物build/preview/preview.jar + lib/，JDK25。四张实际Minecraft环境PNG在JAR assets/minecraft/...，来源哈希清单assets/omm_preview/texture-sources.json；不依赖外部PNG目录、不启动Minecraft，素材暂存build/preview/resources/不提交。
- 五类静态资源输入均接通：YSM目录/ZIP/加密.ysm、Figura目录/ZIP。原OMP26.3容器/manifest/geometry/crypto/binary/texture、bbmodel v4/v5复用；typed内存数据树替代NBT宿主，真实文件/UUID/日志/ParentType元数据适配。许可证与变更见tools/preview/vendor/omp/NOTICE.md。
- YSM显式命名clip + 真实Molang现在已连接GPU骨骼播放：PreviewMesh保留bind-world顶点、owners、PreviewSkeleton父级/pivot/rest/inverseBind、动画源；PreviewAnimation共享解析clip，State独占Molang/pose/world/palette。GPU vertex owner + RGBA32F texture-buffer delta palette（worldAnimated*inverseBindWorld）消费实际骨骼。静态Figura/cube用identity palette。Java GPU目前single/count1/threads1；native/CPU/multi明确拒绝。完整资源benchmark仍拒绝，不冒充完整controller/Lua行为。
- 原YsmAnimationParser/Sampler/Clip/Channel/Keyframe/Event/BonePose/Easing已复制并适配，显式engine；去掉Avatar默认engine/活动bone-filter scope，解析/求值错误传播而不吞异常。YsmBinaryAnimationMapper处理加密动画。PreviewAnimation按原YsmRenderer轴向（position X负、rotation X/Y负Z正）、父级、scale、blend、有限值hold采样；Molang timeline排序/跨帧执行，循环/时钟回退重置；仅Molang事件，其他事件明确拒绝。
- PreviewMolang模拟states.query与states.ysm实际标量，running进入q.is_running而不冻结YSM全部动画；q.anim_time是clip时间、life_time为播放时钟，缺失值带配置路径报错。原math/parser/AST/evaluator/budget，独占变量/temp/RNG，同seed可复现。LuaJ仍未接，缓存com.github.FiguraMC.luaj/luaj-core与luaj-jse/3.0.8-figura已确认。
- configs/ysm-animation.json示例播放01_taisho_maid的idle，states.ysm.food_level=20。animation.enabled/clip/speed/timeOffsetSeconds/fixedTimeSeconds可配，负预滚钳制0并记录；静态配置保留。实际01_taisho_maid已目视头部动画，自有模型已两次目视子骨骼姿态变化。new.ysm与old.ysm idle各60实窗帧PASS（build/preview/test-new.ysm-animation.log与test-old.ysm-animation.log）。这些不是完整状态机/脚本基准。
- 新YsmVisibilityDefaults复用OMP player的base slot/恒零scale规则（pre_parallel/parallel0..7及下划线槽位、标量/向量/keyframe pre+post、动态排除）；默认分析只对literal scale用真实parser/sampler，非Molang执行stub。后声明同名资源覆盖，原firstPersonAnimationPaths公开给preview，fp_arm与二进制type11排除主模型。Skeleton default/rest visibility、State父子有效可见性、GPU退化隐藏、按可见bind部分取景；不删mesh，非base显式scale可恢复附件。真实01模型原蓝色辅助框已在实窗确认默认隐藏，run build/preview/runs/ysm-animation/20261107-090717-676-23112。初始化controller的隐藏规则尚未接，必须继续。
- 自有测试：已知骨骼点(-1.5,.5,.0075)、父级与轴向、timeline顺序、seek reset、实例隔离、palette column-major、附件/子附件隐藏及显式scale恢复、base槽位范围/dynamic/prepost排除/资源后覆盖/FP资源隔离均PASS。曾用heap FloatBuffer触发JOML Unsafe写地址0x10崩溃，栈writePalette→Matrix4f.get(FloatBuffer)，现改显式NIO四列写入，原RED已GREEN。保留hs_err_pid14240.log及build-preview-animation-green.log，不用JVM flag掩盖。
- 环境：无限草地解析ray/plane + 原grass top；天空渐变+原云/太阳/满月；同相机正确深度，方向/云高固定。ground与sky开关/参数可配；HUD深色底板可读。model/ground/sky三个GPU query各自按frameID异步回收，CSV各分项CPU/GPU/Draw Call；total_draw_calls=模型+地面+天空，不含HUD。animated的animation_ms/bones_ms已分列；峰值RSS和GPU路径CPU vertices仍NA。
- 最新build与Test-Preview PASS：资源、Molang、动画、visibility、JAR 4贴图/哈希、6个真实窗口、三个GPU query及环境off。输出smoke ...090659-579-18824、demo ...090701-638-15212、staticYSM ...090703-476-20484、Figura ...090705-258-27888、envOff ...090706-828-21756、fixedAnimation ...090708-537-22128。59个本地YSM/Figura资源最终加载回归PASS（build-preview-corpus.log，53YSM+6Figura）。这里只证明其加载，不证明所有动画/脚本/蒙皮。
- 全部窗口正常关闭，最后人工观察进程exit0，无Minecraft、无offscreen。R/Escape自动化未观察回调仍MANUAL/PENDING，close和参数式结束正常；不作为blocked，不改生产输入来凑测试。
- 未完成：初始化controller（含初始隐藏、转场/层叠）、Lua/事件及Figura动画、YSM/Figura轮盘/模型设置、加权armature蒙皮、半透明排序/附属实体、native CPU/GPU 1/8/16、单/多实例/混合1/5/20/50/100、资源共享/完整状态隔离、真实可见/提交计数（当前single计数仍固定）、峰值内存及正式分路径报告。原M3不收尾，预览器不宣称完整交付。
- 最小下一步：先接controller初始化与可见性，再接LuaJ、原YSM动作/控制定义与Figura action_wheel/avatar_controls，使轮盘和模型设置作用于真实状态；不能仅做UI列表。已定位OMP YsmAnimationPlayer collectControllerInitialHiddenBones，controller包YsmControllerRuntime/Parser，model/ysm/action/YsmActionWheelLayoutStore，avatar/control/AvatarControlDefinition/Runtime/Type/ValueStore，lua/api/avatar_controls/AvatarControlsAPI，以及原Figura action wheel API（后续需查实际依赖）。所有代码留tools/preview/。
- 最新可靠额度：周窗口已用23%、剩余77%，未达Luna交接阈值。无Luna调用，不猜订阅/额度。下一阶段前后按实际接口检查。
## 此前 bootstrap 历史记录（不得覆盖上面的新状态）

- 源码/构建/配置/说明在tools/preview/；已构建build/preview/preview.jar及lib/依赖目录，CLI使用--config，另有--help/--frames/--state/--no-record。
- LWJGL3.4.1（本机Gradle缓存）、Gson2.11.0、JOML1.10.8，JDK25构建且--release17。当前实窗测试为Java25；其他JDK实窗未测。
- 已有实际可见GLFW/OpenGL窗口、自有旋转立方体、FPS/帧耗时/GPU HUD、异步timer query、逐帧CSV、配置快照、GPU Markdown摘要。每次运行新增run-id目录。
- 当前仅builtin:cube / Java GPU / 1模型。其他输入/路径明确拒绝，不能把fixture结果冒充真实模型或native渲染成绩。
- 已由Computer Use实际观察图形和HUD，GL3.3/NVIDIA RTX5060 Laptop GPU；窗口关闭按钮正常退出，相关Java进程均已结束，未用offscreen，未启动Minecraft。
- build-preview-build.log、build-preview-help.log、build-preview-window.log、build-preview-test.log。Test-Preview.ps1 PASS：非法state拒绝、两个真实窗口自动退出、70帧/60样本、原frame ID GPU关联、running=false覆盖和no-record不写帧行。此前实窗CSV验证14125条唯一有序frame ID及14125条GPU结果。
- R/Escape自动化输入后未观察到回调；键盘快捷键MANUAL/PENDING。不要因此停下其余实施，也不要添加仅为测试的替代渲染路径。

## 下一步（预览器，不是继续原M3收尾）

1. 五类输入的静态模型/贴图已接通；继续连接原动画/controller/Molang/Lua宿主到真实模型骨骼，不用M2 harness空stub冒充运行。
2. 独立JavaGPU基线和native CPU/GPU绘制接入；复用已有M3模型API/worker预算，不随意改架构。预览器所需native能力可作为最小必要依赖实施，不把M3/M4/M5其余任务提前宣称完成。
3. 配置文件主入口java -jar preview.jar --config scene.json；命令行模拟必要Minecraft状态（例running=true/false），记录最终解析状态/时间/种子，真实执行脚本且实例隔离。
4. 完成单模型、相同模型多实例、不同模型混合两类场景；1/5/20/50/100档。固定随机种子/相机/分辨率/动画时间，确保模型进入可见范围；记录配置/可见/提交数量。
5. Java单线程/native1/8/16组合在CPU/GPU分别测试，CPU动画/骨骼/顶点/GPU绘制/Present/Draw Call/FPS/p50/p95/p99/峰值内存分别记录。逐帧CSV/TSV按frame ID关联GPU；CPU/GPU独立Markdown，每模型/场景H2+独立表格并返回路径。
6. 验证共享不可变mesh/纹理、骨骼/脚本状态隔离及动画正确性；全预览器验收通过才提交/推送/确认/查额度，再恢复原流程。

## 用户授权和边界

- 预览器可以直接复用任意参考版本模组的Molang、.ysm、.bbmodel、Lua代码，带最小所需依赖并保留来源/许可证。不重写已有解析器；.ysm保留Java/native比较。
- 所有当前测试不启动Minecraft；实窗预览与Minecraft实机性能分开报告。游戏实机项MANUAL/PENDING，不作为blocked依据。
- 编译/测试/打包都必须前台PowerShell、2>&1 | Tee-Object、立即保存检查LASTEXITCODE，失败读日志。GUI必须真实Windows窗口，严禁Qt offscreen。
- Computer Use技能已读；其sky API通过node_repl初始化。UI自动化只用Computer Use JS API，不混用PowerShell UIA，不接触其他会话进程。
- 无关重构/额外后端不属于本次授权。第三方模型/解密内容不提交、不上传CI。

## 暂停保留的 M3 事实

- 未提交的src/model（math/model/api/bake/kernels/sort及架构源）、src/abi/cpu_executor.*、src/jni/model_jni.*、src/ysmparser/access.hpp与api.cpp内部资产视图、ABI0.3/mesh1头文件、Java bridge、CMake和测试均需保留。
- Model/instance/bake有C ABI生命周期/拓扑/索引/预算/full-delta revision/失败原子性测试；instance持有model backing，asset释放后可继续使用。
- CPU kernel实际执行scalar/SSE2/AVX2，1/8/16线程输出一致；NEON源码未在目标运行，UNVERIFIED。
- 新JNI已在Java17/21/25测试创建/bake/查询/update/CPU snapshot transform/sort与错误边界。Release CTest9/9、JNI通过；新增JNI/线程复用后的最终Debug未重跑。
- 线程复用诊断RED连续两次native16约2.13ms、native1约0.30ms；runtime惰性复用worker后GREEN约native16 0.295ms。保留线程上限、调用线程参与、同步完成、输入序异常传播；不递归进入同一个executor。同runtime的CPU frame工作串行提交。
- 全53个YSM及附属几何replay已重跑PASS，比较92439792顶点，max_scaled_float_error=1.12406799e-07，检查初始bind和三个程序化动画时刻、1/8/16及所有本机kernels、矩阵/顶点/normal/UV/owner/sort。
- 最终CPU模拟基准docs/m3/cpu-replay-benchmark.md：5模型/100行，每路径30预热、5轮*120帧。报告实际DLL哈希；程序化宿主动画，不含GPU/Present/模型原脚本，不冒称正式Minecraft基准。原始build/m3/cpu-benchmark.tsv；之前基线保留cpu-benchmark-before.tsv。
- M3剩余最终审计/精确拓扑证据等暂不收尾。M2 parse基准docs/m2/parser-benchmark.md已完成，不重跑；M2完整差分53/18/6及六section schema通过，真实v1/current第三方样本UNVERIFIED。

## 参考定位

- docs/preview/Preview-plan.md；tools/preview/README.md；docs/m3/M3-model-simd.md；docs/OmniMorph-native-api.md。
- 已发现参考类：OMP26.3 top/aihmc/omnimorph/parsers/BlockbenchModelParser（有Minecraft NBT依赖）、org/figuramc/figura/parsers/BlockbenchParser2、top/aihmc/omnimorph/molang/MolangEngineImpl、org/figuramc/figura/lua/FiguraLuaRuntime。实际依赖要继续核对，不以名称猜可独立编译。
- 当前最后可靠额度：周窗口已用21%，剩余79%，未达交接阈值；无Luna调用。≤10%预警，≤2%安全交接且仅手册编写委派gpt-6-luna。不可猜订阅/额度，不擅自切换日常开发模型。

## 最新增量覆盖：2026-11-07 12:06

- PreviewFiguraParts新增partToWorldMatrix，保存实际层级提交的pivot-relative像素到block矩阵，getter返回原FiguraMat4独立副本，保持最后提交语义。新增tests/PreviewPartWorldMatrixTest.java并纳入Test-Preview.ps1。
- canonical preview.jar已构建；build/preview/test-part-world.log数值PASS。最后完整build-preview-test.log PASS，17真实窗口及伴随窗正常退出，最终60帧run build/preview/fixtures/runs/20261107-120624-385-15108；session13819 exit0。此覆盖前文16窗数量。当前无运行测试session，不需进程清理。
- 真实AI Hobbyist默认5秒probe PASS；仅打开global开关不会覆盖实际粒子分支，不能据此宣称粒子支持。进一步通过pings同时打开global、aways、8尾后，5秒回放在lib/particle:52失败：particles为nil，见build/preview/host-probe-particles.log。此前partToWorldMatrix调用已通过。build/preview/FiguraHostProbe.java为本地诊断，不进入源码交付。
- 下一最小任务：接入实际particles宿主及效果绘制，复用原Figura粒子API语义，不注册空spawn来掩盖失败；再继续既定CPU/native/多模型/报告验收。整个预览器未完成，主M3保持暂停。
- main仍M2 f51df7d，无本轮commit/push；原M3修改全部保留。可靠额度接口周used29%/remaining71%，未触发Luna交接。

## 最新状态覆盖：2026-11-07 12:24 粒子宿主已接通

- 本轮有实际进展，目标仍active；预览器整体交付尚未完成，不恢复主M3，不新建阶段、不把粒子增量当完整preview里程碑。main仍M2 f51df7d，上游不变，本轮无commit/push。保留既有M3及preview未提交修改，无代码重置。
- 新增PreviewParticles.java：实例隔离、固定seed/20Hz、实际位置/速度/重力/阻尼/寿命/淡出/平面地面碰撞；states.particles.water默认false，true为全局模拟水，气泡在空气下一tick消失。PreviewAnimation.createLua初始化地面，修复seek重建Lua后环境丢失。此为独立六类型模拟，不是Minecraft粒子引擎等价实现；叶片固定风场、平面碰撞、无方块/生物群系/完整光照语义已明确写README。
- 原Figura ParticleAPI/LuaParticle已放vendor/omp/org/figuramc/figura/lua/api/particle并适配真实后端；原public API/别名/向量参数/getter拷贝/lifetime/power/scale语义保留，去掉live Minecraft/Avatar/权限暂停依赖和冗长文档overload注解。原parseVec4复制进PreviewVectorArguments。没有空spawn或静默不支持fallback。
- 新ParticleRenderer.java：游戏JAR六类原JSON+41额外PNG动态合atlas、相机朝向quad、透明深度排序、实际GL draw。Build-Preview现hash校验47总PNG+6JSON，原游戏资源保持build生成产物，不提交第三方贴图/模型/cache。
- FrameLog追加particles（提交数）、particle_draw_calls、particle_submit_ms、particle_gpu_ms，独立GPU query且total_draw_calls包含粒子。粒子tick计入animation_ms；模型GPU、地面、天空计时独立。现有summary动画描述仍有旧YSM命名，在正式report阶段需按真实Figura/控制器/动作路径修正（不当本轮正式benchmark）。
- PreviewParticleTest及Test-Preview已纳入真实状态、原方法四种newParticle重载、nil查询、位置/色彩/scale/power、spawn/clear/remove/lifetime、seed/owner隔离、气泡空气/水、地面和seek回归。PackagedTexturesTest验证47PNG、6JSON来源hash与JSON引用全齐。
- 最终canonical build/preview/preview.jar构建成功：build-preview-build.log。最终完整build-preview-test.log PASS18真实GLFW窗口及伴随窗清理、session91907 exit0；最后六精灵60帧run build/preview/fixtures/runs/20261107-122402-334-14024，每帧6粒子/1draw/有效GPU query。旧17及第一次18回归仅历史。修改代码已formatter，新增/修改文件diff空白通过。
- Computer Use已目视自有6类精灵和真实ZIP八尾彩色特效、正常Escape退出。真实ZIP run build/preview/runs/real-figura-particles/20261107-121852-453-31604：4426帧、4311帧有粒子draw，最后164粒子、1粒子draw，总draw9。session60692/自有session11058均exit0，不需清理JVM；最后node_repl仍有sky/window绑定但窗口已关闭，后续重新list选择，不复用旧句柄。未启动Minecraft、未offscreen。
- 真实目录/ZIP五秒probe状态完全匹配：40粒子，poseSHA884bb225f68ce5bc2dca53cf3cb8e63d70cb4cbc5905387400b89dc08f2c92b6，particleSHA d0846951e6d0edfd74398e74e238714ce015a02474076ff01c17a4b8d65328e9。日志build/preview/host-probe-particles-folder.log和-zip.log。moving/gliding/swimming各5秒成功生成500粒子。probe及真实ZIP在build/preview为本地诊断，不进仓库。
- **下一最小任务：sounds宿主。** DYING已复现lib/particle:113 sounds:playSound nil（build/preview/host-probe-dying.log，失败项，不能PASS）；模拟player.pose=DYING，全局/常驻/8尾开启。参考原Figura声音API后接真实播放/状态，不能以空playSound绕过。再完成既定设置持久化、完整YSM控制器宿主、CPU/native、多模型/计数/内存/报告验收。原M3所有状态保留暂停。
- 最新可靠额度接口周used29%/remaining71%，未触发Luna。Root AGENTS.md仍不存在；按用户要求只在预览器交付并提交推送+查额度后写入已验证用法。参考docs/preview/Preview-plan.md、tools/preview/README.md和vendor/omp/NOTICE.md。

## 最新状态覆盖：2026-11-07 12:43 声音宿主已接通

- 本轮为progress，目标保持active；独立预览器整体尚未交付，原M3继续暂停。main仍M2 f51df7d，上游origin/main无新提交；既有M3/preview未提交修改全部保留。没有新增里程碑或把声音增量当完整preview阶段提交。
- 新增PreviewAudio.java：真实OpenAL3.4.1默认设备/上下文，STB Vorbis mono/stereo PCM完整缓冲，内容hash共享buffer、原生source状态/位置/音量/pitch/attenuation/loop、暂停继续自然结束、相机listener，最终清理source/buffer/context/device。设备按需打开，不需要音频的模型不打开设备。音频遵循墙钟，不以模拟时间虚构播放进度。
- 新增PreviewSounds.java及vendor/omp/org/figuramc/figura/lua/api/sound/SoundAPI.java、LuaSound.java，原白名单方法/别名/向量解析、newSound base64/字节表及播放流程适配真实后端。原版事件固定seed选变体，本实例ON_PLAY_SOUND可取消；不做跨avatar广播/游戏全局音量分类/字幕组件翻译UI，字幕目前保存文本，音频目前为完整静态Vorbis buffer，未宣称Minecraft完整声音引擎等价。未知/未打包声音明确报错，无空playSound。
- PreviewLua.Program新增sounds及兼容原2参数构造，目录/嵌套ZIP .ogg按原Figura路径点分命名；模型自定义OGG无需原版事件清单。PreviewLua implements AutoCloseable，默认独占audio供直接测试，共享构造供PreviewAnimation；后者owns一个audio跨State共享并implements AutoCloseable。seek先关闭旧Lua本实例声音，Main最终GLFW清理后关闭audio再释放Swing，audio.json记录关闭后closed/activeSources及历史play/解码/source计数。原生声音清理轮询放render，覆盖非frame调用的controller路径。
- Build-Preview增加精确OpenAL3.4.1 Java/native依赖及-MinecraftAssets/-AssetIndex/-SoundEvents；读取本机Gradle Loom assets/indexes最新index（本机26.1.2-30）和已存在objects，默认squid hurt/death/squirt三事件10OGG及原JSON定义子集，校验SHA1，保存SHA256/index/catalog来源。未下载、未安装、未启动Minecraft。构建现在使用PowerShell7，README已说明。游戏音频/贴图仍是build生成输出，不提交完整第三方模型/音频/cache。
- 新PreviewSoundTest覆盖真实PCM上传大小、真实AL播放游标推进/暂停不动/恢复/自然停止、原LuaAPI与标量/向量调用、回调取消、base64与字节表newSound、异常不注册、buffer共享、owner隔离、seek与device清理；目录/嵌套ZIP custom sound bytes/ID相同。原生游标探测listener gain0，未当成人工听音PASS；听音MANUAL/PENDING不阻塞。PackagedTexturesTest补OGG来源hash及事件清单验证。
- 最终build-preview-build.log构建成功，canonical build/preview/preview.jar包含最新Sound来源NOTICE。最终完整build-preview-test.log PASS19真实窗口及伴随窗清理、session41438 exit0；最后音频ZIP60帧run build/preview/fixtures/runs/20261107-124058-610-28388。此前session70146首次19亦PASS但仅历史。追加字节表test-sound.log PASS；最终打包JAR另60帧run .../20261107-124214-670-29812、session90515 exit0，playCalls1/decodedBufferLoads1/activeSources0/closed=true。test-final-packaged-assets.log及test-final-audio-artifact.log来源hash/NOTICE/清理PASS，全部修改格式/diff空白检查通过。
- 真实AI Hobbyist原脚本目录DYING与ZIP swimming+配置keybind“游泳喷墨(按住)”=true，各完成301姿态提交/5秒模拟回放且矩阵finite：DYING800粒子、play100/分配100source/3buffer；INK600粒子、play50/分配50source/4buffer（含预载death1用于打开并静音设备）。测试不取消播放、未修改原脚本，确实创建真实ALsource，但listener gain0。日志build/preview/real-sound-dying-green.log和real-sound-ink-green.log均exit0；原红host-probe-dying.log保留为历史失败。临时FiguraSoundProbe.java在build，不进正式源码。
- 当前无运行测试句柄，无需进程清理。本轮未使用Computer Use（没有新增可视布局）；此前窗口绑定已关闭，后续自动化需重新list选窗口。未Qt offscreen，未Minecraft。旧授权仍只涵盖本会话确认渲染/写入结束且仅AWT等待的测试JVM，不能扩到其他进程。
- 下一最小任务：设置持久化与既定剩余宿主接入（YSM controller声音/函数/builtin事件仍未接；可复用新的真实声音后端），再完成native CPU/GPU、多模型可见/提交计数、共享/隔离/内存及正式Markdown矩阵。FrameLog旧summary动画命名还需在正式report阶段修正。不要重复实现粒子/皮肤/声音，也不要提前恢复M3。
- 最新可靠额度：周used30%/remaining70%，未触发Luna。Root AGENTS.md仍不存在，按用户顺序待完整preview交付/提交推送确认/查额度后再写已验证用法。详见docs/preview/Preview-plan.md、tools/preview/README.md、vendor/omp/NOTICE.md。

## 最新状态覆盖：2026-11-07 13:04 设置持久化已接通

- 本轮为progress，目标保持active；独立preview整体未完成，原M3暂停。main仍M2 f51df7d，上游origin/main不变，无本轮阶段commit/push；既有M3/preview修改全部保留。不要把设置增量当成完整preview交付或新增阶段。
- 新PreviewSettings.java按原OMP AvatarControlValueStore策略接JSON文件：默认settings.enabled=true，默认JAR旁model-settings目录，settings.directory可覆盖；按规范模型路径+引擎hash隔离，settings.modelKey显式稳定值可让同引擎文件夹/ZIP共享。控件ID hash，YSM枚举仅存序号避免私有标签/表达式落盘，Figura枚举保留脚本原始文本。控件类型匹配后restore；按钮/标签/分隔线/persistent(false)不保存。接受变更在同步绑定之后、控件onChange之前写入，全局拦截true不保存，相同值不重复写。新实例读取当前偏好，存活实例状态互相隔离。
- benchmark.enabled=true强制不读不写偏好，使用显式scripts.controls/交互重放；普通运行可settings.enabled=false。配置值在保存值之后应用，reset保存默认。文件同目录临时写入+ATOMIC_MOVE替换，IO错误明确上抛。每次运行settings.json记录有效mode/file/storedControls/writes。
- PreviewControlRuntime注入settings，注册先restore以支持Lua立即读和后续range/options归一化，hydrateInitial先applySaved避免live绑定盖偏好。原AvatarControlDefinition仅setPersistent(false)增加6行：清除pending restored值并恢复作者默认，不覆盖作者显式setValue；原文件其它源码/布局保留（未进行整文件第三方格式化）。
- PreviewLua构造注入偏好，PreviewAnimation持有模型偏好，与原audio/particles模块并行。既有16个直接创建PreviewConfig的测试夹具明确settings.enabled=false，保证跨测试/用户状态不污染；直接PreviewLua单元测试保持无磁盘偏好，专用Settings测试才启用隔离缓存目录。
- 新PreviewSettingsTest通过：Figura/YSM实际palette保存→新运行时恢复、范围/早期Lua读取、Figura文本枚举/YSM序号隐私、非持久化链式opt-out、拦截、实例隔离、reset/配置覆盖/seek、benchmark不读写和同modelKey目录/嵌套ZIP共享。完整回归增加两引擎各两次独立JVM60帧保存/重启，读取writes0、文件相同、Figura offset40/YSM width2.5。
- 发现并复现退出顺序BUG：Main先关闭audio再Interaction.close释放按键，释放回调playSound抛Audio device already closed（build/preview/test-settings-release-red.log，exit1）。调整为GLFW原生清理→interaction关闭/释放回调/最终交互状态→关闭audio并写最终audio/settings记录；始终保留GLFW在Swing之前的旧AWT修复。首次GREEN复用了RED保存的24而触发expected16，保留test-settings-release-stale-fixture.log；重新创建隔离fixture后green30帧exit0 .../20261107-130057-532-20312。没有把失败记PASS、没有清理/覆盖用户偏好。
- 最终build-preview-build.log构建成功，canonical build/preview/preview.jar含最新来源NOTICE。最终完整build-preview-test.log PASS24真实GLFW窗口及伴随窗清理，session14030 exit0；最终held-key run build/preview/fixtures/runs/20261107-130300-245-30868：release保存offset24、settings writes1，实际audio playCalls1/activeSources0/closed=true，按键最终全释放。旧session48843的23窗PASS仅历史。修改代码已按范围formatter，新增/修改文件diff空白通过；README多余末尾空行已修正。
- 当前所有测试句柄均终止，无需JVM清理。未启动Minecraft、未用Qt offscreen；本轮未使用Computer Use（无新增UI布局，采用真实窗口行为验证）。旧授权仍只限本会话日志/命令行确认写入结束且只AWT等待的JVM，不能扩到其他进程。既往听音MANUAL/PENDING仍未替代为PASS。
- 下一最小任务：既定YSM controller声音/函数/builtin宿主接入，可复用原函数运行时和新的真实声音后端；再推进CPU/native/GPU、多模型/真实可见提交计数、资源共享/状态隔离/峰值内存及正式Markdown报告。FrameLog旧summary中动画路径命名仍需在正式报告阶段修正。不要重复实施皮肤/向量/时钟/按键/粒子/声音/偏好，也不要提前恢复主M3或提交未完成的preview阶段。
- 最新可靠额度：周used30%/remaining70%，未触发Luna阈值。root AGENTS.md仍不存在；按用户顺序待完整preview交付→提交/推送确认→查额度后再更新已验证用法。计划docs/preview/Preview-plan.md，使用说明tools/preview/README.md，来源vendor/omp/NOTICE.md。

本轮最终检查：tracked git diff --check 与本轮音频新增/修改范围空白检查 PASS。全量未跟踪 vendor 扫描另见原复制 Action/Page/EventsAPI/LuaMethodDoc/Blockbench/YsmAnimationChannel 的既有空白，未作无关第三方格式清理；不宣称全量 vendor 无空白。当前无活动测试句柄。

本轮最终外部CIM只读确认：PID21424已不存在；session43319完整回归exit0，无已知本会话测试残留。
