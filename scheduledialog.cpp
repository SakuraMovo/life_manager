// ============================================================
// 日程管理实现
// 流程：从任务库选择模板 → 设定时间/时长 → 添加到当日日程
// 功能：时间冲突检测、实时结束时间预览、完成/删除日程
// ============================================================

#include "scheduledialog.h"
#include "datamanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QGroupBox>
#include <QHeaderView>
#include <QMessageBox>

ScheduleDialog::ScheduleDialog(const QDate &date, QWidget *parent)
    : QDialog(parent), m_date(date)
{
    setWindowTitle(QString::fromUtf8("日程管理 - %1").arg(date.toString("yyyy年M月d日")));
    resize(620, 620);
    setMinimumSize(500, 460);
    setupUi();
    refreshTaskSelector();
    refreshScheduleTable();
}

void ScheduleDialog::setupUi()
{
    setStyleSheet("QDialog { background-color: rgba(18, 14, 42, 0.97); }");

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    // === 当日已有日程列表 ===
    QLabel *todayTitle = new QLabel(QString::fromUtf8("📋 当日日程"), this);
    todayTitle->setStyleSheet("color: #e2e0f0; font-size: 15px; font-weight: bold;");
    mainLayout->addWidget(todayTitle);

    m_scheduleTable = new QTableWidget(this);
    m_scheduleTable->setColumnCount(5);
    m_scheduleTable->setHorizontalHeaderLabels({QString::fromUtf8("时间"),
                                                  QString::fromUtf8("任务"),
                                                  QString::fromUtf8("时长"),
                                                  QString::fromUtf8("状态"),
                                                  QString::fromUtf8("操作")});
    m_scheduleTable->setStyleSheet(
        "QTableWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; color: #c0bcd8; }"
        "QTableWidget::item { padding: 3px; }"
        "QHeaderView::section { background: rgba(255,255,255,0.04); color: #9e9ab8; border: none; padding: 5px; font-size: 10px; }");
    m_scheduleTable->verticalHeader()->setVisible(false);
    m_scheduleTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_scheduleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_scheduleTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);  // 任务名自适应
    m_scheduleTable->setColumnWidth(0, 110);
    m_scheduleTable->setColumnWidth(2, 50);
    m_scheduleTable->setColumnWidth(3, 85);
    m_scheduleTable->setMaximumHeight(200);
    mainLayout->addWidget(m_scheduleTable);

    // === 添加新日程 ===
    QLabel *addTitle = new QLabel(QString::fromUtf8("➕ 添加日程"), this);
    addTitle->setStyleSheet("color: #e2e0f0; font-size: 15px; font-weight: bold;");
    mainLayout->addWidget(addTitle);

    // 步骤1：选择任务模板
    QGroupBox *taskGroup = new QGroupBox(QString::fromUtf8("选择任务"), this);
    taskGroup->setStyleSheet(
        "QGroupBox { color: #9e9ab8; font-size: 12px; font-weight: bold; border: 1px solid rgba(255,255,255,0.10); border-radius: 10px;"
        "  margin-top: 10px; padding-top: 18px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 2px 10px; }");
    QVBoxLayout *taskLayout = new QVBoxLayout(taskGroup);
    m_taskCombo = new QComboBox(taskGroup);
    m_taskCombo->setStyleSheet(
        "QComboBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10); border-radius: 8px;"
        "  padding: 6px 10px; color: #e2e0f0; font-size: 12px; }"
        "QComboBox:hover { background: rgba(255,255,255,0.08); }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: rgba(20,16,42,0.95); color: #e2e0f0; selection-background-color: rgba(139,158,246,0.30); }");
    m_taskCombo->setMinimumHeight(32);
    taskLayout->addWidget(m_taskCombo);
    mainLayout->addWidget(taskGroup);

    // 步骤2：设定时间
    QGroupBox *timeGroup = new QGroupBox(QString::fromUtf8("设定时间"), this);
    timeGroup->setStyleSheet(taskGroup->styleSheet());
    QVBoxLayout *timeLayout = new QVBoxLayout(timeGroup);
    timeLayout->setSpacing(8);

    // 开始时间行
    QHBoxLayout *timeRow = new QHBoxLayout();
    QLabel *startLabel = new QLabel(QString::fromUtf8("开始时间:"), timeGroup);
    startLabel->setStyleSheet("color: #c0bcd8; font-size: 12px; background: transparent;");
    timeRow->addWidget(startLabel);
    m_startTimeEdit = new QTimeEdit(QTime::currentTime(), timeGroup);
    m_startTimeEdit->setDisplayFormat("HH:mm");
    m_startTimeEdit->setStyleSheet(
        "QTimeEdit { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
        "  border-radius: 8px; padding: 5px 10px; color: #e2e0f0; font-size: 13px; }");
    timeRow->addWidget(m_startTimeEdit);
    timeRow->addStretch();
    timeLayout->addLayout(timeRow);

    // 计划时长行
    QHBoxLayout *durRow = new QHBoxLayout();
    QLabel *durLabel = new QLabel(QString::fromUtf8("计划时长:"), timeGroup);
    durLabel->setStyleSheet("color: #c0bcd8; font-size: 12px; background: transparent;");
    durRow->addWidget(durLabel);
    m_durationSpinBox = new QDoubleSpinBox(timeGroup);
    m_durationSpinBox->setRange(0.25, 24.0);
    m_durationSpinBox->setSingleStep(0.25);
    m_durationSpinBox->setValue(1.0);
    m_durationSpinBox->setSuffix(QString::fromUtf8(" 小时"));
    m_durationSpinBox->setStyleSheet(
        "QDoubleSpinBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
        "  border-radius: 8px; padding: 5px 10px; color: #e2e0f0; font-size: 13px; }");
    durRow->addWidget(m_durationSpinBox);
    durRow->addStretch();
    timeLayout->addLayout(durRow);

    // 预计结束时间（自动计算）
    QHBoxLayout *endRow = new QHBoxLayout();
    QLabel *endLabel = new QLabel(QString::fromUtf8("预计结束:"), timeGroup);
    endLabel->setStyleSheet("color: #c0bcd8; font-size: 12px; background: transparent;");
    endRow->addWidget(endLabel);
    m_endTimeLabel = new QLabel(timeGroup);
    m_endTimeLabel->setStyleSheet("color: #5eeadb; font-size: 16px; font-weight: bold; background: transparent;");
    endRow->addWidget(m_endTimeLabel);
    endRow->addStretch();
    timeLayout->addLayout(endRow);

    // 开始时间/时长变化 → 自动更新结束时间
    connect(m_startTimeEdit, &QTimeEdit::timeChanged, this, &ScheduleDialog::updateEndTime);
    connect(m_durationSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &ScheduleDialog::updateEndTime);

    // 选择任务模板 → 自动填充建议时长
    connect(m_taskCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        if (idx >= 0) {
            int planId = m_taskCombo->currentData().toInt();
            PlanItem plan = DataManager::instance()->getPlanItem(planId);
            if (plan.id > 0 && plan.plannedHours > 0) {
                m_durationSpinBox->setValue(plan.plannedHours);
            }
        }
        updateEndTime();
    });

    mainLayout->addWidget(timeGroup);
    updateEndTime();

    // === 底部按钮：关闭 / 添加 ===
    QHBoxLayout *btnRow = new QHBoxLayout();
    btnRow->addStretch();

    QPushButton *cancelBtn = new QPushButton(QString::fromUtf8("关闭"), this);
    cancelBtn->setStyleSheet(
        "QPushButton { color: #c0bcd8; background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
        "  border-radius: 8px; padding: 8px 20px; font-size: 12px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.10); color: #e2e0f0; }");
    connect(cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnRow->addWidget(cancelBtn);

    QPushButton *addBtn = new QPushButton(QString::fromUtf8("➕ 添加"), this);
    addBtn->setStyleSheet(
        "QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 8px;"
        "  padding: 8px 20px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #a0b0ff; }");
    connect(addBtn, &QPushButton::clicked, this, &ScheduleDialog::onAddToTimeline);
    btnRow->addWidget(addBtn);

    mainLayout->addLayout(btnRow);
}

// 刷新任务模板下拉选择器：从任务库加载所有任务模板
void ScheduleDialog::refreshTaskSelector()
{
    m_taskCombo->clear();
    auto plans = DataManager::instance()->getPlanItems(true);

    // 构建技能ID→名称映射
    QMap<int, QString> skillNames;
    auto skills = DataManager::instance()->getSkills();
    for (const auto &s : skills) {
        skillNames[s.id] = s.name;
    }

    for (const auto &p : plans) {
        QString label = p.title;
        if (p.skillId > 0 && skillNames.contains(p.skillId)) {
            label = QString::fromUtf8("[%1] %2").arg(skillNames[p.skillId], p.title);
        }
        m_taskCombo->addItem(label, p.id);  // 显示文本 + 隐藏的 PlanItem ID
    }

    if (m_taskCombo->count() == 0) {
        m_taskCombo->addItem(QString::fromUtf8("(暂无任务模板，请先在「任务库」页面创建)"), -1);
    }
}

// 刷新当日日程表格
void ScheduleDialog::refreshScheduleTable()
{
    if (!m_scheduleTable) return;
    auto schedules = DataManager::instance()->getSchedules(m_date);
    m_scheduleTable->setRowCount(schedules.size());
    for (int i = 0; i < schedules.size(); ++i) {
        const auto &s = schedules[i];

        // 时间段
        QString timeStr = QString::fromUtf8("%1 - %2")
            .arg(s.startTime.toString("HH:mm"), s.endTime().toString("HH:mm"));
        m_scheduleTable->setItem(i, 0, new QTableWidgetItem(timeStr));

        // 任务名（含技能前缀）
        PlanItem plan = DataManager::instance()->getPlanItem(s.planId);
        QString taskName = plan.id > 0 ? plan.title : QString::fromUtf8("(已删除)");
        if (plan.skillId > 0) {
            SkillItem sk = DataManager::instance()->getSkill(plan.skillId);
            if (sk.id > 0)
                taskName = QString::fromUtf8("[%1] %2").arg(sk.name, taskName);
        }
        m_scheduleTable->setItem(i, 1, new QTableWidgetItem(taskName));

        m_scheduleTable->setItem(i, 2, new QTableWidgetItem(
            QString::fromUtf8("%1h").arg(s.plannedHours, 0, 'f', 1)));

        // 状态判断：已完成 / 未到时间 / 待完成
        QTime now = QTime::currentTime();
        QDate today = QDate::currentDate();
        bool arrived = (m_date < today) || (m_date == today && now >= s.startTime);

        QString statusText;
        QString statusColor;
        if (s.completed) {
            statusText = QString::fromUtf8("✅ 已完成");
            statusColor = "#81c784";
        } else if (!arrived) {
            statusText = QString::fromUtf8("⏳ 未到时间");
            statusColor = "#9e9ab8";
        } else {
            statusText = QString::fromUtf8("🕐 待完成");
            statusColor = "#f08daa";
        }
        QTableWidgetItem *statusItem = new QTableWidgetItem(statusText);
        statusItem->setForeground(QColor(statusColor));
        statusItem->setTextAlignment(Qt::AlignCenter);
        m_scheduleTable->setItem(i, 3, statusItem);

        // 操作按钮：完成（需确认） / 删除
        QWidget *btnW = new QWidget();
        QHBoxLayout *bl = new QHBoxLayout(btnW);
        bl->setContentsMargins(2, 2, 2, 2);
        bl->setSpacing(4);

        int schedId = s.id;
        if (!s.completed) {
            QPushButton *completeBtn = new QPushButton(QString::fromUtf8("✓"));
            if (arrived) {
                completeBtn->setStyleSheet(
                    "QPushButton { color: #81c784; background: transparent; border: 1px solid rgba(129,199,132,0.4);"
                    "  border-radius: 4px; padding: 2px 8px; font-size: 11px; }"
                    "QPushButton:hover { background: rgba(129,199,132,0.15); }");
                completeBtn->setToolTip(QString::fromUtf8("标记完成"));
                connect(completeBtn, &QPushButton::clicked, this, [this, schedId]() {
                    QMessageBox::StandardButton reply = QMessageBox::question(
                        this, QString::fromUtf8("确认完成"),
                        QString::fromUtf8("确定要标记此任务为已完成吗？\n\n"
                                          "完成后将结算经验值和技能时长。"),
                        QMessageBox::Yes | QMessageBox::No,
                        QMessageBox::No);
                    if (reply == QMessageBox::Yes) {
                        DataManager::instance()->markScheduleComplete(schedId);
                        refreshScheduleTable();
                    }
                });
            } else {
                // 未到达开始时间，按钮禁用
                completeBtn->setStyleSheet(
                    "QPushButton { color: #6e6a88; background: transparent; border: 1px solid rgba(255,255,255,0.08);"
                    "  border-radius: 4px; padding: 2px 8px; font-size: 11px; }");
                completeBtn->setEnabled(false);
                completeBtn->setToolTip(QString::fromUtf8("尚未到达开始时间 (%1)，无法完成")
                    .arg(s.startTime.toString("HH:mm")));
            }
            bl->addWidget(completeBtn);
        }

        QPushButton *delBtn = new QPushButton(QString::fromUtf8("✕"));
        delBtn->setStyleSheet(
            "QPushButton { color: #f08daa; background: transparent; border: none; font-size: 14px; }"
            "QPushButton:hover { color: #f55; }");
        connect(delBtn, &QPushButton::clicked, this, [this, schedId]() {
            DataManager::instance()->removeSchedule(schedId);
            refreshScheduleTable();
        });
        bl->addWidget(delBtn);
        bl->addStretch();
        m_scheduleTable->setCellWidget(i, 4, btnW);
    }
}

// 根据开始时间+时长自动计算结束时间（支持跨日）
void ScheduleDialog::updateEndTime()
{
    QTime start = m_startTimeEdit->time();
    int totalSecs = start.hour() * 3600 + start.minute() * 60
                    + static_cast<int>(m_durationSpinBox->value() * 3600);
    totalSecs %= 86400;  // 处理跨日情况
    QTime end(totalSecs / 3600, (totalSecs % 3600) / 60);
    m_endTimeLabel->setText(end.toString("HH:mm"));
}

// 将日程添加到当天时间线（含时间冲突检测）
void ScheduleDialog::onAddToTimeline()
{
    int planId = m_taskCombo->currentData().toInt();
    if (planId <= 0) return;

    QTime newStart = m_startTimeEdit->time();
    double newHours = m_durationSpinBox->value();
    int newStartMin = newStart.hour() * 60 + newStart.minute();
    int newEndMin = newStartMin + static_cast<int>(newHours * 60);

    // 时间冲突检测：与已有日程的时间段有重叠则拒绝
    auto existing = DataManager::instance()->getSchedules(m_date);
    for (const auto &es : existing) {
        int esStartMin = es.startTime.hour() * 60 + es.startTime.minute();
        int esEndMin = esStartMin + static_cast<int>(es.plannedHours * 60);
        if (newStartMin < esEndMin && newEndMin > esStartMin) {
            PlanItem conflictPlan = DataManager::instance()->getPlanItem(es.planId);
            QString conflictName = conflictPlan.id > 0 ? conflictPlan.title : QString::fromUtf8("未知任务");
            QMessageBox::warning(this, QString::fromUtf8("时间冲突"),
                QString::fromUtf8("该时间段与已有日程冲突：\n\n"
                                  "「%1」\n%2 - %3\n\n"
                                  "请调整开始时间或时长后重试。")
                    .arg(conflictName, es.startTime.toString("HH:mm"),
                         es.endTime().toString("HH:mm")));
            return;
        }
    }

    // 无冲突，保存日程
    ScheduleItem item;
    item.planId = planId;
    item.date = m_date;
    item.startTime = newStart;
    item.plannedHours = newHours;
    item.completed = false;

    DataManager::instance()->saveSchedule(item);
    refreshScheduleTable();
}
