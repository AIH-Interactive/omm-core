# 每实例palette缓冲接入计时 v10

生产Java增加每个Animation.State独立的JVM直接palette缓冲。JNI按offset写该状态缓冲，现有world/palette对象仍同步；单模型与多实例渲染器直接用状态缓冲执行CPU transform或GPU上传，省去渲染前逐矩阵再次打包。原Java路径和无native更新路径仍从矩阵对象打包。

本轮操作接口保留world/palette对象并产出palette缓冲，java_objects_and_palette与native_objects_and_palette比较。Java对照使用直接Matrix4f.get打包，不人为采用更慢的逐字段写入。受控local姿态、真实层级/inverse bind，每类加载一次，10000预热+5×2000，表为轮均值中位数µs。

| 类别 | 骨骼数 | Java对象+palette | native对象+palette | 耗时差 | 相对Java |
|---|---:|---:|---:|---:|---:|
| ysm-folder | 153 | 4.992 | 5.383 | +0.390 | +7.82% |
| ysm-encrypted | 419 | 13.929 | 14.878 | +0.949 | +6.81% |
| figura | 1319 | 45.249 | 52.592 | +7.343 | +16.23% |
| ysm-legacy | 206 | 6.851 | 7.530 | +0.679 | +9.91% |

构建PASS：build/preview/build-direct-palette-java.log；操作计时exit0：test-operations-v10.log。JAR已重建，DLL沿用v9。此测试测到生产JNI写指定缓冲，不覆盖Animation.State包装及真实渲染器调用的端到端正确性，后续仍需验证。没有运行实窗/动画/Lua，不报告FPS或画质通过。

预备元数据尚未接入，矩阵回填仍保留；全部下降如实保留，完整组合未完成。Figura苍梧吟月Pro.zip的fs2000.png外部纹理缺失提示保留在日志，解析后完成计时。
