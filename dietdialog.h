// ============================================================
// DietDialog — 饮食记录弹窗
// 用于查看和管理指定日期的饮食记录（早餐/午餐/晚餐/加餐）
// ============================================================
#ifndef DIETDIALOG_H
#define DIETDIALOG_H

#include <QDialog>
#include <QDate>
#include <QTableWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QTimeEdit>

class DietDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DietDialog(const QDate &date, QWidget *parent = nullptr);

private slots:
    void addRecord();     // 添加一条饮食记录
    void refreshTable();  // 刷新记录表格

private:
    QDate m_date;              // 记录日期
    QTableWidget *m_table;     // 记录表格
    QTimeEdit *m_timeEdit;     // 时间选择器
    QComboBox *m_mealType;     // 餐次选择（早/午/晚/加餐）
    QLineEdit *m_foodInput;    // 食物名称输入框
};

#endif // DIETDIALOG_H
