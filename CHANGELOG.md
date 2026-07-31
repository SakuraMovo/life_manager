# Life Manager 开发日志

## 项目概述

**个人生活综合管理软件** — Qt 6.9.1 (MinGW) 桌面应用，三栏布局，JSON 本地持久化。

- **技术栈**: C++17 / Qt 6.9.1 / CMake + qmake
- **目标平台**: Windows 10/11 64-bit
- **数据存储**: `%AppData%/LifeManager/lifedata.json`

---

## 2026-07-28

### 🐛 Bug 修复

#### 每日复盘「生成今日报告」跳转到成就殿堂
- **文件**: `centerpanel.cpp:2755`
- **原因**: `generateReport()` 末尾 `setCurrentIndex(6)` 指向成就殿堂（应为5=复盘页）
- **修复**: 改为 `setCurrentIndex(5)`

#### 每日报告任务完成情况始终为空
- **文件**: `centerpanel.cpp` (`generateReport()`)
- **原因**: 使用了旧版 `getTasks()` 读取 `tasks_YYYYMMDD`，而非新版 `getSchedules()` 读取日程数据
- **修复**: 改为读取 `getSchedules()` + `getActiveHabits()`，与时间线/首页数据源统一

#### 时间线饮水滑块回弹
- **文件**: `centerpanel.cpp` (构造函数)
- **原因**: 1秒定时器每 tick 调用 `refreshTimelineView()`，该函数每次将滑块重置为已保存值
- **修复**: 定时器回调改为只调用 `refreshTimelineTasks()`（倒计时+日程），不刷新饮水/睡眠等数据

### ✨ 新功能

#### 首页显示今日习惯
- **文件**: `centerpanel.cpp` (`refreshHomeTasks()`)
- **内容**: 首页日程卡片下方新增"🌱 今日习惯"区域，显示当日活跃习惯的完成进度（次数/小时/分钟）
- **状态**: 绿色边框仅在日程和习惯全部完成时显示

#### 任务模板经验结算改为时长比例制
- **文件**: `datamanager.h/cpp`, `centerpanel.cpp`
- **旧逻辑**: `XP = 10 × 星级^1.5`（固定值，与时长无关）
- **新逻辑**: `XP = (实际时长 / xpBaseHours) × xp`
- **数据结构**: `PlanItem` 新增 `xpBaseHours`（每X小时）和 `xp`（获得Y经验）
- **UI 变更**: 任务库创建表单"难度/星级"替换为"每 X 小时 / 获得 Y 经验"
- **旧数据兼容**: 自动迁移，`xpBaseHours = 原计划时长`，`xp = 旧公式`

#### 历史复盘删除功能
- **文件**: `datamanager.h/cpp`, `centerpanel.cpp`
- **内容**: 复盘历史表格新增第4列 🗑 删除按钮，同时删除 JSON 记录和 Markdown 文件
- **DataManager**: 新增 `deleteReview(const QDate &date)` 方法

### 🎨 UI 优化

#### 成就殿堂滚动自适应
- **文件**: `centerpanel.cpp` (`buildAchievementPage()`)
- **修复**: 滚动区域设置 `ml->addWidget(scroll, 1)` + `setSizePolicy(Expanding, Expanding)`，填满页面剩余空间

#### 技能表滚动自适应
- **文件**: `centerpanel.cpp` (`buildSkillsPage()`)
- **修复**: 技能表格包裹在 `QScrollArea` 中，`ml->addWidget(tableScroll, 1)`，技能多时可滚动

#### 技能达成展示重新设计
- **文件**: `centerpanel.cpp` (`buildSkillsPage()` / `refreshSkillsPage()`)
- **旧版**: 40×40 圆形标签，只显示首字母
- **新版**: 药丸形彩色标签显示完整文本 `🎯 技能名 ✓ XXh`
- **布局**: QScrollArea 水平滚动，不限数量

#### 首页背景自适应显示
- **文件**: `centerpanel.h/cpp` (`setupHomePage()` / `renderBackground()` / `eventFilter()`)
- **旧版**: `setScaledContents(true)` 强制拉伸填满
- **新版**:
  - 适应/拉伸两种模式可切换
  - 水平对齐（◀ ● ▶）和垂直对齐（▲ ● ▼）
  - `renderBackground()` 用 `QPainter` 按比例缩放+定位绘制
  - `eventFilter` 监听 Resize 事件自动重绘
- **底部工具栏**: 新增 📐 适应/拉伸 切换按钮 + 6 个对齐按钮

### 📝 代码质量

#### 全项目中文化注释
- **范围**: 全部 17 个源文件（.h + .cpp）
- **内容**: 文件头说明、数据结构字段注释、函数功能说明、关键逻辑解释

### 📦 打包部署

- **工具**: `windeployqt` 自动收集 Qt DLL + MinGW 运行时
- **输出**: `release/LifeManager_portable.zip` (25MB)
- **内容**: LifeManager.exe + Qt6 核心库 + 插件（platforms/imageformats/styles/tls）
- **使用**: 解压到任意目录，双击 `LifeManager.exe` 即可运行，无需安装任何依赖

---

## 项目架构

```
life/
├── main.cpp                 # 入口：QApplication + QSS + MainWindow
├── mainwindow.h/cpp/ui      # 主窗口：QSplitter 三栏布局
├── leftpanel.h/cpp/ui       # 左侧：用户信息 + 7导航 + 最近完成
├── centerpanel.h/cpp/ui     # 中央：QStackedWidget(7页) + 所有业务逻辑
├── rightpanel.h/cpp/ui      # 右侧：时钟/评分/体力/心情/健康/睡眠
├── datamanager.h/cpp        # 单例数据层：JSON持久化 + 经验结算
├── taskdialog.h/cpp         # 旧版任务弹窗（观察/编辑模式）
├── dietdialog.h/cpp         # 饮食记录弹窗
├── scheduledialog.h/cpp     # 日程管理弹窗（含冲突检测）
└── resources/               # QRC 资源文件
    ├── style/               # 导航图标 PNG + global.qss + cat.png
    └── backgrounds/         # 首页背景图
```

### StackedWidget 页面索引

| 索引 | 页面 | 内容 |
|------|------|------|
| 0 | 🏠 首页 | 背景+语录+今日任务/习惯+透明度/背景控制 |
| 1 | 📅 时间线 | 日历+倒计时+日程表+睡眠/饮水/饮食/运动编辑 |
| 2 | 📚 任务库 | 任务模板创建/管理 + 习惯养成 |
| 3 | ✅ 已完成 | 完成历史表格 |
| 4 | 🎯 技能表 | 技能管理+进度条+已达成标签 |
| 5 | 📝 每日复盘 | 编辑器+历史+报告生成 |
| 6 | 🏆 成就殿堂 | 里程碑/毅力/速度/分类成就卡片 |

### 经验 & 等级系统

- **等级**: `Lv = totalXp / 100 + 1`
- **任务经验**: `XP = (日程时长 / 模板.xpBaseHours) × 模板.xp`
- **技能 XP**: `10 × 星级^1.5` (habit)
- **升级**: `addXp()` 自动检测升级并更新档案

### 数据存储结构 (JSON)

```json
{
  "profile": { "username", "bio", "avatarPath", "level", "xp" },
  "skills": [{ "id", "name", "plannedHours", "investedHours" }],
  "plans": [{ "id", "title", "plannedHours", "xpBaseHours", "xpAmount", "skillId", "stars", "status" }],
  "schedules_YYYYMMDD": [{ "id", "planId", "startTime", "plannedHours", "completed" }],
  "reviews": [{ "date", "content" }],
  "completed_records": [{ "completedDate", "scheduleDate", "planTitle", "skillName", "plannedHours", "xp" }],
  "habits": [{ "id", "name", "habitMode", "completionMode", "targetHours/targetCount/targetMinutes" }],
  "sleep_YYYYMMDD": { "sleepTime", "wakeTime" },
  "water_YYYYMMDD": { "targetMl", "currentMl" },
  "diet_YYYYMMDD": [{ "mealType", "foodName" }],
  "exercise_YYYYMMDD": [{ "exerciseType", "durationMinutes" }],
  "tasks_YYYYMMDD": [{ "time", "content", "completed" }]
}
```
