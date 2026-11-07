# 单线程AVX2候选操作计时 v8

每轮四类随机抽样、每类加载一次；真实骨架/inverse bind配受控local姿态。无窗口/动画/Lua/GL，每操作10000预热、5轮×2000次，轮转顺序。表为轮平均耗时中位数（µs）；所有轮次保留。

仅同world/palette/visibility直接缓冲输出契约比较，尚未包含生产脚本/剔除消费者适配成本，不是FPS。双精度、不启用fast-math、单调用线程。AVX2隔离编译且在调用前检查CPU支持和OS的XMM/YMM状态支持，不自动把不支持记录为PASS。

| 类别 | 骨骼数 | Java发布 | 候选 | 耗时 | 差值 | 相对Java |
|---|---:|---:|---|---:|---:|---:|
| ysm-folder | 206 | 6.962 | candidate_checked_simd_publish | 5.954 | -1.008 | -14.48% |
| ysm-folder | 206 | 6.962 | candidate_avx_publish | 5.680 | -1.281 | -18.41% |
| ysm-folder | 206 | 6.962 | candidate_batched_avx_publish | 3.082 | -3.879 | -55.72% |
| ysm-encrypted | 151 | 4.981 | candidate_checked_simd_publish | 4.316 | -0.664 | -13.34% |
| ysm-encrypted | 151 | 4.981 | candidate_avx_publish | 5.031 | +0.050 | +1.01% |
| ysm-encrypted | 151 | 4.981 | candidate_batched_avx_publish | 2.138 | -2.843 | -57.07% |
| figura | 1319 | 44.537 | candidate_checked_simd_publish | 38.623 | -5.914 | -13.28% |
| figura | 1319 | 44.537 | candidate_avx_publish | 44.886 | +0.349 | +0.78% |
| figura | 1319 | 44.537 | candidate_batched_avx_publish | 19.551 | -24.986 | -56.10% |
| ysm-legacy | 206 | 6.881 | candidate_checked_simd_publish | 6.042 | -0.839 | -12.20% |
| ysm-legacy | 206 | 6.881 | candidate_avx_publish | 6.997 | +0.117 | +1.70% |
| ysm-legacy | 206 | 6.881 | candidate_batched_avx_publish | 3.039 | -3.842 | -55.84% |

avx候选逐矩阵跨编译单元调用；batched_avx把完整骨骼循环、矩阵乘法、8-lane finite检查和double→float转换放入AVX编译单元。整批方案同时改变调用粒度与转换实现，收益不能全部归因于某个单项。

构建与运行exit 0，日志build/preview/test-operations-v8.log。没有修改生产JAR或omm_core.dll。

本轮四类整批AVX2均低于Java耗时，是本轮比较的优先集成候选；不宣称所有模型或最终FPS已达标。下一步补齐生产子范围更新与每实例生命周期，适配world查询、palette上传/CPU变换和剔除消费者，再验证正确性及实际性能。

Figura苍梧吟月Pro.zip报告外部fs2000.png缺失后完成解析，日志保留；计时不证明纹理显示正确。
