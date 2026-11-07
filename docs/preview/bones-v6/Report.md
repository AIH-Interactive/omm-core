# bones-v6：延迟 Figura 脚本矩阵换算

针对 3500 个模型骨骼的临时实窗计时（原版 12 骨骼另行排除）：

| 阶段 | Java µs | native µs |
|---|---:|---:|
| 局部姿态（Java 同时完成层级/palette） | 443.790 | 336.530 |
| native compose 调用（含打包/回填） | 不适用 | 221.453 |
| 保存脚本 partToWorld 矩阵 | 76.503 | 109.263 |

这是插桩诊断，不作 FPS 对比。首版实例计数被每轮状态重建打断，无完整结果；第二版混入原版骨骼，弃作归因依据；最终按模型骨骼分开累计并覆盖 120 预热后 3000 次更新。原日志 profile-figura-java-separated.log/profile-figura-native-separated.log 保存在 build/preview。所有临时计数、计时及模型骨骼数过滤均已移除。

每帧保留独立 world 和 pivot 快照，把 pivot 平移与像素→方块单位换算推迟到脚本首次读取。重复读取复用换算结果，返回值仍是独立副本；修改未提交 pivot 不改变最后提交的姿态。Java/native 共用该实现，Java 基线也享有相同优化。

实窗回归扩展 PreviewStatePublicationWindowTest：三个时刻逐一检查 3500 个部件在首次查询前修改 pivot、重复查询及修改返回副本，与独立计算的 world/pivot 期望矩阵比较，同时保留实例隔离与 CPU/GPU shader 一致性。编译/执行 exit 0，日志 build/preview/test-lazy-part-world.log。初次测试编译因直接访问 private world 失败，已改为仅测试反射读取，没有修改生产可见性。

## 正式结果

固定原样本小符玄，1280×720，单模型，Java/native 单线程，120 预热 + 5×600；四组合均 exit 0，采样全部聚焦，可见/提交 1。

| 路径 | native FPS | 相对 Java FPS | 相对 Java 变化 |
|---|---:|---:|---:|
| CPU | 345.59 | -26.49 | -7.12% |
| GPU | 440.34 | +36.92 | +9.15% |

保留局部优化检查点，但 CPU 仍下降、整体未达标。不能以单轮 GPU 提升宣称全部路径通过。下一步拆分并减少 compose 打包/回填的约 221 µs 成本；其他三类路径需在后续全矩阵重新验证。最终多模型未开始，不恢复 M3–M6。人工画质 MANUAL/PENDING。完整帧 CSV/config 在 build/preview/benchmark/bones-v6/render，产物哈希与清单随报告保存。
