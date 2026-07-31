// ============================================================
// CenterPanel — 中央内容面板
// 使用 QStackedWidget 管理 7 个子页面：
//   0: 首页（背景+语录+今日任务/习惯+透明度/背景控制）
//   1: 时间线（日历+倒计时+日程表+睡眠/饮水/饮食/运动编辑）
//   2: 任务库（任务模板管理+习惯养成）
//   3: 已完成记录（完成历史表格）
//   4: 技能表（技能管理+进度条+已达成药丸标签）
//   5: 每日复盘（编辑器+历史+报告生成）
//   6: 成就殿堂（里程碑/毅力/速度/分类成就卡片）
// ============================================================
#ifndef CENTERPANEL_H
#define CENTERPANEL_H

#include <QWidget>
#include <QStackedWidget>
#include <QLabel>
#include <QSlider>
#include <QComboBox>
#include <QPushButton>
#include <QTableWidget>
#include <QTextEdit>
#include <QDateEdit>
#include <QTimeEdit>
#include <QSpinBox>
#include <QTimer>
#include <QLineEdit>
class TaskDialog;
class DietDialog;

namespace Ui {
class CenterPanel;
}

class CenterPanel : public QWidget
{
    Q_OBJECT

public:
    explicit CenterPanel(QWidget *parent = nullptr);
    ~CenterPanel();
    // 由 MainWindow 调用，根据左侧导航索引切换页面
    void switchToPage(int index);

private slots:
    void onEditQuote();          // 编辑首页语录
    void onChangeBackground();   // 更换首页背景
    void refreshCurrentPage();   // 刷新当前激活的子页面
    void generateReport();       // 生成每日综合报告

private:
    // ---- 首页 ----
    void setupHomePage();
    void loadDefaultBackground();     // 加载默认背景
    void applyBackgroundSettings();   // 应用透明度
    void renderBackground();          // 根据对齐/适应模式重绘背景
    bool eventFilter(QObject *obj, QEvent *event) override; // 处理首页尺寸变化

    // ---- 子页面构建 ----
    void setupBusinessPages();
    QWidget* buildTimelineViewPage();   // 时间线（统一视图）
    QWidget* buildStudyPlanPage();      // 任务库 + 习惯养成
    QWidget* buildCompletedPage();      // 已完成记录
    QWidget* buildSkillsPage();         // 技能表
    QWidget* buildReviewPage();         // 每日复盘
    QWidget* buildAchievementPage();    // 成就殿堂

    // ---- 日历组件 ----
    QWidget* createCalendarWidget(QWidget *parent);
    void refreshCalendar(QWidget *calendar, const QDate &targetDate);
    QDate m_calendarDate;  // 当前选中日期

    // ---- 数据刷新 ----
    void refreshDataCards(QWidget *cardsWidget, const QDate &date);  // 数据卡片
    void refreshDietTable();                                          // 饮食表格
    void refreshExerciseTable();                                      // 运动表格
    void refreshTimelineView();         // 时间线全景
    void refreshTimelineTasks();        // 日程表+倒计时+习惯
    void refreshHomeTasks();            // 首页今日任务+习惯
    void refreshStudyPlanPage();        // 任务库页面
    void refreshCompletedPage();        // 已完成页面
    void refreshSkillsPage();           // 技能页面
    void refreshReviewPage();           // 复盘页面
    void refreshAchievementPage();      // 成就页面
    void onCalendarDateDoubleClicked(const QDate &date);  // 日历双击→打开日程编辑
    void onSkillSelectorChanged(int skillId);             // 任务库技能筛选变化

    Ui::CenterPanel *ui;

    // ---- 首页控件 ----
    QLabel *m_backgroundLabel;        // 背景图标签
    QLabel *m_quoteLabel;             // 语录标签
    QLabel *m_antiProcrastinateLabel; // 防拖延提示
    QSlider *m_opacitySlider;         // 透明度滑块

    // ---- 子页面指针 ----
    QWidget *m_timelineViewPage = nullptr;   // 时间线
    QWidget *m_studyPlanPage = nullptr;      // 任务库
    QWidget *m_completedPage = nullptr;      // 已完成
    QWidget *m_skillsPage = nullptr;         // 技能表
    QWidget *m_reviewPage = nullptr;         // 每日复盘
    QWidget *m_achievementPage = nullptr;    // 成就殿堂

    // ---- 时间线控件 ----
    QWidget *m_timelineCalendar = nullptr;       // 日历
    QWidget *m_timelineCards = nullptr;          // 数据卡片（已废弃）
    QTableWidget *m_planTable = nullptr;         // 任务模板表格
    QComboBox *m_sleepH = nullptr;               // 睡眠-小时
    QComboBox *m_sleepM = nullptr;               // 睡眠-分钟
    QComboBox *m_wakeH = nullptr;                // 醒来-小时
    QComboBox *m_wakeM = nullptr;                // 醒来-分钟
    QLabel *m_sleepDisplayLabel = nullptr;       // 睡眠数据显示
    QSlider *m_waterSlider = nullptr;            // 饮水滑块
    QLabel *m_waterLabel = nullptr;              // 饮水数据显示
    QTableWidget *m_viewDietTable = nullptr;     // 饮食记录表格
    QTimeEdit *m_dietTime = nullptr;             // 饮食时间
    QComboBox *m_dietMealCb = nullptr;           // 饮食餐次
    QLineEdit *m_dietFoodInput = nullptr;        // 饮食食物输入
    QComboBox *m_exerciseTypeCb = nullptr;       // 运动类型
    QSpinBox *m_exerciseMinInput = nullptr;      // 运动时长
    QLabel *m_exerciseLabel = nullptr;           // 运动数据显示
    QLabel *m_exerciseDisplayBox = nullptr;      // 运动详情卡片
    QTableWidget *m_viewExerciseTable = nullptr; // 运动记录表格
    QLabel *m_planProgressLabel = nullptr;       // 计划进度标签
    QTableWidget *m_skillsTable = nullptr;       // 技能表格
    QWidget *m_bubbleArea = nullptr;             // 已完成技能标签区域
    QComboBox *m_skillSelector = nullptr;        // 技能筛选下拉
    QLabel *m_skillPlanSummaryLabel = nullptr;   // 技能摘要标签
    QTextEdit *m_reviewEditor = nullptr;         // 复盘编辑器
    QTableWidget *m_reviewHistory = nullptr;     // 复盘历史表格
    QLabel *m_reportLabel = nullptr;             // 报告预览标签

    // ---- 倒计时 & 日程 ----
    QTimer *m_countdownTimer = nullptr;              // 倒计时定时器（每秒）
    QTableWidget *m_timelineScheduleTable = nullptr; // 日程表格
    QTableWidget *m_timelineHabitTable = nullptr;    // 习惯表格
    QLabel *m_countdownClockLabel = nullptr;         // 倒计时钟
    QLabel *m_countdownHintLabel = nullptr;          // 倒计时提示
    QLabel *m_homeScheduleLabel = nullptr;           // 首页日程+习惯标签

    // ---- 习惯养成 ----
    QTableWidget *m_habitsTable = nullptr;          // 习惯表格
    QComboBox *m_habitModeCb = nullptr;             // 习惯周期下拉
    QComboBox *m_habitCompleteModeCb = nullptr;     // 习惯完成方式下拉

    // ---- 背景控制 ----
    double m_backgroundOpacity = 0.7;    // 当前透明度
    QPixmap m_backgroundPixmap;          // 原始背景图
    bool m_backgroundFitMode = true;     // 适应模式（true=保持比例, false=拉伸填充）
    int m_bgHAlign = 1;                  // 水平对齐 0=左 1=中 2=右
    int m_bgVAlign = 1;                  // 垂直对齐 0=上 1=中 2=下
};

#endif // CENTERPANEL_H
