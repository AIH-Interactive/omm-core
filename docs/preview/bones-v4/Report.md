# bones-v4 实窗验证

生产代码检查点 9f21f9f。复用 bones-v3 原抽样四类模型，不换样；1280×720、单模型、Java/native 单线程、CPU/GPU 分开比较。每组合 120 预热 + 5×600 采样，16 组合均 exit 0，采样全部聚焦、可见/提交均为 1。原始配置与帧 CSV 位于 build/preview/benchmark/bones-v4/render；完整元数据和均值保存在 case-summary.json。

| 类别 | CPU 相对 Java FPS | CPU 变化 | GPU 相对 Java FPS | GPU 变化 |
|---|---:|---:|---:|---:|
| ysm-folder | +1.43 | +0.17% | +79.49 | +6.70% |
| ysm-encrypted | -65.17 | -7.67% | -414.59 | -28.49% |
| figura | +20.00 | +5.97% | -19.16 | -4.17% |
| ysm-legacy | +59.90 | +7.23% | +15.95 | +1.41% |

## 一致性证据

PreviewStatePublicationWindowTest 编译与真实可见窗口执行 exit 0，日志 build/preview/test-state-publication-window.log。每个模型只加载一次，三个时刻对照 Java 与 native；同一桥先更新两个不同动画时间的 State，再消费首个实例的缓冲，检查状态隔离、延迟 palette 对象同步和实际 CPU/GPU shader 顶点一致性。Figura 运行原 Lua（1 个脚本）、3512 骨骼（含原版区间）；三个 YSM 按正式配置禁用 Lua，因此不宣称 YSM Lua 全覆盖。

## 结果与下一步

本轮仍未总体达标，所有下降保留，不开始最终多模型矩阵。银狼 GPU 的 Present 为 Java 0.424 ms / native 0.610 ms，差异明显大于骨骼阶段差；实际报告不删除异常帧。下一步先诊断帧呈现与骨骼耗时来源，保留同条件 Java 对照，不能仅靠矩阵微基准推断整帧收益。沙箱进程列表为空，但未取得整机宿主快照，报告保留未完整核查标记。人工视觉为 MANUAL/PENDING，专项数值通过不替代人工画质验收。
