// ============================================================
// LeftPanel — 左侧导航面板
// 负责：用户头像/用户名/签名展示、等级显示、
//       7 个导航按钮（首页+6个业务页）、最近完成记录
// ============================================================
#ifndef LEFTPANEL_H
#define LEFTPANEL_H

#include <QWidget>
#include <QPushButton>
#include <QLabel>

namespace Ui {
class LeftPanel;
}

class LeftPanel : public QWidget
{
    Q_OBJECT

public:
    explicit LeftPanel(QWidget *parent = nullptr);
    ~LeftPanel();

    // 从 DataManager 加载用户信息到 UI
    void loadUserProfile();
    // 刷新最近完成记录列表
    void refreshRecentActivity();

protected:
    // 事件过滤器：处理头像/用户名/签名的点击事件
    bool eventFilter(QObject *obj, QEvent *event) override;

signals:
    // 业务页面切换信号，index 对应 0=时间线, 1=任务库, ...
    void navigationChanged(int index);
    // 返回首页信号
    void goHome();

private slots:
    // 导航按钮被点击（非首页）
    void onNavButtonClicked(int index);
    // 点击头像 → 选择新头像文件
    void onAvatarClicked();
    // 点击用户名 → 弹窗修改
    void onUsernameClicked();
    // 点击签名 → 弹窗修改
    void onBioClicked();

private:
    // 初始化 7 个导航按钮
    void setupNavButtons();
    // 刷新等级显示
    void refreshLevelDisplay();

    Ui::LeftPanel *ui;
    QList<QPushButton*> m_navButtons;   // 所有导航按钮（含首页）
    int m_activeIndex = 0;              // 当前激活的按钮索引
    QLabel *m_recentActivityLabel = nullptr;  // 最近完成记录标签
};

#endif // LEFTPANEL_H
