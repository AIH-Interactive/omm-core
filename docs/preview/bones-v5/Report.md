# bones-v5：Figura 局部平移初始化候选（未采用）

基线生产源码为 6a2f8cb。JFR 使用真实可见窗口、固定小符玄样本与原 Lua，每例 120 预热 + 5×1200 帧；Java/native 进程均 exit 0，记录位于 build/preview/benchmark/profile-v5。JFR 输出 Windows PDH counter 不可用警告，原日志保留，不据此判断整机 CPU 负载。JFR 仅诊断，不计正式 FPS。

顶层采样中，native PreviewFiguraParts.State.update 的 identity 所在行有 166 个样本，Java 同行无样本；两者保存 partToWorld 的行也有样本。该证据指向局部姿态构造，但不是函数耗时的精确归因，内联和采样位置会影响结果。Java/native 分别取得 1080/1058 个带栈样本（包含启动）。

候选仅将 native 分支 identity 后 translate 改为 translation，保持旋转/缩放/父子组合顺序。JAR 构建成功；真实资产 State 隔离、延迟矩阵同步和 CPU/GPU shader 数值对照 exit 0，日志 build/preview/test-figura-translation-window.log。

## 正式实窗性能

1280×720，单模型、单线程，120 预热 + 5×600；四组合 exit 0，均 3000 聚焦采样帧，可见/提交 1。比较只使用本轮同路径 Java。完整表见 Benchmark-CPU.md / Benchmark-GPU.md，原始 CSV 位于 build/preview/benchmark/bones-v5/render。

| 路径 | native FPS | 相对 Java FPS | 相对 Java 变化 |
|---|---:|---:|---:|
| CPU | 347.16 | -21.90 | -5.94% |
| GPU | 377.60 | -65.91 | -14.86% |

未证明该候选有效，不保留生产变更。JOML 1.10.8 本地 javap 字节码确认 translation 内部依旧调用 MemUtil.identity，因此该替换并没有消除矩阵重置本身。候选补丁保存在 translation-candidate.patch，测试时 DLL/JAR SHA256 在 candidate-artifacts.json；源码已恢复 6a2f8cb 并重新构建成功（build-figura-translation-restored.log）。当前 JAR 已不是候选产物，不应与候选哈希混淆。

继续定位局部姿态生成与 JNI 打包/回填的成本；未证明 Present 波动的根因，不能归责于驱动或硬件。所有下降保留，未进行多模型测试，不恢复 M3–M6。人工画质 MANUAL/PENDING。
