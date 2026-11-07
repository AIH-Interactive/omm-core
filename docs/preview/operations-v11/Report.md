# 固定范围预校验接入计时 v11

在v10每状态palette输出基础上，单线程桥首次调用native prepare校验(offset,count)的父先子后顺序和覆盖，成功后缓存范围。parents/order为构造时复制到桥的私有缓冲，随后不再修改。重复帧跳过拓扑向量分配和扫描；句柄、范围、缓冲容量、local有限仿射检查仍逐帧保留。多线程仍使用原层级校验/分桶，当前未测多线程。

每类本轮随机抽样、加载一次；真实骨架/inverse bind与受控local姿态。10000预热+5×2000，表为轮均值中位数µs。对照保留world/palette对象并提供直接palette缓冲；不是独立候选的无回填契约，不换算FPS。

| 类别 | 骨骼数 | Java对象+palette | native对象+palette | 耗时差 | 相对Java |
|---|---:|---:|---:|---:|---:|
| ysm-folder | 582 | 24.267 | 25.224 | +0.957 | +3.94% |
| ysm-encrypted | 1315 | 48.629 | 48.886 | +0.257 | +0.53% |
| figura | 195 | 6.428 | 6.353 | -0.076 | -1.18% |
| ysm-legacy | 206 | 7.152 | 6.812 | -0.340 | -4.75% |

native与Java已接近，但两类仍下降，不标记总体达标。实际动画、Lua/剔除消费者、实窗CPU/GPU FPS与完整正确性仍待验证。构建PASS：build-prepared-ranges.log/build-prepared-ranges-java.log；计时exit0：test-operations-v11.log。DLL和JAR均已重建，无活动测试。私有JNI方法签名已变更，交付需使用本次配套JAR与DLL。
