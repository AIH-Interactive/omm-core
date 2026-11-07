# 独立骨骼操作计时 v3

每类本轮重新随机抽样、加载一次。真实骨架与inverse bind，受控局部平移/旋转；无窗口、无Lua/动画/GL。每操作10000预热，5轮×2000次，轮转操作顺序，表为轮均值中位数（µs）。正耗时差表示比同轮Java慢。原始轮次全部保留。

候选为独立preview_pose_candidate.dll，不链接或替换omm_core.dll。初始化复制并校验固定拓扑，缓存直接缓冲地址，每实例持有连续输入/world/palette/visibility存储。逐帧保留finite-affine校验、double中间值、world/palette回填及可见性继承。单调用线程，无工作线程；尚未支持生产子范围API。SSE2保持每项加法顺序，未开启fast-math。

| 类别 | 骨骼数 | Java组合 | 候选 | 耗时 | 差值 | 相对Java |
|---|---:|---:|---|---:|---:|---:|
| ysm-folder | 262 | 7.681 | native_compose_cycle | 19.812 | +12.131 | +157.93% |
| ysm-folder | 262 | 7.681 | candidate_prepared_cycle | 15.463 | +7.782 | +101.32% |
| ysm-folder | 262 | 7.681 | candidate_simd_cycle | 12.895 | +5.214 | +67.89% |
| ysm-encrypted | 262 | 7.749 | native_compose_cycle | 19.894 | +12.145 | +156.72% |
| ysm-encrypted | 262 | 7.749 | candidate_prepared_cycle | 15.387 | +7.638 | +98.56% |
| ysm-encrypted | 262 | 7.749 | candidate_simd_cycle | 12.854 | +5.105 | +65.87% |
| figura | 147 | 4.320 | native_compose_cycle | 11.442 | +7.122 | +164.84% |
| figura | 147 | 4.320 | candidate_prepared_cycle | 8.701 | +4.380 | +101.38% |
| figura | 147 | 4.320 | candidate_simd_cycle | 7.330 | +3.010 | +69.66% |
| ysm-legacy | 206 | 6.132 | native_compose_cycle | 15.597 | +9.465 | +154.37% |
| ysm-legacy | 206 | 6.132 | candidate_prepared_cycle | 12.073 | +5.941 | +96.89% |
| ysm-legacy | 206 | 6.132 | candidate_simd_cycle | 10.129 | +3.997 | +65.19% |

组合周期含相同输入复位，native还包含打包/JNI/回填，不是纯native内核耗时。最优组合尚未确认；所有候选仍慢于Java，暂不集成。下一步评估直接姿态参数计算或减少结果往返；不能以改变可见性、world查询或精度造成画质损失来换成绩。

日志：build/preview/test-operations-v3.log，exit 0。生产DLL/JAR未改。v2是在SSE2候选加入前运行；v3同时测量标量和SSE2候选。
