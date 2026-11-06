# omm-core

OmniMorph 跨版本 native 库设计：Figura + YSM、SIMD + GPU、native ysmparser。

- [实现计划](docs/OmniMorph-native-plan.md)
- [Native API 设计（统一 `omm_` 前缀）](docs/OmniMorph-native-api.md)
- [手动 GitHub Actions 配置](.github/workflows/native-manual.yml)
- [M0 证据与契约冻结](docs/m0/M0-contract.md)
- [M1 构建与 ABI](docs/m1/M1-build-abi.md)：`tools/build-native.ps1 -Target windows-x64 -OutputDirectory dist/windows-x64`

当前已完成 M1（C ABI、JNI 桥接、Windows 构建入口）；尚无渲染、解析实现或性能验证结果。
