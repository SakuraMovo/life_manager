# 项目问题总结

回顾整个开发过程，按类别梳理遇到的所有问题与解决思路。

---

## 一、空指针 & 初始化顺序

### 1. 段错误：右侧面板 timer 初始化顺序错误
- **现象**：程序启动即崩溃（SIGSEGV）
- **原因**：`m_clockTimer` 在 `setup*Module()` 方法中被 `connect` 使用，但 `m_clockTimer = new QTimer(this)` 在部分 `setup*` 调用之后执行
- **解决**：将 `m_clockTimer` 的初始化提前到所有 `setup*` 方法之前
- **教训**：构造函数中成员初始化的顺序必须与使用顺序一致；依赖该成员的代码应排在初始化之后

---

## 二、数据源不一致

### 2. 每日报告读取不到任务数据
- **现象**：报告显示"总任务数: 0，完成率: 0%"
- **原因**：`generateReport()` 调用的是旧版 `getTasks()`（读写 `tasks_YYYYMMDD`），但用户通过时间线创建的是 Schedule（`schedules_YYYYMMDD`），两套数据互不相通
- **解决**：统一改为读取 `getSchedules()` + `getActiveHabits()`
- **教训**：重构数据模型后，需全局搜索旧 API 调用点并逐一迁移；遗留兼容接口应加 `[[deprecated]]` 标记

### 3. 经验结算依赖旧版数据结构
- **现象**：经验值计算与用户的实际投入时长无关
- **原因**：`PlanItem::xpValue()` 返回固定值 `10 × 星级^1.5`，与每次日程安排的实际时长毫无关联
- **解决**：新增 `xpBaseHours` / `xp` 字段，改为 `XP = (实际时长 / xpBaseHours) × xp`，`markScheduleComplete()` 中按照 `schedule.plannedHours` 动态结算
- **旧数据兼容**：加载时若检测到无新字段，则 `xpBaseHours = plannedHours`（即完成完整任务才获得旧版满额经验）

---

## 三、UI 刷新策略

### 4. 饮水滑块回弹（最典型的刷新策略问题）
- **现象**：拖动饮水滑块到目标值，1 秒内自动弹回旧值
- **原因**：1 秒定时器每 tick 调用 `refreshTimelineView()`，该函数无条件执行 `m_waterSlider->setValue(w.currentMl)` 从已保存数据重置滑块
- **解决**：定时器改为只调用 `refreshTimelineTasks()`（仅刷新倒计时+日程），不再刷新睡眠/饮水/饮食等无需秒级更新的数据
- **教训**：定时刷新要与数据变更区分粒度——秒级只刷新"当前时刻相关的状态"，数据展示在用户手动保存或切页时刷新

### 5. 成就/技能页面内容被截断
- **现象**：卡片/表格超出窗口可视区域，无法滚动查看
- **原因**：`QScrollArea` 或表格直接放在页面布局中，但没有 `addWidget(scroll, 1)` 给予拉伸权重，组件被压缩到最小高度
- **解决**：`ml->addWidget(scrollArea, 1)` + `setSizePolicy(Expanding, Expanding)` + `setWidgetResizable(true)`
- **教训**：任何可能内容超出的容器（表格、卡片列表），都需要包裹在 QScrollArea 中并设置正确的 stretch factor

---

## 四、索引偏移

### 6. "生成今日报告"跳转到成就殿堂
- **现象**：点击报告按钮，页面切到了成就殿堂而非复盘页
- **原因**：`setCurrentIndex(6)` 指向成就殿堂（stackedWidget index 6），复盘页实际在 index 5
- **解决**：改为 `setCurrentIndex(5)`
- **教训**：QStackedWidget 索引与导航索引之间存在偏移（因为 index 0 是首页），任何硬编码索引都应该提取为常量枚举

### 7. 导航按钮与页面索引的隐式映射
- **现状**：左侧 7 个按钮（btnIdx 0~6）→ 首页特殊处理（emit goHome → -1）→ 业务页 navIdx = btnIdx - 1 → CenterPanel::switchToPage 再做 +1
- **隐患**：三重索引转换（btn → nav → page），任何一环出错都导致跳转错误
- **建议**：用 `enum class Page { Home=0, Timeline, StudyPlan, ... }` 统一管理，消除魔数

---

## 五、GUI 渲染

### 8. 背景图片拉伸变形
- **现象**：更换背景后图片被强制拉伸填满窗口，比例失调
- **原因**：`QLabel::setScaledContents(true)` 不考虑原始宽高比
- **解决**：用 QPainter 手动渲染：`QPixmap::scaled(labelSize, KeepAspectRatio)` + 按九宫格对齐参数计算偏移量 + `drawPixmap(x, y, scaled)` 绘制到透明画布
- **扩展**：增加适应/拉伸双模式、6 方向对齐按钮、通过 eventFilter 监听 Resize 事件实现动态重绘

### 9. 技能达成标签截断
- **现象**：只显示技能名首字母，文字超长时无法阅读
- **原因**：40×40 固定圆形 QLabel，只能容纳一个汉字
- **解决**：改为药丸形标签（圆角+padding），设置 `setSizePolicy(Preferred, Fixed)` 按内容自适应宽度，外包裹 QScrollArea 水平滚动

---

## 六、构建与工程

### 10. .qrc 路径含中文括号导致 CMake 构建失败
- **现象**：cmake build 报资源文件错误
- **原因**：Qt 资源系统对非 ASCII 路径支持有限，中文全角括号在编译 rcc 时出错
- **解决**：将资源路径中的中文符号替换为 ASCII 字符
- **教训**：跨平台/跨编译器的资源文件路径一律使用 ASCII

### 11. Qt Creator 影子构建缓存污染
- **现象**：修改代码后编译运行，仍是旧版本行为
- **原因**：`build/kit11-Debug/` 残留旧的 moc/ui 生成文件，增量编译未触发重新生成
- **解决**：编译前删除影子构建目录，或使用独立 `build-debug` 目录
- **建议**：CI 脚本中加入 `clean` 步骤，或使用 CMake 的 `--fresh` 参数

### 12. 部署依赖缺失
- **现象**：编译好的 exe 复制到其他电脑无法运行，提示"缺少 Qt6Core.dll"
- **解决**：`windeployqt LifeManager.exe` 自动解析所有 Qt 依赖 + 插件 + MinGW 运行时，打包为绿色免安装 zip
- **注意**：windeployqt 默认跳过 OpenSSL 后端（`qopensslbackend.dll`），若用到 HTTPS 需加 `-force-openssl` 参数

---

## 问题分布统计

| 类别 | 数量 | 典型问题 |
|------|------|---------|
| 初始化顺序 / 空指针 | 1 | timer 先于 setup 初始化 |
| 数据源不一致 | 2 | getTasks vs getSchedules、旧 XP 公式 |
| 刷新策略 | 2 | 滑块回弹、页面截断 |
| 索引偏移 | 2 | 报告跳错页、导航三重转换 |
| GUI 渲染 | 2 | 背景拉伸、标签截断 |
| 构建工程 | 3 | qrc ASCII、构建缓存、部署依赖 |
| **合计** | **12** | |

## 高频根因

1. **隐式约定大于显式声明** —— 魔数索引、新旧 API 并存、字段缺失时的默认行为，都容易在修改中遗漏
2. **定时刷新粒度太粗** —— 把"每秒级动态数据"和"用户主动修改的静态数据"放在同一个刷新函数里
3. **UI 容器没有预留伸缩空间** —— QTableWidget / QScrollArea 不设 stretch factor，导致窗口缩放时内容被压缩
