# 单线程AVX2候选操作计时 v7

每轮四类随机抽样、每类加载一次；真实骨架/inverse bind配受控local姿态。无窗口/动画/Lua/GL，每操作10000预热、5轮×2000次，轮转顺序。表为轮平均耗时中位数（µs）；所有轮次保留。

仅同world/palette/visibility直接缓冲输出契约比较，尚未包含生产脚本/剔除消费者适配成本，不是FPS。双精度、不启用fast-math、单调用线程。AVX2隔离编译且在调用前检查CPU支持和OS的XMM/YMM状态支持，不自动把不支持记录为PASS。

| 类别 | 骨骼数 | Java发布 | 候选 | 耗时 | 差值 | 相对Java |
|---|---:|---:|---|---:|---:|---:|
| ysm-folder | 582 | 20.356 | candidate_checked_simd_publish | 16.561 | -3.795 | -18.64% |
| ysm-folder | 582 | 20.356 | candidate_avx_publish | 15.772 | -4.584 | -22.52% |
| ysm-encrypted | 208 | 7.091 | candidate_checked_simd_publish | 6.053 | -1.038 | -14.64% |
| ysm-encrypted | 208 | 7.091 | candidate_avx_publish | 7.544 | +0.453 | +6.38% |
| figura | 195 | 6.545 | candidate_checked_simd_publish | 5.487 | -1.058 | -16.16% |
| figura | 195 | 6.545 | candidate_avx_publish | 7.028 | +0.483 | +7.39% |
| ysm-legacy | 206 | 7.092 | candidate_checked_simd_publish | 5.856 | -1.236 | -17.42% |
| ysm-legacy | 206 | 7.092 | candidate_avx_publish | 7.463 | +0.371 | +5.24% |

avx候选逐矩阵跨编译单元调用；batched_avx把完整骨骼循环、矩阵乘法、8-lane finite检查和double→float转换放入AVX编译单元。整批方案同时改变调用粒度与转换实现，收益不能全部归因于某个单项。

构建与运行exit 0，日志build/preview/test-operations-v7.log。没有修改生产JAR或omm_core.dll。
