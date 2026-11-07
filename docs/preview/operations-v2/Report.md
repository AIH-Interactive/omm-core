# 独立骨骼操作计时 v2

每类本轮重新随机抽样、加载一次。真实骨架与inverse bind，受控局部平移/旋转；无窗口、无Lua/动画/GL。每操作10000预热，5轮×2000次，轮转操作顺序，表为轮均值中位数（µs）。正耗时差表示比同轮Java慢。原始轮次全部保留。

候选为独立preview_pose_candidate.dll，不链接或替换omm_core.dll。初始化复制并校验固定拓扑，缓存直接缓冲地址，每实例持有连续输入/world/palette/visibility存储。逐帧保留finite-affine校验、double中间值、world/palette回填及可见性继承。单调用线程，无工作线程；尚未支持生产子范围API。SSE2保持每项加法顺序，未开启fast-math。

| 类别 | 骨骼数 | Java组合 | 候选 | 耗时 | 差值 | 相对Java |
|---|---:|---:|---|---:|---:|---:|
| ysm-folder | 227 | 6.888 | native_compose_cycle | 17.421 | +10.533 | +152.93% |
| ysm-folder | 227 | 6.888 | candidate_prepared_cycle | 12.863 | +5.975 | +86.76% |
| ysm-encrypted | 582 | 17.933 | native_compose_cycle | 43.675 | +25.741 | +143.54% |
| ysm-encrypted | 582 | 17.933 | candidate_prepared_cycle | 33.129 | +15.196 | +84.73% |
| figura | 67 | 1.971 | native_compose_cycle | 5.186 | +3.215 | +163.15% |
| figura | 67 | 1.971 | candidate_prepared_cycle | 3.827 | +1.856 | +94.16% |
| ysm-legacy | 206 | 6.239 | native_compose_cycle | 15.847 | +9.608 | +153.99% |
| ysm-legacy | 206 | 6.239 | candidate_prepared_cycle | 11.803 | +5.564 | +89.18% |

组合周期含相同输入复位，native还包含打包/JNI/回填，不是纯native内核耗时。最优组合尚未确认；所有候选仍慢于Java，暂不集成。下一步评估直接姿态参数计算或减少结果往返；不能以改变可见性、world查询或精度造成画质损失来换成绩。

日志：build/preview/test-operations-v2.log，exit 0。生产DLL/JAR未改。v2是在SSE2候选加入前运行；v3同时测量标量和SSE2候选。
