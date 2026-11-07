# 独立姿态发布操作计时 v6

本轮四类重新随机抽样，每类加载一次。同轮共享真实骨架、inverse bind与受控local矩阵；不运行动画、Lua、GL或窗口。每操作10000预热，5轮各2000次，轮间轮转顺序；全部原始轮次保留。单位µs，表为轮平均值中位数。

发布操作的共同输出契约为每实例保留的world/palette/visibility缓冲。Java采用直接打包，避免旧cycle测试的冗余输入复位；native输入打包后直接写结果缓冲，不回填Matrix4f。两者只比较同一发布契约，不与旧compose_cycle跨操作比较。生产Java目前有矩阵对象消费者，未来集成仍需评估访问成本，此表不是生产FPS。

| 类别 | 骨骼数 | Java发布 | 候选 | 耗时 | 差值 | 相对Java |
|---|---:|---:|---|---:|---:|---:|
| ysm-folder | 138 | 4.579 | candidate_direct_publish | 5.069 | +0.490 | +10.71% |
| ysm-folder | 138 | 4.579 | candidate_affine_publish | 4.796 | +0.217 | +4.74% |
| ysm-folder | 138 | 4.579 | candidate_checked_simd_publish | 4.015 | -0.564 | -12.32% |
| ysm-encrypted | 208 | 6.913 | candidate_direct_publish | 7.733 | +0.820 | +11.86% |
| ysm-encrypted | 208 | 6.913 | candidate_affine_publish | 7.349 | +0.436 | +6.31% |
| ysm-encrypted | 208 | 6.913 | candidate_checked_simd_publish | 6.133 | -0.780 | -11.28% |
| figura | 3500 | 119.848 | candidate_direct_publish | 148.731 | +28.883 | +24.10% |
| figura | 3500 | 119.848 | candidate_affine_publish | 142.036 | +22.187 | +18.51% |
| figura | 3500 | 119.848 | candidate_checked_simd_publish | 123.395 | +3.546 | +2.96% |
| ysm-legacy | 206 | 6.831 | candidate_direct_publish | 7.668 | +0.837 | +12.26% |
| ysm-legacy | 206 | 6.831 | candidate_affine_publish | 7.234 | +0.403 | +5.90% |
| ysm-legacy | 206 | 6.831 | candidate_checked_simd_publish | 6.026 | -0.804 | -11.77% |

组合：预校验固定拓扑、缓存缓冲地址、连续double world、每实例直接发布。direct使用完整SSE2矩阵乘法；affine利用已校验的仿射末行省去零乘法；checked_simd另将逐帧finite校验向量化，NaN/Inf仍被拒绝。没有启用fast-math或多线程。

生产代码/JAR/DLL未修改；只是独立test DLL及入口。仍有下降，未选择最终最优组合。当前只计时，不声明本轮正确性或画质通过。

运行exit 0；日志build/preview/test-operations-v6.log。对应阶段源码增加了候选，旧行保留作为同轮对照；复现入口见tools/preview/tests/pose-candidate/README.md。
