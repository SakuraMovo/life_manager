// ============================================================
// 任务管理实现
// ============================================================

#include "taskdialog.h"
#include "datamanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QMessageBox>

TaskDialog::TaskDialog(const QDate &date, bool editMode, QWidget *parent)
    : QDialog(parent), m_date(date)
{
    setWindowTitle(QString::fromUtf8("任务管理 - %1").arg(date.toString("yyyy年M月d日")));
    resize(520, 420);
    setMinimumSize(400, 300);
    setupUi(editMode);
    refreshTable();
}

// 根据 editMode 构建不同界面：
//   true  → 编辑模式：顶部有添加表单（时间+内容+添加按钮）
//   false → 观察模式：只有表格，可完成/撤销任务
void TaskDialog::setupUi(bool editMode)
{
    setStyleSheet("QDialog { background-color: rgba(18, 14, 42, 0.97); }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(10);

    // 统计标签：总任务数 / 已完成数 / 完成率
    m_statsLabel = new QLabel(this);
    m_statsLabel->setStyleSheet("color: #9e9ab8; font-size: 12px;");
    mainLayout->addWidget(m_statsLabel);

    // 编辑模式下显示添加表单
    if (editMode) {
        QHBoxLayout *formRow = new QHBoxLayout();
        formRow->setSpacing(8);

        m_timeEdit = new QTimeEdit(QTime::currentTime(), this);
        m_timeEdit->setDisplayFormat("HH:mm");
        m_timeEdit->setStyleSheet(
            "QTimeEdit { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
            "  border-radius: 8px; padding: 5px 8px; color: #e2e0f0; font-size: 12px; }");
        formRow->addWidget(m_timeEdit);

        m_taskInput = new QLineEdit(this);
        m_taskInput->setPlaceholderText(QString::fromUtf8("任务内容..."));
        m_taskInput->setStyleSheet(
            "QLineEdit { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
            "  border-radius: 8px; padding: 6px 10px; color: #e2e0f0; font-size: 12px; }");
        formRow->addWidget(m_taskInput, 1);

        QPushButton *addBtn = new QPushButton(QString::fromUtf8("＋"), this);
        addBtn->setFixedWidth(36);
        addBtn->setStyleSheet(
            "QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 8px;"
            "  font-size: 16px; font-weight: bold; }"
            "QPushButton:hover { background: #a0b0ff; }");
        connect(addBtn, &QPushButton::clicked, this, &TaskDialog::addTask);
        formRow->addWidget(addBtn);

        mainLayout->addLayout(formRow);
    }

    // 任务表格：时间 | 内容 | 状态 | 操作
    m_table = new QTableWidget(this);
    int cols = editMode ? 4 : 4;
    m_table->setColumnCount(cols);
    QStringList headers;
    headers << QString::fromUtf8("时间") << QString::fromUtf8("任务内容")
            << QString::fromUtf8("状态") << QString::fromUtf8("操作");
    m_table->setHorizontalHeaderLabels(headers);
    m_table->setStyleSheet(
        "QTableWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; color: #c0bcd8; }"
        "QTableWidget::item { padding: 4px; }"
        "QHeaderView::section { background: rgba(255,255,255,0.04); color: #9e9ab8; border: none; padding: 6px; font-size: 11px; }");
    m_table->verticalHeader()->setVisible(false);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);  // 任务内容列自适应
    mainLayout->addWidget(m_table);

    // 关闭按钮
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
}

// 添加新的旧版任务
void TaskDialog::addTask()
{
    if (m_taskInput->text().trimmed().isEmpty()) return;

    DataManager::TaskItem t;
    t.date = m_date;
    t.time = m_timeEdit->time().toString("HH:mm");
    t.content = m_taskInput->text().trimmed();
    DataManager::instance()->saveTask(t);
    m_taskInput->clear();
    refreshTable();
}

// 刷新任务表格，显示每行任务的状态和操作按钮
void TaskDialog::refreshTable()
{
    auto tasks = DataManager::instance()->getTasks(m_date);
    m_table->setRowCount(tasks.size());

    int completed = 0;
    for (int i = 0; i < tasks.size(); ++i) {
        const auto &t = tasks[i];
        if (t.completed) ++completed;

        // 时间和内容
        m_table->setItem(i, 0, new QTableWidgetItem(t.time));
        m_table->setItem(i, 1, new QTableWidgetItem(t.content));

        // 状态列：✓ 已完成 / ⏳ 未完成
        QTableWidgetItem *statusItem = new QTableWidgetItem(
            t.completed ? QString::fromUtf8("✓ 已完成") : QString::fromUtf8("⏳ 未完成"));
        statusItem->setForeground(t.completed ? QColor("#81c784") : QColor("#f08daa"));
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_table->setItem(i, 2, statusItem);

        // 操作按钮：完成/撤销 + 删除
        QWidget *btnW = new QWidget();
        QHBoxLayout *bl = new QHBoxLayout(btnW);
        bl->setContentsMargins(2, 2, 2, 2);
        bl->setSpacing(4);

        int taskId = t.id;
        if (!t.completed) {
            QPushButton *completeBtn = new QPushButton(QString::fromUtf8("✓ 完成"));
            completeBtn->setStyleSheet(
                "QPushButton { color: #81c784; background: transparent; border: 1px solid rgba(129,199,132,0.4);"
                "  border-radius: 4px; padding: 2px 8px; font-size: 11px; }"
                "QPushButton:hover { background: rgba(129,199,132,0.15); }");
            connect(completeBtn, &QPushButton::clicked, this, [this, taskId]() {
                auto tasks = DataManager::instance()->getTasks(m_date);
                for (auto &t : tasks) {
                    if (t.id == taskId) { t.completed = true; DataManager::instance()->saveTask(t); break; }
                }
                refreshTable();
            });
            bl->addWidget(completeBtn);
        } else {
            QPushButton *undoBtn = new QPushButton(QString::fromUtf8("↩ 撤销"));
            undoBtn->setStyleSheet(
                "QPushButton { color: #f08daa; background: transparent; border: 1px solid rgba(240,141,170,0.4);"
                "  border-radius: 4px; padding: 2px 8px; font-size: 11px; }"
                "QPushButton:hover { background: rgba(240,141,170,0.15); }");
            connect(undoBtn, &QPushButton::clicked, this, [this, taskId]() {
                auto tasks = DataManager::instance()->getTasks(m_date);
                for (auto &t : tasks) {
                    if (t.id == taskId) { t.completed = false; DataManager::instance()->saveTask(t); break; }
                }
                refreshTable();
            });
            bl->addWidget(undoBtn);
        }

        QPushButton *delBtn = new QPushButton(QString::fromUtf8("✕"));
        delBtn->setStyleSheet(
            "QPushButton { color: #f08daa; background: transparent; border: none; font-size: 14px; }"
            "QPushButton:hover { color: #f55; }");
        connect(delBtn, &QPushButton::clicked, this, [this, taskId]() {
            DataManager::instance()->removeTask(taskId);
            refreshTable();
        });
        bl->addWidget(delBtn);
        bl->addStretch();
        m_table->setCellWidget(i, 3, btnW);
    }

    // 更新统计标签
    m_statsLabel->setText(QString::fromUtf8("共 %1 项任务  |  已完成 %2 项  |  完成率 %3%")
        .arg(tasks.size()).arg(completed)
        .arg(tasks.isEmpty() ? 0 : completed * 100 / tasks.size()));
}
