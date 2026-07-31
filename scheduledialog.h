// ============================================================
// ScheduleDialog — 日程管理弹窗（时间线日程）
// 从任务库中选择任务模板，设定开始时间和时长，
// 添加到指定日期的日程安排中。支持时间冲突检测。
// ============================================================
#ifndef SCHEDULEDIALOG_H
#define SCHEDULEDIALOG_H

#include <QDialog>
#include <QDate>
#include <QComboBox>
#include <QTimeEdit>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QTableWidget>

class ScheduleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScheduleDialog(const QDate &date, QWidget *parent = nullptr);

private slots:
    void onAddToTimeline();      // 将所选任务添加到当日日程
    void updateEndTime();        // 根据开始时间+时长自动计算结束时间
    void refreshScheduleTable(); // 刷新当日日程表格

private:
    void setupUi();              // 构建界面
    void refreshTaskSelector();  // 刷新任务模板下拉选择器

    QDate m_date;                    // 当前日期
    QComboBox *m_taskCombo;          // 任务模板下拉选择器
    QTimeEdit *m_startTimeEdit;      // 开始时间编辑
    QDoubleSpinBox *m_durationSpinBox; // 时长编辑（小时）
    QLabel *m_endTimeLabel;           // 预计结束时间显示
    QTableWidget *m_scheduleTable;    // 当日已有日程表格
};

#endif // SCHEDULEDIALOG_H
