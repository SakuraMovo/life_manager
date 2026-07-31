#include "centerpanel.h"
#include "ui_centerpanel.h"
#include "datamanager.h"
#include "taskdialog.h"
#include "scheduledialog.h"
#include <QDesktopServices>
#include <QUrl>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFileDialog>
#include <QFile>
#include <QInputDialog>
#include <QMessageBox>
#include <QGraphicsOpacityEffect>
#include <QDateTime>
#include <QPainter>
#include <QLinearGradient>
#include <QScrollArea>
#include <QProgressBar>
#include <QComboBox>
#include <QTimeEdit>
#include <QHeaderView>
#include <QGroupBox>
#include <QFrame>
#include <QDebug>

// ============================================================
// CenterPanel 实现 — 中央内容面板
// 管理 QStackedWidget 7 个子页面：
//   0:首页  1:时间线  2:任务库  3:已完成  4:技能表  5:每日复盘  6:成就殿堂
// ============================================================

// ============================================================
//  构造函数 / 析构函数
// ============================================================

CenterPanel::CenterPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::CenterPanel)
{
    ui->setupUi(this);
    setObjectName("centerPanel");
    m_calendarDate = QDate::currentDate();

    setupHomePage();
    setupBusinessPages();
    ui->stackedWidget->setCurrentIndex(0);

    connect(DataManager::instance(), &DataManager::dataChanged,
            this, &CenterPanel::refreshCurrentPage);

    // 倒计时定时器 — 每秒更新
    m_countdownTimer = new QTimer(this);
    m_countdownTimer->start(1000);
    connect(m_countdownTimer, &QTimer::timeout, this, [this]() {
        refreshTimelineTasks();
        refreshHomeTasks();
    });
}

CenterPanel::~CenterPanel()
{
    delete ui;
}

// 左侧导航切换页面：nav index → stackedWidget index (home=0, business=nav+1)
void CenterPanel::switchToPage(int index)
{
    int pageIndex = (index < 0) ? 0 : index + 1;
    if (pageIndex >= 0 && pageIndex < ui->stackedWidget->count()) {
        ui->stackedWidget->setCurrentIndex(pageIndex);
        refreshCurrentPage();
    }
}

// 根据当前页面索引刷新对应内容
void CenterPanel::refreshCurrentPage()
{
    int idx = ui->stackedWidget->currentIndex();
    switch (idx) {
    case 0:
        refreshHomeTasks();
        break;
    case 1:
        if (m_timelineCalendar) refreshCalendar(m_timelineCalendar, m_calendarDate);
        refreshTimelineView();
        break;
    case 2: refreshStudyPlanPage(); break;
    case 3: refreshCompletedPage(); break;
    case 4: refreshSkillsPage(); break;
    case 5: refreshReviewPage(); break;
    case 6: refreshAchievementPage(); break;
    }
}

// ============================================================
//  首页
// ============================================================

void CenterPanel::setupHomePage()
{
    QWidget *homePage = ui->homePage;

    QGridLayout *gridLayout = new QGridLayout(homePage);
    gridLayout->setContentsMargins(0, 0, 0, 0);
    gridLayout->setSpacing(0);

    m_backgroundLabel = new QLabel(homePage);
    m_backgroundLabel->setAlignment(Qt::AlignCenter);
    m_backgroundLabel->setScaledContents(false);
    m_backgroundLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    gridLayout->addWidget(m_backgroundLabel, 0, 0);

    // 安装事件过滤器，捕获首页尺寸变化 → 重新渲染背景
    homePage->installEventFilter(this);

    QWidget *overlayContainer = new QWidget(homePage);
    overlayContainer->setObjectName("overlayContainer");
    overlayContainer->setStyleSheet(
        "#overlayContainer { background: rgba(15, 12, 41, 0.75); border-radius: 14px;"
        "  border: 1px solid rgba(255, 255, 255, 0.10); }");
    overlayContainer->setMinimumWidth(700);
    overlayContainer->setMinimumHeight(500);

    QVBoxLayout *overlayLayout = new QVBoxLayout(overlayContainer);
    overlayLayout->setContentsMargins(30, 40, 30, 40);
    overlayLayout->setSpacing(20);

    m_quoteLabel = new QLabel(QString::fromUtf8("「 业精于勤，荒于嬉 」"), overlayContainer);
    m_quoteLabel->setAlignment(Qt::AlignCenter);
    m_quoteLabel->setWordWrap(true);
    m_quoteLabel->setStyleSheet(
        "QLabel { color: #ffffff; font-size: 22px; font-weight: bold; padding: 20px; background: transparent; }");
    overlayLayout->addWidget(m_quoteLabel);

    m_antiProcrastinateLabel = new QLabel(QString::fromUtf8("⚠️ 拒绝拖延 · 即刻行动"), overlayContainer);
    m_antiProcrastinateLabel->setAlignment(Qt::AlignCenter);
    m_antiProcrastinateLabel->setStyleSheet(
        "QLabel { color: #f08daa; font-size: 18px; font-weight: bold; padding: 15px;"
        "  background: rgba(240, 141, 170, 0.10); border: 1.5px solid rgba(240, 141, 170, 0.25); border-radius: 10px; }");
    overlayLayout->addWidget(m_antiProcrastinateLabel);

    // 今日任务 — 紧凑的日程显示
    {
        m_homeScheduleLabel = new QLabel(overlayContainer);
        m_homeScheduleLabel->setWordWrap(true);
        m_homeScheduleLabel->setMinimumHeight(40);
        m_homeScheduleLabel->setMaximumHeight(160);
        m_homeScheduleLabel->setStyleSheet(
            "QLabel { color: #c0bcd8; font-size: 13px;"
            "  background: rgba(255,255,255,0.04);"
            "  border: 1px solid rgba(255,255,255,0.10); border-radius: 10px;"
            "  padding: 12px 16px; }");
        overlayLayout->addWidget(m_homeScheduleLabel);
    }

    overlayLayout->addStretch();

    gridLayout->addWidget(overlayContainer, 0, 0);
    gridLayout->setAlignment(overlayContainer, Qt::AlignCenter);

    // 底部工具栏
    QWidget *toolbar = new QWidget(homePage);
    toolbar->setObjectName("centerToolbar");
    toolbar->setStyleSheet("#centerToolbar { background: rgba(15, 12, 41, 0.85); border-top: 1px solid rgba(255, 255, 255, 0.08); }");
    QHBoxLayout *tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(16, 6, 16, 6);
    tbLayout->setSpacing(12);

    QLabel *opLabel = new QLabel(QString::fromUtf8("背景透明度:"), toolbar);
    opLabel->setStyleSheet("color: #9e9ab8; font-size: 12px;");
    tbLayout->addWidget(opLabel);

    m_opacitySlider = new QSlider(Qt::Horizontal, toolbar);
    m_opacitySlider->setRange(10, 100);
    m_opacitySlider->setValue(70);
    m_opacitySlider->setMaximumWidth(150);
    m_opacitySlider->setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: rgba(255,255,255,0.08); border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; background: #8b9ef6; border-radius: 7px; border: 1px solid rgba(255,255,255,0.2); }"
        "QSlider::sub-page:horizontal { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgba(139,158,246,0.5),stop:1 rgba(94,234,219,0.3)); border-radius: 2px; }");
    tbLayout->addWidget(m_opacitySlider);

    QLabel *opVal = new QLabel("70%", toolbar);
    opVal->setStyleSheet("color: #c0bcd8; font-size: 12px; min-width: 35px;");
    tbLayout->addWidget(opVal);

    connect(m_opacitySlider, &QSlider::valueChanged, this, [this, opVal](int v) {
        opVal->setText(QString("%1%").arg(v));
        m_backgroundOpacity = v / 100.0;
        applyBackgroundSettings();
    });

    tbLayout->addSpacing(16);

    // 适应模式切换
    QPushButton *fitModeBtn = new QPushButton(QString::fromUtf8("📐 适应"), toolbar);
    fitModeBtn->setCheckable(true);
    fitModeBtn->setChecked(m_backgroundFitMode);
    fitModeBtn->setStyleSheet(
        "QPushButton { color: #c0bcd8; background: rgba(255,255,255,0.06);"
        "  border: 1px solid rgba(255,255,255,0.10); border-radius: 6px; padding: 5px 12px; font-size: 12px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.12); color: #e2e0f0; }"
        "QPushButton:checked { background: rgba(139,158,246,0.20); color: #8b9ef6; border-color: rgba(139,158,246,0.35); }");
    connect(fitModeBtn, &QPushButton::clicked, this, [this, fitModeBtn]() {
        m_backgroundFitMode = fitModeBtn->isChecked();
        fitModeBtn->setText(m_backgroundFitMode ? QString::fromUtf8("📐 适应") : QString::fromUtf8("📐 拉伸"));
        renderBackground();
    });
    tbLayout->addWidget(fitModeBtn);

    // 水平对齐按钮
    auto makeAlignBtn = [](const QString &text, const QString &tooltip) -> QPushButton* {
        QPushButton *b = new QPushButton(text);
        b->setCheckable(true);
        b->setFixedSize(28, 24);
        b->setToolTip(tooltip);
        b->setStyleSheet(
            "QPushButton { color: #9e9ab8; background: rgba(255,255,255,0.04);"
            "  border: 1px solid rgba(255,255,255,0.08); border-radius: 4px; font-size: 14px; padding: 0px; }"
            "QPushButton:hover { background: rgba(255,255,255,0.10); color: #e2e0f0; }"
            "QPushButton:checked { background: rgba(139,158,246,0.18); color: #8b9ef6; border-color: rgba(139,158,246,0.30); }");
        return b;
    };

    QPushButton *hLeft = makeAlignBtn(QString::fromUtf8("◀"), QString::fromUtf8("水平: 靠左"));
    QPushButton *hCenter = makeAlignBtn(QString::fromUtf8("●"), QString::fromUtf8("水平: 居中"));
    QPushButton *hRight = makeAlignBtn(QString::fromUtf8("▶"), QString::fromUtf8("水平: 靠右"));
    hCenter->setChecked(true);
    tbLayout->addWidget(hLeft);
    tbLayout->addWidget(hCenter);
    tbLayout->addWidget(hRight);

    connect(hLeft, &QPushButton::clicked, this, [this, hLeft, hCenter, hRight]() {
        m_bgHAlign = 0; hLeft->setChecked(true); hCenter->setChecked(false); hRight->setChecked(false);
        renderBackground();
    });
    connect(hCenter, &QPushButton::clicked, this, [this, hLeft, hCenter, hRight]() {
        m_bgHAlign = 1; hLeft->setChecked(false); hCenter->setChecked(true); hRight->setChecked(false);
        renderBackground();
    });
    connect(hRight, &QPushButton::clicked, this, [this, hLeft, hCenter, hRight]() {
        m_bgHAlign = 2; hLeft->setChecked(false); hCenter->setChecked(false); hRight->setChecked(true);
        renderBackground();
    });

    QPushButton *vTop = makeAlignBtn(QString::fromUtf8("▲"), QString::fromUtf8("垂直: 靠上"));
    QPushButton *vCenter = makeAlignBtn(QString::fromUtf8("●"), QString::fromUtf8("垂直: 居中"));
    QPushButton *vBottom = makeAlignBtn(QString::fromUtf8("▼"), QString::fromUtf8("垂直: 靠下"));
    vCenter->setChecked(true);
    tbLayout->addWidget(vTop);
    tbLayout->addWidget(vCenter);
    tbLayout->addWidget(vBottom);

    connect(vTop, &QPushButton::clicked, this, [this, vTop, vCenter, vBottom]() {
        m_bgVAlign = 0; vTop->setChecked(true); vCenter->setChecked(false); vBottom->setChecked(false);
        renderBackground();
    });
    connect(vCenter, &QPushButton::clicked, this, [this, vTop, vCenter, vBottom]() {
        m_bgVAlign = 1; vTop->setChecked(false); vCenter->setChecked(true); vBottom->setChecked(false);
        renderBackground();
    });
    connect(vBottom, &QPushButton::clicked, this, [this, vTop, vCenter, vBottom]() {
        m_bgVAlign = 2; vTop->setChecked(false); vCenter->setChecked(false); vBottom->setChecked(true);
        renderBackground();
    });

    tbLayout->addSpacing(16);

    auto makeToolBtn = [](const QString &text) -> QPushButton* {
        QPushButton *b = new QPushButton(text);
        b->setStyleSheet(
            "QPushButton { color: #c0bcd8; background: rgba(255,255,255,0.06);"
            "  border: 1px solid rgba(255,255,255,0.10); border-radius: 6px; padding: 5px 14px; font-size: 12px; }"
            "QPushButton:hover { background: rgba(255,255,255,0.12); color: #e2e0f0; border-color: rgba(255,255,255,0.16); }");
        return b;
    };

    QPushButton *editQuoteBtn = makeToolBtn(QString::fromUtf8("✎ 编辑语录"));
    connect(editQuoteBtn, &QPushButton::clicked, this, &CenterPanel::onEditQuote);
    tbLayout->addWidget(editQuoteBtn);

    QPushButton *changeBgBtn = makeToolBtn(QString::fromUtf8("🖼 更换背景"));
    connect(changeBgBtn, &QPushButton::clicked, this, &CenterPanel::onChangeBackground);
    tbLayout->addWidget(changeBgBtn);

    QPushButton *resetBtn = new QPushButton(QString::fromUtf8("🔄 初始化数据"));
    resetBtn->setStyleSheet(
        "QPushButton { color: #f08daa; background: rgba(240,141,170,0.08);"
        "  border: 1px solid rgba(240,141,170,0.25); border-radius: 6px; padding: 5px 14px; font-size: 12px; }"
        "QPushButton:hover { background: rgba(240,141,170,0.18); color: #f55; border-color: rgba(240,141,170,0.45); }");
    connect(resetBtn, &QPushButton::clicked, this, [this]() {
        QMessageBox::StandardButton reply = QMessageBox::warning(
            this->window(),
            QString::fromUtf8("⚠️ 初始化数据"),
            QString::fromUtf8("确定要重置所有数据吗？\n\n"
                              "此操作将清除：\n"
                              "  • 所有任务模板和计划\n"
                              "  • 所有日程安排\n"
                              "  • 所有习惯记录\n"
                              "  • 所有技能数据\n"
                              "  • 所有饮食/运动/睡眠记录\n"
                              "  • 经验值和等级\n\n"
                              "此操作不可恢复！"),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            DataManager::instance()->resetAllData();
        }
    });
    tbLayout->addWidget(resetBtn);

    tbLayout->addStretch();
    gridLayout->addWidget(toolbar, 1, 0, Qt::AlignBottom);

    loadDefaultBackground();
}

void CenterPanel::loadDefaultBackground()
{
    QPixmap bg(":/resources/backgrounds/background.png");
    if (!bg.isNull()) {
        m_backgroundPixmap = bg;
    } else {
        QPixmap gradientBg(1200, 800);
        QPainter painter(&gradientBg);
        QLinearGradient grad(0, 0, 0, gradientBg.height());
        grad.setColorAt(0.0, QColor("#0F0C29"));
        grad.setColorAt(0.5, QColor("#302B63"));
        grad.setColorAt(1.0, QColor("#24243E"));
        painter.fillRect(gradientBg.rect(), grad);
        painter.end();
        m_backgroundPixmap = gradientBg;
    }
    renderBackground();
    applyBackgroundSettings();
}

void CenterPanel::applyBackgroundSettings()
{
    if (!m_backgroundLabel) return;
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(m_backgroundLabel);
    effect->setOpacity(m_backgroundOpacity);
    m_backgroundLabel->setGraphicsEffect(effect);
}

void CenterPanel::renderBackground()
{
    if (!m_backgroundLabel || m_backgroundPixmap.isNull()) return;

    QSize labelSize = m_backgroundLabel->size();
    if (labelSize.width() <= 0 || labelSize.height() <= 0) return;

    QPixmap result;
    if (m_backgroundFitMode) {
        // 保持宽高比缩放
        QPixmap scaled = m_backgroundPixmap.scaled(labelSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        // 将缩放后的图片绘制到与标签尺寸相同的透明画布上
        result = QPixmap(labelSize);
        result.fill(Qt::transparent);
        QPainter painter(&result);

        // 计算 X 偏移量
        int x = 0;
        if (m_bgHAlign == 1)       x = (labelSize.width() - scaled.width()) / 2;
        else if (m_bgHAlign == 2)  x = labelSize.width() - scaled.width();

        // 计算 Y 偏移量
        int y = 0;
        if (m_bgVAlign == 1)       y = (labelSize.height() - scaled.height()) / 2;
        else if (m_bgVAlign == 2)  y = labelSize.height() - scaled.height();

        painter.drawPixmap(x, y, scaled);
        painter.end();
    } else {
        // 拉伸填充
        result = m_backgroundPixmap.scaled(labelSize, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    m_backgroundLabel->setPixmap(result);
}

bool CenterPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Resize && m_backgroundLabel) {
        // 首页尺寸变化时重新渲染背景
        QWidget *homePage = ui->homePage;
        if (homePage && obj == homePage) {
            renderBackground();
        }
    }
    return QWidget::eventFilter(obj, event);
}

void CenterPanel::onEditQuote()
{
    bool ok;
    QString newQuote = QInputDialog::getText(this, QString::fromUtf8("编辑个人语录"),
                                             QString::fromUtf8("请输入你的专属语录："),
                                             QLineEdit::Normal, m_quoteLabel->text().remove(QString::fromUtf8("「 ")).remove(QString::fromUtf8(" 」")), &ok);
    if (ok && !newQuote.isEmpty()) {
        m_quoteLabel->setText(QString::fromUtf8("「 %1 」").arg(newQuote));
    }
}

void CenterPanel::onChangeBackground()
{
    QString fn = QFileDialog::getOpenFileName(this, QString::fromUtf8("选择背景图片"),
                                              QString(), "Images (*.png *.jpg *.jpeg *.bmp)");
    if (!fn.isEmpty()) {
        QPixmap pm(fn);
        if (!pm.isNull()) {
            m_backgroundPixmap = pm;
            renderBackground();
        } else {
            QMessageBox::warning(this, QString::fromUtf8("加载失败"),
                                 QString::fromUtf8("无法加载所选图片，请检查文件格式。"));
        }
    }
}

// ============================================================
//  日历组件
// ============================================================

QWidget* CenterPanel::createCalendarWidget(QWidget *parent)
{
    QWidget *w = new QWidget(parent);
    w->setStyleSheet("background: transparent;");
    QVBoxLayout *outer = new QVBoxLayout(w);
    outer->setContentsMargins(0, 0, 0, 0);

    // 月份导航
    QHBoxLayout *navRow = new QHBoxLayout();
    QPushButton *prevBtn = new QPushButton("◀", w);
    QPushButton *nextBtn = new QPushButton("▶", w);
    QLabel *monthLabel = new QLabel(w);
    monthLabel->setAlignment(Qt::AlignCenter);
    monthLabel->setStyleSheet("color: #e2e0f0; font-size: 18px; font-weight: bold;");

    // 日历导航按钮 - 放大版
    auto styleNav = [](QPushButton *b) {
        b->setFixedSize(32, 32);
        b->setStyleSheet(
            "QPushButton { color: #9e9ab8; background: transparent; border: 1px solid rgba(255,255,255,0.10); border-radius: 16px; font-size: 12px; padding: 0px; }"
            "QPushButton:hover { color: #e2e0f0; background: rgba(255,255,255,0.08); border-color: rgba(255,255,255,0.16); }");
    };
    styleNav(prevBtn);
    styleNav(nextBtn);

    navRow->addWidget(prevBtn);
    navRow->addWidget(monthLabel, 1);
    navRow->addWidget(nextBtn);
    outer->addLayout(navRow);

    // 星期标题行
    QHBoxLayout *dowRow = new QHBoxLayout();
    QStringList dow = {QString::fromUtf8("一"), QString::fromUtf8("二"), QString::fromUtf8("三"),
                       QString::fromUtf8("四"), QString::fromUtf8("五"), QString::fromUtf8("六"),
                       QString::fromUtf8("日")};
    for (const auto &d : dow) {
        QLabel *lbl = new QLabel(d, w);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setStyleSheet("color: #6e6a88; font-size: 12px; padding: 6px 4px;");
        dowRow->addWidget(lbl);
    }
    outer->addLayout(dowRow);

    // 日期网格
    QGridLayout *grid = new QGridLayout();
    grid->setSpacing(2);
    outer->addLayout(grid);

    // 将网格指针存储在 widget 属性中，供后续访问
    w->setProperty("calGrid", QVariant::fromValue<QGridLayout*>(grid));
    w->setProperty("calMonthLabel", QVariant::fromValue<QLabel*>(monthLabel));
    w->setProperty("calPrevBtn", QVariant::fromValue<QPushButton*>(prevBtn));
    w->setProperty("calNextBtn", QVariant::fromValue<QPushButton*>(nextBtn));
    w->setProperty("calIsEdit", false);  // 由调用者设置

    connect(prevBtn, &QPushButton::clicked, this, [this, w]() {
        m_calendarDate = m_calendarDate.addMonths(-1);
        refreshCalendar(w, m_calendarDate);
    });
    connect(nextBtn, &QPushButton::clicked, this, [this, w]() {
        m_calendarDate = m_calendarDate.addMonths(1);
        refreshCalendar(w, m_calendarDate);
    });

    refreshCalendar(w, m_calendarDate);
    return w;
}

void CenterPanel::refreshCalendar(QWidget *calendar, const QDate &targetDate)
{
    if (!calendar) return;

    QGridLayout *grid = calendar->property("calGrid").value<QGridLayout*>();
    QLabel *monthLabel = calendar->property("calMonthLabel").value<QLabel*>();
    QPushButton *prevBtn = calendar->property("calPrevBtn").value<QPushButton*>();
    QPushButton *nextBtn = calendar->property("calNextBtn").value<QPushButton*>();

    if (!grid || !monthLabel) return;

    monthLabel->setText(targetDate.toString("yyyy年 M月"));

    // 清除现有的日期按钮
    QLayoutItem *item;
    while ((item = grid->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    QDate firstDay(targetDate.year(), targetDate.month(), 1);
    int startDow = firstDay.dayOfWeek() - 1; // 周一=0
    int daysInMonth = firstDay.daysInMonth();
    QDate today = QDate::currentDate();

    for (int day = 1; day <= daysInMonth; ++day) {
        QDate d(targetDate.year(), targetDate.month(), day);
        QPushButton *dayBtn = new QPushButton(QString::number(day));
        dayBtn->setFixedSize(44, 38);
        dayBtn->setCursor(Qt::PointingHandCursor);

        // 检查该日期是否有任务
        auto tasks = DataManager::instance()->getTasks(d);
        bool hasTasks = !tasks.isEmpty();
        bool isToday = (d == today);

        QString style = "QPushButton { font-size: 13px; border-radius: 8px; border: 1px solid transparent; padding: 0px; ";
        if (isToday) {
            style += "color: #fff; background: #8b9ef6; font-weight: bold; ";
        } else if (hasTasks) {
            style += "color: #c0bcd8; background: rgba(139,158,246,0.18); ";
        } else {
            style += "color: #9e9ab8; background: transparent; ";
        }
        style += "} QPushButton:hover { border-color: #8b9ef6; background: rgba(139,158,246,0.28); color: #fff; }";
        dayBtn->setStyleSheet(style);

        int row = (startDow + day - 1) / 7;
        int col = (startDow + day - 1) % 7;
        grid->addWidget(dayBtn, row, col);

        // 将日期存储在按钮属性中
        dayBtn->setProperty("calDate", d.toString("yyyyMMdd"));

        connect(dayBtn, &QPushButton::clicked, this, [this, d, calendar]() {
            m_calendarDate = d;
            refreshCurrentPage();

            // 双击打开日程管理弹窗（400ms 内）
            qint64 now = QDateTime::currentMSecsSinceEpoch();
            QString lastKey = calendar->property("lastClickDate").toString();
            qint64 lastTime = calendar->property("lastClickTime").toLongLong();
            if (lastKey == d.toString("yyyyMMdd") && (now - lastTime) < 400) {
                onCalendarDateDoubleClicked(d);
            }
            calendar->setProperty("lastClickDate", d.toString("yyyyMMdd"));
            calendar->setProperty("lastClickTime", now);
        });
    }

}

void CenterPanel::refreshDietTable()
{
    if (!m_viewDietTable) return;
    auto records = DataManager::instance()->getDiet(m_calendarDate);
    m_viewDietTable->setRowCount(records.size());
    for (int i = 0; i < records.size(); ++i) {
        m_viewDietTable->setItem(i, 0, new QTableWidgetItem(records[i].mealType));
        m_viewDietTable->setItem(i, 1, new QTableWidgetItem(records[i].foodName));
        QPushButton *delBtn = new QPushButton("✕");
        delBtn->setStyleSheet("QPushButton { color: #f08daa; background: transparent; border: none; font-size: 12px; } QPushButton:hover { color: #f55; }");
        int idx = i;
        connect(delBtn, &QPushButton::clicked, this, [this, idx]() {
            auto recs = DataManager::instance()->getDiet(m_calendarDate);
            if (idx < recs.size()) { recs.removeAt(idx); DataManager::instance()->saveDiet(m_calendarDate, recs); }
        });
        m_viewDietTable->setCellWidget(i, 2, delBtn);
    }
}

void CenterPanel::refreshExerciseTable()
{
    if (!m_viewExerciseTable) return;
    auto records = DataManager::instance()->getExercises(m_calendarDate);
    m_viewExerciseTable->setRowCount(records.size());
    for (int i = 0; i < records.size(); ++i) {
        m_viewExerciseTable->setItem(i, 0, new QTableWidgetItem(records[i].exerciseType));
        m_viewExerciseTable->setItem(i, 1, new QTableWidgetItem(
                                               QString::fromUtf8("%1 分钟").arg(records[i].durationMinutes)));
        QPushButton *delBtn = new QPushButton("✕");
        delBtn->setStyleSheet("QPushButton { color: #f08daa; background: transparent; border: none; font-size: 12px; } QPushButton:hover { color: #f55; }");
        int idx = i;
        connect(delBtn, &QPushButton::clicked, this, [this, idx]() {
            DataManager::instance()->removeExercise(m_calendarDate, idx);
        });
        m_viewExerciseTable->setCellWidget(i, 2, delBtn);
    }
}

// 刷新统一时间线视图中的所有数据显示标签
void CenterPanel::refreshTimelineTasks()
{
    auto dm = DataManager::instance();
    QDate date = m_calendarDate;
    QTime now = QTime::currentTime();
    QDate today = QDate::currentDate();

    // --- 刷新日程表格 ---
    if (m_timelineScheduleTable) {
        auto schedules = dm->getSchedules(date);
        m_timelineScheduleTable->setRowCount(schedules.size());
        for (int i = 0; i < schedules.size(); ++i) {
            const auto &s = schedules[i];
            QString timeStr = QString::fromUtf8("%1-%2")
                                  .arg(s.startTime.toString("HH:mm"), s.endTime().toString("HH:mm"));
            m_timelineScheduleTable->setItem(i, 0, new QTableWidgetItem(timeStr));

            PlanItem plan = dm->getPlanItem(s.planId);
            QString taskName = plan.id > 0 ? plan.title : QString::fromUtf8("(已删除)");
            if (plan.skillId > 0) {
                SkillItem sk = dm->getSkill(plan.skillId);
                if (sk.id > 0)
                    taskName = QString::fromUtf8("[%1] %2").arg(sk.name, taskName);
            }
            QTableWidgetItem *nameItem = new QTableWidgetItem(taskName);
            if (s.completed) {
                nameItem->setForeground(QColor("#6e6a88"));
                QFont f = nameItem->font();
                f.setStrikeOut(true);
                nameItem->setFont(f);
            }
            m_timelineScheduleTable->setItem(i, 1, nameItem);

            m_timelineScheduleTable->setItem(i, 2, new QTableWidgetItem(
                                                       QString::fromUtf8("%1h").arg(s.plannedHours, 0, 'f', 1)));

            // 操作按钮
            QWidget *btnW = new QWidget();
            QHBoxLayout *bl = new QHBoxLayout(btnW);
            bl->setContentsMargins(2, 2, 2, 2);
            bl->setSpacing(2);

            int schedId = s.id;
            if (!s.completed) {
                bool arrived = (date < today) || (date == today && now >= s.startTime);
                QPushButton *completeBtn = new QPushButton(QString::fromUtf8("✓"));
                if (arrived) {
                    completeBtn->setStyleSheet(
                        "QPushButton { color: #81c784; background: transparent;"
                        "  border: 1px solid rgba(129,199,132,0.4); border-radius: 3px;"
                        "  padding: 1px 6px; font-size: 10px; }"
                        "QPushButton:hover { background: rgba(129,199,132,0.15); }");
                    completeBtn->setToolTip(QString::fromUtf8("标记完成"));
                    connect(completeBtn, &QPushButton::clicked, this, [this, schedId]() {
                        DataManager::instance()->markScheduleComplete(schedId);
                    });
                } else {
                    completeBtn->setStyleSheet(
                        "QPushButton { color: #6e6a88; background: transparent;"
                        "  border: 1px solid rgba(255,255,255,0.08); border-radius: 3px;"
                        "  padding: 1px 6px; font-size: 10px; }");
                    completeBtn->setEnabled(false);
                }
                bl->addWidget(completeBtn);
            }

            QPushButton *delBtn = new QPushButton(QString::fromUtf8("✕"));
            delBtn->setStyleSheet(
                "QPushButton { color: #f08daa; background: transparent; border: none;"
                "  font-size: 11px; } QPushButton:hover { color: #f55; }");
            connect(delBtn, &QPushButton::clicked, this, [this, schedId]() {
                DataManager::instance()->removeSchedule(schedId);
            });
            bl->addWidget(delBtn);
            bl->addStretch();
            m_timelineScheduleTable->setCellWidget(i, 3, btnW);
        }
    }

    // --- 刷新倒计时钟 ---
    if (m_countdownClockLabel && m_countdownHintLabel) {
        auto schedules = dm->getSchedules(date);
        QTime now = QTime::currentTime();
        QDate today = QDate::currentDate();

        // 查找当前或下一个日程
        const ScheduleItem *currentTask = nullptr;
        const ScheduleItem *nextTask = nullptr;
        int minRemaining = 86400; // 24 小时（秒）

        for (const auto &s : schedules) {
            if (s.completed) continue;

            int startSecs = s.startTime.hour() * 3600 + s.startTime.minute() * 60;
            int endSecs = startSecs + static_cast<int>(s.plannedHours * 3600);
            int nowSecs = now.hour() * 3600 + now.minute() * 60 + now.second();

            // 仅当天日期相关
            if (date == today) {
                // 当前正在进行中？
                if (nowSecs >= startSecs && nowSecs < endSecs) {
                    currentTask = &s;
                    break;
                }
                // 即将到来？
                if (nowSecs < startSecs) {
                    int remaining = startSecs - nowSecs;
                    if (remaining < minRemaining) {
                        minRemaining = remaining;
                        nextTask = &s;
                    }
                }
            }
        }

        if (currentTask) {
            // 任务进行中 — 显示剩余时间
            int endSecs = currentTask->startTime.hour() * 3600 + currentTask->startTime.minute() * 60
                          + static_cast<int>(currentTask->plannedHours * 3600);
            int nowSecs = now.hour() * 3600 + now.minute() * 60 + now.second();
            int remaining = qMax(0, endSecs - nowSecs);

            int rh = remaining / 3600;
            int rm = (remaining % 3600) / 60;
            int rs = remaining % 60;

            m_countdownClockLabel->setText(QString("%1:%2:%3")
                                               .arg(rh, 2, 10, QChar('0'))
                                               .arg(rm, 2, 10, QChar('0'))
                                               .arg(rs, 2, 10, QChar('0')));
            m_countdownClockLabel->setStyleSheet(
                "QLabel { color: #f5cd5c; font-size: 28px; font-weight: bold;"
                "  font-family: 'Consolas', 'Courier New', monospace; background: transparent; }");

            PlanItem plan = dm->getPlanItem(currentTask->planId);
            m_countdownHintLabel->setText(QString::fromUtf8("⏳ 进行中: %1\n剩余时间")
                                              .arg(plan.id > 0 ? plan.title : QString::fromUtf8("未知任务")));
            m_countdownHintLabel->setStyleSheet(
                "QLabel { color: #f5cd5c; font-size: 12px; background: transparent; }");
        } else if (nextTask && date == today) {
            // 即将开始的任务 — 显示倒计时
            int startSecs = nextTask->startTime.hour() * 3600 + nextTask->startTime.minute() * 60;
            int nowSecs = now.hour() * 3600 + now.minute() * 60 + now.second();
            int remaining = startSecs - nowSecs;

            int rh = remaining / 3600;
            int rm = (remaining % 3600) / 60;
            int rs = remaining % 60;

            m_countdownClockLabel->setText(QString("%1:%2:%3")
                                               .arg(rh, 2, 10, QChar('0'))
                                               .arg(rm, 2, 10, QChar('0'))
                                               .arg(rs, 2, 10, QChar('0')));
            m_countdownClockLabel->setStyleSheet(
                "QLabel { color: #5eeadb; font-size: 28px; font-weight: bold;"
                "  font-family: 'Consolas', 'Courier New', monospace; background: transparent; }");

            PlanItem plan = dm->getPlanItem(nextTask->planId);
            m_countdownHintLabel->setText(QString::fromUtf8("⏰ 距离 %1 开始还有")
                                              .arg(plan.id > 0 ? plan.title : QString::fromUtf8("未知任务")));
            m_countdownHintLabel->setStyleSheet(
                "QLabel { color: #9e9ab8; font-size: 12px; background: transparent; }");
        } else if (date == today) {
            // 今日无更多任务
            bool hasAnySchedule = false;
            bool allDone = !schedules.isEmpty();
            for (const auto &s : schedules) {
                if (!s.completed) { allDone = false; break; }
                hasAnySchedule = true;
            }
            // 检查是否所有日程都已完成
            bool anyIncomplete = false;
            for (const auto &s : schedules) {
                if (!s.completed) { anyIncomplete = true; break; }
            }

            m_countdownClockLabel->setText(now.toString("HH:mm:ss"));
            m_countdownClockLabel->setStyleSheet(
                "QLabel { color: #e2e0f0; font-size: 28px; font-weight: bold;"
                "  font-family: 'Consolas', 'Courier New', monospace; background: transparent; }");

            if (!schedules.isEmpty() && !anyIncomplete) {
                m_countdownHintLabel->setText(QString::fromUtf8("🎉 今日任务已全部完成!"));
                m_countdownHintLabel->setStyleSheet(
                    "QLabel { color: #81c784; font-size: 12px; background: transparent; }");
            } else if (schedules.isEmpty()) {
                m_countdownHintLabel->setText(QString::fromUtf8("当日没有日程安排"));
                m_countdownHintLabel->setStyleSheet(
                    "QLabel { color: #9e9ab8; font-size: 12px; background: transparent; }");
            } else {
                // 未来日期有日程安排
                m_countdownHintLabel->setText(QString::fromUtf8("%1 个日程待执行").arg(schedules.size()));
                m_countdownHintLabel->setStyleSheet(
                    "QLabel { color: #9e9ab8; font-size: 12px; background: transparent; }");
            }
        } else {
            // 不是今天 — 显示当前时间
            m_countdownClockLabel->setText(date < today ? QString::fromUtf8("已过期") : QString::fromUtf8("--:--:--"));
            m_countdownClockLabel->setStyleSheet(
                "QLabel { color: #6e6a88; font-size: 28px; font-weight: bold;"
                "  font-family: 'Consolas', 'Courier New', monospace; background: transparent; }");

            if (!schedules.isEmpty()) {
                m_countdownHintLabel->setText(QString::fromUtf8("%1 个日程").arg(schedules.size()));
            } else {
                m_countdownHintLabel->setText(QString::fromUtf8("当日没有日程安排"));
            }
            m_countdownHintLabel->setStyleSheet(
                "QLabel { color: #9e9ab8; font-size: 12px; background: transparent; }");
        }
    }

    // --- 刷新时间线习惯表格 ---
    if (m_timelineHabitTable) {
        auto habits = dm->getActiveHabits(date);
        m_timelineHabitTable->setRowCount(habits.size());
        for (int i = 0; i < habits.size(); ++i) {
            const auto &h = habits[i];
            m_timelineHabitTable->setItem(i, 0, new QTableWidgetItem(h.name));
            m_timelineHabitTable->setItem(i, 1, new QTableWidgetItem(h.targetLabel()));

            double prog = dm->getHabitProgress(h.id, date);
            bool completed = dm->isHabitCompletedForDate(h.id, date);

            // 进度组件（含快捷按钮）
            QWidget *progW = new QWidget();
            QHBoxLayout *progLayout = new QHBoxLayout(progW);
            progLayout->setContentsMargins(2, 1, 2, 1);
            progLayout->setSpacing(3);

            // 迷你进度条
            QProgressBar *bar = new QProgressBar();
            bar->setFixedHeight(12);
            bar->setTextVisible(false);
            bar->setRange(0, 100);
            double pct = 0;
            if (h.completionMode == "hours")
                pct = h.targetHours > 0 ? qMin(prog / h.targetHours * 100.0, 100.0) : 0;
            else if (h.completionMode == "duration")
                pct = h.targetMinutes > 0 ? qMin(prog / h.targetMinutes * 100.0, 100.0) : 0;
            else
                pct = h.targetCount > 0 ? qMin(prog / h.targetCount * 100.0, 100.0) : 0;
            bar->setValue(static_cast<int>(pct));
            QString barColor = completed ? "#81c784" : (pct >= 50 ? "#f5cd5c" : "#5eeadb");
            bar->setStyleSheet(QString(
                                   "QProgressBar { background: rgba(255,255,255,0.06); border: none; border-radius: 6px; }"
                                   "QProgressBar::chunk { background: %1; border-radius: 6px; }").arg(barColor));
            progLayout->addWidget(bar, 1);

            if (!completed) {
                int hid = h.id;
                if (h.completionMode == "count") {
                    QPushButton *incBtn = new QPushButton("+1");
                    incBtn->setFixedSize(24, 16);
                    incBtn->setStyleSheet(
                        "QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 3px;"
                        "  font-size: 9px; font-weight: bold; padding: 0px; }"
                        "QPushButton:hover { background: #a0b0ff; }");
                    connect(incBtn, &QPushButton::clicked, this, [this, hid, date]() {
                        double p = DataManager::instance()->getHabitProgress(hid, date);
                        DataManager::instance()->setHabitProgress(hid, date, p + 1);
                    });
                    progLayout->addWidget(incBtn);
                } else {
                    double incAmt = h.completionMode == "hours" ? 0.5 : 15.0;
                    QString lbl = h.completionMode == "hours" ? "+0.5h" : "+15m";
                    QPushButton *incBtn = new QPushButton(lbl);
                    incBtn->setStyleSheet(
                        "QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 3px;"
                        "  font-size: 8px; font-weight: bold; padding: 0px 3px; }"
                        "QPushButton:hover { background: #a0b0ff; }");
                    incBtn->setFixedHeight(16);
                    connect(incBtn, &QPushButton::clicked, this, [this, hid, date, incAmt]() {
                        double p = DataManager::instance()->getHabitProgress(hid, date);
                        DataManager::instance()->setHabitProgress(hid, date, p + incAmt);
                    });
                    progLayout->addWidget(incBtn);
                }
            } else {
                QLabel *doneLbl = new QLabel(QString::fromUtf8("✓"));
                doneLbl->setStyleSheet("color: #81c784; font-size: 12px; font-weight: bold; background: transparent;");
                doneLbl->setAlignment(Qt::AlignCenter);
                doneLbl->setFixedWidth(24);
                progLayout->addWidget(doneLbl);

                // 重置按钮
                int hid = h.id;
                QPushButton *resetBtn = new QPushButton(QString::fromUtf8("↺"));
                resetBtn->setFixedSize(18, 16);
                resetBtn->setToolTip(QString::fromUtf8("重置"));
                resetBtn->setStyleSheet(
                    "QPushButton { color: #f08daa; background: transparent; border: 1px solid rgba(240,141,170,0.4);"
                    "  border-radius: 3px; font-size: 9px; padding: 0px; }"
                    "QPushButton:hover { background: rgba(240,141,170,0.15); }");
                connect(resetBtn, &QPushButton::clicked, this, [this, hid, date]() {
                    DataManager::instance()->setHabitProgress(hid, date, 0);
                });
                progLayout->addWidget(resetBtn);
            }

            m_timelineHabitTable->setCellWidget(i, 2, progW);

            // 已完成行的颜色
            if (completed) {
                for (int c = 0; c < 2; ++c) {
                    if (auto *item = m_timelineHabitTable->item(i, c))
                        item->setForeground(QColor("#81c784"));
                }
            }
        }
    }
}

void CenterPanel::refreshHomeTasks()
{
    if (!m_homeScheduleLabel) return;

    auto dm = DataManager::instance();
    QDate today = QDate::currentDate();
    auto schedules = dm->getSchedules(today);
    auto habits = dm->getActiveHabits(today);
    QTime now = QTime::currentTime();

    if (schedules.isEmpty() && habits.isEmpty()) {
        m_homeScheduleLabel->setText(
            QString::fromUtf8("📋 今日暂无日程安排\n"
                              "   双击时间线日期添加任务"));
        m_homeScheduleLabel->setStyleSheet(
            "QLabel { color: #6e6a88; font-size: 13px;"
            "  background: rgba(255,255,255,0.04);"
            "  border: 1px solid rgba(255,255,255,0.10); border-radius: 10px;"
            "  padding: 12px 16px; }");
        return;
    }

    QStringList lines;

    // --- 日程区 ---
    int scheduleCompleted = 0;
    const ScheduleItem *currentTask = nullptr;

    if (!schedules.isEmpty()) {
        lines.append(QString::fromUtf8("📋 今日任务"));

        for (const auto &s : schedules) {
            PlanItem plan = dm->getPlanItem(s.planId);
            QString name = plan.id > 0 ? plan.title : QString::fromUtf8("(已删除)");

            int startSecs = s.startTime.hour() * 3600 + s.startTime.minute() * 60;
            int endSecs = startSecs + static_cast<int>(s.plannedHours * 3600);
            int nowSecs = now.hour() * 3600 + now.minute() * 60 + now.second();

            QString statusIcon;
            if (s.completed) {
                statusIcon = QString::fromUtf8("✅");
                ++scheduleCompleted;
            } else if (nowSecs >= startSecs && nowSecs < endSecs) {
                statusIcon = QString::fromUtf8("⏳");
                currentTask = &s;
            } else if (nowSecs >= endSecs) {
                statusIcon = QString::fromUtf8("⚠️");
            } else {
                statusIcon = QString::fromUtf8("🕐");
            }

            lines.append(QString::fromUtf8("  %1 %2  %3")
                             .arg(statusIcon)
                             .arg(s.startTime.toString("HH:mm"))
                             .arg(name));
        }

        // 当前任务提示
        if (currentTask) {
            PlanItem plan = dm->getPlanItem(currentTask->planId);
            int endSecs = currentTask->startTime.hour() * 3600 + currentTask->startTime.minute() * 60
                          + static_cast<int>(currentTask->plannedHours * 3600);
            int nowSecs = now.hour() * 3600 + now.minute() * 60 + now.second();
            int remaining = qMax(0, endSecs - nowSecs);
            int rm = remaining / 60;
            lines.append(QString::fromUtf8("\n  🔥 进行中: %1 (剩余 %2分钟)")
                             .arg(plan.id > 0 ? plan.title : QString::fromUtf8("未知"), QString::number(rm)));
        } else if (scheduleCompleted == schedules.size()) {
            lines.append(QString::fromUtf8("  🎉 全部完成!"));
        } else {
            for (const auto &s : schedules) {
                if (s.completed) continue;
                int startSecs = s.startTime.hour() * 3600 + s.startTime.minute() * 60;
                int nowSecs = now.hour() * 3600 + now.minute() * 60 + now.second();
                if (startSecs > nowSecs) {
                    PlanItem plan = dm->getPlanItem(s.planId);
                    int remaining = startSecs - nowSecs;
                    lines.append(QString::fromUtf8("  ⏰ 下一任务: %1 %2 (还有%3分钟)")
                                     .arg(s.startTime.toString("HH:mm"))
                                     .arg(plan.id > 0 ? plan.title : QString::fromUtf8("未知"))
                                     .arg(remaining / 60));
                    break;
                }
            }
        }
    }

    // --- 习惯区 ---
    int habitsCompleted = 0;
    if (!habits.isEmpty()) {
        if (!schedules.isEmpty())
            lines.append(QString::fromUtf8("\n🌱 今日习惯"));
        else
            lines.append(QString::fromUtf8("🌱 今日习惯"));

        for (const auto &h : habits) {
            double prog = dm->getHabitProgress(h.id, today);
            bool done = dm->isHabitCompletedForDate(h.id, today);
            if (done) ++habitsCompleted;

            QString progStr;
            if (h.completionMode == "hours")
                progStr = QString::fromUtf8("%1/%2h").arg(prog, 0, 'f', 1).arg(h.targetHours, 0, 'f', 1);
            else if (h.completionMode == "duration")
                progStr = QString::fromUtf8("%1/%2min").arg(static_cast<int>(prog)).arg(h.targetMinutes);
            else
                progStr = QString::fromUtf8("%1/%2次").arg(static_cast<int>(prog)).arg(h.targetCount);

            QString icon = done ? QString::fromUtf8("✅") : QString::fromUtf8("  ");
            lines.append(QString::fromUtf8("  %1 %2  %3")
                             .arg(icon, h.name, progStr));
        }
    }

    m_homeScheduleLabel->setText(lines.join("\n"));

    // 基于状态的动态样式
    QString borderColor = "rgba(255,255,255,0.10)";
    if (currentTask) {
        borderColor = "rgba(245,205,92,0.40)";
    } else if (!schedules.isEmpty() && scheduleCompleted == schedules.size()
               && (habits.isEmpty() || habitsCompleted == habits.size())) {
        borderColor = "rgba(129,199,132,0.40)";
    }

    m_homeScheduleLabel->setStyleSheet(
        QString("QLabel { color: #c0bcd8; font-size: 13px;"
                "  background: rgba(255,255,255,0.04);"
                "  border: 1px solid %1; border-radius: 10px;"
                "  padding: 12px 16px; }").arg(borderColor));
}

void CenterPanel::refreshTimelineView()
{
    auto dm = DataManager::instance();
    QDate date = m_calendarDate;

    // --- 睡眠显示 ---
    if (m_sleepDisplayLabel) {
        SleepRecord s = dm->getSleep(date);
        if (s.sleepTime.isValid() && s.wakeTime.isValid()) {
            m_sleepDisplayLabel->setText(QString::fromUtf8("入睡 %1  ·  醒来 %2  ·  %3小时")
                                             .arg(s.sleepTime.toString("HH:mm"), s.wakeTime.toString("HH:mm"))
                                             .arg(s.durationHours(), 0, 'f', 1));
        } else {
            m_sleepDisplayLabel->setText(QString::fromUtf8("暂无数据"));
        }
    }

    // --- 饮水显示 ---
    if (m_waterLabel && m_waterSlider) {
        WaterRecord w = dm->getWater(date);
        m_waterSlider->blockSignals(true);
        m_waterSlider->setValue(w.currentMl);
        m_waterSlider->blockSignals(false);
        m_waterLabel->setText(QString::fromUtf8("%1 / 2000 ml  (%2%)")
                                  .arg(w.currentMl).arg(static_cast<int>(w.percent())));
    }

    // --- 运动显示 ---
    if (m_exerciseLabel || m_exerciseDisplayBox) {
        auto exercises = dm->getExercises(date);
        int totalMins = dm->getTotalExerciseMinutes(date);
        if (totalMins > 0) {
            double hours = totalMins / 60.0;
            int kcal = static_cast<int>(totalMins * 6.5);  // 粗略估算
            QStringList itemLines;
            for (const auto &ex : exercises) {
                itemLines.append(QString::fromUtf8("  · %1：%2 分钟")
                                     .arg(ex.exerciseType).arg(ex.durationMinutes));
            }
            QString displayText = QString::fromUtf8(
                                      "🏃 今日运动\n"
                                      "⏱  总时长：%1 分钟（%2 小时）\n"
                                      "%3\n"
                                      "🔥 预估消耗：%4 千卡")
                                      .arg(totalMins)
                                      .arg(hours, 0, 'f', 1)
                                      .arg(itemLines.join("\n"))
                                      .arg(kcal);
            if (m_exerciseDisplayBox)
                m_exerciseDisplayBox->setText(displayText);
            if (m_exerciseLabel)
                m_exerciseLabel->setText(QString::fromUtf8("✅ %1项运动  总计%2分钟")
                                             .arg(exercises.size()).arg(totalMins));
        } else {
            QString emptyText = QString::fromUtf8(
                "🏃 今日运动\n"
                "⏱  总时长：—\n"
                "🔥 预估消耗：—");
            if (m_exerciseDisplayBox)
                m_exerciseDisplayBox->setText(emptyText);
            if (m_exerciseLabel)
                m_exerciseLabel->setText(QString::fromUtf8("暂无数据"));
        }
    }

    // --- 饮食表格 ---
    refreshDietTable();

    // --- 运动表格 ---
    refreshExerciseTable();

    // --- 日程表格 & 倒计时 ---
    refreshTimelineTasks();
}

// ============================================================
//  数据卡片辅助函数
// ============================================================

void CenterPanel::refreshDataCards(QWidget *cardsWidget, const QDate &date)
{
    if (!cardsWidget) return;
    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(cardsWidget->layout());
    if (!layout) return;

    // 清除现有卡片
    QLayoutItem *item;
    while ((item = layout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    auto dm = DataManager::instance();

    auto makeCard = [](const QString &title, QWidget *parent) -> QFrame* {
        QFrame *f = new QFrame(parent);
        f->setStyleSheet("QFrame { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
                         "  border-radius: 10px; padding: 10px; }");
        QVBoxLayout *fl = new QVBoxLayout(f);
        fl->setContentsMargins(10, 8, 10, 8);
        fl->setSpacing(6);
        QLabel *tl = new QLabel(title, f);
        tl->setStyleSheet("color: #9e9ab8; font-size: 10px; font-weight: bold; background: transparent;");
        fl->addWidget(tl);
        return f;
    };

    // 睡眠卡片
    {
        SleepRecord s = dm->getSleep(date);
        QFrame *card = makeCard(QString::fromUtf8("💤 睡眠数据"), cardsWidget);
        QVBoxLayout *cl = qobject_cast<QVBoxLayout*>(card->layout());
        QString txt = s.sleepTime.isValid() && s.wakeTime.isValid()
                          ? QString::fromUtf8("入睡 %1  ·  醒来 %2  ·  %3小时")
                                .arg(s.sleepTime.toString("HH:mm"), s.wakeTime.toString("HH:mm"))
                                .arg(s.durationHours(), 0, 'f', 1)
                          : QString::fromUtf8("暂无数据");
        QLabel *lbl = new QLabel(txt, card);
        lbl->setStyleSheet("color: #c0bcd8; font-size: 12px; background: transparent;");
        cl->addWidget(lbl);
        layout->addWidget(card);
    }

    // 饮水卡片
    {
        WaterRecord w = dm->getWater(date);
        QFrame *card = makeCard(QString::fromUtf8("💧 饮水记录"), cardsWidget);
        QVBoxLayout *cl = qobject_cast<QVBoxLayout*>(card->layout());
        QLabel *lbl = new QLabel(QString::fromUtf8("%1 / %2 ml").arg(w.currentMl).arg(w.targetMl), card);
        lbl->setStyleSheet("color: #5eeadb; font-size: 12px; background: transparent;");
        cl->addWidget(lbl);
        QProgressBar *bar = new QProgressBar(card);
        bar->setRange(0, 100);
        bar->setValue(static_cast<int>(w.percent()));
        bar->setTextVisible(false);
        bar->setFixedHeight(6);
        bar->setStyleSheet("QProgressBar { background: rgba(255,255,255,0.06); border: none; border-radius: 3px; }"
                           "QProgressBar::chunk { background: #5eeadb; border-radius: 3px; }");
        cl->addWidget(bar);
        layout->addWidget(card);
    }

    // 饮食卡片
    {
        QFrame *card = makeCard(QString::fromUtf8("🍽 饮食记录"), cardsWidget);
        QVBoxLayout *cl = qobject_cast<QVBoxLayout*>(card->layout());
        auto diets = dm->getDiet(date);
        if (diets.isEmpty()) {
            QLabel *lbl = new QLabel(QString::fromUtf8("暂无数据"), card);
            lbl->setStyleSheet("color: #6e6a88; font-size: 12px; background: transparent;");
            cl->addWidget(lbl);
        } else {
            for (const auto &d : diets) {
                QLabel *lbl = new QLabel(QString::fromUtf8("%1: %2").arg(d.mealType, d.foodName), card);
                lbl->setStyleSheet("color: #c0c8d0; font-size: 12px; background: transparent;");
                cl->addWidget(lbl);
            }
        }
        layout->addWidget(card);
    }

    // 计划进度卡片（基于今日任务完成情况）
    {
        QFrame *card = makeCard(QString::fromUtf8("📊 计划进度"), cardsWidget);
        QVBoxLayout *cl = qobject_cast<QVBoxLayout*>(card->layout());
        double prog = dm->taskProgress(date);
        int total = dm->tasksTotal(date);
        int done = dm->tasksCompleted(date);
        QLabel *lbl = new QLabel(QString::fromUtf8("%1 / %2 任务  (%3%)").arg(done).arg(total).arg(prog, 0, 'f', 0), card);
        lbl->setStyleSheet("color: #f5cd5c; font-size: 12px; background: transparent;");
        cl->addWidget(lbl);
        QProgressBar *bar = new QProgressBar(card);
        bar->setRange(0, 100);
        bar->setValue(static_cast<int>(prog));
        bar->setTextVisible(false);
        bar->setFixedHeight(6);
        bar->setStyleSheet("QProgressBar { background: rgba(255,255,255,0.06); border: none; border-radius: 3px; }"
                           "QProgressBar::chunk { background: #f5cd5c; border-radius: 3px; }");
        cl->addWidget(bar);
        layout->addWidget(card);
    }

    // 运动卡片
    {
        auto exercises = dm->getExercises(date);
        int totalMins = dm->getTotalExerciseMinutes(date);
        QFrame *card = makeCard(QString::fromUtf8("🏃 运动记录"), cardsWidget);
        QVBoxLayout *cl = qobject_cast<QVBoxLayout*>(card->layout());
        if (exercises.isEmpty()) {
            QLabel *lbl = new QLabel(QString::fromUtf8("暂无数据"), card);
            lbl->setStyleSheet("color: #6e6a88; font-size: 12px; background: transparent;");
            cl->addWidget(lbl);
        } else {
            for (const auto &ex : exercises) {
                QLabel *lbl = new QLabel(QString::fromUtf8("%1 %2分钟").arg(ex.exerciseType).arg(ex.durationMinutes), card);
                lbl->setStyleSheet("color: #f5cd5c; font-size: 12px; background: transparent;");
                cl->addWidget(lbl);
            }
            QLabel *totalLbl = new QLabel(QString::fromUtf8("总计: %1分钟").arg(totalMins), card);
            totalLbl->setStyleSheet("color: #f08daa; font-size: 11px; font-weight: bold; background: transparent; padding-top: 4px;");
            cl->addWidget(totalLbl);
        }
        layout->addWidget(card);
    }
}

// ============================================================
//  业务页面构建器
// ============================================================

// ---- 时间线视图（统一视图：左侧日历 + 右侧数据/编辑区）----
QWidget* CenterPanel::buildTimelineViewPage()
{
    QWidget *page = new QWidget();
    page->setStyleSheet("background: transparent;");
    QHBoxLayout *mainLayout = new QHBoxLayout(page);
    mainLayout->setContentsMargins(16, 16, 16, 16);
    mainLayout->setSpacing(14);

    // === 左侧：放大版日历 ===
    QWidget *leftSide = new QWidget(page);
    leftSide->setStyleSheet("background: transparent;");
    QVBoxLayout *leftLayout = new QVBoxLayout(leftSide);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    QLabel *pageTitle = new QLabel(QString::fromUtf8("📅 时间线 — 双击日期安排任务"), leftSide);
    pageTitle->setStyleSheet("color: #e2e0f0; font-size: 16px; font-weight: bold;");
    leftLayout->addWidget(pageTitle);

    m_timelineCalendar = createCalendarWidget(leftSide);
    leftLayout->addWidget(m_timelineCalendar);

    // === 倒计时钟 ===
    {
        QFrame *clockCard = new QFrame(leftSide);
        clockCard->setStyleSheet(
            "QFrame { background: rgba(20,16,40,0.70); border: 1px solid rgba(255,255,255,0.12);"
            "  border-radius: 12px; padding: 10px; }");
        QVBoxLayout *clockLayout = new QVBoxLayout(clockCard);
        clockLayout->setContentsMargins(12, 8, 12, 8);
        clockLayout->setSpacing(4);

        m_countdownClockLabel = new QLabel("--:--:--", clockCard);
        m_countdownClockLabel->setAlignment(Qt::AlignCenter);
        m_countdownClockLabel->setStyleSheet(
            "QLabel { color: #5eeadb; font-size: 28px; font-weight: bold;"
            "  font-family: 'Consolas', 'Courier New', monospace; background: transparent; }");
        clockLayout->addWidget(m_countdownClockLabel);

        m_countdownHintLabel = new QLabel(QString::fromUtf8("当日没有日程安排"), clockCard);
        m_countdownHintLabel->setAlignment(Qt::AlignCenter);
        m_countdownHintLabel->setWordWrap(true);
        m_countdownHintLabel->setStyleSheet(
            "QLabel { color: #9e9ab8; font-size: 12px; background: transparent; }");
        clockLayout->addWidget(m_countdownHintLabel);

        leftLayout->addWidget(clockCard);
    }

    // === 日程表格 ===
    {
        QLabel *schedTitle = new QLabel(QString::fromUtf8("📋 当日日程"), leftSide);
        schedTitle->setStyleSheet("color: #e2e0f0; font-size: 13px; font-weight: bold; margin-top: 4px;");
        leftLayout->addWidget(schedTitle);

        m_timelineScheduleTable = new QTableWidget(leftSide);
        m_timelineScheduleTable->setColumnCount(4);
        m_timelineScheduleTable->setHorizontalHeaderLabels({
                                                            QString::fromUtf8("时间"), QString::fromUtf8("任务"),
                                                            QString::fromUtf8("时长"), QString::fromUtf8("")});
        m_timelineScheduleTable->setStyleSheet(
            "QTableWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08);"
            "  border-radius: 6px; color: #c0bcd8; }"
            "QTableWidget::item { padding: 2px; }"
            "QHeaderView::section { background: rgba(255,255,255,0.04); color: #9e9ab8;"
            "  border: none; padding: 3px; font-size: 10px; }");
        m_timelineScheduleTable->verticalHeader()->setVisible(false);
        m_timelineScheduleTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_timelineScheduleTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_timelineScheduleTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_timelineScheduleTable->setColumnWidth(0, 90);
        m_timelineScheduleTable->setColumnWidth(2, 45);
        m_timelineScheduleTable->setColumnWidth(3, 50);
        m_timelineScheduleTable->setMaximumHeight(220);
        leftLayout->addWidget(m_timelineScheduleTable);
    }

    // === 选中日期的习惯 ===
    {
        QLabel *habitTitle = new QLabel(QString::fromUtf8("🌱 今日习惯"), leftSide);
        habitTitle->setStyleSheet("color: #e2e0f0; font-size: 13px; font-weight: bold; margin-top: 4px;");
        leftLayout->addWidget(habitTitle);

        m_timelineHabitTable = new QTableWidget(leftSide);
        m_timelineHabitTable->setColumnCount(3);
        m_timelineHabitTable->setHorizontalHeaderLabels({
                                                         QString::fromUtf8("习惯"), QString::fromUtf8("目标"), QString::fromUtf8("进度")});
        m_timelineHabitTable->setStyleSheet(
            "QTableWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08);"
            "  border-radius: 6px; color: #c0bcd8; }"
            "QTableWidget::item { padding: 2px; }"
            "QHeaderView::section { background: rgba(255,255,255,0.04); color: #9e9ab8;"
            "  border: none; padding: 3px; font-size: 10px; }");
        m_timelineHabitTable->verticalHeader()->setVisible(false);
        m_timelineHabitTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_timelineHabitTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_timelineHabitTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_timelineHabitTable->setColumnWidth(1, 65);
        m_timelineHabitTable->setColumnWidth(2, 130);
        m_timelineHabitTable->setMaximumHeight(200);
        leftLayout->addWidget(m_timelineHabitTable);
    }

    leftLayout->addStretch();
    mainLayout->addWidget(leftSide, 1);

    // === 右侧：统一数据展示 + 编辑区（可滚动）===
    QScrollArea *rightScroll = new QScrollArea(page);
    rightScroll->setWidgetResizable(true);
    rightScroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    QWidget *rightSide = new QWidget(rightScroll);
    rightSide->setStyleSheet("background: transparent;");
    rightSide->setMinimumWidth(300);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightSide);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(10);

    // 共享分组样式 — 毛玻璃卡片
    auto makeGroup = [](const QString &title) -> QGroupBox* {
        QGroupBox *g = new QGroupBox(title);
        g->setAttribute(Qt::WA_StyledBackground, true);
        g->setAutoFillBackground(true);
        g->setStyleSheet(
            "QGroupBox {"
            "  color: #9e9ab8; font-size: 11px; font-weight: bold;"
            "  background-color: rgba(20,16,40,0.70);"
            "  border: 1px solid rgba(255,255,255,0.10);"
            "  border-radius: 10px;"
            "  margin-top: 6px;"
            "  padding: 12px 14px 10px 14px;"
            "}"
            "QGroupBox::title {"
            "  subcontrol-origin: margin;"
            "  left: 12px;"
            "  padding: 1px 8px;"
            "  background-color: transparent;"
            "}");
        return g;
    };

    // --- 睡眠（统一：展示 + 编辑）---
    {
        QGroupBox *g = makeGroup(QString::fromUtf8("💤 睡眠"));
        QVBoxLayout *gl = new QVBoxLayout(g);
        gl->setSpacing(6);

        // 数据显示
        m_sleepDisplayLabel = new QLabel(QString::fromUtf8("暂无数据"), g);
        m_sleepDisplayLabel->setStyleSheet("color: #c0bcd8; font-size: 13px; font-weight: bold; background: transparent; padding: 2px 0;");
        gl->addWidget(m_sleepDisplayLabel);

        // 编辑行
        QHBoxLayout *editRow = new QHBoxLayout();
        editRow->setSpacing(6);

        QLabel *sl = new QLabel(QString::fromUtf8("入睡"), g);
        sl->setStyleSheet("color: #9e9ab8; font-size: 11px; background: transparent;");
        editRow->addWidget(sl);
        m_sleepH = new QComboBox(g);
        m_sleepM = new QComboBox(g);
        for (int i = 0; i < 24; ++i) m_sleepH->addItem(QString::number(i).rightJustified(2, '0'));
        for (int i = 0; i < 60; i += 5) m_sleepM->addItem(QString::number(i).rightJustified(2, '0'));
        QString comboStyle = "QComboBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
                             "  border-radius: 6px; padding: 3px 6px; color: #e2e0f0; font-size: 11px; }"
                             "QComboBox:hover { background: rgba(255,255,255,0.08); } QComboBox::drop-down { border: none; }"
                             "QComboBox QAbstractItemView { background: rgba(20,16,42,0.95); color: #e2e0f0; }";
        m_sleepH->setStyleSheet(comboStyle); m_sleepM->setStyleSheet(comboStyle);
        editRow->addWidget(m_sleepH); editRow->addWidget(new QLabel(":", g)); editRow->addWidget(m_sleepM);
        editRow->addSpacing(12);

        QLabel *wl = new QLabel(QString::fromUtf8("醒来"), g);
        wl->setStyleSheet("color: #9e9ab8; font-size: 11px; background: transparent;");
        editRow->addWidget(wl);
        m_wakeH = new QComboBox(g);
        m_wakeM = new QComboBox(g);
        for (int i = 0; i < 24; ++i) m_wakeH->addItem(QString::number(i).rightJustified(2, '0'));
        for (int i = 0; i < 60; i += 5) m_wakeM->addItem(QString::number(i).rightJustified(2, '0'));
        m_wakeH->setStyleSheet(comboStyle); m_wakeM->setStyleSheet(comboStyle);
        editRow->addWidget(m_wakeH); editRow->addWidget(new QLabel(":", g)); editRow->addWidget(m_wakeM);
        gl->addLayout(editRow);

        QPushButton *saveBtn = new QPushButton(QString::fromUtf8("保存"), g);
        saveBtn->setStyleSheet("QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 6px; padding: 5px 14px; font-size: 11px; font-weight: bold; } QPushButton:hover { background: #a0b0ff; }");
        saveBtn->setMaximumWidth(80);
        connect(saveBtn, &QPushButton::clicked, this, [this]() {
            SleepRecord r; r.date = m_calendarDate;
            r.sleepTime = QTime(m_sleepH->currentText().toInt(), m_sleepM->currentText().toInt());
            r.wakeTime  = QTime(m_wakeH->currentText().toInt(), m_wakeM->currentText().toInt());
            DataManager::instance()->saveSleep(r);
            refreshTimelineView();
        });
        gl->addWidget(saveBtn);
        rightLayout->addWidget(g);
    }

    // --- 饮水（统一：展示 + 滑块）---
    {
        QGroupBox *g = makeGroup(QString::fromUtf8("💧 饮水"));
        QVBoxLayout *gl = new QVBoxLayout(g);
        gl->setSpacing(6);

        m_waterLabel = new QLabel(QString::fromUtf8("0 / 2000 ml  (0%)"), g);
        m_waterLabel->setStyleSheet("color: #5eeadb; font-size: 14px; font-weight: bold;");
        gl->addWidget(m_waterLabel);

        m_waterSlider = new QSlider(Qt::Horizontal, g);
        m_waterSlider->setRange(0, 5000);
        m_waterSlider->setValue(0);
        m_waterSlider->setSingleStep(100);
        m_waterSlider->setStyleSheet(
            "QSlider::groove:horizontal { height: 6px; background: rgba(255,255,255,0.08); border-radius: 3px; }"
            "QSlider::handle:horizontal { width: 16px; height: 16px; margin: -5px 0; background: qlineargradient(x1:0,y1:0,x2:0,y2:1,stop:0 #8b9ef6,stop:1 #5eeadb); border-radius: 8px; border: 1px solid rgba(255,255,255,0.2); }"
            "QSlider::sub-page:horizontal { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,stop:0 rgba(94,234,219,0.4),stop:1 rgba(94,234,219,0.2)); border-radius: 3px; }");
        connect(m_waterSlider, &QSlider::valueChanged, this, [this](int v) {
            m_waterLabel->setText(QString::fromUtf8("%1 / 2000 ml  (%2%)")
                                      .arg(v).arg(v * 100 / 2000));
        });
        gl->addWidget(m_waterSlider);

        QPushButton *saveBtn = new QPushButton(QString::fromUtf8("保存"), g);
        saveBtn->setStyleSheet("QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 6px; padding: 5px 14px; font-size: 11px; font-weight: bold; } QPushButton:hover { background: #a0b0ff; }");
        saveBtn->setMaximumWidth(80);
        connect(saveBtn, &QPushButton::clicked, this, [this]() {
            WaterRecord r; r.date = m_calendarDate; r.currentMl = m_waterSlider->value();
            DataManager::instance()->saveWater(r);
            refreshTimelineView();
        });
        gl->addWidget(saveBtn);
        rightLayout->addWidget(g);
    }

    // --- 饮食（统一：表格 + 添加表单）---
    {
        QGroupBox *g = makeGroup(QString::fromUtf8("🍽 饮食"));
        QVBoxLayout *gl = new QVBoxLayout(g);
        gl->setSpacing(4);

        m_viewDietTable = new QTableWidget(g);
        m_viewDietTable->setColumnCount(3);
        m_viewDietTable->setHorizontalHeaderLabels({QString::fromUtf8("餐次"), QString::fromUtf8("食物"), QString::fromUtf8("")});
        m_viewDietTable->setStyleSheet(
            "QTableWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 6px; color: #c0bcd8; }"
            "QTableWidget::item { padding: 2px; }"
            "QHeaderView::section { background: rgba(255,255,255,0.04); color: #9e9ab8; border: none; padding: 4px; font-size: 10px; }");
        m_viewDietTable->verticalHeader()->setVisible(false);
        m_viewDietTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_viewDietTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_viewDietTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_viewDietTable->setColumnWidth(0, 50);
        m_viewDietTable->setColumnWidth(2, 28);
        m_viewDietTable->setMaximumHeight(160);
        gl->addWidget(m_viewDietTable);

        QHBoxLayout *addRow = new QHBoxLayout();
        addRow->setSpacing(4);
        QComboBox *mealCb = new QComboBox(g);
        mealCb->addItems({QString::fromUtf8("早餐"), QString::fromUtf8("午餐"), QString::fromUtf8("晚餐"), QString::fromUtf8("加餐")});
        mealCb->setStyleSheet("QComboBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10); border-radius: 5px; padding: 3px 6px; color: #e2e0f0; font-size: 11px; }");
        addRow->addWidget(mealCb);
        QLineEdit *foodInput = new QLineEdit(g);
        foodInput->setPlaceholderText(QString::fromUtf8("食物名称"));
        foodInput->setStyleSheet("QLineEdit { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10); border-radius: 5px; padding: 4px 8px; color: #e2e0f0; font-size: 11px; }");
        addRow->addWidget(foodInput, 1);
        QPushButton *addBtn = new QPushButton("+", g);
        addBtn->setFixedSize(26, 22);
        addBtn->setStyleSheet("QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 5px; font-size: 14px; font-weight: bold; padding: 0px; } QPushButton:hover { background: #a0b0ff; }");
        connect(addBtn, &QPushButton::clicked, this, [this, mealCb, foodInput]() {
            if (foodInput->text().trimmed().isEmpty()) return;
            auto records = DataManager::instance()->getDiet(m_calendarDate);
            DietRecord r; r.date = m_calendarDate; r.mealType = mealCb->currentText(); r.foodName = foodInput->text().trimmed();
            records.append(r);
            DataManager::instance()->saveDiet(m_calendarDate, records);
            foodInput->clear();
            refreshDietTable();
        });
        addRow->addWidget(addBtn);
        gl->addLayout(addRow);
        rightLayout->addWidget(g);
    }

    // --- 运动（统一：展示框 + 表格 + 添加表单）---
    {
        QGroupBox *g = makeGroup(QString::fromUtf8("🏃 运动"));
        QVBoxLayout *gl = new QVBoxLayout(g);
        gl->setSpacing(8);

        // 展示框 — 毛玻璃卡片，显示当前记录
        m_exerciseDisplayBox = new QLabel(g);
        m_exerciseDisplayBox->setWordWrap(true);
        m_exerciseDisplayBox->setMinimumHeight(50);
        m_exerciseDisplayBox->setStyleSheet(
            "QLabel {"
            "  color: #f5cd5c; font-size: 13px; font-weight: bold;"
            "  background: rgba(255,255,255,0.04);"
            "  border: 1px solid rgba(255,255,255,0.10);"
            "  border-radius: 8px;"
            "  padding: 12px;"
            "}");
        gl->addWidget(m_exerciseDisplayBox);

        // 快速标签
        m_exerciseLabel = new QLabel(QString::fromUtf8("暂无数据"), g);
        m_exerciseLabel->setStyleSheet("color: #f5cd5c; font-size: 13px; font-weight: bold; background: transparent;");
        gl->addWidget(m_exerciseLabel);

        // 运动表格 — 列出所有条目
        m_viewExerciseTable = new QTableWidget(g);
        m_viewExerciseTable->setColumnCount(3);
        m_viewExerciseTable->setHorizontalHeaderLabels({QString::fromUtf8("项目"), QString::fromUtf8("时长"), QString::fromUtf8("")});
        m_viewExerciseTable->setStyleSheet(
            "QTableWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 6px; color: #c0bcd8; }"
            "QTableWidget::item { padding: 2px; }"
            "QHeaderView::section { background: rgba(255,255,255,0.04); color: #9e9ab8; border: none; padding: 4px; font-size: 10px; }");
        m_viewExerciseTable->verticalHeader()->setVisible(false);
        m_viewExerciseTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_viewExerciseTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_viewExerciseTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_viewExerciseTable->setColumnWidth(1, 80);
        m_viewExerciseTable->setColumnWidth(2, 28);
        m_viewExerciseTable->setMaximumHeight(150);
        gl->addWidget(m_viewExerciseTable);

        QHBoxLayout *typeRow = new QHBoxLayout();
        typeRow->setSpacing(6);
        m_exerciseTypeCb = new QComboBox(g);
        m_exerciseTypeCb->setEditable(true);
        m_exerciseTypeCb->addItems({QString::fromUtf8("跑步"), QString::fromUtf8("游泳"),
                                    QString::fromUtf8("健身"), QString::fromUtf8("骑行"),
                                    QString::fromUtf8("瑜伽"), QString::fromUtf8("篮球"),
                                    QString::fromUtf8("足球"), QString::fromUtf8("跳绳"),
                                    QString::fromUtf8("散步"), QString::fromUtf8("其他")});
        m_exerciseTypeCb->setStyleSheet(
            "QComboBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10); border-radius: 6px;"
            "  padding: 4px 8px; color: #e2e0f0; font-size: 12px; }"
            "QComboBox:hover { background: rgba(255,255,255,0.08); }"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView { background: rgba(20,16,42,0.95); color: #e2e0f0; }");
        typeRow->addWidget(m_exerciseTypeCb, 1);

        m_exerciseMinInput = new QSpinBox(g);
        m_exerciseMinInput->setRange(5, 480);
        m_exerciseMinInput->setValue(30);
        m_exerciseMinInput->setSingleStep(5);
        m_exerciseMinInput->setSuffix(QString::fromUtf8(" 分钟"));
        m_exerciseMinInput->setStyleSheet(
            "QSpinBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
            "  border-radius: 6px; padding: 4px 8px; color: #e2e0f0; font-size: 12px; }");
        typeRow->addWidget(m_exerciseMinInput);
        gl->addLayout(typeRow);

        QPushButton *saveBtn = new QPushButton(QString::fromUtf8("＋ 添加运动"), g);
        saveBtn->setStyleSheet(
            "QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 6px;"
            "  padding: 5px 14px; font-size: 11px; font-weight: bold; }"
            "QPushButton:hover { background: #a0b0ff; }");
        connect(saveBtn, &QPushButton::clicked, this, [this]() {
            ExerciseRecord r;
            r.date = m_calendarDate;
            r.durationMinutes = m_exerciseMinInput->value();
            r.exerciseType = m_exerciseTypeCb->currentText().trimmed();
            if (r.exerciseType.isEmpty()) return;
            DataManager::instance()->saveExercise(r);
            refreshTimelineView();
        });
        gl->addWidget(saveBtn);
        rightLayout->addWidget(g);
    }

    rightLayout->addStretch();
    rightScroll->setWidget(rightSide);
    mainLayout->addWidget(rightScroll);

    refreshCalendar(m_timelineCalendar, m_calendarDate);
    refreshTimelineView();

    return page;
}

// 日历双击 → 打开该日期的日程管理弹窗
void CenterPanel::onCalendarDateDoubleClicked(const QDate &date)
{
    ScheduleDialog dlg(date, this->window());
    dlg.exec();
    refreshCurrentPage();
}

void CenterPanel::onSkillSelectorChanged(int /*skillId*/)
{
    refreshStudyPlanPage();
}

// ---- 任务库 ----
QWidget* CenterPanel::buildStudyPlanPage()
{
    QWidget *page = new QWidget();
    page->setStyleSheet("background: transparent;");
    QVBoxLayout *ml = new QVBoxLayout(page);
    ml->setContentsMargins(16, 16, 16, 16);
    ml->setSpacing(10);

    QLabel *title = new QLabel(QString::fromUtf8("📚 任务库"), page);
    title->setStyleSheet("color: #e2e0f0; font-size: 16px; font-weight: bold;");
    ml->addWidget(title);

    // 技能筛选器
    QHBoxLayout *skillRow = new QHBoxLayout();
    QLabel *skillLbl = new QLabel(QString::fromUtf8("筛选技能:"), page);
    skillLbl->setStyleSheet("color: #9e9ab8; font-size: 12px;");
    skillRow->addWidget(skillLbl);
    m_skillSelector = new QComboBox(page);
    m_skillSelector->setStyleSheet(
        "QComboBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10); border-radius: 6px;"
        "  padding: 5px 10px; color: #e2e0f0; font-size: 12px; min-width: 160px; }"
        "QComboBox:hover { background: rgba(255,255,255,0.08); }"
        "QComboBox::drop-down { border: none; }"
        "QComboBox QAbstractItemView { background: rgba(20,16,42,0.95); color: #e2e0f0; }");
    connect(m_skillSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &CenterPanel::onSkillSelectorChanged);
    skillRow->addWidget(m_skillSelector);
    skillRow->addStretch();
    ml->addLayout(skillRow);

    // 进度摘要
    m_skillPlanSummaryLabel = new QLabel(page);
    m_skillPlanSummaryLabel->setStyleSheet("color: #5eeadb; font-size: 12px; padding: 2px 0;");
    ml->addWidget(m_skillPlanSummaryLabel);

    // 总体进度
    m_planProgressLabel = new QLabel(page);
    m_planProgressLabel->setStyleSheet("color: #f5cd5c; font-size: 14px; font-weight: bold;");
    ml->addWidget(m_planProgressLabel);

    QProgressBar *planBar = new QProgressBar(page);
    planBar->setRange(0, 100);
    planBar->setTextVisible(false);
    planBar->setFixedHeight(8);
    planBar->setStyleSheet("QProgressBar { background: rgba(255,255,255,0.05); border: none; border-radius: 4px; }"
                           "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                           "  stop:0 #5eeadb, stop:1 #8b9ef6); border-radius: 4px; }");
    planBar->setObjectName("planProgressBar");
    ml->addWidget(planBar);

    // 等级显示
    auto profile = DataManager::instance()->getUserProfile();
    QLabel *levelLabel = new QLabel(
        QString::fromUtf8("⭐ Lv.%1  |  %2 / %3 XP  |  总经验: %4")
            .arg(profile.level).arg(profile.xpInCurrentLevel())
            .arg(profile.xpToNextLevel()).arg(profile.totalXp), page);
    levelLabel->setStyleSheet("color: #f5cd5c; font-size: 13px; font-weight: bold; padding: 4px 0;");
    QProgressBar *levelBar = new QProgressBar(page);
    levelBar->setRange(0, profile.xpToNextLevel());
    levelBar->setValue(profile.xpInCurrentLevel());
    levelBar->setTextVisible(false);
    levelBar->setFixedHeight(6);
    levelBar->setStyleSheet("QProgressBar { background: rgba(255,255,255,0.05); border: none; border-radius: 3px; }"
                            "QProgressBar::chunk { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
                            "  stop:0 #f5cd5c, stop:1 #f08daa); border-radius: 3px; }");
    connect(DataManager::instance(), &DataManager::dataChanged, page, [levelLabel, levelBar]() {
        auto p = DataManager::instance()->getUserProfile();
        levelLabel->setText(QString::fromUtf8("⭐ Lv.%1  |  %2 / %3 XP  |  总经验: %4")
                                .arg(p.level).arg(p.xpInCurrentLevel()).arg(p.xpToNextLevel()).arg(p.totalXp));
        levelBar->setRange(0, p.xpToNextLevel());
        levelBar->setValue(p.xpInCurrentLevel());
    });
    ml->addWidget(levelLabel);
    ml->addWidget(levelBar);

    // 添加表单
    QGroupBox *form = new QGroupBox(QString::fromUtf8("添加任务模板"), page);
    form->setStyleSheet(
        "QGroupBox { color: #9e9ab8; font-size: 12px; font-weight: bold; border: 1px solid rgba(255,255,255,0.10); border-radius: 10px;"
        "  margin-top: 10px; padding-top: 18px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 2px 10px; }");
    QGridLayout *fl = new QGridLayout(form);
    fl->setSpacing(8);

    QLineEdit *planTitleInput = new QLineEdit(form);
    planTitleInput->setPlaceholderText(QString::fromUtf8("任务名称"));
    planTitleInput->setStyleSheet("QLineEdit { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
                                  "  border-radius: 8px; padding: 6px 10px; color: #e2e0f0; }");
    fl->addWidget(new QLabel(QString::fromUtf8("名称:"), form), 0, 0);
    fl->addWidget(planTitleInput, 0, 1, 1, 3);

    QDoubleSpinBox *planHours = new QDoubleSpinBox(form);
    planHours->setRange(0.5, 1000); planHours->setValue(1);
    planHours->setSingleStep(0.5);
    planHours->setSuffix(QString::fromUtf8(" 小时"));
    planHours->setStyleSheet("QDoubleSpinBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10); border-radius: 6px; padding: 4px; color: #e2e0f0; }");
    fl->addWidget(new QLabel(QString::fromUtf8("时长:"), form), 1, 0);
    fl->addWidget(planHours, 1, 1);

    QDoubleSpinBox *planXpBaseHours = new QDoubleSpinBox(form);
    planXpBaseHours->setRange(0.25, 100); planXpBaseHours->setValue(1);
    planXpBaseHours->setSingleStep(0.5);
    planXpBaseHours->setSuffix(QString::fromUtf8(" 小时"));
    planXpBaseHours->setToolTip(QString::fromUtf8("每完成这么多小时结算一次经验"));
    planXpBaseHours->setStyleSheet("QDoubleSpinBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10); border-radius: 6px; padding: 4px; color: #e2e0f0; }");
    fl->addWidget(new QLabel(QString::fromUtf8("每"), form), 1, 2);
    fl->addWidget(planXpBaseHours, 1, 3);

    QSpinBox *planXp = new QSpinBox(form);
    planXp->setRange(1, 9999); planXp->setValue(10);
    planXp->setSuffix(QString::fromUtf8(" 经验"));
    planXp->setToolTip(QString::fromUtf8("每段时长获得的经验值"));
    planXp->setStyleSheet("QSpinBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10); border-radius: 6px; padding: 4px; color: #f5cd5c; }");
    fl->addWidget(new QLabel(QString::fromUtf8("获得"), form), 2, 0);
    fl->addWidget(planXp, 2, 1);

    QPushButton *addPlanBtn = new QPushButton(QString::fromUtf8("＋ 添加"), form);
    addPlanBtn->setStyleSheet(
        "QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 6px;"
        "  padding: 6px 16px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: #a0b0ff; }");
    fl->addWidget(addPlanBtn, 3, 0, 1, 4);
    fl->setColumnStretch(1, 1);
    fl->setColumnStretch(3, 1);

    connect(addPlanBtn, &QPushButton::clicked, this, [this, planTitleInput, planHours, planXpBaseHours, planXp]() {
        if (planTitleInput->text().trimmed().isEmpty()) return;
        PlanItem item;
        item.title = planTitleInput->text().trimmed();
        item.plannedHours = planHours->value();
        item.xpBaseHours = planXpBaseHours->value();
        item.xp = planXp->value();
        item.stars = 1;  // legacy
        item.createdDate = QDate::currentDate();
        int selSkillId = m_skillSelector->currentData().toInt();
        if (selSkillId > 0) item.skillId = selSkillId;
        DataManager::instance()->savePlanItem(item);
        planTitleInput->clear();
    });

    ml->addWidget(form);

    // 任务表格
    m_planTable = new QTableWidget(page);
    m_planTable->setColumnCount(6);
    m_planTable->setHorizontalHeaderLabels({QString::fromUtf8("名称"),
                                            QString::fromUtf8("时长"),
                                            QString::fromUtf8("经验"),
                                            QString::fromUtf8("所属技能"),
                                            QString::fromUtf8("创建"),
                                            QString::fromUtf8("操作")});
    m_planTable->setStyleSheet(
        "QTableWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; color: #c0bcd8; }"
        "QHeaderView::section { background: rgba(255,255,255,0.04); color: #9e9ab8; border: none; padding: 6px; font-size: 11px; }");
    m_planTable->verticalHeader()->setVisible(false);
    m_planTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_planTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_planTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ml->addWidget(m_planTable);

    // === 习惯区（合并到任务库中）===
    {
        QFrame *divider = new QFrame(page);
        divider->setFrameShape(QFrame::HLine);
        divider->setStyleSheet("QFrame { color: rgba(255,255,255,0.08); margin: 8px 0; }");
        ml->addWidget(divider);

        QLabel *habitTitle = new QLabel(QString::fromUtf8("🌱 习惯养成"), page);
        habitTitle->setStyleSheet("color: #e2e0f0; font-size: 16px; font-weight: bold;");
        ml->addWidget(habitTitle);

        QLabel *habitDesc = new QLabel(QString::fromUtf8("每天/每周/每月坚持"), page);
        habitDesc->setStyleSheet("color: #6e6a88; font-size: 11px; padding-bottom: 4px;");
        habitDesc->setWordWrap(true);
        ml->addWidget(habitDesc);

        // 添加习惯表单
        QGroupBox *habitForm = new QGroupBox(QString::fromUtf8("添加新习惯"), page);
        habitForm->setStyleSheet(
            "QGroupBox { color: #9e9ab8; font-size: 12px; font-weight: bold;"
            "  border: 1px solid rgba(255,255,255,0.10); border-radius: 10px;"
            "  margin-top: 10px; padding-top: 18px; }"
            "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 2px 10px; }");
        QGridLayout *hfl = new QGridLayout(habitForm);
        hfl->setSpacing(8);

        QLineEdit *habitNameInput = new QLineEdit(habitForm);
        habitNameInput->setPlaceholderText(QString::fromUtf8("习惯名称，如：阅读、冥想、跑步..."));
        habitNameInput->setStyleSheet("QLineEdit { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
                                      "  border-radius: 8px; padding: 6px 10px; color: #e2e0f0; }");
        hfl->addWidget(new QLabel(QString::fromUtf8("名称:"), habitForm), 0, 0);
        hfl->addWidget(habitNameInput, 0, 1, 1, 3);

        m_habitModeCb = new QComboBox(habitForm);
        m_habitModeCb->addItems({QString::fromUtf8("📅 每天"), QString::fromUtf8("📆 每周"), QString::fromUtf8("🗓 每月")});
        m_habitModeCb->setStyleSheet(
            "QComboBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
            "  border-radius: 6px; padding: 5px 8px; color: #e2e0f0; font-size: 12px; }"
            "QComboBox:hover { background: rgba(255,255,255,0.08); }"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView { background: rgba(20,16,42,0.95); color: #e2e0f0; }");
        hfl->addWidget(new QLabel(QString::fromUtf8("周期:"), habitForm), 1, 0);
        hfl->addWidget(m_habitModeCb, 1, 1);

        m_habitCompleteModeCb = new QComboBox(habitForm);
        m_habitCompleteModeCb->addItems({QString::fromUtf8("🔢 次数"), QString::fromUtf8("⏱ 小时"), QString::fromUtf8("🕐 时长(分钟)")});
        m_habitCompleteModeCb->setStyleSheet(m_habitModeCb->styleSheet());
        hfl->addWidget(new QLabel(QString::fromUtf8("完成方式:"), habitForm), 1, 2);
        hfl->addWidget(m_habitCompleteModeCb, 1, 3);

        QSpinBox *hTargetCount = new QSpinBox(habitForm);
        hTargetCount->setRange(1, 100);
        hTargetCount->setValue(1);
        hTargetCount->setSuffix(QString::fromUtf8(" 次"));
        hTargetCount->setStyleSheet("QSpinBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
                                    "  border-radius: 6px; padding: 5px; color: #e2e0f0; }");

        QDoubleSpinBox *hTargetHours = new QDoubleSpinBox(habitForm);
        hTargetHours->setRange(0.25, 24);
        hTargetHours->setValue(1);
        hTargetHours->setSingleStep(0.25);
        hTargetHours->setSuffix(QString::fromUtf8(" 小时"));
        hTargetHours->setStyleSheet(hTargetCount->styleSheet());
        hTargetHours->setVisible(false);

        QSpinBox *hTargetMin = new QSpinBox(habitForm);
        hTargetMin->setRange(5, 1440);
        hTargetMin->setValue(30);
        hTargetMin->setSingleStep(5);
        hTargetMin->setSuffix(QString::fromUtf8(" 分钟"));
        hTargetMin->setStyleSheet(hTargetCount->styleSheet());
        hTargetMin->setVisible(false);

        hfl->addWidget(new QLabel(QString::fromUtf8("目标:"), habitForm), 2, 0);
        hfl->addWidget(hTargetCount, 2, 1);
        hfl->addWidget(hTargetHours, 2, 1);
        hfl->addWidget(hTargetMin, 2, 1);

        QComboBox *hStarCb = new QComboBox(habitForm);
        hStarCb->addItems({"⭐ 1星 (10XP)", "⭐⭐ 2星 (28XP)", "⭐⭐⭐ 3星 (52XP)",
                           "⭐⭐⭐⭐ 4星 (80XP)", "⭐⭐⭐⭐⭐ 5星 (112XP)"});
        hStarCb->setCurrentIndex(0);
        hStarCb->setStyleSheet(
            "QComboBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
            "  border-radius: 6px; padding: 5px 8px; color: #f5cd5c; font-size: 12px; }"
            "QComboBox:hover { background: rgba(255,255,255,0.08); }"
            "QComboBox::drop-down { border: none; }"
            "QComboBox QAbstractItemView { background: rgba(20,16,42,0.95); color: #e2e0f0; }");
        hfl->addWidget(new QLabel(QString::fromUtf8("难度:"), habitForm), 2, 2);
        hfl->addWidget(hStarCb, 2, 3);

        connect(m_habitCompleteModeCb, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [hTargetCount, hTargetHours, hTargetMin](int idx) {
                    hTargetCount->setVisible(idx == 0);
                    hTargetHours->setVisible(idx == 1);
                    hTargetMin->setVisible(idx == 2);
                });

        QPushButton *hAddBtn = new QPushButton(QString::fromUtf8("＋ 添加习惯"), habitForm);
        hAddBtn->setStyleSheet(
            "QPushButton { color: #fff; background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "  stop:0 #81c784, stop:1 #5eeadb); border: none; border-radius: 8px;"
            "  padding: 7px 20px; font-size: 13px; font-weight: bold; }"
            "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
            "  stop:0 #a5d6a7, stop:1 #80cbc4); }");
        hfl->addWidget(hAddBtn, 3, 0, 1, 4);

        auto habitFormSaver = [this, habitNameInput, hTargetCount, hTargetHours, hTargetMin, hStarCb]() {
            if (habitNameInput->text().trimmed().isEmpty()) return;
            HabitItem h;
            h.name = habitNameInput->text().trimmed();
            h.createdDate = QDate::currentDate();
            h.stars = hStarCb->currentIndex() + 1;
            int modeIdx = m_habitModeCb->currentIndex();
            h.habitMode = (modeIdx == 1) ? "weekly" : (modeIdx == 2) ? "monthly" : "daily";
            int compIdx = m_habitCompleteModeCb->currentIndex();
            if (compIdx == 0) { h.completionMode = "count"; h.targetCount = hTargetCount->value(); }
            else if (compIdx == 1) { h.completionMode = "hours"; h.targetHours = hTargetHours->value(); }
            else { h.completionMode = "duration"; h.targetMinutes = hTargetMin->value(); }
            DataManager::instance()->saveHabit(h);
            habitNameInput->clear();
            refreshStudyPlanPage();
        };
        connect(hAddBtn, &QPushButton::clicked, this, habitFormSaver);
        // 同时允许在名称输入框中按回车键提交
        connect(habitNameInput, &QLineEdit::returnPressed, this, habitFormSaver);

        ml->addWidget(habitForm);

        // 习惯表格
        m_habitsTable = new QTableWidget(page);
        m_habitsTable->setColumnCount(5);
        m_habitsTable->setHorizontalHeaderLabels({QString::fromUtf8("习惯"),
                                                  QString::fromUtf8("周期"),
                                                  QString::fromUtf8("目标"),
                                                  QString::fromUtf8("今日进度"),
                                                  QString::fromUtf8("操作")});
        m_habitsTable->setStyleSheet(
            "QTableWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08);"
            "  border-radius: 8px; color: #c0bcd8; }"
            "QHeaderView::section { background: rgba(255,255,255,0.04); color: #9e9ab8;"
            "  border: none; padding: 6px; font-size: 11px; }");
        m_habitsTable->verticalHeader()->setVisible(false);
        m_habitsTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_habitsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_habitsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
        m_habitsTable->setColumnWidth(1, 55);
        m_habitsTable->setColumnWidth(2, 75);
        m_habitsTable->setColumnWidth(3, 170);
        m_habitsTable->setColumnWidth(4, 80);
        m_habitsTable->setMaximumHeight(250);
        ml->addWidget(m_habitsTable);
    }

    refreshStudyPlanPage();
    return page;
}

void CenterPanel::refreshStudyPlanPage()
{
    // 刷新技能筛选器
    if (m_skillSelector) {
        int prevSkillId = m_skillSelector->currentData().toInt();
        m_skillSelector->blockSignals(true);
        m_skillSelector->clear();
        m_skillSelector->addItem(QString::fromUtf8("-- 全部技能 --"), -1);
        auto skills = DataManager::instance()->getSkills();
        for (const auto &s : skills) {
            m_skillSelector->addItem(
                QString::fromUtf8("%1 (%2h/%3h)").arg(s.name).arg(s.investedHours, 0, 'f', 1).arg(s.plannedHours, 0, 'f', 1),
                s.id);
        }
        // 恢复之前的选中项
        int idx = m_skillSelector->findData(prevSkillId);
        if (idx >= 0) m_skillSelector->setCurrentIndex(idx);
        m_skillSelector->blockSignals(false);
    }

    if (!m_planTable) return;
    int selSkillId = m_skillSelector ? m_skillSelector->currentData().toInt() : -1;

    auto items = DataManager::instance()->getPlanItems(true);
    // 按技能筛选
    QList<PlanItem> filtered;
    for (const auto &p : items) {
        if (selSkillId <= 0 || p.skillId == selSkillId)
            filtered.append(p);
    }

    m_planTable->setRowCount(filtered.size());
    for (int i = 0; i < filtered.size(); ++i) {
        const auto &p = filtered[i];
        m_planTable->setItem(i, 0, new QTableWidgetItem(p.title));
        m_planTable->setItem(i, 1, new QTableWidgetItem(QString::fromUtf8("%1h").arg(p.plannedHours, 0, 'f', 1)));
        QTableWidgetItem *xpItem = new QTableWidgetItem(
            QString::fromUtf8("每%1h +%2XP").arg(p.xpBaseHours, 0, 'f', 1).arg(p.xp));
        xpItem->setToolTip(QString::fromUtf8("完成此任务时: 实际时长 / %1h × %2XP").arg(p.xpBaseHours, 0, 'f', 1).arg(p.xp));
        xpItem->setForeground(QColor("#f5cd5c"));
        m_planTable->setItem(i, 2, xpItem);

        // 技能名称
        QString skillName = QString::fromUtf8("—");
        if (p.skillId > 0) {
            SkillItem sk = DataManager::instance()->getSkill(p.skillId);
            if (sk.id > 0) skillName = sk.name;
        }
        m_planTable->setItem(i, 3, new QTableWidgetItem(skillName));

        m_planTable->setItem(i, 4, new QTableWidgetItem(p.createdDate.toString("yyyy-MM-dd")));

        // 操作按钮：编辑 + 删除（任务库 — 无状态）
        QWidget *btnW = new QWidget();
        QHBoxLayout *bl = new QHBoxLayout(btnW);
        bl->setContentsMargins(2, 2, 2, 2);
        bl->setSpacing(4);

        int pid = p.id;
        QPushButton *editBtn = new QPushButton(QString::fromUtf8("✎"));
        editBtn->setStyleSheet(
            "QPushButton { color: #f5cd5c; background: transparent; border: 1px solid rgba(245,205,92,0.4);"
            "  border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
            "QPushButton:hover { background: rgba(245,205,92,0.15); }");
        connect(editBtn, &QPushButton::clicked, this, [this, pid]() {
            PlanItem plan = DataManager::instance()->getPlanItem(pid);
            if (plan.id <= 0) return;
            bool ok;
            QString newTitle = QInputDialog::getText(this, QString::fromUtf8("编辑任务"),
                                                     QString::fromUtf8("名称:"), QLineEdit::Normal, plan.title, &ok);
            if (!ok || newTitle.trimmed().isEmpty()) return;
            double newHours = QInputDialog::getDouble(this, QString::fromUtf8("编辑任务"),
                                                      QString::fromUtf8("计划时长(小时):"), plan.plannedHours, 0.5, 1000, 1, &ok);
            if (!ok) return;
            double newXpBase = QInputDialog::getDouble(this, QString::fromUtf8("编辑任务"),
                                                       QString::fromUtf8("每多少小时结算经验:"), plan.xpBaseHours, 0.25, 100, 2, &ok);
            if (!ok) return;
            int newXp = QInputDialog::getInt(this, QString::fromUtf8("编辑任务"),
                                             QString::fromUtf8("获得经验值:"), plan.xp, 1, 9999, 1, &ok);
            if (!ok) return;
            plan.title = newTitle.trimmed();
            plan.plannedHours = newHours;
            plan.xpBaseHours = newXpBase;
            plan.xp = newXp;
            DataManager::instance()->updatePlanItem(plan);
        });
        bl->addWidget(editBtn);

        QPushButton *delBtn = new QPushButton(QString::fromUtf8("✕"));
        delBtn->setStyleSheet(
            "QPushButton { color: #f08daa; background: transparent; border: 1px solid rgba(240,141,170,0.4);"
            "  border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
            "QPushButton:hover { background: rgba(240,141,170,0.15); }");
        connect(delBtn, &QPushButton::clicked, this, [this, pid]() {
            DataManager::instance()->removePlanItem(pid);
        });
        bl->addWidget(delBtn);
        bl->addStretch();
        m_planTable->setCellWidget(i, 5, btnW);
    }

    // 进度摘要
    double prog = DataManager::instance()->planProgress();
    if (m_planProgressLabel)
        m_planProgressLabel->setText(QString::fromUtf8("总完成率: %1%").arg(prog, 0, 'f', 0));

    QProgressBar *bar = m_planTable->parentWidget()->findChild<QProgressBar*>("planProgressBar");
    if (bar) bar->setValue(static_cast<int>(prog));

    // 技能进度摘要
    if (m_skillPlanSummaryLabel) {
        if (selSkillId > 0) {
            SkillItem sk = DataManager::instance()->getSkill(selSkillId);
            double totalTaskHours = 0;
            for (const auto &p : filtered) {
                totalTaskHours += p.plannedHours;
            }
            m_skillPlanSummaryLabel->setText(
                QString::fromUtf8("📊 技能 [%1]: 任务总时长 %2h / 计划总时长 %3h  |  已投入 %4h")
                    .arg(sk.name).arg(totalTaskHours, 0, 'f', 1)
                    .arg(sk.plannedHours, 0, 'f', 1).arg(sk.investedHours, 0, 'f', 1));
        } else {
            m_skillPlanSummaryLabel->setText(QString::fromUtf8("📊 全部技能 — 选择一个技能查看详情"));
        }
    }

    // --- 刷新习惯表格（从习惯页面合并而来）---
    if (m_habitsTable) {
        auto dm = DataManager::instance();
        auto habits = dm->getHabits();
        QDate today = QDate::currentDate();

        m_habitsTable->setRowCount(habits.size());
        for (int i = 0; i < habits.size(); ++i) {
            const auto &h = habits[i];

            QString starPrefix;
            for (int s = 0; s < h.stars; ++s) starPrefix += "⭐";
            QTableWidgetItem *nameItem = new QTableWidgetItem(starPrefix + " " + h.name);
            nameItem->setToolTip(QString::fromUtf8("创建于 %1 | 难度 %2星 — 完成获得 %3 XP")
                                     .arg(h.createdDate.toString("yyyy-MM-dd")).arg(h.stars).arg(h.xpValue()));
            m_habitsTable->setItem(i, 0, nameItem);

            QString modeStr = h.habitMode == "daily" ? QString::fromUtf8("每天")
                              : h.habitMode == "weekly" ? QString::fromUtf8("每周")
                                                        : QString::fromUtf8("每月");
            m_habitsTable->setItem(i, 1, new QTableWidgetItem(modeStr));
            m_habitsTable->setItem(i, 2, new QTableWidgetItem(h.targetLabel()));

            double prog = dm->getHabitProgress(h.id, today);
            bool completed = dm->isHabitCompletedForDate(h.id, today);

            QWidget *progW = new QWidget();
            QHBoxLayout *progLayout = new QHBoxLayout(progW);
            progLayout->setContentsMargins(4, 2, 4, 2);
            progLayout->setSpacing(4);

            QProgressBar *bar = new QProgressBar();
            bar->setFixedHeight(14);
            bar->setTextVisible(false);
            bar->setRange(0, 100);
            double pct = 0;
            if (h.completionMode == "hours")
                pct = h.targetHours > 0 ? qMin(prog / h.targetHours * 100.0, 100.0) : 0;
            else if (h.completionMode == "duration")
                pct = h.targetMinutes > 0 ? qMin(prog / h.targetMinutes * 100.0, 100.0) : 0;
            else
                pct = h.targetCount > 0 ? qMin(prog / h.targetCount * 100.0, 100.0) : 0;
            bar->setValue(static_cast<int>(pct));
            QString barColor = completed ? "#81c784" : (pct >= 50 ? "#f5cd5c" : "#5eeadb");
            bar->setStyleSheet(QString(
                                   "QProgressBar { background: rgba(255,255,255,0.06); border: none; border-radius: 7px; }"
                                   "QProgressBar::chunk { background: %1; border-radius: 7px; }").arg(barColor));
            progLayout->addWidget(bar, 1);

            QString progLabel;
            if (h.completionMode == "hours")
                progLabel = QString::fromUtf8("%1/%2h").arg(prog, 0, 'f', 1).arg(h.targetHours, 0, 'f', 1);
            else if (h.completionMode == "duration")
                progLabel = QString::fromUtf8("%1/%2m").arg(static_cast<int>(prog)).arg(h.targetMinutes);
            else
                progLabel = QString::fromUtf8("%1/%2").arg(static_cast<int>(prog)).arg(h.targetCount);

            QLabel *progNumLabel = new QLabel(progLabel);
            progNumLabel->setStyleSheet(QString("color: %1; font-size: 10px; font-weight: bold; background: transparent;"
                                                " min-width: 48px;").arg(completed ? "#81c784" : "#c0bcd8"));
            progNumLabel->setAlignment(Qt::AlignCenter);
            progLayout->addWidget(progNumLabel);

            int hid = h.id;
            if (h.completionMode == "count") {
                QPushButton *incBtn = new QPushButton("+1");
                incBtn->setFixedSize(28, 20);
                incBtn->setStyleSheet(
                    "QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 4px;"
                    "  font-size: 10px; font-weight: bold; padding: 0px; }"
                    "QPushButton:hover { background: #a0b0ff; }");
                connect(incBtn, &QPushButton::clicked, this, [this, hid]() {
                    double p = DataManager::instance()->getHabitProgress(hid, QDate::currentDate());
                    DataManager::instance()->setHabitProgress(hid, QDate::currentDate(), p + 1);
                });
                progLayout->addWidget(incBtn);
            } else {
                double incAmt = h.completionMode == "hours" ? 0.5 : 15.0;
                QString incLabel = h.completionMode == "hours" ? "+0.5h" : "+15m";
                QPushButton *incBtn = new QPushButton(incLabel);
                incBtn->setStyleSheet(
                    "QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 4px;"
                    "  font-size: 9px; font-weight: bold; padding: 0px 3px; }"
                    "QPushButton:hover { background: #a0b0ff; }");
                incBtn->setFixedHeight(20);
                connect(incBtn, &QPushButton::clicked, this, [this, hid, incAmt]() {
                    double p = DataManager::instance()->getHabitProgress(hid, QDate::currentDate());
                    DataManager::instance()->setHabitProgress(hid, QDate::currentDate(), p + incAmt);
                });
                progLayout->addWidget(incBtn);
            }

            if (completed) {
                QPushButton *resetBtn = new QPushButton(QString::fromUtf8("↺"));
                resetBtn->setFixedSize(22, 20);
                resetBtn->setToolTip(QString::fromUtf8("重置"));
                resetBtn->setStyleSheet(
                    "QPushButton { color: #f08daa; background: transparent; border: 1px solid rgba(240,141,170,0.4);"
                    "  border-radius: 4px; font-size: 11px; padding: 0px; }"
                    "QPushButton:hover { background: rgba(240,141,170,0.15); }");
                connect(resetBtn, &QPushButton::clicked, this, [this, hid]() {
                    DataManager::instance()->setHabitProgress(hid, QDate::currentDate(), 0);
                });
                progLayout->addWidget(resetBtn);
            }

            m_habitsTable->setCellWidget(i, 3, progW);

            if (completed) {
                for (int c = 0; c < 3; ++c) {
                    if (auto *item = m_habitsTable->item(i, c))
                        item->setForeground(QColor("#81c784"));
                }
            }

            QWidget *btnW = new QWidget();
            QHBoxLayout *bl = new QHBoxLayout(btnW);
            bl->setContentsMargins(2, 2, 2, 2);
            bl->setSpacing(4);

            QPushButton *editBtn = new QPushButton(QString::fromUtf8("✎"));
            editBtn->setStyleSheet(
                "QPushButton { color: #f5cd5c; background: transparent; border: 1px solid rgba(245,205,92,0.4);"
                "  border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
                "QPushButton:hover { background: rgba(245,205,92,0.15); }");
            connect(editBtn, &QPushButton::clicked, this, [this, hid]() {
                HabitItem h = DataManager::instance()->getHabit(hid);
                if (h.id <= 0) return;
                bool ok;
                QString newName = QInputDialog::getText(this, QString::fromUtf8("编辑习惯"),
                                                        QString::fromUtf8("名称:"), QLineEdit::Normal, h.name, &ok);
                if (!ok || newName.trimmed().isEmpty()) return;
                h.name = newName.trimmed();
                QStringList modes = {QString::fromUtf8("每天"), QString::fromUtf8("每周"), QString::fromUtf8("每月")};
                QString curMode = h.habitMode == "daily" ? modes[0] : h.habitMode == "weekly" ? modes[1] : modes[2];
                int curIdx = modes.indexOf(curMode);
                if (curIdx < 0) curIdx = 0;
                QString modeStr = QInputDialog::getItem(this, QString::fromUtf8("编辑习惯"),
                                                        QString::fromUtf8("周期:"), modes, curIdx, false, &ok);
                if (!ok) return;
                h.habitMode = (modeStr == modes[1]) ? "weekly" : (modeStr == modes[2]) ? "monthly" : "daily";

                QStringList starOpts = {QString::fromUtf8("1星 (10XP)"), QString::fromUtf8("2星 (28XP)"),
                                        QString::fromUtf8("3星 (52XP)"), QString::fromUtf8("4星 (80XP)"),
                                        QString::fromUtf8("5星 (112XP)")};
                QString starStr = QInputDialog::getItem(this, QString::fromUtf8("编辑习惯"),
                                                        QString::fromUtf8("难度:"), starOpts, qBound(0, h.stars - 1, 4), false, &ok);
                if (!ok) return;
                h.stars = starOpts.indexOf(starStr) + 1;

                DataManager::instance()->saveHabit(h);
            });
            bl->addWidget(editBtn);

            QPushButton *delBtn = new QPushButton(QString::fromUtf8("✕"));
            delBtn->setStyleSheet(
                "QPushButton { color: #f08daa; background: transparent; border: 1px solid rgba(240,141,170,0.4);"
                "  border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
                "QPushButton:hover { background: rgba(240,141,170,0.15); }");
            connect(delBtn, &QPushButton::clicked, this, [this, hid]() {
                DataManager::instance()->removeHabit(hid);
            });
            bl->addWidget(delBtn);
            bl->addStretch();
            m_habitsTable->setCellWidget(i, 4, btnW);
        }
    }
}

// ---- 已完成记录 ----
QWidget* CenterPanel::buildCompletedPage()
{
    QWidget *page = new QWidget();
    page->setStyleSheet("background: transparent;");
    QVBoxLayout *ml = new QVBoxLayout(page);
    ml->setContentsMargins(16, 16, 16, 16);
    ml->setSpacing(10);

    QLabel *title = new QLabel(QString::fromUtf8("✅ 已完成记录"), page);
    title->setStyleSheet("color: #e2e0f0; font-size: 16px; font-weight: bold;");
    ml->addWidget(title);

    QLabel *hint = new QLabel(QString::fromUtf8("（来自时间线中完成的日程）"), page);
    hint->setStyleSheet("color: #6e6a88; font-size: 11px;");
    ml->addWidget(hint);

    QTableWidget *table = new QTableWidget(page);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({QString::fromUtf8("任务名称"),
                                      QString::fromUtf8("所属技能"),
                                      QString::fromUtf8("时长 / 经验"),
                                      QString::fromUtf8("日程日期"),
                                      QString::fromUtf8("完成日期")});
    table->setStyleSheet(
        "QTableWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; color: #c0bcd8; }"
        "QHeaderView::section { background: rgba(255,255,255,0.04); color: #9e9ab8; border: none; padding: 6px; font-size: 11px; }");
    table->verticalHeader()->setVisible(false);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    ml->addWidget(table);

    // Lambda 刷新函数
    auto refresh = [table]() {
        auto records = DataManager::instance()->getCompletedRecords();
        table->setRowCount(records.size());
        for (int i = 0; i < records.size(); ++i) {
            const auto &r = records[i];
            table->setItem(i, 0, new QTableWidgetItem(r.planTitle));
            table->setItem(i, 1, new QTableWidgetItem(
                                     r.skillName.isEmpty() ? QString::fromUtf8("—") : r.skillName));
            table->setItem(i, 2, new QTableWidgetItem(
                                     QString::fromUtf8("%1h / +%2XP").arg(r.plannedHours, 0, 'f', 1).arg(r.xp)));
            table->setItem(i, 3, new QTableWidgetItem(r.scheduleDate.toString("yyyy-MM-dd")));
            table->setItem(i, 4, new QTableWidgetItem(r.completedDate.toString("yyyy-MM-dd")));
        }
    };

    connect(DataManager::instance(), &DataManager::dataChanged, page, refresh, Qt::QueuedConnection);
    refresh();
    return page;
}

// ---- 技能表 ----
QWidget* CenterPanel::buildSkillsPage()
{
    QWidget *page = new QWidget();
    page->setStyleSheet("background: transparent;");
    QVBoxLayout *ml = new QVBoxLayout(page);
    ml->setContentsMargins(16, 16, 16, 16);
    ml->setSpacing(10);

    // 已完成技能的标签条（可水平滚动）
    QScrollArea *bubbleScroll = new QScrollArea(page);
    bubbleScroll->setWidgetResizable(true);
    bubbleScroll->setFixedHeight(52);
    bubbleScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    bubbleScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bubbleScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    bubbleScroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    m_bubbleArea = new QWidget(bubbleScroll);
    m_bubbleArea->setObjectName("bubbleArea");
    m_bubbleArea->setStyleSheet("#bubbleArea { background: transparent; }");
    QHBoxLayout *bubbleLayout = new QHBoxLayout(m_bubbleArea);
    bubbleLayout->setContentsMargins(0, 0, 0, 0);
    bubbleLayout->setSpacing(8);
    QLabel *bubbleTitle = new QLabel(QString::fromUtf8("🏆 已达成:"), m_bubbleArea);
    bubbleTitle->setStyleSheet("color: #f5cd5c; font-size: 13px; font-weight: bold; background: transparent;");
    bubbleLayout->addWidget(bubbleTitle);
    bubbleLayout->addStretch();
    bubbleScroll->setWidget(m_bubbleArea);
    ml->addWidget(bubbleScroll);

    QLabel *title = new QLabel(QString::fromUtf8("🎯 技能表"), page);
    title->setStyleSheet("color: #e2e0f0; font-size: 16px; font-weight: bold;");
    ml->addWidget(title);

    // 添加表单
    QGroupBox *form = new QGroupBox(QString::fromUtf8("添加技能"), page);
    form->setStyleSheet(
        "QGroupBox { color: #9e9ab8; font-size: 12px; font-weight: bold; border: 1px solid rgba(255,255,255,0.10); border-radius: 10px;"
        "  margin-top: 10px; padding-top: 18px; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 2px 10px; }");
    QHBoxLayout *fl = new QHBoxLayout(form);
    fl->setSpacing(8);

    QLineEdit *nameInput = new QLineEdit(form);
    nameInput->setPlaceholderText(QString::fromUtf8("技能名称"));
    nameInput->setStyleSheet("QLineEdit { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
                             "  border-radius: 8px; padding: 6px 10px; color: #e2e0f0; }");
    fl->addWidget(nameInput, 1);

    QSpinBox *plannedHoursInput = new QSpinBox(form);
    plannedHoursInput->setRange(1, 10000);
    plannedHoursInput->setValue(10);
    plannedHoursInput->setSuffix(QString::fromUtf8(" 小时"));
    plannedHoursInput->setStyleSheet("QSpinBox { background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
                                     "  border-radius: 6px; padding: 4px; color: #e2e0f0; }");
    fl->addWidget(plannedHoursInput);

    QPushButton *addBtn = new QPushButton(QString::fromUtf8("＋ 添加"), form);
    addBtn->setStyleSheet(
        "QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 6px;"
        "  padding: 6px 16px; font-size: 12px; font-weight: bold; }"
        "QPushButton:hover { background: #a0b0ff; }");
    fl->addWidget(addBtn);

    connect(addBtn, &QPushButton::clicked, this, [ nameInput, plannedHoursInput]() {
        if (nameInput->text().trimmed().isEmpty()) return;
        SkillItem s;
        s.name = nameInput->text().trimmed();
        s.plannedHours = plannedHoursInput->value();
        DataManager::instance()->saveSkill(s);
        nameInput->clear();
    });

    ml->addWidget(form);

    // 技能表格（含进度条可视化，放在滚动区域中）
    QScrollArea *tableScroll = new QScrollArea(page);
    tableScroll->setWidgetResizable(true);
    tableScroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    tableScroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    m_skillsTable = new QTableWidget(tableScroll);
    m_skillsTable->setColumnCount(5);
    m_skillsTable->setHorizontalHeaderLabels({QString::fromUtf8("技能"),
                                              QString::fromUtf8("计划(h)"),
                                              QString::fromUtf8("已投入(h)"),
                                              QString::fromUtf8("进度"),
                                              QString::fromUtf8("操作")});
    m_skillsTable->setStyleSheet(
        "QTableWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; color: #c0bcd8; }"
        "QHeaderView::section { background: rgba(255,255,255,0.04); color: #9e9ab8; border: none; padding: 6px; font-size: 11px; }");
    m_skillsTable->verticalHeader()->setVisible(false);
    m_skillsTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_skillsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_skillsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_skillsTable->setColumnWidth(3, 150);
    tableScroll->setWidget(m_skillsTable);
    ml->addWidget(tableScroll, 1);

    refreshSkillsPage();
    return page;
}

void CenterPanel::refreshSkillsPage()
{
    if (!m_skillsTable) return;
    auto skills = DataManager::instance()->getSkills();
    m_skillsTable->setRowCount(skills.size());

    // 标签颜色调色板
    static const QStringList pillColors = {
        "#81c784", "#5eeadb", "#ce93d8", "#f5cd5c", "#8b9ef6",
        "#f08daa", "#a5d6a7", "#ff8a65", "#90a4ae", "#ba68c8"
    };

    // 刷新标签条
    if (m_bubbleArea) {
        QHBoxLayout *bubbleLayout = qobject_cast<QHBoxLayout*>(m_bubbleArea->layout());
        if (bubbleLayout) {
            // 移除旧标签（保留标题在索引 0 处）
            while (bubbleLayout->count() > 2) {
                QLayoutItem *item = bubbleLayout->takeAt(1);
                if (item->widget()) item->widget()->deleteLater();
                delete item;
            }
            // 移除旧的尾部伸缩空间
            if (bubbleLayout->count() > 1) {
                QLayoutItem *item = bubbleLayout->takeAt(bubbleLayout->count() - 1);
                delete item;
            }

            int completedIdx = 0;
            for (const auto &s : skills) {
                if (s.isCompleted()) {
                    QString color = pillColors[completedIdx % pillColors.size()];
                    QString text = QString::fromUtf8("🎯 %1  ✓ %2h").arg(s.name).arg(s.investedHours, 0, 'f', 1);
                    QLabel *pill = new QLabel(text, m_bubbleArea);
                    pill->setAlignment(Qt::AlignCenter);
                    pill->setCursor(Qt::PointingHandCursor);
                    pill->setToolTip(QString::fromUtf8("计划 %1h → 已投入 %2h → 达成!")
                                         .arg(s.plannedHours, 0, 'f', 1).arg(s.investedHours, 0, 'f', 1));
                    pill->setStyleSheet(QString(
                                            "QLabel { background: %1; color: #fff; font-size: 12px; font-weight: bold;"
                                            "  border-radius: 14px; border: 1.5px solid rgba(255,255,255,0.30);"
                                            "  padding: 6px 14px; }"
                                            "QLabel:hover { border-color: #fff; background: %2; }")
                                            .arg(color + "cc", color));
                    pill->setFixedHeight(36);
                    pill->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
                    bubbleLayout->insertWidget(1 + completedIdx, pill);
                    ++completedIdx;
                }
            }
            bubbleLayout->addStretch();
        }
    }

    for (int i = 0; i < skills.size(); ++i) {
        m_skillsTable->setItem(i, 0, new QTableWidgetItem(skills[i].name));
        m_skillsTable->setItem(i, 1, new QTableWidgetItem(
                                         QString::fromUtf8("%1h").arg(skills[i].plannedHours, 0, 'f', 1)));
        m_skillsTable->setItem(i, 2, new QTableWidgetItem(
                                         QString::fromUtf8("%1h").arg(skills[i].investedHours, 0, 'f', 1)));

        // 进度条组件
        QWidget *barW = new QWidget();
        QHBoxLayout *bl = new QHBoxLayout(barW);
        bl->setContentsMargins(0, 2, 4, 2);
        QProgressBar *bar = new QProgressBar();
        bar->setRange(0, 100);
        double prog = skills[i].progressPercent();
        bar->setValue(static_cast<int>(qMin(prog, 100.0)));
        bar->setTextVisible(false);
        bar->setFixedHeight(14);
        QString barColor = prog >= 100 ? "#81c784" : (prog >= 50 ? "#f5cd5c" : "#5eeadb");
        bar->setStyleSheet(QString(
                               "QProgressBar { background: rgba(255,255,255,0.06); border: none; border-radius: 7px; }"
                               "QProgressBar::chunk { background: %1; border-radius: 7px; }").arg(barColor));
        bl->addWidget(bar);
        m_skillsTable->setCellWidget(i, 3, barW);

        // 操作按钮：编辑 + 删除
        QWidget *btnW = new QWidget();
        QHBoxLayout *btnLayout = new QHBoxLayout(btnW);
        btnLayout->setContentsMargins(2, 2, 2, 2);
        btnLayout->setSpacing(4);

        int skillId = skills[i].id;
        QPushButton *editBtn = new QPushButton(QString::fromUtf8("✎"));
        editBtn->setStyleSheet(
            "QPushButton { color: #f5cd5c; background: transparent; border: 1px solid rgba(245,205,92,0.4);"
            "  border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
            "QPushButton:hover { background: rgba(245,205,92,0.15); }");
        connect(editBtn, &QPushButton::clicked, this, [this, skillId]() {
            SkillItem s = DataManager::instance()->getSkill(skillId);
            if (s.id <= 0) return;
            bool ok;
            QString newName = QInputDialog::getText(this, QString::fromUtf8("编辑技能"),
                                                    QString::fromUtf8("名称:"), QLineEdit::Normal, s.name, &ok);
            if (!ok) return;
            double newHours = QInputDialog::getDouble(this, QString::fromUtf8("编辑技能"),
                                                      QString::fromUtf8("计划总时长(小时):"), s.plannedHours, 0.5, 10000, 1, &ok);
            if (!ok) return;
            s.name = newName;
            s.plannedHours = newHours;
            DataManager::instance()->saveSkill(s);
        });
        btnLayout->addWidget(editBtn);

        QPushButton *delBtn = new QPushButton(QString::fromUtf8("✕"));
        delBtn->setStyleSheet(
            "QPushButton { color: #f08daa; background: transparent; border: 1px solid rgba(240,141,170,0.4);"
            "  border-radius: 4px; padding: 2px 6px; font-size: 11px; }"
            "QPushButton:hover { background: rgba(240,141,170,0.15); }");
        connect(delBtn, &QPushButton::clicked, this, [ skillId]() {
            DataManager::instance()->removeSkill(skillId);
        });
        btnLayout->addWidget(delBtn);
        btnLayout->addStretch();
        m_skillsTable->setCellWidget(i, 4, btnW);
    }
}

// ---- 每日复盘 ----
QWidget* CenterPanel::buildReviewPage()
{
    QWidget *page = new QWidget();
    page->setStyleSheet("background: transparent;");
    QVBoxLayout *ml = new QVBoxLayout(page);
    ml->setContentsMargins(16, 16, 16, 16);
    ml->setSpacing(10);

    QLabel *title = new QLabel(QString::fromUtf8("📝 每日复盘"), page);
    title->setStyleSheet("color: #e2e0f0; font-size: 16px; font-weight: bold;");
    ml->addWidget(title);

    // 报告生成按钮
    QHBoxLayout *reportRow = new QHBoxLayout();
    QPushButton *reportBtn = new QPushButton(QString::fromUtf8("📄 生成今日报告"), page);
    reportBtn->setStyleSheet(
        "QPushButton { color: #fff; background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #8b9ef6, stop:1 #5eeadb); border: none; border-radius: 8px;"
        "  padding: 8px 20px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: qlineargradient(x1:0,y1:0,x2:1,y2:0,"
        "  stop:0 #a0b0ff, stop:1 #7efaf0); }");
    connect(reportBtn, &QPushButton::clicked, this, &CenterPanel::generateReport);
    reportRow->addWidget(reportBtn);

    QPushButton *openDirBtn = new QPushButton(QString::fromUtf8("📂 打开文件夹"), page);
    openDirBtn->setStyleSheet(
        "QPushButton { color: #c0bcd8; background: rgba(255,255,255,0.05); border: 1px solid rgba(255,255,255,0.10);"
        "  border-radius: 8px; padding: 8px 16px; font-size: 12px; }"
        "QPushButton:hover { background: rgba(255,255,255,0.10); color: #e2e0f0; }");
    connect(openDirBtn, &QPushButton::clicked, this, []() {
        QString dir = DataManager::instance()->reviewsDir();
        QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
    });
    reportRow->addWidget(openDirBtn);
    reportRow->addStretch();
    ml->addLayout(reportRow);

    // 报告展示区域
    m_reportLabel = new QLabel(page);
    m_reportLabel->setWordWrap(true);
    m_reportLabel->setMinimumHeight(80);
    m_reportLabel->setMaximumHeight(200);
    m_reportLabel->setStyleSheet(
        "QLabel { color: #c0bcd8; font-size: 12px; background: rgba(255,255,255,0.03);"
        "  border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; padding: 12px; }");
    ml->addWidget(m_reportLabel);

    // 编辑器
    QLabel *editorLabel = new QLabel(QString::fromUtf8("今日复盘笔记:"), page);
    editorLabel->setStyleSheet("color: #9e9ab8; font-size: 12px; font-weight: bold; padding-top: 6px;");
    ml->addWidget(editorLabel);

    m_reviewEditor = new QTextEdit(page);
    m_reviewEditor->setPlaceholderText(QString::fromUtf8("今日心得、总结、次日规划..."));
    m_reviewEditor->setMinimumHeight(160);
    m_reviewEditor->setMaximumHeight(280);
    m_reviewEditor->setStyleSheet(
        "QTextEdit { background: rgba(255,255,255,0.04); border: 1px solid rgba(255,255,255,0.10); border-radius: 10px;"
        "  color: #e2e0f0; font-size: 14px; padding: 12px; }"
        "QTextEdit:focus { border-color: rgba(139,158,246,0.50); }");
    ml->addWidget(m_reviewEditor);

    QPushButton *saveBtn = new QPushButton(QString::fromUtf8("💾 保存复盘"), page);
    saveBtn->setStyleSheet(
        "QPushButton { color: #fff; background: #8b9ef6; border: none; border-radius: 8px;"
        "  padding: 8px 24px; font-size: 13px; font-weight: bold; }"
        "QPushButton:hover { background: #a0b0ff; }");
    ml->addWidget(saveBtn);

    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        QString content = m_reviewEditor->toPlainText().trimmed();
        if (!content.isEmpty()) {
            DataManager::instance()->saveReview(content);
            m_reviewEditor->clear();
        }
    });

    // 历史记录
    QLabel *histLabel = new QLabel(QString::fromUtf8("历史复盘:"), page);
    histLabel->setStyleSheet("color: #9e9ab8; font-size: 12px; font-weight: bold; padding-top: 8px;");
    ml->addWidget(histLabel);

    m_reviewHistory = new QTableWidget(page);
    m_reviewHistory->setColumnCount(4);
    m_reviewHistory->setHorizontalHeaderLabels({QString::fromUtf8("日期"),
                                                QString::fromUtf8("内容"),
                                                QString::fromUtf8("文件"),
                                                QString::fromUtf8("")});
    m_reviewHistory->setStyleSheet(
        "QTableWidget { background: rgba(255,255,255,0.03); border: 1px solid rgba(255,255,255,0.08); border-radius: 8px; color: #c0bcd8; }"
        "QHeaderView::section { background: rgba(255,255,255,0.04); color: #9e9ab8; border: none; padding: 6px; font-size: 11px; }");
    m_reviewHistory->verticalHeader()->setVisible(false);
    m_reviewHistory->setSelectionMode(QAbstractItemView::NoSelection);
    m_reviewHistory->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_reviewHistory->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_reviewHistory->setColumnWidth(0, 100);
    m_reviewHistory->setColumnWidth(2, 50);
    m_reviewHistory->setColumnWidth(3, 50);
    ml->addWidget(m_reviewHistory);

    refreshReviewPage();
    return page;
}

// ---- 成就殿堂 ----
QWidget* CenterPanel::buildAchievementPage()
{
    QWidget *page = new QWidget();
    page->setStyleSheet("background: transparent;");
    QVBoxLayout *ml = new QVBoxLayout(page);
    ml->setContentsMargins(16, 16, 16, 16);
    ml->setSpacing(10);

    QLabel *title = new QLabel(QString::fromUtf8("🏆 成就殿堂"), page);
    title->setStyleSheet("color: #f5cd5c; font-size: 18px; font-weight: bold;");
    ml->addWidget(title);

    QLabel *desc = new QLabel(QString::fromUtf8("每一个成就都是你自律路上的里程碑"), page);
    desc->setStyleSheet("color: #6e6a88; font-size: 11px;");
    ml->addWidget(desc);

    // 成就区域滚动区
    QScrollArea *scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    QWidget *scrollContent = new QWidget(scroll);
    scrollContent->setStyleSheet("background: transparent;");
    scrollContent->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QVBoxLayout *cardLayout = new QVBoxLayout(scrollContent);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(10);
    scroll->setWidget(scrollContent);
    ml->addWidget(scroll, 1);

    page->setProperty("achieveCards", QVariant::fromValue<QVBoxLayout*>(cardLayout));
    refreshAchievementPage();
    return page;
}

void CenterPanel::refreshAchievementPage()
{
    if (!m_achievementPage) return;
    QVBoxLayout *cardLayout = m_achievementPage->property("achieveCards").value<QVBoxLayout*>();
    if (!cardLayout) return;

    // 清除现有卡片
    QLayoutItem *item;
    while ((item = cardLayout->takeAt(0)) != nullptr) {
        if (item->widget()) item->widget()->deleteLater();
        delete item;
    }

    auto dm = DataManager::instance();
    auto profile = dm->getUserProfile();
    int totalTasks = dm->getTotalCompletedTasks();
    int streak = dm->getConsecutiveDays();
    int level = profile.level;
    int totalXp = profile.totalXp;

    // 成就卡片创建器
    auto makeAchieveCard = [](const QString &icon, const QString &title, const QString &desc,
                              int current, int target, bool unlocked) -> QFrame* {
        QFrame *card = new QFrame();
        card->setStyleSheet(QString(
                                "QFrame { background: rgba(255,255,255,%1); border: 1px solid rgba(255,255,255,%2);"
                                "  border-radius: 12px; padding: 12px; }")
                                .arg(unlocked ? "0.06" : "0.03")
                                .arg(unlocked ? "0.15" : "0.06"));
        QHBoxLayout *layout = new QHBoxLayout(card);
        layout->setSpacing(12);

        QLabel *iconLabel = new QLabel(icon);
        iconLabel->setFixedSize(48, 48);
        iconLabel->setAlignment(Qt::AlignCenter);
        iconLabel->setStyleSheet(QString(
                                     "QLabel { font-size: 28px; background: %1; border-radius: 24px; }")
                                     .arg(unlocked ? "rgba(245,205,92,0.15)" : "rgba(255,255,255,0.04)"));
        layout->addWidget(iconLabel);

        QVBoxLayout *textLayout = new QVBoxLayout();
        textLayout->setSpacing(2);
        QLabel *titleLbl = new QLabel(title);
        titleLbl->setStyleSheet(QString("color: %1; font-size: 13px; font-weight: bold; background: transparent;")
                                    .arg(unlocked ? "#f5cd5c" : "#9e9ab8"));
        textLayout->addWidget(titleLbl);

        QLabel *descLbl = new QLabel(desc);
        descLbl->setStyleSheet("color: #6e6a88; font-size: 10px; background: transparent;");
        descLbl->setWordWrap(true);
        textLayout->addWidget(descLbl);

        // 进度条
        QProgressBar *bar = new QProgressBar();
        bar->setRange(0, target);
        bar->setValue(qMin(current, target));
        bar->setTextVisible(false);
        bar->setFixedHeight(6);
        bar->setStyleSheet(QString(
                               "QProgressBar { background: rgba(255,255,255,0.05); border: none; border-radius: 3px; }"
                               "QProgressBar::chunk { background: %1; border-radius: 3px; }")
                               .arg(unlocked ? "#f5cd5c" : "#5eeadb"));
        textLayout->addWidget(bar);

        QLabel *progLbl = new QLabel(QString::fromUtf8("%1 / %2  %3")
                                         .arg(qMin(current, target)).arg(target)
                                         .arg(unlocked ? QString::fromUtf8("✅ 已解锁") : QString::fromUtf8("🔒")));
        progLbl->setStyleSheet(QString("color: %1; font-size: 10px; background: transparent;")
                                   .arg(unlocked ? "#81c784" : "#6e6a88"));
        textLayout->addWidget(progLbl);

        layout->addLayout(textLayout, 1);
        return card;
    };

    // 里程碑成就
    QLabel *sectionTitle = new QLabel(QString::fromUtf8("🎯 里程碑成就"));
    sectionTitle->setStyleSheet("color: #e2e0f0; font-size: 14px; font-weight: bold;");
    cardLayout->addWidget(sectionTitle);

    cardLayout->addWidget(makeAchieveCard("⭐", QString::fromUtf8("初次启程"),
                                          QString::fromUtf8("完成第一个任务"), totalTasks, 1, totalTasks >= 1));
    cardLayout->addWidget(makeAchieveCard("📚", QString::fromUtf8("百事通"),
                                          QString::fromUtf8("累计完成 100 个任务"), totalTasks, 100, totalTasks >= 100));
    cardLayout->addWidget(makeAchieveCard("💎", QString::fromUtf8("登峰造极"),
                                          QString::fromUtf8("达到 Lv.10"), level, 10, level >= 10));
    cardLayout->addWidget(makeAchieveCard("👑", QString::fromUtf8("经验大师"),
                                          QString::fromUtf8("累计获得 1000 XP"), totalXp, 1000, totalXp >= 1000));

    // 毅力成就
    QLabel *streakTitle = new QLabel(QString::fromUtf8("\n🔥 毅力成就"));
    streakTitle->setStyleSheet("color: #e2e0f0; font-size: 14px; font-weight: bold;");
    cardLayout->addWidget(streakTitle);

    cardLayout->addWidget(makeAchieveCard("📅", QString::fromUtf8("连续打卡 7 天"),
                                          QString::fromUtf8("坚持就是胜利的第一步"), streak, 7, streak >= 7));
    cardLayout->addWidget(makeAchieveCard("🗓", QString::fromUtf8("连续打卡 30 天"),
                                          QString::fromUtf8("自律已经成为习惯"), streak, 30, streak >= 30));
    cardLayout->addWidget(makeAchieveCard("🏅", QString::fromUtf8("连续打卡 100 天"),
                                          QString::fromUtf8("自律的化身"), streak, 100, streak >= 100));

    // 速度成就
    QLabel *speedTitle = new QLabel(QString::fromUtf8("\n⚡ 速度成就"));
    speedTitle->setStyleSheet("color: #e2e0f0; font-size: 14px; font-weight: bold;");
    cardLayout->addWidget(speedTitle);

    // 检查今日经验
    int todayXp = 0;
    auto records = dm->getCompletedRecords();
    QDate today = QDate::currentDate();
    for (const auto &r : records) {
        if (r.completedDate == today) todayXp += r.xp;
    }
    int todayTasks = 0;
    for (const auto &r : records) {
        if (r.completedDate == today) ++todayTasks;
    }

    cardLayout->addWidget(makeAchieveCard("💨", QString::fromUtf8("效率达人"),
                                          QString::fromUtf8("单日获得 500+ XP"), todayXp, 500, todayXp >= 500));

    // 分类成就
    QLabel *catTitle = new QLabel(QString::fromUtf8("\n📂 分类成就"));
    catTitle->setStyleSheet("color: #e2e0f0; font-size: 14px; font-weight: bold;");
    cardLayout->addWidget(catTitle);

    // 按技能统计
    QMap<QString, int> skillCounts;
    for (const auto &r : records) {
        if (!r.skillName.isEmpty())
            skillCounts[r.skillName]++;
    }
    bool hasSkillAchieve = false;
    for (auto it = skillCounts.begin(); it != skillCounts.end(); ++it) {
        if (it.value() >= 10) {
            cardLayout->addWidget(makeAchieveCard("📖", QString::fromUtf8("[%1] 专项达人").arg(it.key()),
                                                  QString::fromUtf8("在 %1 分类完成 50 个任务").arg(it.key()), it.value(), 50, it.value() >= 50));
            hasSkillAchieve = true;
        }
    }
    if (!hasSkillAchieve) {
        QLabel *noCat = new QLabel(QString::fromUtf8("  暂无分类成就 — 关联技能后完成任务即可解锁"));
        noCat->setStyleSheet("color: #6e6a88; font-size: 11px; background: transparent;");
        cardLayout->addWidget(noCat);
    }

    cardLayout->addStretch();
}

void CenterPanel::refreshCompletedPage()
{
    // 已完成页面通过其内部连接 dataChanged 信号的 lambda 自动刷新
}

void CenterPanel::refreshReviewPage()
{
    if (!m_reviewHistory) return;
    auto reviews = DataManager::instance()->getReviews();
    m_reviewHistory->setRowCount(reviews.size());
    for (int i = 0; i < reviews.size(); ++i) {
        m_reviewHistory->setItem(i, 0, new QTableWidgetItem(reviews[i].date.toString("yyyy-MM-dd")));
        m_reviewHistory->setItem(i, 1, new QTableWidgetItem(reviews[i].content));

        // 打开文件按钮
        QDate revDate = reviews[i].date;
        QPushButton *openBtn = new QPushButton(QString::fromUtf8("📄"));
        openBtn->setStyleSheet(
            "QPushButton { color: #9e9ab8; background: transparent; border: 1px solid rgba(255,255,255,0.10);"
            "  border-radius: 4px; padding: 2px 6px; font-size: 14px; }"
            "QPushButton:hover { color: #e2e0f0; background: rgba(255,255,255,0.08); }");
        openBtn->setToolTip(QString::fromUtf8("用默认程序打开 %1.md").arg(revDate.toString("yyyy-MM-dd")));
        connect(openBtn, &QPushButton::clicked, this, [revDate]() {
            QString path = DataManager::instance()->reviewFilePath(revDate);
            if (QFile::exists(path))
                QDesktopServices::openUrl(QUrl::fromLocalFile(path));
        });
        m_reviewHistory->setCellWidget(i, 2, openBtn);

        // 删除按钮
        QPushButton *delBtn = new QPushButton(QString::fromUtf8("🗑"));
        delBtn->setStyleSheet(
            "QPushButton { color: #f08daa; background: transparent; border: 1px solid rgba(240,141,170,0.4);"
            "  border-radius: 4px; padding: 2px 6px; font-size: 14px; }"
            "QPushButton:hover { color: #f55; background: rgba(240,141,170,0.15); }");
        delBtn->setToolTip(QString::fromUtf8("删除 %1 的复盘").arg(revDate.toString("yyyy-MM-dd")));
        connect(delBtn, &QPushButton::clicked, this, [ revDate]() {
            DataManager::instance()->deleteReview(revDate);
        });
        m_reviewHistory->setCellWidget(i, 3, delBtn);
    }
}

// 生成每日综合报告：汇总日程完成/习惯打卡/睡眠/饮水/饮食/运动/计划进度
void CenterPanel::generateReport()
{
    QDate today = QDate::currentDate();
    auto dm = DataManager::instance();
    auto schedules = dm->getSchedules(today);
    auto habits = dm->getActiveHabits(today);

    int totalTasks = schedules.size();
    int completedTasks = 0;
    for (const auto &s : schedules) { if (s.completed) ++completedTasks; }
    int rate = totalTasks > 0 ? completedTasks * 100 / totalTasks : 0;

    SleepRecord sleep = dm->getSleep(today);
    WaterRecord water = dm->getWater(today);
    auto diets = dm->getDiet(today);

    QString report;
    report += QString::fromUtf8("══════ 每日报告 - %1 ══════\n\n").arg(today.toString("yyyy年M月d日"));

    report += QString::fromUtf8("【任务完成情况】\n");
    report += QString::fromUtf8("  总任务数: %1  已完成: %2  完成率: %3%\n")
                  .arg(totalTasks).arg(completedTasks).arg(rate);
    if (!schedules.isEmpty()) {
        report += QString::fromUtf8("  明细:\n");
        for (const auto &s : schedules) {
            PlanItem plan = dm->getPlanItem(s.planId);
            QString name = plan.id > 0 ? plan.title : QString::fromUtf8("(已删除)");
            report += QString::fromUtf8("    %1-%2  %3  %4\n")
                          .arg(s.startTime.toString("HH:mm"), s.endTime().toString("HH:mm"))
                          .arg(name,
                               s.completed ? QString::fromUtf8("✓") : QString::fromUtf8("⏳"));
        }
    }

    report += QString::fromUtf8("\n【睡眠】\n");
    if (sleep.sleepTime.isValid() && sleep.wakeTime.isValid()) {
        report += QString::fromUtf8("  入睡: %1  醒来: %2  时长: %3小时\n")
                      .arg(sleep.sleepTime.toString("HH:mm"), sleep.wakeTime.toString("HH:mm"))
                      .arg(sleep.durationHours(), 0, 'f', 1);
    } else {
        report += QString::fromUtf8("  暂无数据\n");
    }

    report += QString::fromUtf8("\n【饮水】\n");
    report += QString::fromUtf8("  已完成: %1ml / 目标: %2ml (%3%)\n")
                  .arg(water.currentMl).arg(water.targetMl).arg(water.percent(), 0, 'f', 0);

    report += QString::fromUtf8("\n【饮食记录】\n");
    if (diets.isEmpty()) {
        report += QString::fromUtf8("  暂无数据\n");
    } else {
        for (const auto &d : diets) {
            report += QString::fromUtf8("  %1: %2\n").arg(d.mealType, d.foodName);
        }
    }

    report += QString::fromUtf8("\n【运动】\n");
    auto exercises = dm->getExercises(today);
    int totalExMins = dm->getTotalExerciseMinutes(today);
    if (!exercises.isEmpty()) {
        for (const auto &ex : exercises) {
            report += QString::fromUtf8("  %1: %2分钟\n")
                          .arg(ex.exerciseType).arg(ex.durationMinutes);
        }
        report += QString::fromUtf8("  总时长: %1分钟\n").arg(totalExMins);
    } else {
        report += QString::fromUtf8("  暂无数据\n");
    }

    report += QString::fromUtf8("\n【习惯打卡】\n");
    if (!habits.isEmpty()) {
        for (const auto &h : habits) {
            double prog = dm->getHabitProgress(h.id, today);
            bool done = dm->isHabitCompletedForDate(h.id, today);
            QString progStr;
            if (h.completionMode == "hours")
                progStr = QString::fromUtf8("%1/%2h").arg(prog, 0, 'f', 1).arg(h.targetHours, 0, 'f', 1);
            else if (h.completionMode == "duration")
                progStr = QString::fromUtf8("%1/%2min").arg(static_cast<int>(prog)).arg(h.targetMinutes);
            else
                progStr = QString::fromUtf8("%1/%2次").arg(static_cast<int>(prog)).arg(h.targetCount);
            report += QString::fromUtf8("  %1  %2  %3\n")
                          .arg(done ? QString::fromUtf8("✅") : QString::fromUtf8("  "))
                          .arg(h.name, progStr);
        }
    } else {
        report += QString::fromUtf8("  暂无数据\n");
    }

    report += QString::fromUtf8("\n【计划进度】\n");
    report += QString::fromUtf8("  总体完成率: %1%\n").arg(dm->planProgress(), 0, 'f', 0);

    report += QString::fromUtf8("\n══════════════════════════");

    if (m_reportLabel) {
        m_reportLabel->setText(report);
    }

    // 同时自动填充复盘编辑器
    if (m_reviewEditor) {
        m_reviewEditor->setPlainText(report);
    }

    // 切换到复盘页面以展示报告
    ui->stackedWidget->setCurrentIndex(5);
}

// ============================================================
//  页面初始化
// ============================================================

// 创建 6 个业务子页面并添加到 QStackedWidget（index 1~6）
void CenterPanel::setupBusinessPages()
{
    m_timelineViewPage  = buildTimelineViewPage();  // 页面索引 1
    m_studyPlanPage     = buildStudyPlanPage();      // 页面索引 2
    m_completedPage     = buildCompletedPage();      // 页面索引 3
    m_skillsPage        = buildSkillsPage();         // 页面索引 4
    m_reviewPage        = buildReviewPage();         // 页面索引 5
    m_achievementPage   = buildAchievementPage();    // 页面索引 6

    ui->stackedWidget->addWidget(m_timelineViewPage);
    ui->stackedWidget->addWidget(m_studyPlanPage);
    ui->stackedWidget->addWidget(m_completedPage);
    ui->stackedWidget->addWidget(m_skillsPage);
    ui->stackedWidget->addWidget(m_reviewPage);
    ui->stackedWidget->addWidget(m_achievementPage);
}
