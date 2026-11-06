# M0 证据与契约冻结

日期：2026-11-06。阶段：[实现计划](../OmniMorph-native-plan.md) 第 8 节 M0。本文只记录可从本地参考源码、样本和工具链直接核对的事实；凡需真实游戏或实机的项目标记 `MANUAL/PENDING`。

## 1. 五版 OMP 渲染互操作映射

源码路径前缀：`refs/OMP/<版本>/common/src/main/java/top/aihmc/omnimorph/render/acceleration/`。

| 版本 | Java | 宿主图形接口 | OMP 现有 GPU 路径 | native 互操作结论 |
|---|---|---|---|---|
| 1.20.1 | 17 | LWJGL 直接 GL + `RenderSystem` 状态（`OpenGlAccelerationBackend.java`：`GL15.glGenBuffers`、`GL14.glMultiDrawElements`、`RenderSystem.getShader()` 等） | `OpenGlAccelerationBackend` | 渲染线程上 GL context current，GL 名称为真实整数，可经 proc resolver 进行 native draw：**支持 `omm_backend_gl`** |
| 1.21.1 | 21 | 同上（`shader.setDefaultUniforms(...)` 签名不同） | `OpenGlAccelerationBackend` | 同上：**支持 `omm_backend_gl`**；uniform 映射按版本适配 |
| 26.1.2 | 25 | Blaze3D `com.mojang.blaze3d.systems.GpuDevice` / `RenderPass` / `GpuFence`（`MinecraftGpuDeviceAdapter.java`），仅 GL 后端 | `GpuAccelerationBackend` | 已有 `mixin/render/GlBufferAccessor.java` 读取 `GlBuffer` GL 名称，证明 GL 对象可导出：**支持 `omm_backend_gl`**，需在 RenderPass 边界之外插入 native draw 并恢复宿主状态（M4/M5 验证） |
| 26.2 | 25 | Blaze3D 新接口（`GpuFormat`、`BindGroupLayout`、五参数 `drawIndexed`），原版新增**实验 Vulkan 后端**（`refs/OMP/OMP_26.2/docs/26.2-vulkan-research.md`） | `GpuAccelerationBackend` | GL 后端下：源码中无 `Gl*` accessor，适配器需新增导出 GL 名称的能力后才支持 `omm_backend_gl`；Vulkan 后端下：**native GL draw 不成立** |
| 26.3 | 25 | `com.mojang.renderpearl.api.*`（GpuDevice/RenderPass/GpuFence 迁包），`GpuStorageSupport.java:57` 明确同时处理 GL 与 Vulkan | `GpuAccelerationBackend` | 与 26.2 相同；包名迁移只影响适配器，不影响 native ABI |

不能把 `GpuBuffer`/`GpuTexture` Java 对象或其地址当作 GL 名称（计划第 2 节已约束）。只有能拿到真实 GL 名称和 current context 时才 attach。

### 1.1 不支持的后端（明确记录）

- **Vulkan（26.2+ 实验后端）**：计划第 7 节和交接记录规定当前设计不擅自加入 Vulkan/Metal。当 F3 显示实际后端为 Vulkan 时，适配器不调用 `omm_device_attach`。native 解析、bake 和 SIMD prepare 仍然可用；绘制继续走 OMP 现有 Blaze3D 路径，并由能力报告标明 `native GPU draw unsupported: vulkan`。是否为 Vulkan 增加 native 后端属于计划第 4 节所说的“统一 native 能力升级”，需单独审批（见第 6 节的后续事项）。
- **GLES**：Android 启动器的实际 GL 形态在 M6 记录；ABI 已预留 `omm_backend_gles`。

## 2. 格式与双引擎样本清单

清单由 [`tools/m0/Probe-Samples.ps1`](../../tools/m0/Probe-Samples.ps1) 按内容生成：读取魔数，ZIP 读取条目名，不解密也不解包。结果写入 [`sample-manifest.json`](sample-manifest.json)，包含 SHA-256 和大小。两次连续运行的输出字节完全一致，可重复。样本本体位于被忽略的 `refs/avatars`，不提交、不上传 CI。

| 容器分类 | 数量 | 引擎 | 判定依据 |
|---|---|---|---|
| `directory/ysm-raw-current` | 32 | YSM | 目录内含 `ysm.json` |
| `directory/ysm-raw-legacy` | 3 | YSM | 无 `ysm.json`，含 `main.json` + `*.animation.json`（`YSM_OLD_Model`、`芙宁娜1`、`纳西妲1`） |
| `ysgp-text-header` | 17 | YSM | 可选 UTF-8 BOM + `YSGP` + 文本元数据块；OMP `YsmEncryptedSource` 将其按 crypto v3 处理，M2 再逐个确认 |
| `ysgp-binary-legacy-v2` | 1 | YSM | `YSGP` + 大端 int 2（`old.ysm`），对应 OMP `YsmLegacyDecoder` |
| `directory/figura-avatar` | 2 | Figura | 含 `avatar.json`（`小符玄`、`AI Hobbyist Founder Public Server`） |
| `zip/figura-avatar` | 3 | Figura | ZIP 条目含 `avatar.json` |
| `zip/unrecognized-layout` | 1 | — | `anan-fullmodel-0.0.7-rel.zip` 有 bbmodel/Lua 但缺 `avatar.json`，作为负样本 |

样本缺口：
- 没有 YSGP crypto v1 样本，v1 解码只能依据 OMP 源码对照，必须在 M2 记录为证据缺口，不能宣称已覆盖。
- M3 的自有 fixtures（可分发的最小 Figura/YSM 模型）在 M2/M3 创建，因为第三方模型不得进入 CI。

角色分配（M2–M5 复用）：
- Figura 脚本样本：`小符玄`
- raw YSM 主样本：`01_taisho_maid`
- 旧版 raw：`YSM_OLD_Model`
- 二进制旧版：`old.ysm`
- 文本头：`new.ysm`
- 大体量样本：`芙宁娜v3.14中文配音.ysm`（13.7 MB）
- 混合场景：小符玄 + 01_taisho_maid

## 3. 性能基线

以下方法冻结，复用计划第 9 节：
- 每次测量固定设备、驱动、分辨率、模型 SHA-256（取自上述清单）、相机、VSync 和预热。
- 实例数取 1/10/50，Figura、YSM、混合各至少 5 轮。
- 报告 CPU p50/p95/p99、GPU timer query、draw/JNI 次数、上传字节、GC、native 内存、VRAM 和加载时延。

基线路径分别为 OMP 的 Java 路径、`NativeSimdBackend` 路径和 GPU 路径（1.x 为 `OpenGlAccelerationBackend`，26.x 为 `GpuAccelerationBackend`）。CPU 准备阶段可复用 OMP 自带的 `AccelerationPreparationBenchmark` 在 M3 测量；帧级数值需要真实游戏窗口。

- 冻结目标：CPU p95 -30%，GPU p95 回退不超过 5%，parser p95 -20%，小模型无显著回退。
- 实测基线数值：`MANUAL/PENDING`。本阶段未获启动游戏授权，也没有测量，不提供任何倍率。

## 4. API 冻结项（供 M1 头文件落地）

[API 草案](../OmniMorph-native-api.md) 的结构按原样冻结，M0 只补充以下数值与语义：

- `omm_backend`：`omm_backend_gl = 1`、`omm_backend_gles = 2`；`3` 预留给 Vulkan，当前任何 attach 都返回 `omm_status_unsupported`。`0` 非法。
- 适配器在 attach 之前自行判断宿主实际后端；native 不探测 Minecraft 类。
- 容器分类枚举与第 2 节一致：`directory`、`zip`、`ysgp_legacy_v1`、`ysgp_legacy_v2`、`ysgp_v3`、`memory`。`ysgp_legacy_v1` 保留，但样本缺失。
- ABI major = 0 用于 M1–M5 开发期；首个稳定发布（M6）定为 1.0。在此之前允许按实验结果调整，变更须写入 API 文档。

## 5. 平台工具链核验（本机）

| 项目 | 计划基线 | 本机实测 | 结论 |
|---|---|---|---|
| CMake | — | 4.3.1-msvc1（VS 自带） | 可用 |
| MSVC | MSVC 2022 | Visual Studio 18 Community（`18.10.12217.157`） | 基线改为“VS 2022 或更新”；本地使用 VS 18，CI `windows-latest` 用其自带版本，产物 manifest 记录实际编译器 |
| Ninja | — | 未安装 | Windows 使用 Visual Studio generator |
| JDK | 17/21/25 | `D:\Program Files\Java\17`（17.0.18）、`21`（21.0.9）、`25`（25.0.2） | 三版齐全，可做同一 DLL 加载验证 |
| Python | — | 不可用（只有 Store 别名） | 构建与检查工具统一用 PowerShell |
| Linux/Android/macOS | 见计划第 7 节 | 本地不编译 | 由手动 CI 在 M1/M6 验证 |

## 6. 验收结果

| M0 退出条件 | 结果 |
|---|---|
| 所有目标映射有来源 | PASS：第 1 节每行都给出参考文件 |
| parser 与双引擎样本清单可重复 | PASS：脚本生成，两次运行字节一致 |
| 不支持的后端明确记录 | PASS：第 1.1 节（Vulkan、GLES 待定） |
| 性能实测基线 | `MANUAL/PENDING`（需真实游戏） |

后续事项（不在 M0 范围）：
- 26.2/26.3 的 Vulkan 后端是否需要 native 绘制能力，需由用户决定。
- 26.2/26.3 的 GL 后端需新增 GL 名称导出 accessor，在 M5 实施。
