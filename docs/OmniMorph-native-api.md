# OmniMorph Native API 设计

日期：2026-11-06。状态：v0.1 设计草案，ABI 0.x 开发期。M1 已实现第 1–3 节的版本与 runtime 入口（[`include/omm/omm.h`](../include/omm/omm.h)），其余入口是 M2–M5 的实现合同。实施计划见 [OmniMorph native 计划](OmniMorph-native-plan.md)。

## 1. 命名与稳定边界

所有业务函数、公开 typedef、结构体 tag、常量、宏及回调 typedef 均以小写 `omm_` 开头。动态库名为 `omm_core.dll` / `libomm_core.so` / `libomm_core.dylib`。导出使用 C linkage；Windows 使用 cdecl；不跨 ABI 传递 STL、C++ 异常、C++ bool 或平台宽度的 long。

JNI 通过 `JNI_OnLoad` 注册固定类 `top.aihmc.omm.NativeApi` 的 `omm_` 方法；JNI 规定的生命周期入口是命名前缀例外。C 调用者可直接调用相同语义的 C API。生产库不以 Minecraft 版本命名，不依赖游戏映射名。

外部 ABI 使用固定宽度整数、IEEE-754 float/double、UTF-8 与不透明 64 位句柄。首批仅支持 64 位进程。句柄 0 无效，非零值是带 generation 的表索引，不能作为地址解引用。所有函数返回 `omm_status`，输出参数最后排列；诊断通过调用方提供的 `omm_error*` 返回，可传 NULL。

## 2. 基础类型

以下为拟公开头文件的核心声明片段；宏的编译器分支及完整 descriptor 定义由 M1 生成正式头文件，本页后续表格规定其字段语义。

```c
typedef int32_t omm_status;
typedef uint64_t omm_handle;
typedef uint32_t omm_cpu_mode;
typedef uint32_t omm_render_mode;

typedef struct omm_bytes {
    const uint8_t *data;
    uint64_t size;
} omm_bytes;

typedef struct omm_mut_bytes {
    uint8_t *data;
    uint64_t capacity;
} omm_mut_bytes;

typedef struct omm_error {
    uint32_t struct_size;
    int32_t code;
    uint32_t stage;
    uint32_t reserved;
    uint64_t source_offset;
    char message[512];
} omm_error;
```

`omm_bytes` 同时表示二进制或有明确字段说明的 UTF-8；字符串不要求 NUL 结尾。`omm_error.message` 是 NUL 结尾 UTF-8，截断不能截断码点；它仅供诊断，业务按 code 分支。未知 source_offset 使用 UINT64_MAX。不提供共享的全局 last-error，以免多线程覆盖。

除 bytes/view 这种固定叶子结构外，所有公开 descriptor 第一个字段为 `uint32_t struct_size`，第二个为 `uint32_t version`。调用方清零 reserved 与尾部，库按已知前缀读取；缺少必需字段、未知必需 flags 或不相容 major 返回错误。禁止用 `#pragma pack(1)` 改变所有结构对齐；正式头文件对 sizeof/offsetof 生成 C/C++/Java 一致性测试。

| 状态常量 | 值 | 语义 |
|---|---:|---|
| `omm_ok` | 0 | 操作成功，合法空结果也成功 |
| `omm_error_argument` | 1 | 参数、范围、非有限数值或结构尺寸无效 |
| `omm_error_abi` | 2 | ABI/schema 不兼容 |
| `omm_error_handle` | 3 | 错误类型、已释放或 generation 过期 |
| `omm_error_unsupported` | 4 | 已识别但当前能力不支持 |
| `omm_error_format` | 5 | 容器、JSON、索引或模型结构损坏 |
| `omm_error_limit` | 6 | 超过配置预算 |
| `omm_error_memory` | 7 | 内存分配失败 |
| `omm_error_capacity` | 8 | 输出空间不足，返回所需容量 |
| `omm_error_thread` | 9 | 在错误线程调用设备操作 |
| `omm_error_device_lost` | 10 | 上下文丢失或设备 generation 失效 |
| `omm_error_shader` | 11 | shader/pipeline 编译或兼容性验证失败 |
| `omm_error_state` | 12 | 调用顺序、宿主 pass 或对象状态错误 |
| `omm_error_partial_submit` | 13 | 已发出部分 draw，不能整体重放 |
| `omm_error_internal` | 14 | native 内部异常；诊断保留原阶段 |

所有 C++ 异常在 C/JNI 边界转换为状态；不采用静默空模型。错误不保证已发生 GPU draw 可回滚，具体见提交协议。

## 3. 版本、运行时与能力

```c
omm_status omm_get_version(omm_version *out_version, omm_error *error);
omm_status omm_runtime_create(const omm_runtime_desc *desc,
                              omm_handle *out_runtime, omm_error *error);
omm_status omm_runtime_get_caps(omm_handle runtime,
                                omm_runtime_caps *out_caps, omm_error *error);
omm_status omm_runtime_destroy(omm_handle runtime, omm_error *error);
```

| 结构 | 必需字段与语义 |
|---|---|
| `omm_version` | struct_size/version、abi_major/minor、asset_schema、mesh_schema、cache_version、构建 ID 的定长 UTF-8 字段 |
| `omm_runtime_desc` | ABI 请求、cpu_mode、worker_count、总 CPU 内存预算、解析默认限额；worker_count=0 按可用逻辑核心数一核一个执行线程，含调用线程；1 为串行，非零值显式指定总执行线程数 |
| `omm_runtime_caps` | CPU 可用 kernel 位图、实际 kernel、已编译 parser 格式位图、运行时版本；不在未附着 device 时报告 GPU 可用 |

CPU 枚举：`omm_cpu_auto=0`、`omm_cpu_scalar=1`、`omm_cpu_sse2=2`、`omm_cpu_avx2=3`、`omm_cpu_neon=4`。显式指定不可用 kernel 返回 unsupported，不能悄悄降级导致测试误判。auto 按已核验 CPU/OS 能力选择。

运行时退出前必须释放所有子句柄；仍有活动子对象返回 state，不强制释放调用方仍在使用的资源。不同 runtime 的句柄不能混用。

`worker_count` 表示每次同步 CPU 操作的总执行线程上限，调用线程也计入；独立任务少于线程数时只启动实际需要的线程，不创建空闲线程。M2 对单个模型内部的动画/controller 资源及动画文件内独立 clip 并行解析，再按输入顺序合并，不改变事件、诊断或资源序号；文件与 clip 两层共享该线程上限。调用方同时发起多个同步操作时，每个操作分别遵守此上限。

CPU 内存预算用于同一 runtime 的资产及 section 缓存累计容量核算：包含资源、规范化模型、字符串及容器容量，索引按保守开销计入；超限返回 `omm_error_limit`，失败不发布资产，release 归还计数。解码输入/展开限额同时不得超过该预算。`omm_asset_info.retained_bytes` 返回该资产当前核算容量（含已缓存 section）；这是库拥有数据的容量核算，不是进程 RSS 或分配器元数据/临时峰值的硬限制。

## 4. ysmparser API

```c
omm_status omm_ysm_probe(omm_bytes input,
                         omm_ysm_probe_result *out_result, omm_error *error);
omm_status omm_ysm_parse(omm_handle runtime, omm_bytes input,
                         const omm_ysm_parse_desc *desc,
                         omm_handle *out_asset, omm_error *error);
omm_status omm_ysm_parse_entries(omm_handle runtime,
                                 const omm_ysm_entry *entries, uint32_t count,
                                 const omm_ysm_parse_desc *desc,
                                 omm_handle *out_asset, omm_error *error);
omm_status omm_asset_get_info(omm_handle asset,
                              omm_asset_info *out_info, omm_error *error);
omm_status omm_asset_read_section(omm_handle asset, uint32_t section,
                                  omm_mut_bytes output,
                                  uint64_t *out_required, omm_error *error);
omm_status omm_asset_read_resource(omm_handle asset, uint32_t resource_id,
                                   uint64_t offset, omm_mut_bytes output,
                                   uint64_t *out_written, omm_error *error);
omm_status omm_asset_release(omm_handle asset, omm_error *error);
```

`omm_ysm_probe_result` 返回格式、源版本、是否可解析和所需最低输入长度；probe 不解压全包，不等于 parse 校验通过。格式常量为 `omm_ysm_unknown`、`omm_ysm_raw`、`omm_ysm_zip`、`omm_ysm_legacy_v1`、`omm_ysm_legacy_v2`、`omm_ysm_legacy_v3`、`omm_ysm_asset_container`。未知格式返回 unsupported，已识别但截断/损坏返回 format。

`omm_ysm_entry` 包含 UTF-8 相对路径与 bytes；`parse_entries` 的条目集合表达 raw 目录，输入全部同步借用。`parse` 用于完整容器字节。两者返回前完成输入消费，不能保存输入地址；成功返回的 asset 拥有后续所需数据。

`omm_ysm_parse_desc` 字段：size/version、source_profile、允许格式位图、input_bytes_limit、expanded_bytes_limit、entry_bytes_limit、entry_count_limit、nesting_limit、bone_limit、vertex_limit、keyframe_limit。0 表示采用 runtime 已公布默认值，不表示无限。实际默认值在 M0 根据合法样本冻结，正式头文件与查询结果同时提供。

source_profile 区分 `omm_profile_omp_compat` 与 `omm_profile_ysm_current`。同样字段在两个来源中有不同默认值时不能混用；不支持的组合返回 unsupported。

M2 中 0 按格式选择来源：raw/ZIP/YSGP 使用 OMP 兼容语义；当前 Asset Container 使用 YSM 语义。显式 `omm_profile_ysm_current` 只接收当前容器，显式 OMP profile 不接收它。当前支持容器版本 `0.1.0-unstable`、`mixel/character` schema `0.1.1-unstable`；校验 BLAKE3 头/块摘要并支持 raw/zstd 块。ED25519 和其他模型 schema 返回 unsupported。

当前 protobuf 时间以 tick 编码，输出统一转为秒；几何直接生成与既有 asset 相同的 quad 数据。Portable Program 以 `program: {format, source}` 或 `program: {format, bytecode_hex}` 交给宿主。存在 program 时不得把 component 的数值 fallback 当作表达式结果；native 不解释或执行字节码。原始 manifest/definition/blob 块保留为资源，metadata 的 manifest_resource 和身份字段指向其来源。

`omm_asset_info` 返回来源格式/版本、target/geometry/bone/animation/resource 数量、retained bytes 和 schema 版本。asset 内部结构不暴露；`omm_model_create_from_asset` 直接消费其几何，省去反序列化到 Java。

`omm_asset_read_section` 的 section 包括 `omm_section_metadata`、`omm_section_animations`、`omm_section_controllers`、`omm_section_expressions`、`omm_section_resources`、`omm_section_diagnostics`。M2 输出 schema v1 的 UTF-8 JSON，正式 JSON Schema 随 SDK 发布：

定义见 [`schemas/asset-section-v1.schema.json`](../schemas/asset-section-v1.schema.json)，安装于 `share/omm_core/schemas/`。

- 每节有 schema_version 和数组 records；所有引用用稳定的 asset 内 uint32 ID，不用内存地址。
- 动画：名称、length 及“未声明/无界”标记、loop、bone/channel、时间、pre/post、插值、blend weight、同时间事件的 ordinal；不把无界编码成非法 JSON Infinity。
- 表达式：原文、语言/角色、来源相对路径、UTF-8 字节区间；数值和表达式使用 tagged union，不把表达式当数值 0。
- controller：state、transition 的目标与条件引用、entry/exit/event 顺序。
- 资源：ID、规范路径、媒体类型、长度；不内嵌大块图片或音频 base64。
- 未知可选字段可忽略，未知必需语义升级 schema。坐标转换、默认值与关键帧排序只做一次。

可用空 output 查询 required；空间不足返回 capacity 且不部分写出 JSON。按需单次读取这些节供宿主绑定动画，不允许逐帧重新序列化。资源读取允许分块，返回实际字节，EOF 成功且 written=0。读取 asset 不执行脚本、不创建 GL 对象、不解压写磁盘。失败的 parse 将 out_asset 置 0，临时资源全部释放；诊断含条目路径/offset，严重错误不发布部分模型。

## 5. 模型、实例和姿态

```c
omm_status omm_model_create_from_asset(omm_handle runtime, omm_handle asset,
                                       const omm_bake_desc *desc,
                                       omm_handle *out_model, omm_error *error);
omm_status omm_model_create(omm_handle runtime, const omm_model_desc *desc,
                            omm_handle *out_model, omm_error *error);
omm_status omm_model_release(omm_handle model, omm_error *error);
omm_status omm_instance_create(omm_handle model,
                               omm_handle *out_instance, omm_error *error);
omm_status omm_instance_update(omm_handle instance,
                               const omm_pose_update *update, omm_error *error);
omm_status omm_instance_query_parts(omm_handle instance,
                                    const uint32_t *part_ids, uint32_t count,
                                    omm_part_result *out_parts, omm_error *error);
omm_status omm_instance_release(omm_handle instance, omm_error *error);
```

`omm_bake_desc` 选择 asset target/geometry ID、来源坐标 profile、纹理 alpha 分类数据与预算。尚无纹理 alpha 时不能按“不透明”误剔除。返回 model 持有所需 backing，调用方之后可以释放 asset 外部引用。

`omm_model_desc` 是 Figura 适配器的入口，也可用于测试合成模型，包含：

| 字段组 | 合同 |
|---|---|
| parts | 连续 uint32 part ID；parent ID，根为 UINT32_MAX；局部 bind matrix；验证无环 |
| vertices | position float32[3]、normal float32[3]、UV float32[2]、part_id uint32；基础记录 36 bytes，无隐式 padding；切线是可选独立流 |
| indices | uint32 三角形索引；合法空 mesh；每个三角形的刚性 part 归属一致 |
| surfaces | first_index/index_count/material_id、透明/双面/正反面策略、稳定 ordinal |
| materials | 引擎语义 ID、主/次纹理槽、alpha/emissive/light/overlay 约定，不内嵌 GL ID |
| profile | `omm_geometry_figura` 或 `omm_geometry_ysm`；用于明确来源转换，不用引擎名猜坐标 |

模型创建同步复制/规范化输入，返回后可释放原数组。内部 CPU/GPU 布局不是公开文件格式。多实例共享 model，修改几何需建新 model 后切换，不修改正在被使用的不可变 model。

规范空间使用游戏方块单位、Y 向上、列主序矩阵、列向量 `world = parent * local * position`。来源坐标/1:16 比例由 profile 转换一次；UV 在规范数据中为归一化数值，方向转换归 profile。角度不出现在 pose ABI，防止度/弧度混用。normal matrix 独立保存逆转置语义；singular pose 处理规则在 M0 对照原实现冻结，不能任意改成单位矩阵。

`omm_pose_update` 包含单调 revision、full/delta 标志、part IDs、float64 local matrices、可见性、颜色/UV/material overrides。CPU 保留必要 double 精度，上传 GPU 时按已验证策略转 float；camera-relative 根矩阵避免远坐标精度丢失。重复 ID、越界或非法值拒绝整次更新，旧 revision 保持不变。prepare 不做动画 tick。

`omm_instance_query_parts` 返回指定部件的规范模型空间矩阵、normal matrix 和可见性，供脚本/附着点消费；宿主另乘 entity/world 变换。不回传全部顶点。一次 query 与 update 对同一 instance 串行。

## 6. 设备与 GPU 互操作

```c
typedef void (*omm_proc)(void);
typedef omm_proc (*omm_get_proc_fn)(void *user, const char *name);

omm_status omm_device_attach(omm_handle runtime, const omm_device_desc *desc,
                             omm_handle *out_device, omm_error *error);
omm_status omm_device_get_caps(omm_handle device,
                               omm_device_caps *out_caps, omm_error *error);
omm_status omm_device_invalidate(omm_handle device, omm_error *error);
omm_status omm_device_collect(omm_handle device, omm_error *error);
omm_status omm_device_detach(omm_handle device, omm_error *error);
```

`omm_device_desc` 包含 size/version、`omm_backend_gl` / `omm_backend_gles`、宿主 context identity/generation、proc resolver/user、VRAM/上传预算。attach 时上下文必须 current，库记录当前线程；不创建窗口、不 makeCurrent、不 swap。resolver 的函数指针调用约定由平台 GL 定义，宿主在 device 寿命内保证其有效。JNI 桥接需有受控平台 resolver，不能把 Java 方法引用伪装成 C 函数指针。

`omm_device_caps` 独立报告基础 native GPU draw、UBO/texture-buffer/SSBO/compute 能力、buffer 对齐与最大容量、已验证 shader profile、当前 generation。是否支持某个材质/pass 还需逐次 preflight。库编译含 GPU 代码不等于 device 已能使用。

宿主纹理/program/FBO 用显式导入的 descriptor（object kind、native name、context generation、语义 profile、借用范围）描述；texture ID 的整数相同但 generation 不同仍无效。对象由宿主持有，不由 native 删除。native 自己创建的 buffer/shader/fence 由 device 回收。M0 必须验证每版适配器可合法导出这些信息；无法导出时报告 unsupported，不能透传 Java GpuBuffer 指针。

invalidate 停止新 prepare/draw，旧 prepared 不可继续提交。collect 在渲染线程非阻塞检查 fence 并回收已退休资源；detach 只在 prepared/frame 均释放、已提交资源安全回收后成功，未满足时返回 state。实际 context 已丢失时销毁 CPU 记录而不向失效 context 发 GL 调用。

## 7. 帧、准备与绘制

```c
omm_status omm_frame_begin(omm_handle device, const omm_frame_desc *desc,
                           omm_handle *out_frame, omm_error *error);
omm_status omm_render_prepare(omm_handle frame,
                              const omm_draw_item *items, uint32_t count,
                              const omm_prepare_desc *desc,
                              omm_handle *out_prepared, omm_error *error);
omm_status omm_render_draw(omm_handle frame, omm_handle prepared,
                           const omm_pass_desc *pass,
                           omm_submit_result *out_result, omm_error *error);
omm_status omm_prepared_release(omm_handle prepared, omm_error *error);
omm_status omm_frame_end(omm_handle frame,
                         omm_frame_stats *out_stats, omm_error *error);
```

`omm_frame_desc`：frame ID、context generation、camera-relative view/projection、viewport、时间戳。一个 device 同时最多一个活动 frame。frame_end 不交换窗口；成功后释放 frame 句柄，并把已提交 GPU 资源转入 fence 退休队列。失败帧也必须结束并释放所有 prepared。

`omm_draw_item`：instance handle、确切 pose revision、根变换、材质覆盖、part 范围/可见性过滤、stable order。`omm_prepare_desc`：render_mode、scratch 预算与语义段 ID。render mode 为 `omm_render_auto=0`、`omm_render_gpu=1`、`omm_render_cpu_simd=2`。两种显式模式在不支持时返回 unsupported；auto 在提交前按能力选择。

prepare 捕获不可变 pose/material 快照并持有所需 model/instance backing；之后 instance update 不改变已准备结果。只能在同一 frame/device generation 使用 prepared。多个 pass 可以消费同一个快照，但事件不重放；pass-specific 透明排序需在该 pass 的 camera/语义下重新准备，不能复用错误顺序。

`omm_pass_desc` 必须完整表达 pass ID、generation、宿主绑定的 framebuffer、viewport/scissor、shader/material profile、纹理与 sampler、light/overlay/fog、blend/depth/cull、前后宿主状态恢复契约。透明批次保持宿主顺序；只有状态等价且无回调/宿主任务屏障的相邻段可以合并。基础 profile 由库编译 shader；宿主 shader pack profile 必须具备经验证的输入布局、资源和语义映射，不在 native 中任意拼接未知 shader。

| 提交结果 | status 与语义 | 允许行为 |
|---|---|---|
| `omm_submit_none=0` | draw 前被拒绝，issued_draws=0 | 可在明确兼容时准备另一条路径 |
| `omm_submit_complete=1` | omm_ok；含合法空 draw，issued_draws 可为 0 | 按宿主当前消费合同接管 |
| `omm_submit_partial=2` | partial_submit/device_lost；issued_draws 为已发出的数量 | 禁止整段回放，记录并终止 |

`omm_submit_result` 始终初始化，含 disposition、selected_mode、issued_draws、submitted_indices、generation。complete 表示命令已提交成功，不表示 GPU fence 已完成。GPU 延后错误由设备诊断报告，不能伪造可回滚事务。

Figura 回调示例顺序：宿主 preRender → update/query → midRender → prepare/draw 该段 → 宿主 RenderTask → 子部件 → postRender。精确顺序以参考调用路径为准；不得为了“一帧一次 JNI”改变回调可观察状态。无回调区域可批量 prepare。

## 8. 统计、线程与所有权

`omm_frame_stats` 返回实际 cpu_mode/render_mode、draw 次数、JNI 桥接计数（由桥接层合并）、上传字节、CPU prepare 时间、GPU timer 是否 ready、有效 GPU 时间、resident/retired GPU bytes。异步 GPU 时间没有 ready 时标记未就绪，不返回 0 冒称无成本。

| 对象/操作 | 线程规则 | 持有关系 |
|---|---|---|
| runtime create / parse / model bake | CPU worker 可用；同一可变对象不并发修改 | runtime 拥有注册表，子对象显式释放 |
| asset 读取 | immutable，可并发；release 与借用互斥 | model 持有所需 backing，外部 release 不提前释放 |
| instance update/query | 同实例串行，不同实例可并发 | instance 持有 model |
| device / frame / prepare / draw / collect | 当前绑定渲染线程 | prepared 持有快照和资源；GPU 使用期间 fence 保活 |
| release | CPU 对象可在结束借用后释放；GPU 对象在渲染线程 | 逻辑释放立即失效句柄，物理回收可能延后 |

每次同步 API 返回前结束其内部 worker；不持有 JNI array 临界区执行 GL 或长期工作。跨线程没有隐式 Java callback。错误时新建 out_handle=0；已有对象修改保持原状态，除非接口明确声明 device invalidation 或 partial submit。禁止把本库分配的内存交给宿主 free；所有复制输出由调用者分配。

## 9. JNI 映射与调用流程

Bridge 使用同名 `omm_` 方法封装 C API：handle→Java long、uint32 有范围检查、bytes→DirectByteBuffer+offset+length、descriptor→版本化 direct buffer。缓冲区的 offset/length 均是字节；检查 direct、容量、对齐和溢出。输入 ByteBuffer position 不被隐式推进。错误转换为包含 code/stage/message 的固定 bridge 异常，不依赖游戏类。

Java descriptor 的具体 offset 由正式 C 头文件生成并在三种 JDK 验证，不直接把 JVM object 布局作为 C struct。API 文档未冻结前，不手写散落于五版模组的常数。

典型 YSM 流程：

1. `omm_get_version` → `omm_runtime_create` → 能力查询。
2. `omm_ysm_probe` → `omm_ysm_parse`，或 raw 用 `omm_ysm_parse_entries`。
3. 按需读取 metadata/animation/controller/expression 节，宿主绑定 Molang 与游戏查询。
4. `omm_model_create_from_asset` → `omm_instance_create`；释放不再需要的 asset 外部引用。
5. 渲染线程 `omm_device_attach`；每帧宿主求值后 `omm_instance_update`。
6. `omm_frame_begin` → 有序 `omm_render_prepare` / `omm_render_draw` → release prepared → `omm_frame_end`。
7. 定期 `omm_device_collect`；卸载时释放实例/model，回收完后 detach，再 runtime_destroy。

Figura 从 `omm_model_create` 接入，其余生命周期相同；保留回调与 RenderTask 屏障。

## 10. ABI 冻结与验收

M0/M1 必须补齐正式 descriptor 头文件、枚举数值、JSON Schema、布局生成器和错误用例，冻结前允许基于互操作实验调整本草案。首个稳定 ABI 发布后，不改变旧字段含义或复用错误码。新字段通过 size/minor 协商；普通 OMP/MC 版本升级不得升级必需 ABI major。确有破坏性协议需求时另行批准，并在统一 native 中保留旧入口/语义，不能要求历史模组迁移后才能运行。

### 未来 OMP 兼容合同

- 最低稳定 ABI 定义双引擎、SIMD/GPU 与 ysmparser 的基础能力集；未来 OMP 的基本运行不得依赖之后追加的可选入口。
- 新 OMP 先调用既有 `omm_get_version` 与能力查询，再构造旧库可接受的 descriptor 前缀与版本。不能仅凭 minor 较低就拒绝，也不能向旧库发送它不理解的必需 flags。
- JNI 基础方法注册集合与签名永久稳定；新增可选方法由新版库探测后注册，旧库仍只注册原集合。Bridge 类保留旧声明和签名，新 OMP 在能力确认前不得调用可选 native 方法，避免旧库加载失败或 UnsatisfiedLinkError。
- 字段缺省、错误码、枚举含义、句柄寿命和调用顺序是 ABI 的一部分；保持符号名但改变这些语义同样属于破坏兼容。
- 新 native 必须接受历史 descriptor 与 schema；内部缓存可失效重建，但不能因此拒绝旧适配器的合法输入。新格式或新图形 API 的实现需求独立评估，不把版本协商当作该能力已存在。
- 发布验证固定两个方向：新 OMP + 归档最低稳定 native，以及历史 OMP + 最新统一 native。逐目标核对同一二进制 SHA-256，独立覆盖 SIMD/GPU 和双引擎，不允许靠测试时重新编库满足通过条件。

验收包括：导出符号前缀、C/C++ 头文件、三 JDK 同库加载、句柄过期/跨 runtime、缓冲区不足、parse 失败清理、格式差分、SIMD 强制路径、GPU 强制路径、回调快照、部分提交不重放、热重载/fence、双引擎同库。API 自测通过与真实游戏呈现通过分别记录。
