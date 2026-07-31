# Life Manager — 个人生活综合管理软件

为了养成自律的好习惯，一个时间管理桌面软件，规划时间、记录睡眠、饮水、健身、饮食。

基于 Qt 6 开发，三栏布局，JSON 本地持久化。

## 功能模块

| 模块 | 说明 |
|------|------|
| 🏠 **首页** | 自定义背景 + 语录 + 今日任务/习惯概览 |
| 📅 **时间线** | 日历 + 倒计时 + 日程表 + 睡眠/饮水/饮食/运动管理 |
| 📚 **任务库** | 任务模板创建管理 + 习惯养成（支持次数/小时/时长三种模式） |
| ✅ **已完成** | 完成历史记录表格 |
| 🎯 **技能表** | 技能进度管理 + 已达成标签展示 |
| 📝 **每日复盘** | 编辑器 + 历史记录 + 每日综合报告生成 |
| 🏆 **成就殿堂** | 里程碑/毅力/速度/分类成就卡片 |

## 技术栈

- **语言**: C++17
- **框架**: Qt 6 (支持 Qt 5 回退)
- **构建**: CMake + qmake
- **平台**: Windows 10/11 64-bit
- **数据存储**: 本地 JSON (`%AppData%/LifeManager/lifedata.json`)

## 构建

### 依赖

- Qt 6.x (Widgets, Sql, Network) 或 Qt 5.15+
- CMake 3.16+
- MinGW 64-bit 或 MSVC

### CMake 构建

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### qmake 构建

```bash
qmake life.pro
make
```

## 项目结构

```
life/
├── main.cpp                 # 入口：QApplication + QSS + MainWindow
├── mainwindow.h/cpp/ui      # 主窗口：QSplitter 三栏布局
├── leftpanel.h/cpp/ui       # 左侧：用户信息 + 导航 + 最近完成
├── centerpanel.h/cpp/ui     # 中央：QStackedWidget(7页) + 所有业务逻辑
├── rightpanel.h/cpp/ui      # 右侧：时钟/评分/体力/心情/健康/睡眠
├── datamanager.h/cpp        # 单例数据层：JSON持久化 + 经验结算
├── taskdialog.h/cpp         # 任务管理弹窗
├── dietdialog.h/cpp         # 饮食记录弹窗
├── scheduledialog.h/cpp     # 日程管理弹窗（含时间冲突检测）
├── resources/
│   ├── style/               # 导航图标 + 全局 QSS 样式表
│   └── backgrounds/         # 首页背景图
├── CHANGELOG.md             # 开发日志
└── ISSUES.md                # 项目问题总结
```

## StackedWidget 页面索引

| 索引 | 页面 |
|------|------|
| 0 | 🏠 首页 |
| 1 | 📅 时间线 |
| 2 | 📚 任务库 |
| 3 | ✅ 已完成 |
| 4 | 🎯 技能表 |
| 5 | 📝 每日复盘 |
| 6 | 🏆 成就殿堂 |

## 经验 & 等级系统

- **等级**: `Lv = totalXp / 100 + 1`
- **任务经验**: `XP = (日程时长 / xpBaseHours) × xp`
- **技能经验**: 按星级计算 `10 × stars^1.5`

## 数据存储

所有数据存储在 `%AppData%/LifeManager/lifedata.json`，结构如下：

```json
{
  "profile": { "username", "bio", "avatarPath", "level", "xp" },
  "skills": [{ "id", "name", "plannedHours", "investedHours" }],
  "plans": [{ "id", "title", "plannedHours", "xpBaseHours", "xpAmount" }],
  "schedules_YYYYMMDD": [{ "id", "planId", "startTime", "plannedHours", "completed" }],
  "habits": [{ "id", "name", "habitMode", "completionMode" }],
  "sleep_YYYYMMDD": { "sleepTime", "wakeTime" },
  "water_YYYYMMDD": { "targetMl", "currentMl" },
  "diet_YYYYMMDD": [{ "mealType", "foodName" }],
  "exercise_YYYYMMDD": [{ "exerciseType", "durationMinutes" }]
}
```

## 许可证

本项目基于 MIT 许可证开源，详见 [LICENSE](./LICENSE)。
