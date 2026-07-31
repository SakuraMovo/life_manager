// ============================================================
// MainWindow — 主窗口
// 负责组装三栏布局（左面板 + 中央面板 + 右面板），
// 使用 QSplitter 实现可拖拽的分割线
// ============================================================
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "leftpanel.h"
#include "centerpanel.h"
#include "rightpanel.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    // 接收左侧导航栏的页面切换信号，转发给中央面板
    void onNavigationChanged(int index);

private:
    // 建立信号槽连接
    void setupConnections();

    Ui::MainWindow *ui;
    LeftPanel   *m_leftPanel;    // 左侧面板：用户信息 + 导航
    CenterPanel *m_centerPanel;  // 中央面板：QStackedWidget 多页面切换
    RightPanel  *m_rightPanel;   // 右侧面板：时间/天气/状态信息
};

#endif // MAINWINDOW_H
