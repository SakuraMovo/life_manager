// ============================================================
// 右侧状态面板实现
// 模块顺序：时间 → 综合评分 → 当日状态 → 体力 → 心情 → 健康 → 睡眠
// 每秒刷新时钟，dataChanged 信号触发时刷新健康指标和评分
// ============================================================

#include "rightpanel.h"
#include "ui_rightpanel.h"
#include "datamanager.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDateTime>
#include <QFrame>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>

RightPanel::RightPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::RightPanel)
{
    ui->setupUi(this);
    setObjectName("rightPanel");

    // 时钟定时器：每秒刷新
    m_clockTimer = new QTimer(this);
    m_clockTimer->start(1000);

    // 按顺序初始化各模块
    setupTimeModule();
    setupScoreModule();
    setupStatusModule();
    setupEnergyModule();
    setupMoodModule();
    setupHealthModule();
    setupSleepModule();

    // 底部留白
    static_cast<QVBoxLayout*>(ui->scrollContent->layout())->addStretch();

    // 数据变更时自动刷新所有指标
    connect(DataManager::instance(), &DataManager::dataChanged,
            this, &RightPanel::refreshAll);
    refreshAll();
}

RightPanel::~RightPanel()
{
    delete ui;
}

// ============================================================
//  辅助函数：创建统一样式的分区卡片
// ============================================================

static QFrame* createSectionCard(const QString &title, QWidget *parent)
{
    QFrame *card = new QFrame(parent);
    card->setObjectName("rightSectionCard");
    card->setStyleSheet(
        "#rightSectionCard {"
        "  background: rgba(255, 255, 255, 0.04);"
        "  border: 1px solid rgba(255, 255, 255, 0.10);"
        "  border-radius: 12px;"
        "  padding: 14px;"
        "  margin: 4px 0px;"
        "}"
    );

    QVBoxLayout *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(12, 10, 12, 10);
    cardLayout->setSpacing(8);

    QLabel *titleLabel = new QLabel(title, card);
    titleLabel->setStyleSheet(
        "QLabel {"
        "  color: #9e9ab8;"
        "  font-size: 11px;"
        "  font-weight: bold;"
        "  letter-spacing: 1px;"
        "  padding-bottom: 4px;"
        "  border-bottom: 1px solid rgba(255,255,255,0.08);"
        "  background: transparent;"
        "}"
    );
    cardLayout->addWidget(titleLabel);

    return card;
}

// ============================================================
//  时间模块 — 数字时钟 + 日期 + 星期
// ============================================================

void RightPanel::setupTimeModule()
{
    QFrame *card = createSectionCard(QString::fromUtf8("⏰ 时间"), this);
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(card->layout());

    // 数字时钟 HH:mm:ss
    QLabel *clockLabel = new QLabel(card);
    clockLabel->setObjectName("digitalClock");
    clockLabel->setAlignment(Qt::AlignCenter);
    clockLabel->setStyleSheet(
        "QLabel {"
        "  color: #e2e0f0; font-size: 32px; font-weight: bold;"
        "  font-family: 'Consolas', 'Courier New', monospace;"
        "  background: transparent;"
        "}"
    );
    layout->addWidget(clockLabel);

    // 日期 yyyy年MM月dd日
    QLabel *dateLabel = new QLabel(card);
    dateLabel->setObjectName("dateLabel");
    dateLabel->setAlignment(Qt::AlignCenter);
    dateLabel->setStyleSheet("color: #9e9ab8; font-size: 13px; background: transparent;");
    layout->addWidget(dateLabel);

    // 星期几
    QLabel *weekdayLabel = new QLabel(card);
    weekdayLabel->setAlignment(Qt::AlignCenter);
    weekdayLabel->setStyleSheet("color: #6e6a88; font-size: 11px; background: transparent;");
    layout->addWidget(weekdayLabel);

    // 每秒更新
    connect(m_clockTimer, &QTimer::timeout, this, [clockLabel, dateLabel, weekdayLabel]() {
        QDateTime now = QDateTime::currentDateTime();
        clockLabel->setText(now.toString("HH:mm:ss"));
        dateLabel->setText(now.toString("yyyy年MM月dd日"));
        QStringList weekDays = {QString::fromUtf8("星期一"), QString::fromUtf8("星期二"),
                                QString::fromUtf8("星期三"), QString::fromUtf8("星期四"),
                                QString::fromUtf8("星期五"), QString::fromUtf8("星期六"),
                                QString::fromUtf8("星期日")};
        weekdayLabel->setText(weekDays[now.date().dayOfWeek() - 1]);
    });

    static_cast<QVBoxLayout*>(ui->scrollContent->layout())->addWidget(card);
}

// ============================================================
//  综合评分模块 — 基于饮水/运动/睡眠加权计算
// ============================================================

void RightPanel::setupScoreModule()
{
    QFrame *card = createSectionCard(QString::fromUtf8("⭐ 综合评分"), this);
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(card->layout());

    m_scoreLabel = new QLabel("--", card);
    m_scoreLabel->setAlignment(Qt::AlignCenter);
    m_scoreLabel->setStyleSheet("color: #e2e0f0; font-size: 36px; font-weight: bold; background: transparent;");
    layout->addWidget(m_scoreLabel);

    // 渐变色进度条：红 → 黄 → 绿
    m_scoreBar = new QProgressBar(card);
    m_scoreBar->setRange(0, 100);
    m_scoreBar->setValue(0);
    m_scoreBar->setTextVisible(false);
    m_scoreBar->setFixedHeight(10);
    m_scoreBar->setStyleSheet(
        "QProgressBar { background: rgba(255,255,255,0.05); border: none; border-radius: 5px; }"
        "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #f08daa, stop:0.5 #f5cd5c, stop:1 #81c784); border-radius: 5px; }");
    layout->addWidget(m_scoreBar);

    m_scoreGradeLabel = new QLabel(QString::fromUtf8("—"), card);
    m_scoreGradeLabel->setAlignment(Qt::AlignCenter);
    m_scoreGradeLabel->setStyleSheet("color: #9e9ab8; font-size: 12px; font-weight: bold; background: transparent;");
    layout->addWidget(m_scoreGradeLabel);

    static_cast<QVBoxLayout*>(ui->scrollContent->layout())->addWidget(card);
}

// ============================================================
//  当日状态 — 手动选择当前状态标签
// ============================================================

void RightPanel::setupStatusModule()
{
    QFrame *card = createSectionCard(QString::fromUtf8("📌 当日状态"), this);
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(card->layout());

    QComboBox *statusCombo = new QComboBox(card);
    statusCombo->addItems({
        QString::fromUtf8("🔴 专注"),
        QString::fromUtf8("🟢 放松"),
        QString::fromUtf8("🔵 忙碌"),
        QString::fromUtf8("🟡 休息")
    });
    statusCombo->setStyleSheet(
        "QComboBox {"
        "  color: #e2e0f0; background: rgba(255,255,255,0.05);"
        "  border: 1px solid rgba(255,255,255,0.10); border-radius: 8px;"
        "  padding: 6px 12px; font-size: 13px;"
        "}"
        "QComboBox:hover { background: rgba(255,255,255,0.08); }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView {"
        "  background: rgba(20,16,42,0.95); color: #e2e0f0;"
        "  selection-background-color: rgba(139,158,246,0.30);"
        "  border: 1px solid rgba(255,255,255,0.10);"
        "}"
    );
    layout->addWidget(statusCombo);

    static_cast<QVBoxLayout*>(ui->scrollContent->layout())->addWidget(card);
}

// ============================================================
//  体力模块 — 根据运动量动态计算剩余体力
// ============================================================

void RightPanel::setupEnergyModule()
{
    QFrame *card = createSectionCard(QString::fromUtf8("⚡ 体力状态"), this);
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(card->layout());

    QHBoxLayout *energyRow = new QHBoxLayout();
    QLabel *energyLabel = new QLabel(QString::fromUtf8("剩余体力"), card);
    energyLabel->setStyleSheet("color: #c0bcd8; font-size: 12px; background: transparent;");
    energyRow->addWidget(energyLabel);

    m_energyValueLabel = new QLabel("100 / 100", card);
    m_energyValueLabel->setStyleSheet("color: #5eeadb; font-size: 14px; font-weight: bold; background: transparent;");
    energyRow->addStretch();
    energyRow->addWidget(m_energyValueLabel);
    layout->addLayout(energyRow);

    m_energyBar = new QProgressBar(card);
    m_energyBar->setRange(0, 100);
    m_energyBar->setValue(100);
    m_energyBar->setTextVisible(false);
    m_energyBar->setFixedHeight(8);
    m_energyBar->setStyleSheet(
        "QProgressBar {"
        "  background: rgba(255,255,255,0.05);"
        "  border: none; border-radius: 4px;"
        "}"
        "QProgressBar::chunk {"
        "  background: qlineargradient(x1:0, y1:0, x2:1, y2:0,"
        "    stop:0 #f08daa, stop:0.3 #f5cd5c, stop:1 #5eeadb);"
        "  border-radius: 4px;"
        "}"
    );
    layout->addWidget(m_energyBar);

    static_cast<QVBoxLayout*>(ui->scrollContent->layout())->addWidget(card);
}

// ============================================================
//  心情模块 — 手动选择心情
// ============================================================

void RightPanel::setupMoodModule()
{
    QFrame *card = createSectionCard(QString::fromUtf8("💭 心情"), this);
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(card->layout());

    QComboBox *moodCombo = new QComboBox(card);
    moodCombo->addItems({
        QString::fromUtf8("😊 开心"),
        QString::fromUtf8("😌 平静"),
        QString::fromUtf8("😰 焦虑"),
        QString::fromUtf8("😩 疲惫"),
        QString::fromUtf8("🤩 充满活力")
    });
    moodCombo->setStyleSheet(
        "QComboBox {"
        "  color: #f5cd5c; background: rgba(255,255,255,0.05);"
        "  border: 1px solid rgba(255,255,255,0.10); border-radius: 8px;"
        "  padding: 6px 12px; font-size: 13px;"
        "}"
        "QComboBox:hover { background: rgba(255,255,255,0.08); }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView {"
        "  background: rgba(20,16,42,0.95); color: #e2e0f0;"
        "  selection-background-color: rgba(139,158,246,0.30);"
        "  border: 1px solid rgba(255,255,255,0.10);"
        "}"
    );
    layout->addWidget(moodCombo);

    static_cast<QVBoxLayout*>(ui->scrollContent->layout())->addWidget(card);
}

// ============================================================
//  健康数据摘要 — 饮水 / 运动 / 睡眠 三项指标
// ============================================================

void RightPanel::setupHealthModule()
{
    QFrame *card = createSectionCard(QString::fromUtf8("❤️ 健康数据"), this);
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(card->layout());

    // 辅助 lambda：创建"图标 + 值"行
    auto addMetricRow = [](QVBoxLayout *parent, const QString &icon, QLabel *&valueLabel,
                           const QString &valueColor) {
        QHBoxLayout *row = new QHBoxLayout();
        QLabel *lbl = new QLabel(icon);
        lbl->setStyleSheet("color: #c0bcd8; font-size: 12px; background: transparent;");
        row->addWidget(lbl);
        valueLabel = new QLabel("--");
        valueLabel->setStyleSheet(QString("color: %1; font-weight: bold; font-size: 12px; background: transparent;")
                                  .arg(valueColor));
        row->addStretch();
        row->addWidget(valueLabel);
        parent->addLayout(row);
    };

    addMetricRow(layout, QString::fromUtf8("💧 饮水"), m_waterHealthLabel, "#64b5f6");
    addMetricRow(layout, QString::fromUtf8("🏃 锻炼"), m_exerciseHealthLabel, "#f5cd5c");
    addMetricRow(layout, QString::fromUtf8("😴 睡眠"), m_sleepHealthLabel, "#ce93d8");

    static_cast<QVBoxLayout*>(ui->scrollContent->layout())->addWidget(card);
}

// ============================================================
//  睡眠详情模块 — 时长 / 评分 / 质量评价
// ============================================================

void RightPanel::setupSleepModule()
{
    QFrame *card = createSectionCard(QString::fromUtf8("🌙 睡眠详情"), this);
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(card->layout());

    QHBoxLayout *sleepRow = new QHBoxLayout();

    m_sleepDurationLabel = new QLabel("--h", card);
    m_sleepDurationLabel->setStyleSheet("color: #e2e0f0; font-size: 26px; font-weight: bold; background: transparent;");
    sleepRow->addWidget(m_sleepDurationLabel);
    sleepRow->addStretch();

    QVBoxLayout *sleepDetail = new QVBoxLayout();
    m_sleepScoreLabel = new QLabel(QString::fromUtf8("评分: --"), card);
    m_sleepScoreLabel->setStyleSheet("color: #f5cd5c; font-size: 12px; background: transparent;");
    sleepDetail->addWidget(m_sleepScoreLabel);

    m_sleepQualityLabel = new QLabel(QString::fromUtf8("质量: —"), card);
    m_sleepQualityLabel->setStyleSheet("color: #81c784; font-size: 12px; background: transparent;");
    sleepDetail->addWidget(m_sleepQualityLabel);

    sleepRow->addLayout(sleepDetail);
    layout->addLayout(sleepRow);

    static_cast<QVBoxLayout*>(ui->scrollContent->layout())->addWidget(card);
}

// ============================================================
//  refreshAll — 刷新所有数据指标
//  读取饮水/运动/睡眠数据 → 计算体力 → 计算综合评分
// ============================================================

void RightPanel::refreshAll()
{
    auto *dm = DataManager::instance();
    QDate today = QDate::currentDate();

    // ---- 健康数据摘要 ----
    WaterRecord water = dm->getWater(today);
    if (m_waterHealthLabel) {
        m_waterHealthLabel->setText(QString::fromUtf8("%1ml / %2ml (%3%)")
            .arg(water.currentMl).arg(water.targetMl)
            .arg(static_cast<int>(water.percent())));
    }

    int totalExerciseMins = dm->getTotalExerciseMinutes(today);
    if (m_exerciseHealthLabel) {
        if (totalExerciseMins > 0) {
            double hours = totalExerciseMins / 60.0;
            m_exerciseHealthLabel->setText(QString::fromUtf8("%1 分钟 (%2h)")
                .arg(totalExerciseMins).arg(hours, 0, 'f', 1));
        } else {
            m_exerciseHealthLabel->setText(QString::fromUtf8("暂无"));
        }
    }

    SleepRecord sleep = dm->getSleep(today);
    double sleepHours = sleep.durationHours();
    if (m_sleepHealthLabel) {
        if (sleep.sleepTime.isValid() && sleep.wakeTime.isValid())
            m_sleepHealthLabel->setText(QString::fromUtf8("%1h").arg(sleepHours, 0, 'f', 1));
        else
            m_sleepHealthLabel->setText(QString::fromUtf8("暂无"));
    }

    // ---- 睡眠详情 ----
    if (m_sleepDurationLabel) {
        if (sleep.sleepTime.isValid() && sleep.wakeTime.isValid()) {
            int h = static_cast<int>(sleepHours);
            int m = static_cast<int>((sleepHours - h) * 60);
            m_sleepDurationLabel->setText(QString::fromUtf8("%1h %2m").arg(h).arg(m));
        } else {
            m_sleepDurationLabel->setText("--h");
        }
    }

    // 睡眠评分：7-8.5h 满分，6h+ 按比例扣分，不足 6h 大幅扣分
    int sleepScore = 0;
    if (sleepHours > 0) {
        if (sleepHours >= 7.0 && sleepHours <= 8.5) sleepScore = 100;
        else if (sleepHours >= 6.0) sleepScore = static_cast<int>(100 - (7.0 - sleepHours) * 15);
        else sleepScore = qMax(0, 85 - static_cast<int>((7.0 - sleepHours) * 20));
    }
    if (m_sleepScoreLabel)
        m_sleepScoreLabel->setText(QString::fromUtf8("评分: %1").arg(sleepScore));

    if (m_sleepQualityLabel) {
        if (sleepHours >= 7.0 && sleepHours <= 8.5)
            m_sleepQualityLabel->setText(QString::fromUtf8("质量: 🌟 优秀"));
        else if (sleepHours >= 6.0)
            m_sleepQualityLabel->setText(QString::fromUtf8("质量: 👍 良好"));
        else if (sleepHours > 0)
            m_sleepQualityLabel->setText(QString::fromUtf8("质量: ⚠️ 不足"));
        else
            m_sleepQualityLabel->setText(QString::fromUtf8("质量: —"));
    }

    // ---- 体力：100 减去运动消耗（每3分钟运动扣1点）----
    int energy = 100 - qMin(totalExerciseMins / 3, 95);
    energy = qMax(5, energy);
    QString energyColor = energy >= 60 ? "#5eeadb" : (energy >= 30 ? "#f5cd5c" : "#f08daa");
    if (m_energyValueLabel) {
        m_energyValueLabel->setText(QString("%1 / 100").arg(energy));
        m_energyValueLabel->setStyleSheet(
            QString("color: %1; font-size: 14px; font-weight: bold; background: transparent;")
                .arg(energyColor));
    }
    if (m_energyBar) m_energyBar->setValue(energy);

    // ---- 综合评分：饮水 30% + 运动 35% + 睡眠 35% ----
    double waterScore = qMin(water.percent(), 100.0);
    double exerciseScore = totalExerciseMins >= 30 ? 100.0
        : qMax(0.0, totalExerciseMins * 3.33);
    double sleepScoreCalc = sleepScore;
    int composite = static_cast<int>(waterScore * 0.3 + exerciseScore * 0.35 + sleepScoreCalc * 0.35);

    // 评级
    QString grade;
    QString gradeColor;
    if (composite >= 90)      { grade = QString::fromUtf8("🌟 活力满满"); gradeColor = "#81c784"; }
    else if (composite >= 70) { grade = QString::fromUtf8("👍 状态不错"); gradeColor = "#f5cd5c"; }
    else if (composite >= 50) { grade = QString::fromUtf8("💪 需要调整"); gradeColor = "#f08daa"; }
    else                      { grade = QString::fromUtf8("😴 好好休息"); gradeColor = "#f08daa"; }

    if (m_scoreLabel) m_scoreLabel->setText(QString("%1").arg(composite));
    if (m_scoreBar) m_scoreBar->setValue(composite);
    if (m_scoreGradeLabel) {
        m_scoreGradeLabel->setText(grade);
        m_scoreGradeLabel->setStyleSheet(
            QString("color: %1; font-size: 12px; font-weight: bold; background: transparent;")
                .arg(gradeColor));
    }
}
