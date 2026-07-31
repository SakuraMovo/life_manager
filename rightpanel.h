// ============================================================
// RightPanel — 右侧状态面板
// 显示：时间/日期、综合评分、体力状态、心情、健康数据、
//       睡眠详情等模块，每秒刷新时钟，数据变更时刷新健康指标
// ============================================================
#ifndef RIGHTPANEL_H
#define RIGHTPANEL_H

#include <QWidget>
#include <QTimer>
#include <QLabel>
#include <QProgressBar>
#include <QComboBox>

namespace Ui {
class RightPanel;
}

class RightPanel : public QWidget
{
    Q_OBJECT

public:
    explicit RightPanel(QWidget *parent = nullptr);
    ~RightPanel();

    void refreshAll();  // 刷新所有数据指标

private:
    // 各模块初始化
    void setupTimeModule();    // 时间显示
    void setupStatusModule();  // 当日状态
    void setupEnergyModule();  // 体力状态
    void setupMoodModule();    // 心情选择
    void setupHealthModule();  // 饮水/锻炼/睡眠摘要
    void setupSleepModule();   // 睡眠详情
    void setupScoreModule();   // 综合评分

    Ui::RightPanel *ui;
    QTimer *m_clockTimer;  // 每 1 秒刷新的时钟定时器

    // -- 体力模块 --
    QLabel *m_energyValueLabel;   // 体力值显示
    QProgressBar *m_energyBar;    // 体力进度条

    // -- 健康数据 --
    QLabel *m_waterHealthLabel;     // 饮水状态
    QLabel *m_exerciseHealthLabel;  // 运动状态
    QLabel *m_sleepHealthLabel;     // 睡眠状态

    // -- 综合评分 --
    QLabel *m_scoreLabel;          // 评分数字
    QLabel *m_scoreGradeLabel;     // 评分等级文本
    QProgressBar *m_scoreBar;      // 评分进度条

    // -- 睡眠详情 --
    QLabel *m_sleepDurationLabel;  // 睡眠时长
    QLabel *m_sleepScoreLabel;     // 睡眠评分
    QLabel *m_sleepQualityLabel;   // 睡眠质量等级
};

#endif // RIGHTPANEL_H
