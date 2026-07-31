// ============================================================
// DataManager — 全局数据管理单例
// 负责所有数据的增删改查、JSON 持久化、经验结算、成就统计
// 数据文件：%AppData%/LifeManager/lifedata.json
// ============================================================
#ifndef DATAMANAGER_H
#define DATAMANAGER_H

#include <QObject>
#include <QString>
#include <QDate>
#include <QTime>
#include <QJsonObject>
#include <QJsonArray>
#include <QMap>
#include <QtMath>

// ---- 数据结构定义 ----

// 睡眠记录：入睡时间 + 醒来时间
struct SleepRecord {
    QDate date;
    QTime sleepTime;
    QTime wakeTime;
    double durationHours() const;  // 计算睡眠时长（小时）
};

// 饮水记录：目标量 + 当前量
struct WaterRecord {
    QDate date;
    int targetMl = 2000;      // 目标饮水（毫升）
    int currentMl = 0;        // 当前已饮（毫升）
    double percent() const;   // 完成百分比
};

// 饮食记录：餐次 + 食物名
struct DietRecord {
    QDate date;
    QTime time;
    QString mealType;  // 早餐/午餐/晚餐/加餐
    QString foodName;
};

// 运动记录：项目 + 时长（分钟）
struct ExerciseRecord {
    QDate date;
    int durationMinutes = 0;   // 运动时长（分钟）
    QString exerciseType;       // 运动项目（如跑步、游泳、健身）
};

// 技能：名称 + 计划总时长 + 已投入时长
struct SkillItem {
    int id = 0;
    QString name;
    double plannedHours = 0;     // 计划总时长（小时）
    double investedHours = 0;    // 已投入时长（小时）
    double progressPercent() const;  // 完成百分比 invested / planned * 100
    bool isCompleted() const;        // 是否达成：invested >= planned && planned > 0
};

// 任务模板（任务库）：名称 + 计划时长 + 经验结算规则
struct PlanItem {
    int id = 0;
    int skillId = -1;               // 关联技能ID，-1=未关联
    QString title;                  // 任务名称
    double plannedHours = 1;        // 计划总时长（小时）
    double xpBaseHours = 1;         // 经验结算单位：每多少小时
    int xp = 10;                    // 每 xpBaseHours 小时获得的经验值
    int stars = 1;                  // 难度星级 1-5（旧版兼容字段）
    QString status = "not_started"; // 状态：not_started / in_progress / completed
    QDate createdDate;              // 创建日期
    QDate completedDate;            // 完成日期
    bool isCompleted() const { return status == "completed"; }
    // 按实际时长比例结算经验：hours / xpBaseHours * xp
    int xpForHours(double hours) const {
        if (xpBaseHours <= 0) return 0;
        return static_cast<int>(hours / xpBaseHours * xp);
    }
    // 完成整个计划可获得的总经验
    int xpValue() const { return xpForHours(plannedHours); }
};

// 日程安排：将任务模板安排到具体日期和时间段
struct ScheduleItem {
    int id = 0;
    int planId;                // 关联的 PlanItem ID
    QDate date;                // 安排日期
    QTime startTime;           // 开始时间
    double plannedHours;       // 本次安排的时长
    QTime endTime() const;     // 自动计算结束时间：startTime + plannedHours
    bool completed = false;    // 是否已完成
};

// 用户档案：等级/经验 + 昵称/签名/头像
struct UserProfile {
    QString username = QString::fromUtf8("用户名");
    QString bio = QString::fromUtf8("自律即自由");
    QString avatarPath;
    int level = 1;             // 当前等级
    int totalXp = 0;           // 累计总经验
    int xpToNextLevel() const { return level * 100; }   // 升级所需经验
    int xpInCurrentLevel() const { return totalXp % 100; } // 当前等级内已有经验
};

// 完成记录：记录每次完成日程的详细信息
struct CompletedRecord {
    QDate completedDate;       // 完成日期
    QDate scheduleDate;        // 日程所在日期
    QString planTitle;         // 任务名称
    QString skillName;         // 关联技能名
    double plannedHours = 0;   // 时长
    int xp = 0;                // 获得经验
};

// 每日复盘条目
struct ReviewEntry {
    QDate date;
    QString content;
};

// 习惯养成项：支持每天/每周/每月模式，次数/小时/时长三种完成方式
struct HabitItem {
    int id = 0;
    QString name;                          // 习惯名称
    QString habitMode = "daily";           // 周期模式：daily / weekly / monthly
    QString completionMode = "count";      // 完成方式：count(次数) / hours(小时) / duration(分钟)
    double targetHours = 1;                // 目标（小时模式）
    int targetCount = 1;                   // 目标（次数模式）
    int targetMinutes = 30;                // 目标（时长模式，分钟）
    int stars = 1;                         // 难度星级 1-5
    QDate createdDate;
    QString targetLabel() const;           // 获取目标的展示文本
    QString periodKey(const QDate &date) const;  // 获取指定日期所属周期的标识键
    int xpValue() const { return static_cast<int>(10.0 * qPow(stars, 1.5)); }
};

// ============================================================
// DataManager — 单例数据管理器
// ============================================================
class DataManager : public QObject
{
    Q_OBJECT

public:
    // 获取全局唯一实例
    static DataManager* instance();

    // -- 睡眠 --
    SleepRecord getSleep(const QDate &date) const;
    void saveSleep(const SleepRecord &record);

    // -- 饮水 --
    WaterRecord getWater(const QDate &date) const;
    void saveWater(const WaterRecord &record);

    // -- 饮食 --
    QList<DietRecord> getDiet(const QDate &date) const;
    void saveDiet(const QDate &date, const QList<DietRecord> &records);

    // -- 运动 --
    ExerciseRecord getExercise(const QDate &date) const;         // 兼容旧接口（首条记录）
    QList<ExerciseRecord> getExercises(const QDate &date) const; // 全部记录
    int getTotalExerciseMinutes(const QDate &date) const;        // 当日总运动分钟
    void saveExercise(const ExerciseRecord &record);             // 追加一条记录
    void removeExercise(const QDate &date, int index);           // 按索引删除

    // -- 用户档案 --
    UserProfile getUserProfile() const;
    void saveUserProfile(const UserProfile &profile);
    void addXp(int amount);  // 增加经验，自动处理升级

    // -- 技能 --
    QList<SkillItem> getSkills() const;
    SkillItem getSkill(int id) const;
    void saveSkill(const SkillItem &skill);        // 添加或更新
    void removeSkill(int id);
    void addInvestedHours(int skillId, double hours);  // 累加已投入时长

    // -- 任务模板（任务库） --
    QList<PlanItem> getPlanItems(bool includeCompleted = true) const;
    QList<PlanItem> getPlansBySkill(int skillId) const;  // 按技能筛选
    PlanItem getPlanItem(int id) const;
    void savePlanItem(const PlanItem &item);              // 新增
    void updatePlanItem(const PlanItem &item);            // 更新
    void removePlanItem(int id);                          // 删除
    void setPlanStatus(int id, const QString &status, bool awardHours = false);  // 设置状态并结算经验
    double planProgress() const;                          // 总完成率

    // -- 日程安排（时间线） --
    QList<ScheduleItem> getSchedules(const QDate &date) const;
    void saveSchedule(const ScheduleItem &item);
    void removeSchedule(int id);
    void markScheduleComplete(int id);  // 完成日程，结算经验+技能时长+完成记录

    // -- 完成记录 --
    QList<CompletedRecord> getCompletedRecords() const;

    // -- 数据重置 --
    void resetAllData();

    // -- 成就 & 连续天数 --
    int getConsecutiveDays() const;     // 连续完成任务的天数
    int getTotalCompletedTasks() const; // 累计完成任务总数

    // -- 每日复盘 --
    QList<ReviewEntry> getReviews() const;
    void saveReview(const QString &content);            // 保存复盘内容（JSON + Markdown文件）
    void deleteReview(const QDate &date);               // 删除指定日期的复盘
    QString reviewsDir() const;                         // 复盘文件目录
    QString reviewFilePath(const QDate &date) const;    // 指定日期的复盘文件路径

    // -- 习惯养成 --
    void extracted(QList<HabitItem> &result, QJsonArray &arr) const;
    QList<HabitItem> getHabits() const;
    HabitItem getHabit(int id) const;
    void saveHabit(const HabitItem &item);
    void removeHabit(int id);
    double getHabitProgress(int habitId, const QDate &date) const;        // 获取当日进度
    void setHabitProgress(int habitId, const QDate &date, double progress); // 设置当日进度
    bool isHabitCompletedForDate(int habitId, const QDate &date) const;   // 是否已完成
    QList<HabitItem> getActiveHabits(const QDate &date) const;            // 获取当日活跃习惯

    // -- 旧版任务列表（向后兼容） --
    struct TaskItem {
        int id = 0;
        QDate date;
        QString time;
        QString content;
        bool completed = false;
    };
    QList<TaskItem> getTasks(const QDate &date) const;
    void saveTask(const TaskItem &task);
    void removeTask(int id);
    int tasksCompleted(const QDate &date) const;   // 当日已完成数
    int tasksTotal(const QDate &date) const;        // 当日总任务数
    double taskProgress(const QDate &date) const;   // 当日完成率

signals:
    // 任何数据变更时发射，UI 层监听并刷新
    void dataChanged();

private:
    explicit DataManager(QObject *parent = nullptr);
    void load();                          // 从 JSON 文件加载数据
    void save() const;                    // 保存数据到 JSON 文件
    int nextId(const QString &key);       // 生成自增 ID

    static DataManager *s_instance;       // 单例指针
    QString m_dataPath;                   // 数据文件路径
    QJsonObject m_data;                   // 内存中的数据
};

#endif // DATAMANAGER_H
