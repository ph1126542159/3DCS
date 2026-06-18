# 3DCS 功能与 GUI 缺口审计

## 审计口径

- 功能来源：`doc/需求文档.md`，该文档已按 3DCS 官方帮助网页逐页整理；网页入口为 `https://community.3dcs.com/help_manual/index.html?about_this_manual.htm`，目录页为 `hmcontent.htm`。
- 当前实现来源：`src/README.md`、`doc/交付差距审计.md`、`doc/全量需求实现路线图.md`、`src/ui`、`src/engine`、`src/domain`、`src/tests`。
- 判断分级：
  - `真实可用`：后端和 GUI 都有可执行工作流或测试覆盖。
  - `后端为主`：计算/数据层存在，但 GUI 不完整。
  - `窗口壳`：只有菜单入口或概览对话框，不是 3DCS 描述的真实业务界面。
  - `缺失`：功能和 GUI 都未形成可用产品能力。

## 结论

当前软件不是完整 3DCS GUI 复刻。已有较强的 C++17 算法/数据基础和一部分 Qt 桌面能力，但 `doc/需求文档.md` 描述的大量 3DCS 功能仍缺少真实 GUI，尤其是完整 MTM 编辑器、AAO、Mechanical、FEA Compliant、CAD 集成和工业级报告/可视化工作流。上一轮新增的高级菜单入口只能算 `窗口壳`，不能视为功能完成。

## 功能与 GUI 对照

| 需求章节 | 3DCS 功能/GUI | 当前状态 | 主要缺口 |
| --- | --- | --- | --- |
| 第 0 章 总览 | 产品配置、模块授权、四层架构、窗口调用关系 | 后端架构基础存在 | 缺少产品级模块中心、授权状态驱动的 UI 裁剪、全局工作流导航 |
| 第 1 章 入门 | Welcome、软件概述、建模流程、快捷键、示例模型 | 窗口壳/文档为主 | 缺少启动页、示例工程选择器、建模向导、快捷键帮助面板 |
| 第 2 章 许可/共享计算 | License Status、共享内存、分布式计算、系统信息 | 窗口壳 | 缺少授权源管理、诊断、共享计算资源监控、分布式任务 UI |
| 第 3 章 功能与操作 | Model Navigator、Update、Extract、Features、Data、Validation、Display、Preferences、Save/Load | 部分真实可用 | Navigator/属性编辑/保存加载/验证已有；缺 Tree Link 完整向导、Update/Extract/Data 管理工具、完整 Display/Preferences 九分页 |
| 第 4 章 Moves | 21 种 Move 及各自参数对话框、条件/迭代/柔性 Move | 后端为主 | 部分 solver 存在；缺 21 种 Move 的真实 QML 参数编辑器、非空 Iteration、AutoBend/柔性弯曲基础设施 |
| 第 5 章 Tolerances/GD&T | GD&T、点级公差、分布、Bonus/Datum Shift、PCDB | 后端为主 | 缺完整 GD&T/QML 编辑器、PCDB 管理、Datum/DRF/Composite/Bonus Shift 产品级界面 |
| 第 6 章 Measures | Point/Feature/GD&T/Equation/Combination/User-DLL Measure、Related List、Measurement Generator | 后端为主 | 缺完整 Measure 编辑器、Equation 构建器、Related List/Measurement Generator、User-DLL Measure 绑定 UI |
| 第 7 章 仿真 | Run Analysis、Monte Carlo、Contributor、GeoFactor、Simulation Window、Show Graph/Samples、Batch Processor | 部分真实可用 | Run/Results/Batch 初版存在；缺完整分析设置、图表交互、HST/HLM 管理体验、批处理队列监控 |
| 第 8 章 可视化/报告 | Nominal Build、Assemble、Separate、Deviate、Sweep、Color Contour、Report、Spec Study、Gap & Flush | 部分真实可用/窗口壳 | Qt3D 点线视图和 Color Contour 基础存在；缺动画状态机、报告模板、PowerPoint/Google/APQP、Spec Study/Gap&Flush 真实界面 |
| 第 9 章 AAO | SBS、GeoFactor Matrix、CTI、TO、SO、DO、LSA、矩阵保存加载 | 窗口壳/少量后端 | 缺完整 AAO 数据模型、矩阵编辑、优化器、结果图表、SOV/GF2/DOP/JOP 工作流 |
| 第 10 章 Mechanical | Constraints、Joints、Kinematics、Gear、DOF Counter、Collision/Part Distance | 窗口壳 | 缺机构约束求解、关节编辑器、DOF 矩阵、运动学动画、碰撞/距离批量生成 |
| 第 11 章 FEA Compliant | StiffGen、Load FEA、Clamp/Join/LockDOF、Force/Thermal/Gravity、柔性验证 | 窗口壳/缺失 | 缺柔性件数据模型、刚度矩阵/ASET/网格绑定、柔性 Move、载荷与验证器 UI |
| 第 12 章 CAD 集成 | NX/SW/CATIA/Creo/3DX/Multi-CAD、PMI/GD&T/约束提取 | 缺失 | 缺 CAD Adapter、导入/更新/提取工作流、PMI/约束映射 UI |
| 第 13 章 User DLL | C ABI、注册、加载、Move/Tolerance/Measure 挂接、示例 DLL | 后端为主/窗口壳 | PluginHost 基础存在；缺 SDK 管理、DLL 加载界面、Tolerance 挂接、例程测试面板 |
| 第 14 章 教程/附录 | 计算定义、GD&T 标准、建模技巧、教程模型 | 文档为主 | 缺内置帮助中心、教程项目、标准/公式速查面板 |

## GUI 重写方向

QML 重写不应继续做“手册目录浏览器”。正确方向是高端工业工作台：

- 左侧：Model Navigator / Logic Tree，承担建模中枢。
- 中央：Qt3D/Quick 3D 视口，承载装配、偏差、Color Contour、动画。
- 右侧：属性与 MTM 参数编辑面板，按 Move/Tolerance/GD&T/Measure 动态切换。
- 底部：Simulation Window 数据区，统一 MC/GeoFactor/Contributor/Samples/Graph。
- 顶部：模块化 Ribbon/Command Bar，按 Modeling、Moves、Tolerances、Measures、Simulation、Visualization、AAO、Mechanical、FEA、Reports、System 分组。
- 高级模块：AAO、Mechanical、FEA 用专业工作区而不是占位弹窗。

## 下一步执行建议

1. 先用 Product Design 产出 3 个视觉方向，选择一种后再写 QML。
2. 移除上一轮“help-tree catalog”式主界面，保留 JSON 只作为审计参考，不作为主 UI。
3. 以 `Feature/GUI Coverage` 为测试门禁：每个需求章节至少有真实工作区、真实命令入口和明确未实现状态。
4. QML 第一版优先做主工作台、Navigator、MTM 编辑面板、Simulation/Report/AAO 工作区骨架，再逐步接真实后端。
