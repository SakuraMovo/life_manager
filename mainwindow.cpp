// ============================================================
// MainWindow 实现
// ============================================================

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "datamanager.h"
#include <QHBoxLayout>
#include <QSplitter>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 窗口基础属性
    setWindowTitle("个人生活综合管理");
    resize(1400, 900);
    setMinimumSize(1100, 700);

    // 创建三个面板组件
    m_leftPanel   = new LeftPanel(this);
    m_centerPanel = new CenterPanel(this);
    m_rightPanel  = new RightPanel(this);

    // 设置面板宽度策略：左右固定宽度，中间自适应拉伸
    m_leftPanel->setMinimumWidth(220);
    m_leftPanel->setMaximumWidth(260);

    m_centerPanel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    m_rightPanel->setMinimumWidth(260);
    m_rightPanel->setMaximumWidth(320);

    // 使用 QSplitter 组装三栏布局，支持拖拽调整宽度
    QSplitter *splitter = new QSplitter(Qt::Horizontal, ui->centralwidget);
    splitter->setHandleWidth(1);               // 分割线宽度 1px
    splitter->addWidget(m_leftPanel);
    splitter->addWidget(m_centerPanel);
    splitter->addWidget(m_rightPanel);

    // 初始宽度分配：左 230px / 中 880px / 右 290px
    splitter->setSizes({230, 880, 290});
    splitter->setChildrenCollapsible(false);   // 不允许将面板折叠为 0

    // 将 splitter 放入主窗口布局
    QHBoxLayout *mainLayout = new QHBoxLayout(ui->centralwidget);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(splitter);

    setupConnections();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupConnections()
{
    // 左侧导航栏 → 业务页面切换
    connect(m_leftPanel, &LeftPanel::navigationChanged,
            this, &MainWindow::onNavigationChanged);
    // 首页按钮（特殊处理，index = -1）
    connect(m_leftPanel, &LeftPanel::goHome, this, [this]() {
        m_centerPanel->switchToPage(-1);
    });
    // 数据变更 → 刷新左侧面板用户信息和最近完成
    connect(DataManager::instance(), &DataManager::dataChanged, this, [this]() {
        m_leftPanel->loadUserProfile();
        m_leftPanel->refreshRecentActivity();
    });
}

void MainWindow::onNavigationChanged(int index)
{
    m_centerPanel->switchToPage(index);
}
