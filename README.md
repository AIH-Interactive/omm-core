# omm-core

OmniMorph 跨版本 native 库设计：Figura + YSM、SIMD + GPU、native ysmparser。

- [实现计划](docs/OmniMorph-native-plan.md)
- [Native API 设计（统一 `omm_` 前缀）](docs/OmniMorph-native-api.md)
- [手动 GitHub Actions 配置](.github/workflows/native-manual.yml)
- [M0 证据与契约冻结](docs/m0/M0-contract.md)
- [M1 构建与 ABI](docs/m1/M1-build-abi.md)：`tools/build-native.ps1 -Target windows-x64 -OutputDirectory dist/windows-x64`

- [M2 native ysmparser](docs/m2/M2-ysmparser.md)：raw/ZIP/YSGP/当前 Asset Container、可配置并行解析、C ABI/JNI 资源读取与差分工具。

M2 实施与自动验收已完成，正完成阶段提交检查点；M3–M6 的模型、SIMD、GPU 和宿主集成尚未实施。格式覆盖、性能目标及未验证项见各阶段报告。
