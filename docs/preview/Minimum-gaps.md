# 独立预览器当前最小缺口清单

更新：2026-11-07。本文约束本轮补缺范围；新增发现不自动成为交付前置条件。

## 当前边界

- 公共修复只放在预览器及适配层，优先复用 OMP 已有实现；不按模型名、路径或作者分支，不修改模型资产。
- 补缺期间每类模型只抽样、加载一次，复用已选样本和已有证据。停止逐项修复后重复加载真实模型；公共接口用不加载模型文件的针对性测试验证。
- 性能工作优先 Java 单线程与 native 单线程；后续 8/16 线程测量为 `SKIPPED`（用户决定），保留已完成数据。
- 不启动 Minecraft；不重建 Minecraft 状态机，也不为所有模型或全部宿主 API 补齐兼容层。

## 缺口跟踪表

每补齐一项，立即更新对应行的状态、完成记录及验证证据，再开始下一项；不把实现完成与真实模型验证混为一谈。新发现先归入待确认，不自动扩大必补清单。

| 编号 | 公共缺口 | 当前状态 | 最小补充与影响范围 | 完成/验证记录 |
|---|---|---|---|---|
| G01 | Lua `listFiles` | 已补齐；专项通过 | `PreviewLua` 枚举实际脚本清单，复用 OMP 路径、递归和模块名规则 | 2026-11-07：`PreviewLuaFilesTest` RED→GREEN；`build/preview/test-list-files-green.log` |
| G02 | `client.getFiguraVersion/compareVersions` | 已补齐；专项通过 | 显式配置版本字符串；复用 OMP 原 `Version.java` 比较算法 | 2026-11-07：`PreviewLuaClientTest` RED→GREEN；`build/preview/test-client-green.log` |
| G03 | `avatar:getNBT()` 的已保留模型/动画数据 | 已补齐；专项通过 | `FiguraLoader/PreviewFiguraParts/PreviewAvatarData/PreviewLua` 连接实际 parser 模型树与动画/timeline；返回新 Lua 表，不伪造完整 Minecraft NBT | 2026-11-07：`PreviewAvatarDataTest` RED→GREEN；`build/preview/test-avatar-data-green.log` |
| G04 | 默认隐藏误读编辑器元数据 | 已补齐；专项通过 | `YsmVisibilityDefaults` 忽略 `bezier_linked`，保持实际缩放规则；所有 YSM 共用，不按酒狐模型名特判 | 2026-11-07：最小测试 RED→GREEN；`build/preview/test-visibility-metadata-green.log`；已核对 12 个系列模型的基础规则 |
| G05 | Lua 元表导出及 `type/pairs/ipairs` 语义 | 已补齐；专项通过 | `PreviewLua` 导出真实元表并复用 OMP 基础函数；轮盘颜色统一为已有真实 `FiguraVec3`，消除同名类型冲突 | 2026-11-07：`PreviewLuaMetatablesTest` RED→GREEN；真实动画拦截、Vector3 颜色运算、遍历和实例隔离通过；Files/Client/Wheel 相关回归通过，均未加载模型文件。日志 `build/preview/test-g05-*.log` |

G01–G05 的最新构建成功，程序为 `build/preview/preview.jar`。专项通过只证明表中覆盖范围；后续新增修改不能沿用旧 PASS。

### G05 已完成范围与验收

- 注册实际 `Animation/AnimationAPI` 类型后导出元表；脚本拦截作用于真实动画对象，元表修改按实例隔离。
- 复用 OMP `FiguraLuaRuntime.loadExtraLibraries` 的类型及遍历语义，同时保留普通 Lua 回退行为。
- `PreviewLuaMetatablesTest` GREEN，覆盖真实动画方法拦截、`__type/__pairs/__ipairs`、普通类型/遍历及实例隔离；构建、diff/空白检查通过。
- 测试只用内存构造对象，不加载模型文件，不重新抽样。
- 影响范围：`PreviewLua` 及对应测试/来源说明。实际导出时发现旧 `PreviewWheelValues.Color` 与真实 `FiguraVec3` 都命名为 Vector3，导致 `dumpMetatables` 明确拒绝；必要依赖修正限于 `PreviewWheelValues`、轮盘 `Action` 适配、`PreviewInteraction` 颜色读取及其测试，统一使用已复用的真实向量，删除旧适配类型。无需修改稳定生产运行时或模型资产。

## 正式测试新增确认项

| 编号 | 缺口 | 状态 | 最小边界及证据 |
|---|---|---|---|
| G06 | `vanilla_model.ARMOR` 分组缺失 | 已补齐；专项通过 | 2026-11-07：对照 OMP VanillaPart/VanillaGroupPart，提供无装备状态独立的 nullable 可见性覆盖，不映射 PLAYER、不增加盔甲几何。`PreviewArmorVisibilityTest` RED→GREEN，覆盖 nil/false/true、链式返回、玩家及实例隔离；未加载模型。构建通过，日志 `build/preview/test-armor-{red,green}.log`、`build/preview/build-g06.log` |
| G07 | 根级 `models:getChildren()` 缺失 | 已补齐；专项通过 | 2026-11-07：按 OMP 返回从 1 开始的真实子部件新表，方法放元表，保留命名访问和 pairs(models)。`PreviewModelRootsTest` RED→GREEN，覆盖根/子层级、对象身份、新表及实例隔离；Armor/Metatables 回归通过，未加载模型。日志 `build/preview/test-g07-*.log`，构建 `build-g07.log` |

原正式测试失败不计性能成绩；本项修复后未重复加载模型探测。真实 Figura 端到端仍未验证，不将专项通过写成完整兼容通过。

## 测量条件的最小补充

| 编号 | 已确认缺口 | 状态 | 限定范围与验收 |
|---|---|---|---|
| G08 | 帧日志未记录窗口焦点，无法区分失焦数据 | 已补齐；真实窗口专项通过 | 2026-11-07：单/多模型记录帧首尾真实 GLFW 焦点快照，CSV 增加 window_focused；报告按聚焦/失焦/未记录分组，Java只匹配同焦点组。构建通过；PreviewWindowFocusTest 两个真实可见窗口转移焦点，CSV true/false 验证通过，日志 test-window-focus.log；单/多模型内置立方体各60帧实窗冒烟通过（test-focus-smoke-*.log），报告聚焦/失焦配对及红绿数值回归通过（test-report-focus.log）。未加载模型样本，不改变渲染或强抢生产窗口焦点。 |

动画库集中调用分析、脚本版本/哈希、必经与条件分支见 [Figura 动画库脚本缺口分析](Figura-script-gaps.md)。

## Figura 正式运行前已确认的必要参数适配

| 编号 | 已确认调用 | 状态 | 最小范围与证据 |
|---|---|---|---|
| G09 | `host:isHost()`、`player:getVehicle/getHeldItem()`、`world.getBlockState()` | 已补齐；专项通过 | 原选样 EZAnims 的每 tick getInfo（364行起）无条件调用；配置明确提供 host布尔、无载具null、固定方块、空手物品对象及属性/查询返回值（getHeldItem在453/454行无条件调用）。只补公共配置值适配，不实现世界或实体状态机，不改模型。2026-11-07：PreviewConfiguredQueriesTest RED→GREEN；null载具、空手物品、固定方块、host布尔、新返回对象及原world时钟通过，未加载模型。日志 test-PreviewConfiguredQueriesTest-{red,green}.log。 |
| G10 | 根级 `models:partToWorldMatrix()` | 已补齐；专项通过 | EZAnims:412 在每 tick 计算移动方向；复用现有真实矩阵类型及预览器根坐标约定，限根入口与无模型数值测试，不扩充相机/实体系统。2026-11-07：PreviewRootMatrixTest RED→GREEN，真实矩阵方向变换、1/16模型单位和返回副本隔离通过；日志 test-PreviewRootMatrixTest-{red,green}.log。 |

上述必要性来自实际原脚本的无条件调用路径，不是因为API尚未实现就要求补齐全部宿主能力。此次只读取既有样本脚本文本，未重新加载或解析模型。

## 暂不实施的候选项

| 候选项 | 当前证据 | 处理方式 |
|---|---|---|
| `host:isHost/isFlying` | isHost已由每tick无条件调用证明必需；isFlying仅host分支 | isHost移至G09；isFlying仅按显式配置提供，不实现飞行状态机 |
| 物品、目标、载具查询 | 载具查询已证明必需，移至G09；其余按原getInfo调用核对 | 仅必要配置返回值，不实现实体/库存系统 |

若后续证据证明必须补充，先记录具体调用、配置模拟是否足够、OMP 对应实现和最小影响范围，再更新本表。

不提前实现完整客户端、库存、实体世界、渲染任务、第一人称或其他未被当前失败证明必需的能力。

## 酒狐系列默认隐藏核对

已有 01_taisho_maid 至 12_little 共 12 个模型的基础常零缩放隐藏规则，与 OMP 原 parser/隐藏判定对照无遗漏，包括随机 `08_sta` 的 31 条隐藏规则。该核对不等于这些模型全部动画和实窗行为通过。

第 13 个模型的独立参考探测遇到基础缩放表达式解析失败，后续模型未据此记为 PASS；不为此扩展修复无关表达式。日志：`build/preview/visibility-reference.log`。当前没有证据支持新增模型专属隐藏规则。

## 验证与后续工作区分

- 保留已有 Java/native 数值一致性及真实窗口证据；未验证的最新行为明确标为未验证，不以接口测试代替端到端 PASS。
- 本清单补完仅表示当前已确认公共缺口完成，不表示所有模型兼容或整个预览器阶段完成。
- 随机单线程正式 benchmark、分模型 Markdown 报告、最终审计和阶段提交推送仍属于原计划剩余工作；补缺规则不因这些后续工作而自动扩大。
- 随机样本清单已保存为 `build/preview/benchmark/random-sample.json`。新增复杂 Figura `苍梧吟月Pro.zip`、`wangshu.zip` 已在候选池；不为补缺另行加载它们。


## 本次完成记录

2026-11-07，G05 已完成实现与无模型加载的专项回归，并立即更新上表。本次额外发现的两个直接阻碍已在 G05 范围内解决：旧颜色适配器与真实 Vector3 同名，已统一真实向量并清除对应遗留编译类；迭代封装的局部 name 被 LuaJ 父类同名字段遮蔽，已改明确命名，`__pairs/__ipairs` 回归通过。没有启动真实模型窗口或重新抽样，最新真实模型端到端状态仍未验证，不记为 PASS。
