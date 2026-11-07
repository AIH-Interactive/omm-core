# 独立骨骼操作计时 v1

本轮仅无窗口操作计时，生产路径未改。每类真实模型加载一次，使用其真实骨骼层级、inverse bind和rest visibility；局部姿态是受控平移/旋转，不运行模型动画、Lua、原版玩家、OpenGL或呈现。此表不是正式动画耗时或FPS。

每操作预热10000次，5轮各2000次，轮间轮转操作顺序。下表为每轮平均耗时的中位数，单位µs；原始5轮全部保留在operations.csv。耗时差为候选−Java，负值表示更快。Java/native compose均包含输入复位；native包含Java打包、JNI及world/palette回填，没有把总时间冒称为纯JNI。

| 类别 | 骨骼数 | Java组合 | native组合 | 耗时差 | 相对Java | Java逐字段打包 | 候选直接打包 | 耗时差 | 相对Java |
|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|
| ysm-folder | 149 | 4.466 | 11.534 | +7.069 | +158.29% | 0.527 | 0.387 | -0.140 | -26.57% |
| ysm-encrypted | 409 | 12.905 | 32.522 | +19.616 | +152.00% | 1.466 | 1.144 | -0.322 | -21.94% |
| figura | 3500 | 110.713 | 345.009 | +234.296 | +211.62% | 14.796 | 12.100 | -2.696 | -18.22% |
| ysm-legacy | 206 | 6.144 | 15.493 | +9.349 | +152.18% | 0.738 | 0.576 | -0.162 | -21.95% |

直接打包候选使用JOML Matrix4f.get(index, direct FloatBuffer)，仅适用于本测试直接缓冲。当前native骨骼周期显著慢于Java；打包节省量不足以抵消差额。尚未选出最佳组合，不集成生产路径。下一候选重点是native侧直接消费姿态参数并发布每实例结果，避免Java local构建、通用矩阵组合及往返之间的额外开销。

复现：编译tools/preview/tests/PreviewPoseOperationBenchmark.java到build/preview/test-classes；Java 25使用现有classes及classpath.txt，依次传入本轮sample.json、omm_core.dll、输出CSV路径。运行日志build/preview/test-operations-v1.log，exit 0。随机清单记录本轮种子、模型路径及SHA256；不因结果下降更换样本。
