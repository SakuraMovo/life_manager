// ============================================================
// 导航面板实现
// ============================================================

#include "leftpanel.h"
#include "ui_leftpanel.h"
#include "datamanager.h"
#include <QFileDialog>
#include <QInputDialog>
#include <QPixmap>
#include <QMouseEvent>

LeftPanel::LeftPanel(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::LeftPanel)
{
    ui->setupUi(this);
    setObjectName("leftPanel");

    // 头像区域：安装事件过滤器，鼠标点击时触发换头像
    ui->avatarLabel->installEventFilter(this);
    ui->avatarLabel->setCursor(Qt::PointingHandCursor);

    // 用户名：点击可修改
    ui->usernameLabel->installEventFilter(this);
    ui->usernameLabel->setCursor(Qt::PointingHandCursor);
    ui->usernameLabel->setToolTip(QString::fromUtf8("点击修改用户名"));

    // 签名：点击可修改
    ui->bioLabel->installEventFilter(this);
    ui->bioLabel->setCursor(Qt::PointingHandCursor);
    ui->bioLabel->setToolTip(QString::fromUtf8("点击修改签名"));

    setupNavButtons();
    loadUserProfile();
}

LeftPanel::~LeftPanel()
{
    delete ui;
}

// ----------------------------------------------------------------
// 用户信息加载
// ----------------------------------------------------------------

void LeftPanel::loadUserProfile()
{
    auto p = DataManager::instance()->getUserProfile();
    ui->usernameLabel->setText(p.username);
    ui->bioLabel->setText(p.bio);
    // 如果有自定义头像则加载
    if (!p.avatarPath.isEmpty()) {
        QPixmap pm(p.avatarPath);
        if (!pm.isNull())
            ui->avatarLabel->setPixmap(pm.scaled(70, 70, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    }
    refreshLevelDisplay();
}

void LeftPanel::refreshRecentActivity()
{
    if (!m_recentActivityLabel) return;

    auto records = DataManager::instance()->getCompletedRecords();
    // 显示最近 5 条完成记录
    QStringList lines;
    int count = qMin(5, records.size());
    for (int i = 0; i < count; ++i) {
        const auto &r = records[i];
        QString line = QString::fromUtf8("  %1  +%2 XP🔥\n    %3")
            .arg(r.planTitle).arg(r.xp)
            .arg(r.completedDate.toString("MM-dd"));
        lines.append(line);
    }

    if (lines.isEmpty()) {
        m_recentActivityLabel->setText(QString::fromUtf8("  暂无完成记录\n  开始你的第一个任务吧! 🚀"));
    } else {
        m_recentActivityLabel->setText(lines.join("\n"));
    }
}

void LeftPanel::refreshLevelDisplay()
{
    auto p = DataManager::instance()->getUserProfile();
    QString levelText = QString::fromUtf8("Lv.%1  ⭐%2/%3")
        .arg(p.level).arg(p.xpInCurrentLevel()).arg(p.xpToNextLevel());
    ui->bioLabel->setToolTip(QString::fromUtf8("Lv.%1 | %2 XP (点击修改签名)").arg(p.level).arg(p.totalXp));
    // 在导航标题区域显示等级
    ui->navTitleLabel->setText(QString::fromUtf8("  %1").arg(levelText));
}

// ----------------------------------------------------------------
// 导航按钮初始化
// ----------------------------------------------------------------

void LeftPanel::setupNavButtons()
{
    // 导航项定义：名称、图标路径、高亮颜色
    struct NavItem {
        QString text;
        QString iconPath;
        QString accentColor;
    };

    QList<NavItem> navItems = {
        {" 首页",         ":/resources/style/home.png",        "#ffffff"},
        {" 时间线",       ":/resources/style/timetable_view.png", "#8eb8f0"},
        {" 任务库",       ":/resources/style/study_plan.png",     "#80cbc4"},
        {" 已完成记录",   ":/resources/style/completed.png",      "#81c784"},
        {" 技能表",       ":/resources/style/skills.png",         "#ce93d8"},
        {" 每日复盘",     ":/resources/style/review.png",         "#ffab91"},
        {" 成就殿堂",     ":/resources/style/achievement.png",    "#f5cd5c"}
    };

    QVBoxLayout *navLayout = qobject_cast<QVBoxLayout*>(ui->navContainer->layout());

    // 导航按钮基础样式
    const QString baseStyle = R"(
        QPushButton {
            text-align: left;
            padding: 10px 14px;
            padding-left: 12px;
            border: 1px solid transparent;
            border-radius: 10px;
            font-size: 13px;
            font-weight: 500;
            color: #9e9ab8;
            background: transparent;
            margin: 1px 0px;
        }
        QPushButton:hover {
            background: rgba(255,255,255,0.06);
            color: #e2e0f0;
            border: 1px solid rgba(255,255,255,0.10);
        }
    )";

    for (int i = 0; i < navItems.size(); ++i) {
        const NavItem &item = navItems[i];

        QPushButton *btn = new QPushButton(this);
        btn->setObjectName(QString("navBtn_%1").arg(i));
        btn->setToolTip(item.text);
        btn->setMinimumHeight(46);
        btn->setCursor(Qt::PointingHandCursor);
        btn->setCheckable(true);                          // 可选中状态
        btn->setIcon(QIcon(item.iconPath));
        btn->setIconSize(QSize(20, 20));
        btn->setText(item.text);

        // 按钮激活样式（选中时显示对应颜色）
        QString activeStyle;
        if (i == 0) {
            // 首页使用白色渐变高亮
            activeStyle = R"(
                QPushButton:checked {
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                        stop:0 rgba(255,255,255,0.10), stop:1 rgba(255,255,255,0.05));
                    color: #e2e0f0;
                    font-weight: 600;
                    border: 1px solid rgba(255,255,255,0.18);
                }
            )";
        } else {
            // 业务页使用各自的主题色
            activeStyle = QString(R"(
                QPushButton:checked {
                    background: qlineargradient(x1:0, y1:0, x2:1, y2:0,
                        stop:0 %1, stop:1 rgba(255,255,255,0.04));
                    color: %2;
                    font-weight: 600;
                    border: 1px solid %3;
                }
            )").arg(item.accentColor + "18", item.accentColor, item.accentColor + "35");
        }

        btn->setStyleSheet(baseStyle + activeStyle);

        // 首页按钮 → 发射 goHome() 信号
        if (i == 0) {
            connect(btn, &QPushButton::clicked, this, [this]() {
                if (m_activeIndex != 0) {
                    m_navButtons[m_activeIndex]->setChecked(false);
                    m_navButtons[0]->setChecked(true);
                    m_activeIndex = 0;
                }
                emit goHome();
            });
        } else {
            // 业务页按钮 → 发射 navigationChanged(idx)
            int navIdx = i - 1; // 跳过首页，从 0 开始
            connect(btn, &QPushButton::clicked, this, [this, navIdx]() {
                onNavButtonClicked(navIdx);
            });
        }

        navLayout->addWidget(btn);
        m_navButtons.append(btn);
    }

    navLayout->addStretch();  // 按钮靠上对齐

    // 默认选中首页
    if (!m_navButtons.isEmpty()) {
        m_navButtons[0]->setChecked(true);
    }

    // ---- 最近完成活动区域（导航下方） ----
    QVBoxLayout *navSectionLayout = qobject_cast<QVBoxLayout*>(ui->navSection->layout());
    if (navSectionLayout) {
        // 分割线
        QFrame *divider = new QFrame(this);
        divider->setFrameShape(QFrame::HLine);
        divider->setStyleSheet("QFrame { color: rgba(255,255,255,0.08); margin: 8px 12px; }");
        divider->setMaximumHeight(1);
        navSectionLayout->addWidget(divider);

        QLabel *recentTitle = new QLabel(QString::fromUtf8("  🔥 最近完成"), this);
        recentTitle->setStyleSheet("color: #6e6a88; font-size: 10px; font-weight: bold; letter-spacing: 3px;"
                                   " padding: 6px 8px; background: transparent;");
        navSectionLayout->addWidget(recentTitle);

        m_recentActivityLabel = new QLabel(this);
        m_recentActivityLabel->setWordWrap(true);
        m_recentActivityLabel->setStyleSheet(
            "QLabel { color: #9e9ab8; font-size: 11px; padding: 4px 12px; background: transparent;"
            "  line-height: 1.4; }");
        m_recentActivityLabel->setMinimumHeight(20);
        m_recentActivityLabel->setMaximumHeight(140);
        navSectionLayout->addWidget(m_recentActivityLabel);
    }

    refreshRecentActivity();
}

void LeftPanel::onNavButtonClicked(int index)
{
    // index 是业务页索引（0=时间线, 1=任务库, ...）
    // 点击同一页不做任何事
    if (index == m_activeIndex - 1 && m_activeIndex != 0)
        return;

    // 取消之前选中的按钮
    if (m_activeIndex > 0)
        m_navButtons[m_activeIndex]->setChecked(false);
    m_navButtons[0]->setChecked(false);  // 取消首页选中

    int btnIdx = index + 1;  // 跳过首页按钮
    m_navButtons[btnIdx]->setChecked(true);
    m_activeIndex = btnIdx;

    emit navigationChanged(index);
}

// ----------------------------------------------------------------
// 用户信息交互
// ----------------------------------------------------------------

void LeftPanel::onAvatarClicked()
{
    QString fn = QFileDialog::getOpenFileName(this, QString::fromUtf8("选择头像"),
        QString(), "Images (*.png *.jpg *.jpeg *.bmp *.gif)");
    if (!fn.isEmpty()) {
        QPixmap pm(fn);
        if (!pm.isNull()) {
            ui->avatarLabel->setPixmap(pm.scaled(70, 70, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            auto p = DataManager::instance()->getUserProfile();
            p.avatarPath = fn;
            DataManager::instance()->saveUserProfile(p);
        }
    }
}

void LeftPanel::onUsernameClicked()
{
    auto p = DataManager::instance()->getUserProfile();
    bool ok;
    QString name = QInputDialog::getText(this, QString::fromUtf8("修改用户名"),
        QString::fromUtf8("新用户名:"), QLineEdit::Normal, p.username, &ok);
    if (ok && !name.trimmed().isEmpty()) {
        p.username = name.trimmed();
        DataManager::instance()->saveUserProfile(p);
        loadUserProfile();
    }
}

void LeftPanel::onBioClicked()
{
    auto p = DataManager::instance()->getUserProfile();
    bool ok;
    QString bio = QInputDialog::getText(this, QString::fromUtf8("修改签名"),
        QString::fromUtf8("新签名:"), QLineEdit::Normal, p.bio, &ok);
    if (ok && !bio.trimmed().isEmpty()) {
        p.bio = bio.trimmed();
        DataManager::instance()->saveUserProfile(p);
        loadUserProfile();
    }
}

// ----------------------------------------------------------------
// 事件过滤器：拦截头像/用户名/签名的鼠标点击
// ----------------------------------------------------------------

bool LeftPanel::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        if (obj == ui->avatarLabel) {
            onAvatarClicked();
            return true;
        }
        if (obj == ui->usernameLabel) {
            onUsernameClicked();
            return true;
        }
        if (obj == ui->bioLabel) {
            onBioClicked();
            return true;
        }
    }
    return QWidget::eventFilter(obj, event);
}
