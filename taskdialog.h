// ============================================================
// TaskDialog — 旧版任务管理弹窗（向后兼容）
// 支持两种模式：观察模式（完成/撤销）、编辑模式（添加/删除）
// 注意：主要任务管理已迁移到 ScheduleDialog + 时间线
// ============================================================
#ifndef TASKDIALOG_H
#define TASKDIALOG_H

#include <QDialog>
#include <QDate>
#include <QTableWidget>
#include <QLineEdit>
#include <QTimeEdit>
#include <QLabel>

class TaskDialog : public QDialog
{
    Q_OBJECT

public:
    // editMode: true = 编辑模式（可添加/删除任务）
    //           false = 观察模式（仅可完成/撤销已有任务）
    explicit TaskDialog(const QDate &date, bool editMode = false, QWidget *parent = nullptr);

private slots:
    void addTask();       // 添加新任务
    void refreshTable();  // 刷新任务表格

private:
    void setupUi(bool editMode);  // 根据模式构建界面

    QDate m_date;             // 当前日期
    QTableWidget *m_table;    // 任务表格
    QTimeEdit *m_timeEdit;    // 时间选择器（编辑模式）
    QLineEdit *m_taskInput;   // 任务内容输入框（编辑模式）
    QLabel *m_statsLabel;     // 统计标签（任务数/完成率）
};

#endif // TASKDIALOG_H
