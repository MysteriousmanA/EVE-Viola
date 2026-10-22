# EVE-Viola 架构与维护约定

## 模块边界

| 模块 | 职责与依赖 |
| --- | --- |
| `eve_viola_domain` | 配置值类型、图色状态机、像素分类、小窗身份与持久状态。依赖 Qt Core/Gui，不依赖 Widgets 或原生 DLL。 |
| `eve_viola_cpp_core` | 激活策略、窗口目录、预设恢复、统计与截图调度、原生适配和诊断。依赖 domain 与原生 DLL。 |
| `eve_viola_cpp_ui` | 窗口、页面、交互控制器、状态绑定和音频设备适配。依赖 core 与 Qt Widgets/Multimedia。 |
| `eve_viola_native` | Win32/DWM/GDI 操作及原生帧的资源生命周期。保持现有 v3 API。 |

`MainWindow` 组装这些服务，负责标签导航、应用生命周期和跨页面呈现协调。区域截取会话由
`RegionCaptureController` 管理，账号切屏与轮盘会话由 `AccountSwitchController`
管理。`PresetService` 负责预设快照、源窗口唯一匹配和暂缺窗口的恢复计划。

## 源码目录归属

`cpp/src` 按责任组织源文件，所有应用代码和资源都归入对应目录：

| 路径 | 归属 |
| --- | --- |
| `config/` | 配置类型、校验、JSON 读写与兼容包含入口 |
| `features/` | 按业务能力组织的状态、计算和服务；`replica/` 拥有小窗状态、工作区与统计调度，`replica_list/` 保留列表负载估算 |
| `platform/` | 原生窗口操作、窗口目录和单实例机制 |
| `app/` | 应用入口与激活协调 |
| `runtime/` | 运行策略、日志和性能记录 |
| `ui/` | 主窗口、页面、控件、界面控制器与音频反馈 |
| `resources/` | Qt 资源清单与随应用编译的资源源文件 |

这些目录说明源码归属；库和可执行目标的编译边界仍由 `cpp/CMakeLists.txt` 管理。

## 主窗口实现归属

`ui/main_window.cpp` 保留构造入口及按顺序组装服务和页面的流程。具体实现按功能
放入 `ui/main_window/`，文件统一使用 `main_window_` 前缀：

| 文件后缀 | 职责 |
| --- | --- |
| `layout.cpp` | 主窗口布局、标签导航和远征计时页刷新接线 |
| `home.cpp` | 首页接线、软件选项广播、预设栏和数量摘要 |
| `region_presets.cpp` | 区域预设页接线、预设更新广播和从小窗保存区域 |
| `color_rules.cpp` | 图色页接线、向小窗/行/桌宠广播规则 |
| `page_partition.cpp` | 分区页接线和游戏存在状态监控 |
| `desktop_pet.cpp` | 桌宠页与账号轮盘、摸鱼模式接线 |
| `quick_switch.cpp` | 账号控制器创建、快捷键服务接线、快捷切屏页和钩子启动 |
| `region_capture.cpp` | 截图控制器创建和跨页面回调 |
| `window_catalog.cpp` | 共享窗口目录的刷新/失败事件接线 |
| `configuration.cpp` | 配置加载、默认预设选择、配置刷新、快照保存与延迟保存定时器 |
| `replica_lifecycle.cpp` | 小窗创建、复制、所有权、关闭回调和有序查询 |
| `replica_restore.cpp` | 预设重建、暂缺游戏窗口恢复和区域恢复重试 |
| `replica_sync.cpp` | 跨源小窗同步对话框与副本生成 |
| `replica_bindings.cpp` | 小窗与列表行之间的信号连接及跨页面事件接线 |
| `replica_list.cpp` | 小窗列表页接线、排序、筛选、分组和折叠同步 |
| `replica_stacking.cpp` | 重叠窗口分组、层叠顺序和合并更新定时器 |
| `replica_flash.cpp` | 按来源/区域/预设聚合闪烁状态 |
| `runtime_policy.cpp` | 摸鱼/截取模式的显示、音频和自动激活策略下发 |
| `tray.cpp` | 托盘菜单、主窗口显示与激活 |
| `shutdown.cpp` | 托盘关闭判断、强制退出、后台截图排空和析构清理 |

这些文件实现同一个 `MainWindow` 类，继续共享服务与界面状态，不增加第二份
配置或小窗所有权。提取的初始化函数保留原构造顺序和信号连接顺序。页面控件、
业务控制器及公共服务仍位于各自既有模块；这里保留应用层接线与协调。

主窗口拆分后 Release 构建与全量 21 个 CTest 目标通过。展开提取的辅助函数后，
55 个原有方法与拆分前的内容一致；主文件由 1669 行减至 60 行。

## 页面归属与界面/业务分离

每个标签页拥有独立的界面实现。`ui/pages` 按页面组织新增模块；已经独立的
分区、远征计时和桌宠页面继续使用原文件路径。`configuration_pages.h` 仅保留
兼容包含，不再承载类定义或实现。

| 标签页 | 界面与交互协调 | 业务与数据 |
| --- | --- | --- |
| 首页 | `ui/pages/home/HomePage`、`HomeController` | 通用 `PresetService`、配置值类型 |
| 小窗列表 | `ui/pages/replica_list/ReplicaListPage`，拥有筛选、分组、折叠与列表布局 | `features/replica_list` 统计负载估算；通用 `ReplicaController` 拥有小窗身份与顺序 |
| 区域预设 | `ui/pages/region_preset/RegionPresetPage`、`RegionPresetController` | 配置中的预设值类型与增删改函数 |
| 图色逻辑 | `ui/pages/color_rule/ColorRulePage`、`ColorRuleController`、规则编辑对话框 | 通用图色状态机与规则配置 |
| 快捷切屏 | `ui/pages/quick_switch/QuickSwitchPage`、`QuickSwitchController` | 通用 `QuickSwitchService` 与账号切换服务 |
| 页面分区 | `ui/page_partition_page.*`，拥有输入、屏幕覆盖层与显示 | `features/page_partition/PagePartitionModel` 拥有线条状态和撤销历史 |
| 远征计时 | `ui/expedition_timing_page.*`，拥有输入、结果展示和剪贴板操作 | `features/expedition_timing/ExpeditionTimingService` 计算默认时长与到期文本 |
| 摸鱼桌宠 | `ui/desktop_pet_page.*`，拥有头像裁剪、浮窗和消息呈现 | `features/desktop_pet/DesktopPetSettingsStore` 读取与原子写入配置、头像 |

依赖方向为页面输入 → 页面控制器 → 业务模块/共享服务，结果通过信号或值数据
返回页面。页面控制器可以依赖 Widgets 以协调对话框；`features` 中的值数据与计算模块编入
`eve_viola_domain`，仅依赖 Qt Core/Gui，不包含 QWidget 或其他页面头文件。
业务层不通过控件读取配置，也不操作界面对象。`features/quick_switch` 中的
Windows 快捷键运行时模块编入 `eve_viola_cpp_core`，与编入 domain 的按键文本
工具及账号排序工具区分；运行时依赖 Win32、原生网关与 Qt Core，不依赖 Widgets。

像素统计算法独立放在 `cpp/src/features/replica/pixel_statistics_calculator.cpp`，包括红白像素判定、
区域裁剪、图像格式适配和像素计数。`pixel_statistics_calculator.h` 定义输入/输出接口
与 `PixelStatistics` 结果类型。后续修改颜色阈值、分类规则或扫描算法优先修改该文件。
`pixel_statistics.cpp` 只保留界面使用的颜色名称，原 `pixel_statistics.h` 保留兼容包含。
`NativeGateway` 截图后调用计算器，`StatisticsCoordinator` 负责调度，图色状态机负责
消费结果；计算器不依赖这三者或任何 QWidget。

只供一个页面使用的代码留在该页面模块。通用标题、说明和表格初始化放在
`ui/common/page_widgets.*`；截图、窗口目录、激活、配置保存和小窗生命周期
继续复用已有服务。跨页面更新由主窗口接收信号协调，页面不直接查找其他页控件。
列表页接收当前有序小窗和行控件快照；主窗口在创建、关闭、恢复和排序后更新快照，
不再保存列表布局、筛选框和分组装饰控件。

分区撤销只还原线条，不回退软件设置，最多保留 20 次线条改动。桌宠存储拒绝
覆盖不可读或损坏的既有配置，保持原 JSON 格式与头像缓存文件名。

页面分离后的 Release 构建通过，全量 20 个 CTest 目标通过。新增的
`cpp_page_feature_services_test` 不启动界面，覆盖线条撤销边界、跨年和负偏移计时、
同源统计合并、未知尺寸、桌宠损坏配置保护与头像往返。无临时诊断输出的应用
启动退出路径另外连续通过 10 次复测；第一次全量测试中的启动异常在重建后未再复现。

## 小窗列表行实现归属

`ui/replica_row.cpp` 保留 `ReplicaRow` 构造组装和身份访问。各功能的控件创建、
信号连接与状态更新分别位于 `ui/replica_row/`：

| 文件 | 职责 |
| --- | --- |
| `replica_row_source.cpp` | 源窗口下拉框、来源名称显示、重名 HWND 区分与选中项保留 |
| `replica_row_region.cpp` | 区域文本编辑、重截/保存按钮、预设选择与容差匹配 |
| `replica_row_geometry.cpp` | 位置文本编辑、尺寸展示、缩放输入与焦点保护 |
| `replica_row_statistics.cpp` | 红白像素数量展示、统计间隔控件与信号 |
| `replica_row_color_logic.cpp` | 图色规则/槽位、槽位互斥、启用状态和边框颜色选择 |
| `replica_row_visibility.cpp` | 小窗/角色名显示开关及隐藏时槽位 2 控件约束 |
| `replica_row_grouping.cpp` | 关闭/复制/折叠/同步动作、组代表呈现和性能估算文字 |
| `replica_row_drag_drop.cpp` | 拖动手柄、MIME 数据与拖放排序请求 |
| `replica_row_layout.cpp` | 内容面板、来源单元格和整行布局 |
| `scroll_passthrough_controls.h` | 下拉框/缩放框忽略滚轮，让事件交给列表滚动；来源弹出刷新回调 |
| `replica_row_constants.h` | 来源单元格与折叠摘要的共享宽度 |

所有实现继续使用同一个 `ReplicaRow` 的状态和接口。文本解析、来源匹配等仅供
单个功能使用的工具留在对应 `.cpp` 内；业务状态仍由 `ReplicaController` 与绑定层
负责。组装顺序和信号连接顺序保持一致。

列表行拆分后 Release 构建与全量 22 个 CTest 目标通过。展开提取的初始化函数后，
40 个原有方法内容一致；原文件由 1139 行减至 27 行。已有主窗口测试覆盖来源
重名与选择、区域文本和预设匹配、位置/缩放编辑、槽位互斥及同源分组折叠。

## 桌宠实现归属

`ui/desktop_pet_page.cpp` 保留页面组装、设置恢复调用和析构清理。功能实现位于
`ui/desktop_pet/`，各控件具有单独的头文件，不再在页面 `.cpp` 内定义类：

| 文件 | 职责 |
| --- | --- |
| `desktop_pet_layout.cpp` | 编辑面板、外观控件与规则文案表布局 |
| `desktop_pet_avatar.cpp` | 图片选择、解码、头像预览更新与编辑控件接线 |
| `avatar_crop_widget.*` | 圆形头像生成、缩放、偏移限制与拖动裁剪 |
| `avatar_preview_widget.*` | 头像预览绘制 |
| `avatar_painting.*` | 裁剪路径和透明背景棋盘格绘制 |
| `desktop_pet_lifecycle.cpp` | 桌宠启停、启动显示、初始位置、轮盘/摸鱼信号转发 |
| `desktop_pet_messages.cpp` | 规则文案编辑、账号名提取、同文案账号合并与延迟保存 |
| `desktop_pet_settings.cpp` | 设置值与控件状态转换、头像恢复、屏幕位置校验 |
| `floating_pet_window.*` | 悬浮窗接口、组装与头像绘制 |
| `floating_pet_input.cpp` | 点击/拖动、累积滚轮缩放与右键菜单 |
| `floating_pet_messages.cpp` | 气泡测量与展示、到期关闭、临时显示后自动隐藏 |
| `floating_pet_geometry.cpp` | 头像/气泡区域、窗口尺寸与屏幕边界限制 |
| `desktop_pet_constants.h` | 头像输出尺寸、桌宠尺寸范围和气泡尺寸/时长常量 |

`desktop_pet_settings.cpp` 调用已有 `DesktopPetSettingsStore`，实际 JSON 和图片
读写仍由业务存储模块负责。配置字段、损坏文件保护、气泡尺寸算法以及多账号
提示合并行为保留。Qt 元对象从独立控件头文件生成，不再依赖页面的内嵌 moc。

桌宠拆分后 Release 构建与全量 22 个 CTest 目标通过。新增
`cpp_desktop_pet_widgets_test` 覆盖圆形裁剪与偏移、滚轮累积和尺寸边界、气泡
完整显示/自动收起、账号合并以及临时提示不改变禁用设置。展开提取的辅助函数
后，12 个页面方法和 32 个控件方法的实现内容一致；原文件由 1209 行减至 32 行。

## 小窗状态

`ui/replica_window.cpp` 负责窗口组装、身份访问和关闭/析构清理。各功能的实现
位于 `ui/replica_window/`，继续使用 `ReplicaWindow` 的接口和状态：

| 文件 | 职责 |
| --- | --- |
| `replica_window_source.cpp` | 源窗口绑定、缩略图更新、源失效处理及监控定时器 |
| `replica_window_capture.cpp` | 后台冻结请求与回调、区域选择/确认/取消、预设应用、坐标映射 |
| `replica_window_statistics.cpp` | 统计信号连接、间隔设置与协调器登记 |
| `replica_window_input.cpp` | 鼠标/键盘事件、长按定时器和 Windows 点击激活拦截 |
| `replica_window_geometry.cpp` | 宽高比、最小尺寸、缩放及逻辑/设备像素矩形 |
| `replica_window_menu.cpp` | 右键菜单和菜单动作连接 |
| `replica_window_color_logic.cpp` | 图色规则、槽位设置和统计结果送入状态机 |
| `replica_window_activation.cpp` | 点击激活、图色触发自动激活、冷却设置及相关日志 |
| `replica_window_feedback.cpp` | 图色效果派发、音频完成/失败、弹出反馈和摸鱼音频抑制 |
| `replica_window_presentation.cpp` | 绘制、源名称显示、边框/闪烁及显示抑制 |

可见且播放实时画面的小窗每 2 s 重新建立 DWM 缩略图关系，避免某次关系停留
在旧画面后持续无法恢复。先注册并配置新关系，再释放旧关系；重建失败时保留
旧关系并沿用既有源失效检查。隐藏、最小化、角色名覆盖及冻结选区（含等待截图）
期间不重建。这个低频恢复不依赖焦点或统计间隔，不调用 `PrintWindow`，不激活
源窗口，也不重置选区、图色状态或统计任务。DWM 的正常逐帧同步仍由系统负责。

Win+D 会同时最小化源游戏，单纯重建缩略图无法让暂停渲染的游戏产生新帧。
实时小窗的恢复流程因此先通过可选的 v3 扩展 `eve_viola_resume_thumbnail_source`
检查源窗口；仅在最小化时以 `SW_SHOWNOACTIVATE` 异步恢复并置于窗口栈底部，
不转移前台焦点。源窗口会离开最小化状态；已经正常显示的源窗口不改变层级。
该步骤遵守上述小窗显示/冻结条件，失败时保留旧缩略图并在下一周期重试。

信号连接与定时器初始化随对应功能迁移，构造顺序保持一致。以上文件是同一个
Qt 窗口类的分功能实现，共享窗口状态；图色计算状态机、像素计算、统计调度和
激活仲裁仍由既有独立服务负责。

小窗拆分后 Release 构建与全量 21 个 CTest 目标通过。对照拆分前实现，展开
提取的初始化、点击激活与图色触发辅助函数后，83 个原有函数内容一致。

`ReplicaController` 统一拥有小窗身份、生命周期、顺序和持久配置，复用
`ReplicaManager` 与 `ReplicaWorkspace`。`ReplicaBinding` 将用户操作与窗口事件
写入状态；保存配置读取控制器快照，不从多个控件重新拼装配置。

用户隐藏小窗的规则属于控制器：清空槽位 2、启用槽位回到 1，并启用图色逻辑。
列表控件只呈现这些结果。隐藏时 Qt 可能延迟位置与尺寸事件，因此位置和缩放
命令还会立即记录实际接受的几何尺寸。

恢复过程中，临时的源尺寸、裁剪和界面信号不得覆盖尚待恢复的预期区域。
恢复源窗口与新副本位置在恢复流程结束时写入状态。复制使用同一恢复流程，
保留配置并采用新小窗的位置。

## 临时运行模式

`RuntimePolicy` 分别保存摸鱼和区域截取标志，并计算实际显示与自动激活权限。
用户设置的可见性不因临时模式而改变。任一临时隐藏条件仍成立时，小窗不会
恢复显示；新建小窗也遵循同一策略。摸鱼模式暂停音频，截取模式保留音频。

窗口状态只在开始临时隐藏时保存一次，最后一个隐藏条件解除后恢复。
分区线和图色弹窗同样遵循这些临时抑制条件。

## 配置与保存

原 `app_config.cpp` 已按职责拆分，`app_config.h` 仅保留兼容包含入口。新代码优先
包含所需模块头文件，避免经兼容入口依赖所有配置工具。

| 路径 | 职责 |
| --- | --- |
| `config/config_types.h` | 配置值类型、默认值和范围常量，不包含存储或计算实现 |
| `config/json_config_store.*` | 分章节加载/保存、保存前完整验证、旧版本章节迁移 |
| `config/json_document_io.*` | JSON 文件读取、未变内容跳过写入、`QSaveFile` 原子替换 |
| `config/serialization/region_presets_json.*` | 区域预设 JSON 转换 |
| `config/serialization/color_rules_json.*` | 图色规则 JSON 转换和旧字段兼容 |
| `config/serialization/replicas_json.*` | 小窗 JSON 转换及 `replicas.json` 章节组合 |
| `config/serialization/presets_json.*` | 整套小窗预设 JSON 转换 |
| `config/serialization/page_partition_json.*` | 分区设置与线条 JSON 转换 |
| `config/serialization/quick_switch_json.*` | 快捷键与账号列表 JSON 转换和旧单键配置兼容 |
| `config/serialization/application_options_json.*` | 软件选项 JSON 转换 |
| `config/config_validation.*` | 时间间隔和激活设置的范围验证、间隔显示文本 |
| `features/quick_switch/quick_switch_keys.*` | 按键编码/解码、规范化、去重和冲突排除 |
| `features/quick_switch/radial_account_order.*` | 根据实时窗口列表计算账号顺序 |
| `features/color_rule/trigger_condition.*` | 图色条件文本解析、边界计算与条件判断 |
| `features/color_rule/color_rule_config.*` | 图色规则增删改、反馈和后续动作名称映射 |
| `features/region_preset/region_preset_config.*` | 区域预设增删改与重名验证 |

序列化模块只转换值数据和 `QJsonObject`/`QJsonArray`，不读写文件；快捷键与
图色条件工具不依赖 JSON 或配置存储。存储层通过这些模块组装章节，保留原文件名、
JSON 字段和迁移顺序。软件选项与快捷键仍写入 `replicas.json`，由对应序列化模块
生成字段，再由小窗章节序列化器组合；预设文件继续只保存其名称、启用标志和小窗。

配置拆分后 Release 构建与全量 21 个 CTest 目标通过。新增
`cpp_config_modules_test` 直接使用各模块接口，覆盖章节组合、快捷键冲突与规范化、
旧快捷键/图色字段迁移，以及区域预设和分区值的独立 JSON 往返。

保存与界面刷新分离。位置、尺寸和全局选项保存不会重新下发规则，也不会取消
正在等待的自动激活。区域预设和图色规则通过各自的变更路径更新受影响对象。
重复设置相同规则或槽位是无副作用的操作。

`JsonConfigStore` 在写入之前验证全部已有章节，拒绝覆盖损坏的 JSON。主配置
加载失败时本次运行暂停保存；修复文件后需重启。桌宠配置采用同样的加载失败
保护，且保留原有头像与消息功能。

JSON 内容未变的章节不重写。变更章节继续使用 `QSaveFile` 原子替换，保持现有
文件格式和旧版本迁移。原子性目前覆盖单文件，不是跨文件事务。

## 快捷切换服务实现归属

`quick_switch_service.cpp` 保留服务构造和析构。运行时实现位于
`features/quick_switch/`，统一使用 `quick_switch_` 前缀：

| 文件后缀 | 职责 |
| --- | --- |
| `service_impl.h` | 私有共享状态、原子键映射、按键/轮盘会话状态和线程同步字段 |
| `key_mapping.cpp` | 键盘身份索引、键鼠方向映射、冲突优先级与映射版本检查 |
| `capture.cpp` | 录入启动/取消、已按下键忽略、可捕获输入的消费和录入通知 |
| `input_dispatch.cpp` | 通用按下/松开流程、重复键消费、普通切换请求与释放清理 |
| `keyboard_input.cpp` | 键盘钩子事件解码、左右修饰键解析与合成输入过滤 |
| `radial_hold.cpp` | 轮盘长按会话、取消/前台校验、短按方向切换或原键回放 |
| `mouse_input.cpp` | 鼠标按键解码、侧键切换和 EVE 鼠标按下/释放跟踪 |
| `foreground.cpp` | EVE 进程识别、鼠标目标缓存和前台窗口判断 |
| `signal_dispatch.cpp` | 从钩子线程排队投递 Qt 信号，以 `QPointer` 检查服务生命周期 |
| `hook_thread.cpp` | 钩子注册/注销、消息循环、启动握手和线程停止/回收 |

运行时模块共享同一个私有 `Impl`，保留原子操作的内存序和钩子线程中每键状态。
轮盘状态处理与录入处理返回可选结果：有结果时直接消费/传递当前事件；无结果时
继续原有调度流程。短按回放仍携带专用合成标记，避免自己的回放再次触发钩子。
非 Windows 的服务接口继续采用原有失败通知/空操作实现。

快捷切换拆分后 Release 构建与全量 23 个 CTest 目标通过。新增
`cpp_quick_switch_service_test` 在不安装全局钩子、不发送真实输入的情况下覆盖
普通/扩展键映射、方向冲突优先级、映射更新期间的读取保护、一次性录入与松开
清理、注入输入排除、轮盘取消会话和服务销毁后的排队信号。展开提取的录入与
轮盘处理步骤后，32 个原有方法的实现内容一致；原文件由 886 行减至 14 行。

## 平台服务与线程

原生 DLL 按功能拆分，`native/src/eve_viola_native.cpp` 只保留 API 版本入口。

| 文件（位于 `native/src`） | 职责 |
| --- | --- |
| `eve_window_identity.cpp` | 查询窗口进程路径，确认 EVE 客户端身份；枚举和激活共用。 |
| `window_enumeration.cpp` | 任务窗口过滤、标题匹配、窗口枚举与排序、列表分配及释放。 |
| `window_thumbnail.cpp` | DWM 缩略图注册、源尺寸查询、属性更新与注销。 |
| `window_activation.cpp` | 输入线程临时连接、窗口还原、前台激活重试及失败闪烁。 |
| `window_capture_pixels.cpp` | 兼容像素缓冲截图、分阶段性能记录、缓冲分配及释放。 |
| `window_capture_frame.cpp` | DIB 帧截图、工作线程缓存、保留帧所有权及 GDI 资源释放。 |

`native_platform.h` 统一 Windows 头文件设置，`native_status.h` 提供内部返回码，
`capture_timer.h` 提供两条截图路径共用的计时工具。公开头文件及 v3 C ABI 不变；
13 个原有导出函数的实现内容保持一致。原文件从 638 行减至 9 行，截图缓存仍按
工作线程隔离，保留帧不会被后续截图覆盖。

原生模块拆分后 Release 构建通过，现有截图测试覆盖客户区像素、兼容像素接口、
保留帧、缩放、工作线程隔离和 GDI 资源生命周期。首次全量测试中主窗口测试
无诊断输出地失败一次，单独重跑通过，随后全量 23 个 CTest 目标均通过。

`ActivationCoordinator` 拥有冷却、鼠标静默期、待激活请求和触发仲裁。时钟与
实际激活操作可注入。鼠标活动接口供钩子线程调用，其余策略操作在应用线程
执行。`NativeGateway` 保留转发接口以兼容现有调用，并专注原生 API 与资源。

`WindowCatalog` 共享短期窗口快照，并用一个五秒监控器服务游戏恢复和分区检测。
显式源列表刷新与轮盘激活前校验使用新快照。枚举失败不解释为所有游戏退出。

`StatisticsCoordinator` 继续使用两个捕获工作线程、同源互斥和版本检查。
冻结画面也在后台执行，工作任务只持有原生函数表和值数据，不操作 QWidget。
冻结请求有并发上限，取消后相同小窗的旧请求仍占据运行名额直到完成，避免
快速取消和重试堆积任务；迟到结果按请求编号丢弃。隐藏小窗或编辑待冻结画面的
区域、显示内容和缩放时取消旧请求；调整统计登记不取消交互截图。

停止调度立即失效登记和冻结请求，不在界面线程等待。正常退出在事件循环中
等待 `idle`，随后销毁小窗和服务；直接销毁协调器时仍保留线程等待屏障，以
保证资源安全。`PrintWindow` 没有在此实现中可安全强制取消的机制，所以不保证
异常源窗口下的退出时间上限；等待期间正常关闭流程继续处理界面事件。

性能记录增加 `scheduler.result_age`，表示派发到结果交付的单调时钟耗时。
捕获后端、颜色阈值、统计间隔及同源合并规则未改变；进一步更换捕获后端应
依据实际捕获、扫描、延迟和失败指标。

## 验证

新增架构服务测试覆盖状态快照、隐藏约束、临时模式叠加、预设恢复、可注入时钟
和目录缓存。补充配置损坏保护、无关章节不重写、保存不取消激活、桌宠配置保护、
复制和隐藏几何编辑、后台冻结取消以及停止后排空测试。

冻结界面的测试等待异步帧，并关闭该测试小窗的统计，以分别验证缓冲区释放和
统计管线。主窗口测试改为经控制器验证隐藏约束；已有分组断言按同源全组行为
验证，时间页用例通过再次点击当前时间页验证刷新。

Windows Release 使用 CMake/CTest 验证。受限沙箱可能无法写入 Qt 创建的私有
临时目录，测试应在具有正常临时目录权限的环境执行。真实多客户端 CPU、GPU
和音频设备行为仍需桌面场景测量，自动化通过不代表这些性能指标已重新测量。

2026-10-14：Release 构建与全量 19 个 CTest 目标通过。最后的隐藏冻结取消改动
再次通过小窗、主窗口和应用启动三个受影响目标；`git diff --check` 通过。

分发脚本保留已有配置和日志，使用 Windows 平台插件检查启动。已验证从测试
进程的 PATH 移除 Qt SDK、清除插件路径变量后启动成功，也验证了打包前后的
配置与日志文件保留。开发测试使用的 `offscreen` 插件不属于分发包。
