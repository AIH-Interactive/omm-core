# Figura 动画库脚本缺口分析

日期：2026-11-07。对象：既有随机样本 `sickle.zip`。仅分析原包中脚本文本及现有预览器调用路径，没有重新解析或加载模型。本文用于限定补充范围，不把库的全部可选功能升级为必补项。

## 脚本来源

| 脚本 | 版本 / 原始行数 | 原始文件 SHA256 |
|---|---|---|
| EZAnims.lua | 3.0.1 / 775 | 5dc0980fc436aa189631a76562b75e05a699726016b96e201236c266c251e084 |
| GSAnimBlend.lua | 2.4.1 / 2271 | 0b5c01bda4d52fd9e260526888237d7bf2cc1fc7e2bb0870ea6948c7251fac52 |
| script.lua | 8 | c71fbf36d989d9a28e205972c387c87d114aa90c3ea052c2ce2c72895fb4e50d |

行号对应 ZIP 原始文本。只读分析缓存位于 `build/preview/script-analysis/`；模型资产不修改。抽样来源见 [固定随机清单](random-sample.json)。

## 实际运行链

`EZAnims` 97–99 行用 `listFiles` 查找并 require `GSAnimBlend`。后者初始化时读取客户端版本、实际动画 NBT 与真实 Animation/AnimationAPI 元表，并安装混合播放方法。`ENTITY_INIT` 时 EZAnims 721–722 行通过根模型子列表自动发现动画；随后其 TICK 回调 662–663 行每次执行 `getInfo()`，据显式宿主快照选择实际动画。GSAnimBlend 的 TICK/RENDER 回调负责原库的混合进度及动画权重更新。

用户 `script.lua` 只隐藏 PLAYER/ARMOR，没有调用库的可选原版姿态混合回调。

## 必经接口与已确认缺口

| 调用 / 原始位置 | 当前情况 | 必要的最小处理 | 跟踪 |
|---|---|---|---|
| listFiles / require（EZ 97–99） | 已连接实际脚本清单 | 保留原模块加载与路径语义 | G01 已通过专项 |
| client.getFiguraVersion / compareVersions（GS 26–33） | 已支持 | 版本通过配置显式指定，复用真实版本比较 | G02 已通过专项 |
| avatar:getNBT().animations（GS 267–270） | 已连接实际 parser 保留数据 | 返回真实动画 metadata，不造空 NBT | G03 已通过专项 |
| figuraMetatables.Animation / AnimationAPI（GS 328–329） | 已导出真实元表 | 保留原库拦截及实例隔离 | G05 已通过专项 |
| vanilla_model.PLAYER/ARMOR（script 4、7） | 已支持皮肤部件与无装备可见性覆盖 | PLAYER 和 ARMOR 独立 | G06 已通过专项 |
| models:getChildren、部件 getName/getChildren（EZ 668–675） | 根入口已补齐，部件已有 | 枚举真实根和子部件，不虚构动画模型 | G07 已通过专项 |
| host:isHost（EZ 365） | 必经；尚未接入配置适配 | 当前场景显式 `false`，不读取游戏客户端 | G09 已通过专项 |
| player:getVehicle（EZ 384） | 必经；当前通用适配不接受 null | 显式配置 `null`，表示本场景无载具 | G09 已通过专项 |
| world.getBlockState / block:isSolidBlock / block.id（EZ 406、409、420） | 必经；缺对象返回值适配 | 显式固定方块对象；无需世界查询或方块注册表 | G09 已通过专项 |
| player:getHeldItem / item:getUseAction / item.tag（EZ 453–470） | 左右手均必经；缺对象返回值适配 | 当前双手明确为空气、NONE、空 tag；不实现库存 | G09 已通过专项 |
| models:partToWorldMatrix（EZ 412） | 根入口缺失 | 复用预览器坐标单位及真实矩阵类型，供方向变换 | G10 已通过专项 |
| player 的姿态、速度、移动/水中/攀爬状态、生命、用手与挥手值（EZ 380–472） | 已有显式标量/真实向量查询 | 使用原抽样配置的快照，不写自动运动状态机 | 保留现有实现 |
| player:isLoaded（GS 448、1342） | 已支持 | 显式 loaded=true | 保留现有实现 |
| Animation 播放/暂停/重启、权重、时间、长度、循环、速度、延迟、新代码，以及 AnimationAPI 枚举（GS 351–369、472–496、1338–1418、1795） | 已复用原类及实际播放状态 | 保留原库逻辑；用完整原脚本和内存动画对象验证接线 | 不另造播放状态机 |
| events ENTITY_INIT/TICK/RENDER/POST_RENDER/POST_WORLD_RENDER；pings、type/pairs/ipairs、Lua load、数学/向量 | 已有实际事件/语言与数学实现 | 验证原库回调，不以空函数占位 | 无新增必补项 |

专项通过只证明相应接口，不能据此宣称该真实模型的完整脚本播放通过。当前必要补充范围限定为 G09、G10（两项均已通过无模型专项）；先做无模型公共接口测试，再进行原选样的正式 Figura 测量。

## 条件分支与暂不实施项

| 调用 / 原始位置 | 触发条件 | 当前配置与处理 |
|---|---|---|
| host:isFlying（EZ 367） | isHost=true 且 flyinit 分支 | 当前 isHost=false；若后续场景需要，直接配置布尔，不实现飞行状态机 |
| vehicle:getControllingPassenger/getPos/isOnGround（EZ 386、407、409） | 非空载具或坐姿 | 当前无载具、STANDING；需要测试时显式配置对象和查询返回值 |
| player:getTargetedEntity（EZ 475–476） | swingTime==1 | 当前 swingTime=0，不要求目标实体系统；攻击场景再声明必要模拟值及类型语义 |
| player:getTargetedBlock / block:getTextures（EZ 655–659） | 原库放在 pcall 内 | 原库有失败回退；当前 idle 基线不因此补完整方块纹理宿主。需要测试挖掘行为时再配置目标结果 |
| events.damage（EZ 335、602） | 可选事件存在时使用，否则走生命差值路径 | 依原库分支，不因可选事件不存在增加游戏伤害系统 |
| part:getParentType/offsetRot、vanilla_model 部件 getOriginRot（GS 599、615、630–633） | 显式使用库提供的原版姿态混合回调 | 当前脚本未调用；记录为独立候选，不补完整原版动画/装备/姿态宿主 |
| 用户自定义 blend callback、曲线、延迟和停止触发（GS 多处导出） | 模型主动调用相应库扩展 | 使用真实 Animation 方法和 Lua 回调；只在实际必需调用失败时最小修复，不穷举所有库组合 |

## 配置模拟的边界

G09 只提供配置明确声明的常量查询返回值：布尔、null、字符串、数字、数组及带属性/查询的对象。调用参数在这种固定场景中不改变返回值，左右手都为空气就是明确的测试设定。配置对象不是生产实体、ItemStack 或 BlockState，不能以它证明生产宿主实现通过。

若之后需要左右手不同物品、按位置返回不同方块或类型识别，先记录具体调用与所需参数，再决定最小配置表达；本轮不预先实现。

## 后续验证顺序

1. G09/G10 无模型接口 RED→最小实现→GREEN，每完成一项即更新 [最小缺口表](Minimum-gaps.md)。
2. 用原动画库完整文本和内存动画对象检查初始化、tick、render 接线，不加载模型资产；它只验证接线，不能替代真实模型。
3. 对原选样运行正式 Figura 单模型 Java/native 单线程 × CPU/GPU，随后补同模型多实例与包含 Figura 的混合场景；保留脚本、固定随机种子、动画时钟与焦点分组。
4. 每完成一个组合立即记录；真实失败保留，不更换样本，不关闭原脚本，不增加与当前路径无关的 Minecraft 功能。

## 本次验证记录

2026-11-07：G09、G10 均完成接口 RED→GREEN。原 EZAnims/GSAnimBlend 文本配合真实内存 Animation/AnimationAPI 对象完成初始化、60 帧 tick/render/postRender，实际 idle 处于 playing；日志 `build/preview/test-animation-libraries.log`。没有加载模型资产；该测试证明原库接线，不作为真实模型端到端 PASS。

正式 Figura 实窗测试随后执行，结果按组合写入 CPU/GPU 报告。

正式运行结果：`random-figura-single-g08` 的 Java CPU、native CPU、Java GPU、native GPU 四组合均完成120预热+5×600采样帧，脚本保持开启，可见/提交均1；session52891退出0。每例完成后立即刷新 [CPU报告](random-single/Benchmark-CPU.md) / [GPU报告](random-single/Benchmark-GPU.md)。这证明当前显式静止/空手场景下原选样运行；不覆盖上表未触发的条件分支或所有Figura模型。
