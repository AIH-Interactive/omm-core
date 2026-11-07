# 首步生产内核接入操作计时 v9

omm_core.dll的预览JNI单线程路径已接入整批AVX2骨骼计算。复用runtime.active_kernel检测，仅AVX2+单线程分支进入；其余原路径保留。仍用原句柄/锁、逐次拓扑校验、输入打包、world/palette对象回填。子范围索引保持原接口。与独立最佳组合相比，预备元数据和每实例直接结果缓冲尚未接入。

为保持原接口语义，生产AVX2采用完整4×4乘法，保留inverse输入末行参与计算；未擅自假定原接口inverse已校验为仿射。单独编译AVX2，不启用fast-math/FMA收缩。

本轮只操作计时，无窗口、无动画/Lua执行。每类随机抽样并加载一次，真实骨架与inverse bind，受控局部姿态。每操作10000预热、5×2000测量；表为轮均值中位数µs，完整原始数据保留。比较相同compose_cycle，均含输入复位；native含打包/JNI/回填。

| 类别 | 骨骼数 | Java更新周期 | 集成native更新周期 | 耗时差 | 相对Java |
|---|---:|---:|---:|---:|---:|
| ysm-folder | 93 | 2.758 | 3.450 | +0.692 | +25.10% |
| ysm-encrypted | 409 | 12.434 | 14.857 | +2.423 | +19.48% |
| figura | 52 | 1.515 | 2.016 | +0.501 | +33.04% |
| ysm-legacy | 206 | 6.194 | 7.578 | +1.384 | +22.34% |

构建PASS：build/preview/build-integrated-pose-avx.log；计时exit 0：build/preview/test-operations-v9.log。本轮没有执行正确性实窗或最终CPU/GPU性能矩阵，不能标记完整集成、画质或FPS通过。生产JAR未修改，DLL已重建。下一步接入预备元数据和每实例直接缓冲并验证消费者语义。
