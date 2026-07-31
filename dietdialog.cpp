// ============================================================
// 饮食记录窗口实现
// ============================================================

#include "dietdialog.h"
#include "datamanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>

DietDialog::DietDialog(const QDate &date, QWidget *parent)
    : QDialog(parent), m_date(date)
{
    setWindowTitle(QString::fromUtf8("饮食记录 - %1").arg(date.toString("yyyy年M月d日")));
    resize(540, 400);
    setMinimumSize(420, 300);
    setStyleSheet("QDialog { background-color: rgba(18, 14, 42, 0.97); }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    // ---- 添加表单行：时间 + 餐次 + 食物名称 + 添加按钮 ----
    QHBoxLayout *formRow = new QHBoxLayout();
    formRow->setSpacing(8);

    m_timeEdit = new QTimeEdit(QTime::currentTime(), this);
    m_timeEdit->setDisplayFormat("HH:mm");
    m_timeEdit->setStyleSheet(
        "QTimeEdit { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
        "  border-radius: 8px; padding: 5px 8px; color: #e2e0f0; font-size: 12px; }");
    formRow->addWidget(m_timeEdit);

    m_mealType = new QComboBox(this);
    m_mealType->addItems({QString::fromUtf8("早餐"), QString::fromUtf8("午餐"),
                          QString::fromUtf8("晚餐"), QString::fromUtf8("加餐")});
    m_mealType->setStyleSheet(
        "QComboBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
        "  border-radius: 8px; padding: 5px 10px; color: #e2e0f0; font-size: 12px; }"
        "QComboBox:hover { background: rgba(255,255,255,0.08); }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: rgba(20,16,42,0.95); color: #e2e0f0; selection-background-color: rgba(139,158,246,0.30); }");
    formRow->addWidget(m_mealType);

    m_foodInput = new QLineEdit(this);
    m_foodInput->setPlaceholderText(QString::fromUtf8("食物名称..."));
    m_foodInput->setStyleSheet(
        "QLineEdit { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
        "  border-radius: 8px; padding: 6px 10px; color: #e2e0f0; font-size: 12px; }");
    formRow->addWidget(m_foodInput, 1);

    QPushButton *addBtn = new QPushButton(QString::fromUtf8("＋ 添加"), this);
    addBtn->setStyleSheet(
        "QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 8px;"
        "  padding: 6px 14px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: #a0b0ff; }");
    connect(addBtn, &QPushButton::clicked, this, &DietDialog::addRecord);
    formRow->addWidget(addBtn);

    mainLayout->addLayout(formRow);

    // ---- 记录表格：时间 | 餐次 | 饮食内容 | 操作 ----
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({QString::fromUtf8("时间"),
                                         QString::fromUtf8("餐次"),
                                         QString::fromUtf8("饮食内容"),
                                         QString::fromUtf8("操作")});
    m_table->setStyleSheet(
        "QTableWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; color: #c0bcd8; }"
        "QTableWidget::item { padding: 4px; }"
        "QHeaderView::section { background: rgba(255,255,255,0.04); color: #9e9ab8; border: none; padding: 6px; font-size: 11px; }");
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);  // 食物名称列自适应
    m_table->setColumnWidth(0, 55);
    m_table->setColumnWidth(1, 55);
    mainLayout->addWidget(m_table);

    // ---- 关闭按钮 ----
    QHBoxLayout *bottomRow = new QHBoxLayout();
    bottomRow->addStretch();
    QPushButton *closeBtn = new QPushButton(QString::fromUtf8("关闭"), this);
    closeBtn->setStyleSheet(
        "QPushButton { color: #c0bcd8; background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
        "  border-radius: 8px; padding: 6px 24px; font-size: 12px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.10); color: #e2e0f0; }");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    bottomRow->addWidget(closeBtn);
    mainLayout->addLayout(bottomRow);

    refreshTable();
}

// 添加一条饮食记录到 DataManager
void DietDialog::addRecord()
{
    if (m_foodInput->text().trimmed().isEmpty()) return;

    auto records = DataManager::instance()->getDiet(m_date);
    DietRecord r;
    r.date = m_date;
    r.time = m_timeEdit->time();
    r.mealType = m_mealType->currentText();
    r.foodName = m_foodInput->text().trimmed();
    records.append(r);
    DataManager::instance()->saveDiet(m_date, records);
    m_foodInput->clear();
    refreshTable();
}

// 刷新表格：从 DataManager 读取当日饮食记录并填充
void DietDialog::refreshTable()
{
    auto records = DataManager::instance()->getDiet(m_date);
    m_table->setRowCount(records.size());
    for (int i = 0; i < records.size(); ++i) {
        QString timeStr = records[i].time.isValid() ? records[i].time.toString("HH:mm") : "--:--";
        m_table->setItem(i, 0, new QTableWidgetItem(timeStr));
        m_table->setItem(i, 1, new QTableWidgetItem(records[i].mealType));
        m_table->setItem(i, 2, new QTableWidgetItem(records[i].foodName));

        // 删除按钮
        QPushButton *delBtn = new QPushButton(QString::fromUtf8("✕"));
        delBtn->setStyleSheet(
            "QPushButton { color: #f08daa; background: transparent; border: none; font-size: 14px; }"
            "QPushButton:hover { color: #f55; }");
        int idx = i;
        connect(delBtn, &QPushButton::clicked, this, [this, idx]() {
            auto records = DataManager::instance()->getDiet(m_date);
            if (idx < records.size()) {
                records.removeAt(idx);
                DataManager::instance()->saveDiet(m_date, records);
            }
        });
        m_table->setCellWidget(i, 3, delBtn);
    }
}
