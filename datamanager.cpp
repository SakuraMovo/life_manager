// ============================================================
// DataManager 实现 — 全局数据管理单例
// 所有数据的 JSON 持久化存储于 %AppData%/LifeManager/lifedata.json
// 各模块按顺序：睡眠/饮水/饮食 → 运动 → 用户档案 → 技能 →
//              任务模板 → 日程 → 完成记录 → 复盘 → 习惯 → 旧版任务
// ============================================================

#include "datamanager.h"
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <QStandardPaths>
#include <QTextStream>
#include <QDebug>
#include <QSet>
#include <algorithm>

/* ============================================================================
 * 模块说明：DataManager — 全局数据管理单例
 *
 * 【核心职责】
 *   1. 统一管理所有用户数据的读写（CRUD操作）
 *   2. 使用JSON格式持久化存储到本地文件
 *   3. 通过信号-槽机制通知界面数据变化
 *   4. 提供数据迁移和兼容性处理
 *
 * 【存储架构】
 *   数据文件位置：%AppData%/LifeManager/lifedata.json
 *   存储结构：单个JSON对象，包含多个顶级键
 *
 *   {
 *     "profile": {...},           // 用户档案
 *     "skills": [...],            // 技能列表
 *     "plans": [...],             // 任务模板
 *     "schedules_20260730": [...],// 日程（按日期分键）
 *     "sleep_20260730": {...},    // 睡眠（按日期分键）
 *     "water_20260730": {...},    // 饮水（按日期分键）
 *     "diet_20260730": [...],     // 饮食（按日期分键）
 *     "exercise_20260730": [...], // 运动（按日期分键）
 *     "habits": [...],            // 习惯列表
 *     "habitprog_1_20260730": {...}, // 习惯进度（按习惯ID+周期）
 *     "completed_records": [...], // 完成记录
 *     "reviews": [...],           // 复盘记录
 *     "nextSkillId": 5,           // ID自增计数器
 *     "nextPlanId": 12,
 *     "nextScheduleId": 45,
 *     "nextHabitId": 8
 *   }
 *
 * 【设计模式】
 *   - 单例模式：全局唯一实例，所有模块共享
 *   - 观察者模式：dataChanged信号通知所有界面刷新
 *   - 工厂模式：从JSON反序列化创建对象
 *
 * 【数据流向】
 *   界面操作 → DataManager方法 → 修改内存JSON → save()持久化 → emit dataChanged() → 界面刷新
 * ============================================================================ */

// ============================================================
//  单例实现
// ============================================================

DataManager* DataManager::s_instance = nullptr;

/**
 * @brief 获取DataManager单例实例
 * @return 全局唯一的DataManager实例指针
 *
 * 【线程安全】此实现不是线程安全的，仅在主线程中使用
 * 【延迟初始化】首次调用时创建实例，之后复用
 */
DataManager* DataManager::instance()
{
    if (!s_instance) {
        s_instance = new DataManager();
    }
    return s_instance;
}

/**
 * @brief 构造函数 — 初始化数据存储路径并加载数据
 * @param parent 父对象（用于Qt内存管理）
 *
 * 【初始化流程】
 *   1. 获取应用数据目录（跨平台路径）
 *      - Windows: C:/Users/用户名/AppData/Roaming/LifeManager
 *      - macOS: ~/Library/Application Support/LifeManager
 *      - Linux: ~/.local/share/LifeManager
 *   2. 创建目录（如果不存在）
 *   3. 构建数据文件路径
 *   4. 加载现有数据（如果文件存在）
 */
DataManager::DataManager(QObject *parent)
    : QObject(parent)
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);                             // 创建目录（会自动创建所有父目录）
    m_dataPath = dir + "/lifedata.json";
    load();
}

/**
 * @brief 从磁盘加载数据
 *
 * 【加载流程】
 *   1. 尝试打开数据文件
 *   2. 如果文件存在，读取全部内容
 *   3. 解析JSON文档为QJsonObject
 *   4. 如果文件不存在，m_data保持空对象（后续save时会创建）
 *
 * 【错误处理】
 *   - 文件不存在：静默处理，m_data为空
 *   - JSON格式错误：QJsonDocument会返回空对象，不抛出异常
 */
void DataManager::load()
{
    QFile file(m_dataPath);
    if (file.open(QIODevice::ReadOnly)) {
        m_data = QJsonDocument::fromJson(file.readAll()).object();
        file.close();
    }
}

/**
 * @brief 保存数据到磁盘
 *
 * 【保存流程】
 *   1. 将QJsonObject转换为JSON文档
 *   2. 以写入模式打开文件
 *   3. 将JSON数据格式化为带缩进的文本（便于人类阅读和调试）
 *   4. 写入文件并关闭
 *
 * 【性能考虑】
 *   - 每次save()都会完整写入整个文件
 *   - 对于大量数据，可以考虑增量保存或异步写入
 *   - 当前设计适用于个人数据规模（通常<1MB）
 */
void DataManager::save() const
{
    QFile file(m_dataPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(m_data).toJson());  // 使用默认格式（紧凑）
        file.close();
    }
}

/**
 * @brief 生成自增ID
 * @param key 计数器键名（如 "nextSkillId"）
 * @return 新的唯一ID
 *
 * 【实现原理】
 *   1. 从m_data中读取当前计数器值（默认为1）
 *   2. 将计数器值加1并写回
 *   3. 返回原始值作为新ID
 *
 * 【为什么不用UUID？】
 *   - 整数ID更短、可读性强
 *   - 便于调试和人工查询
 *   - 适合个人应用的规模
 *
 * 【注意】
 *   此方法会修改m_data但不会自动调用save()
 *   调用者需要负责保存数据
 */
int DataManager::nextId(const QString &key)
{
    int id = m_data[key].toInt(1);
    m_data[key] = id + 1;
    return id;
}

// ============================================================
//  睡眠数据模块
// ============================================================

/**
 * @brief 计算睡眠时长（小时）
 * @return 睡眠总小时数
 *
 * 【计算逻辑】
 *   1. 计算入睡时间到醒来时间的秒数差
 *   2. 如果跨天（入睡在晚上，醒来在早上），增加24小时
 *   3. 转换为小时数（保留小数）
 *
 * 【示例】
 *   23:00 → 07:00 = 8小时
 *   02:00 → 01:00（次日）= 23小时（跨天睡眠）
 */
double SleepRecord::durationHours() const
{
    int secs = sleepTime.secsTo(wakeTime);
    if (secs < 0) secs += 86400;                    // 跨天处理（24小时=86400秒）
    return secs / 3600.0;
}

/**
 * @brief 获取指定日期的睡眠记录
 * @param date 日期
 * @return SleepRecord 睡眠记录对象（如果不存在则返回空记录）
 *
 * 【数据格式】
 *   sleep_20260730: { "sleep": "23:00", "wake": "07:00" }
 */
SleepRecord DataManager::getSleep(const QDate &date) const
{
    SleepRecord r;
    r.date = date;
    QString key = QString("sleep_%1").arg(date.toString("yyyyMMdd"));
    if (m_data.contains(key)) {
        QJsonObject o = m_data[key].toObject();
        r.sleepTime = QTime::fromString(o["sleep"].toString(), "HH:mm");
        r.wakeTime  = QTime::fromString(o["wake"].toString(), "HH:mm");
    }
    return r;
}

/**
 * @brief 保存睡眠记录
 * @param record 睡眠记录对象
 *
 * 【保存流程】
 *   1. 按日期生成键名（如 sleep_20260730）
 *   2. 构建JSON对象
 *   3. 存入m_data
 *   4. 持久化到磁盘
 *   5. 触发dataChanged信号通知界面更新
 */
void DataManager::saveSleep(const SleepRecord &record)
{
    QString key = QString("sleep_%1").arg(record.date.toString("yyyyMMdd"));
    QJsonObject o;
    o["sleep"] = record.sleepTime.toString("HH:mm");
    o["wake"]  = record.wakeTime.toString("HH:mm");
    m_data[key] = o;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

// ============================================================
//  饮水数据模块
// ============================================================

/**
 * @brief 计算饮水完成百分比
 * @return 当前饮水量占目标量的百分比
 */
double WaterRecord::percent() const
{
    return targetMl > 0 ? (currentMl * 100.0 / targetMl) : 0;
}

/**
 * @brief 获取指定日期的饮水记录
 * @param date 日期
 * @return WaterRecord 饮水记录（默认目标2000ml）
 *
 * 【数据格式】
 *   water_20260730: { "target": 2000, "current": 1500 }
 */
WaterRecord DataManager::getWater(const QDate &date) const
{
    WaterRecord r;
    r.date = date;
    QString key = QString("water_%1").arg(date.toString("yyyyMMdd"));
    if (m_data.contains(key)) {
        QJsonObject o = m_data[key].toObject();
        r.targetMl  = o["target"].toInt(2000);
        r.currentMl = o["current"].toInt(0);
    }
    return r;
}

/**
 * @brief 保存饮水记录
 * @param record 饮水记录对象
 */
void DataManager::saveWater(const WaterRecord &record)
{
    QString key = QString("water_%1").arg(record.date.toString("yyyyMMdd"));
    QJsonObject o;
    o["target"]  = record.targetMl;
    o["current"] = record.currentMl;
    m_data[key] = o;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

// ============================================================
//  饮食数据模块
// ============================================================

/**
 * @brief 获取指定日期的饮食记录列表
 * @param date 日期
 * @return QList<DietRecord> 饮食记录列表
 *
 * 【数据格式】
 *   diet_20260730: [
 *     { "time": "08:00", "meal": "早餐", "food": "面包和牛奶" },
 *     { "time": "12:30", "meal": "午餐", "food": "米饭和炒菜" }
 *   ]
 */
QList<DietRecord> DataManager::getDiet(const QDate &date) const
{
    QList<DietRecord> result;
    QString key = QString("diet_%1").arg(date.toString("yyyyMMdd"));
    if (m_data.contains(key)) {
        QJsonArray arr = m_data[key].toArray();
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            DietRecord r;
            r.date     = date;
            r.time     = QTime::fromString(o["time"].toString(), "HH:mm");
            r.mealType = o["meal"].toString();
            r.foodName = o["food"].toString();
            result.append(r);
        }
    }
    return result;
}

/**
 * @brief 保存饮食记录列表
 * @param date 日期
 * @param records 饮食记录列表
 *
 * 【注意】会完全替换该日期的所有饮食记录
 *       而不是增量追加（符合UI的编辑模式）
 */
void DataManager::saveDiet(const QDate &date, const QList<DietRecord> &records)
{
    QString key = QString("diet_%1").arg(date.toString("yyyyMMdd"));
    QJsonArray arr;
    for (const auto &r : records) {
        QJsonObject o;
        o["time"] = r.time.isValid() ? r.time.toString("HH:mm") : QString();
        o["meal"] = r.mealType;
        o["food"] = r.foodName;
        arr.append(o);
    }
    m_data[key] = arr;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

// ============================================================
//  运动数据模块
// ============================================================

/**
 * @brief 获取指定日期的第一条运动记录（向后兼容）
 * @param date 日期
 * @return 第一条运动记录（如果不存在则返回空记录）
 * @deprecated 建议使用 getExercises()
 */
ExerciseRecord DataManager::getExercise(const QDate &date) const
{
    auto list = getExercises(date);
    return list.isEmpty() ? ExerciseRecord() : list.first();
}

/**
 * @brief 获取指定日期的所有运动记录
 * @param date 日期
 * @return QList<ExerciseRecord> 运动记录列表
 *
 * 【数据迁移】
 *   支持两种数据格式：
 *   1. 新格式：数组 [{ "type": "跑步", "duration": 30 }, ...]
 *   2. 旧格式：单个对象 { "type": "跑步", "duration": 30 }
 *      自动迁移到新格式
 *
 * 【数据格式】
 *   exercise_20260730: [
 *     { "type": "跑步", "duration": 30 },
 *     { "type": "游泳", "duration": 45 }
 *   ]
 */
QList<ExerciseRecord> DataManager::getExercises(const QDate &date) const
{
    QList<ExerciseRecord> result;
    QString key = QString("exercise_%1").arg(date.toString("yyyyMMdd"));
    if (m_data.contains(key)) {
        QJsonValue val = m_data[key];
        if (val.isArray()) {
            // 新格式：数组
            QJsonArray arr = val.toArray();
            for (const auto &v : arr) {
                QJsonObject o = v.toObject();
                ExerciseRecord r;
                r.date = date;
                r.durationMinutes = o["duration"].toInt(0);
                r.exerciseType    = o["type"].toString();
                result.append(r);
            }
        } else if (val.isObject()) {
            // 旧格式：单个对象 — 自动迁移
            QJsonObject o = val.toObject();
            ExerciseRecord r;
            r.date = date;
            r.durationMinutes = o["duration"].toInt(0);
            r.exerciseType    = o["type"].toString();
            if (r.durationMinutes > 0)
                result.append(r);
        }
    }
    return result;
}

/**
 * @brief 获取指定日期的总运动时长（分钟）
 * @param date 日期
 * @return 总分钟数
 */
int DataManager::getTotalExerciseMinutes(const QDate &date) const
{
    int total = 0;
    auto list = getExercises(date);
    for (const auto &r : list)
        total += r.durationMinutes;
    return total;
}

/**
 * @brief 保存运动记录（追加模式）
 * @param record 运动记录
 *
 * 【保存流程】
 *   1. 读取该日期现有的运动记录（支持旧格式迁移）
 *   2. 追加新记录到数组
 *   3. 写回数据
 *
 * 【为什么用追加而不是替换？】
 *   允许用户在同一天记录多项运动（如跑步+游泳）
 *   符合实际使用场景
 */
void DataManager::saveExercise(const ExerciseRecord &record)
{
    QString key = QString("exercise_%1").arg(record.date.toString("yyyyMMdd"));
    QJsonArray arr;

    // 读取现有记录（自动迁移旧格式）
    if (m_data.contains(key)) {
        QJsonValue val = m_data[key];
        if (val.isArray()) {
            arr = val.toArray();
        } else if (val.isObject()) {
            // 迁移旧格式
            QJsonObject old = val.toObject();
            if (old["duration"].toInt(0) > 0) {
                arr.append(old);
            }
        }
    }

    // 追加新记录
    QJsonObject o;
    o["duration"] = record.durationMinutes;
    o["type"]     = record.exerciseType;
    arr.append(o);

    m_data[key] = arr;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

/**
 * @brief 删除指定日期的运动记录（按索引）
 * @param date 日期
 * @param index 要删除的记录索引
 */
void DataManager::removeExercise(const QDate &date, int index)
{
    QString key = QString("exercise_%1").arg(date.toString("yyyyMMdd"));
    if (m_data.contains(key)) {
        QJsonValue val = m_data[key];
        if (val.isArray()) {
            QJsonArray arr = val.toArray();
            if (index >= 0 && index < arr.size()) {
                arr.removeAt(index);
                m_data[key] = arr;
                save();
                emit const_cast<DataManager*>(this)->dataChanged();
            }
        }
    }
}

// ============================================================
//  用户档案模块
// ============================================================

/**
 * @brief 获取用户档案
 * @return UserProfile 用户档案对象
 *
 * 【数据格式】
 *   profile: {
 *     "username": "张三",
 *     "bio": "自律即自由",
 *     "avatar": "/path/to/avatar.png",
 *     "level": 5,
 *     "xp": 420
 *   }
 *
 * 【默认值】
 *   如果档案不存在，返回默认用户档案
 *   - 用户名："用户名"
 *   - 签名："自律即自由"
 *   - 等级：1
 *   - 经验：0
 */
UserProfile DataManager::getUserProfile() const
{
    UserProfile p;
    QJsonObject o = m_data["profile"].toObject();
    if (!o.isEmpty()) {
        p.username   = o["username"].toString(QString::fromUtf8("用户名"));
        p.bio        = o["bio"].toString(QString::fromUtf8("自律即自由"));
        p.avatarPath = o["avatar"].toString();
        p.level      = o["level"].toInt(1);
        p.totalXp    = o["xp"].toInt(0);
        if (p.level < 1) p.level = 1;
    }
    return p;
}

/**
 * @brief 保存用户档案
 * @param profile 用户档案对象
 */
void DataManager::saveUserProfile(const UserProfile &profile)
{
    QJsonObject o;
    o["username"] = profile.username;
    o["bio"]      = profile.bio;
    o["avatar"]   = profile.avatarPath;
    o["level"]    = profile.level;
    o["xp"]       = profile.totalXp;
    m_data["profile"] = o;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

/**
 * @brief 增加经验值（自动处理升级）
 * @param amount 要增加的经验值
 *
 * 【升级公式】
 *   Lv = totalXp / 100 + 1
 *   每100经验升1级
 *
 * 【示例】
 *   totalXp=0  → Lv=1
 *   totalXp=50 → Lv=1
 *   totalXp=100 → Lv=2
 *   totalXp=250 → Lv=3
 *
 * 【设计考虑】
 *   - 采用线性升级曲线，简单明了
 *   - 每级所需经验固定为100，易于理解和规划
 *   - 未来可扩展为多级难度曲线
 */
void DataManager::addXp(int amount)
{
    UserProfile p = getUserProfile();
    p.totalXp += amount;
    p.level = p.totalXp / 100 + 1;
    saveUserProfile(p);
}

// ============================================================
//  技能数据模块
// ============================================================

/**
 * @brief 计算技能完成百分比
 * @return 已投入时间占计划时间的百分比
 */
double SkillItem::progressPercent() const
{
    if (plannedHours <= 0) return 0;
    return investedHours * 100.0 / plannedHours;
}

/**
 * @brief 判断技能是否已完成
 * @return true 如果已投入时间 >= 计划时间
 */
bool SkillItem::isCompleted() const
{
    return plannedHours > 0 && investedHours >= plannedHours;
}

/**
 * @brief 获取所有技能列表
 * @return QList<SkillItem> 技能列表
 *
 * 【数据格式】
 *   skills: [
 *     { "id": 1, "name": "C++", "plannedHours": 100, "investedHours": 35.5 },
 *     { "id": 2, "name": "英语", "plannedHours": 50, "investedHours": 20 }
 *   ]
 *
 * 【数据迁移】
 *   - 旧版本使用 "level" 字段（0-100的任意值）
 *   - 自动迁移为 plannedHours=100, investedHours=level
 */
QList<SkillItem> DataManager::getSkills() const
{
    QList<SkillItem> result;
    QJsonArray arr = m_data["skills"].toArray();
    for (const auto &v : arr) {
        QJsonObject o = v.toObject();
        SkillItem s;
        s.id            = o["id"].toInt();
        s.name          = o["name"].toString();
        s.plannedHours  = o["plannedHours"].toDouble();
        s.investedHours = o["investedHours"].toDouble();

        // 如果没有id字段，自动生成（向后兼容）
        if (s.id <= 0) {
            s.id = result.size() + 1;
        }
        // 迁移旧版"level"数据
        if (s.plannedHours <= 0 && o.contains("level")) {
            s.plannedHours = 100;                    // 默认计划100小时
            s.investedHours = o["level"].toDouble(); // 旧版level作为已投入时间
        }
        result.append(s);
    }
    return result;
}

/**
 * @brief 根据ID获取技能
 * @param id 技能ID
 * @return SkillItem 技能对象（如果不存在则返回空对象）
 */
SkillItem DataManager::getSkill(int id) const
{
    auto skills = getSkills();
    for (const auto &s : skills) {
        if (s.id == id) return s;
    }
    return SkillItem();
}

/**
 * @brief 保存技能（新增或更新）
 * @param skill 技能对象
 *
 * 【保存逻辑】
 *   1. 如果id<=0，分配新ID
 *   2. 查找是否已存在相同ID的技能
 *   3. 如果存在，更新；否则追加
 */
void DataManager::saveSkill(const SkillItem &skill)
{
    QJsonArray arr = m_data["skills"].toArray();

    SkillItem s = skill;
    if (s.id <= 0) {
        s.id = nextId("nextSkillId");
    }

    // 更新已存在或追加
    bool found = false;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject o = arr[i].toObject();
        if (o["id"].toInt() == s.id) {
            o["id"]            = s.id;
            o["name"]          = s.name;
            o["plannedHours"]  = s.plannedHours;
            o["investedHours"] = s.investedHours;
            arr[i] = o;
            found = true;
            break;
        }
    }
    if (!found) {
        QJsonObject o;
        o["id"]            = s.id;
        o["name"]          = s.name;
        o["plannedHours"]  = s.plannedHours;
        o["investedHours"] = s.investedHours;
        arr.append(o);
    }

    m_data["skills"] = arr;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

/**
 * @brief 删除技能
 * @param id 技能ID
 */
void DataManager::removeSkill(int id)
{
    QJsonArray arr = m_data["skills"].toArray();
    QJsonArray filtered;
    for (int i = 0; i < arr.size(); ++i) {
        if (arr[i].toObject()["id"].toInt() != id)
            filtered.append(arr[i]);
    }
    m_data["skills"] = filtered;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

/**
 * @brief 增加技能已投入时间
 * @param skillId 技能ID
 * @param hours 要增加的小时数
 *
 * 【调用时机】
 *   完成任务日程时，将实际投入时间累加到对应技能
 *   用于追踪技能成长进度
 */
void DataManager::addInvestedHours(int skillId, double hours)
{
    QJsonArray arr = m_data["skills"].toArray();
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject o = arr[i].toObject();
        if (o["id"].toInt() == skillId) {
            double current = o["investedHours"].toDouble();
            o["investedHours"] = current + hours;
            arr[i] = o;
            break;
        }
    }
    m_data["skills"] = arr;
    save();
    // 注意：这里不发送dataChanged，由调用者（markScheduleComplete）处理
    // 避免重复触发信号
}

// ============================================================
//  任务模板模块（Study Plan）
// ============================================================

/**
 * @brief 获取所有任务模板
 * @param includeCompleted 是否包含已完成的任务
 * @return QList<PlanItem> 任务模板列表
 *
 * 【数据格式】
 *   plans: [
 *     {
 *       "id": 1,
 *       "skillId": 2,
 *       "title": "C++基础学习",
 *       "plannedHours": 10,
 *       "xpBaseHours": 1,
 *       "xpAmount": 10,
 *       "stars": 3,
 *       "status": "in_progress",
 *       "created": "20260701",
 *       "completedDate": ""
 *     }
 *   ]
 *
 * 【状态说明】
 *   - "not_started": 未开始
 *   - "in_progress": 进行中
 *   - "completed": 已完成
 *
 * 【数据迁移】
 *   支持从旧版 "duration"（时长）和 "xp"（经验）字段迁移
 *   旧版经验计算方式：XP = 10 * stars^1.5
 */
QList<PlanItem> DataManager::getPlanItems(bool includeCompleted) const
{
    QList<PlanItem> result;
    QJsonArray arr = m_data["plans"].toArray();
    for (const auto &v : arr) {
        QJsonObject o = v.toObject();
        PlanItem item;
        item.id            = o["id"].toInt();
        item.skillId       = o["skillId"].toInt(-1);
        item.title         = o["title"].toString();
        // 支持新版"plannedHours"和旧版"duration"
        item.plannedHours  = o["plannedHours"].toDouble(o["duration"].toDouble(1));
        item.stars         = o["stars"].toInt(o["xp"].toInt(1));  // 旧版xp→stars
        item.createdDate   = QDate::fromString(o["created"].toString(), "yyyyMMdd");
        if (!o["completedDate"].toString().isEmpty())
            item.completedDate = QDate::fromString(o["completedDate"].toString(), "yyyyMMdd");

        // 加载XP配置（新版）
        if (o.contains("xpBaseHours")) {
            item.xpBaseHours = o["xpBaseHours"].toDouble(1);
            item.xp          = o["xpAmount"].toInt(10);
        } else {
            // 旧版：从星级推导
            item.xpBaseHours = item.plannedHours;
            item.xp          = static_cast<int>(10.0 * qPow(item.stars, 1.5));
        }

        // 状态迁移
        if (o.contains("status")) {
            item.status = o["status"].toString();
        } else if (o["completed"].toBool()) {
            item.status = "completed";
        } else {
            item.status = "not_started";
        }

        if (includeCompleted || item.status != "completed")
            result.append(item);
    }
    return result;
}

/**
 * @brief 获取指定技能的所有任务
 * @param skillId 技能ID
 * @return QList<PlanItem> 任务列表
 */
QList<PlanItem> DataManager::getPlansBySkill(int skillId) const
{
    QList<PlanItem> result;
    auto all = getPlanItems(true);
    for (const auto &p : all) {
        if (p.skillId == skillId)
            result.append(p);
    }
    return result;
}

/**
 * @brief 根据ID获取任务模板
 * @param id 任务ID
 * @return PlanItem 任务对象（不存在则返回空对象）
 */
PlanItem DataManager::getPlanItem(int id) const
{
    auto all = getPlanItems(true);
    for (const auto &p : all) {
        if (p.id == id) return p;
    }
    return PlanItem();
}

/**
 * @brief 保存任务模板（新增）
 * @param item 任务对象
 *
 * 【注意】此方法用于新增任务，总是追加
 *        更新任务请使用 updatePlanItem()
 */
void DataManager::savePlanItem(const PlanItem &item)
{
    QJsonArray arr = m_data["plans"].toArray();

    PlanItem p = item;
    if (p.id <= 0) {
        p.id = nextId("nextPlanId");
    }

    QJsonObject o;
    o["id"]           = p.id;
    o["skillId"]      = p.skillId;
    o["title"]        = p.title;
    o["plannedHours"] = p.plannedHours;
    o["xpBaseHours"]  = p.xpBaseHours;
    o["xpAmount"]     = p.xp;
    o["stars"]        = p.stars;
    o["status"]       = p.status;
    o["created"]      = p.createdDate.toString("yyyyMMdd");
    o["completedDate"] = p.completedDate.toString("yyyyMMdd");
    arr.append(o);

    m_data["plans"] = arr;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

/**
 * @brief 更新任务模板
 * @param item 任务对象（必须包含有效ID）
 *
 * 【与savePlanItem的区别】
 *   - savePlanItem: 总是追加新记录
 *   - updatePlanItem: 根据ID查找并更新
 */
void DataManager::updatePlanItem(const PlanItem &item)
{
    QJsonArray arr = m_data["plans"].toArray();
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject o = arr[i].toObject();
        if (o["id"].toInt() == item.id) {
            o["skillId"]       = item.skillId;
            o["title"]         = item.title;
            o["plannedHours"]  = item.plannedHours;
            o["xpBaseHours"]   = item.xpBaseHours;
            o["xpAmount"]      = item.xp;
            o["stars"]         = item.stars;
            o["status"]        = item.status;
            o["completedDate"] = item.completedDate.toString("yyyyMMdd");
            arr[i] = o;
            break;
        }
    }
    m_data["plans"] = arr;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

/**
 * @brief 删除任务模板
 * @param id 任务ID
 */
void DataManager::removePlanItem(int id)
{
    QJsonArray arr = m_data["plans"].toArray();
    QJsonArray filtered;
    for (int i = 0; i < arr.size(); ++i) {
        if (arr[i].toObject()["id"].toInt() != id)
            filtered.append(arr[i]);
    }
    m_data["plans"] = filtered;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

/**
 * @brief 设置任务状态
 * @param id 任务ID
 * @param status 新状态 ("not_started", "in_progress", "completed")
 * @param awardHours 是否将计划时间计入技能投入（用于直接完成）
 *
 * 【核心逻辑】
 *   1. 查找任务并更新状态
 *   2. 如果状态变为"completed"且之前未完成：
 *      a. 记录完成日期
 *      b. 按计划时间奖励经验
 *      c. 如果awardHours为true，将计划时间计入技能投入
 *   3. 保存更新
 *
 * 【经验奖励公式】
 *   XP = (计划时间 / xpBaseHours) * xp
 *   即按比例奖励经验
 */
void DataManager::setPlanStatus(int id, const QString &status, bool awardHours)
{
    PlanItem p = getPlanItem(id);
    if (p.id > 0) {
        QString oldStatus = p.status;
        p.status = status;
        if (status == "completed" && oldStatus != "completed") {
            p.completedDate = QDate::currentDate();
            // 按比例奖励经验
            addXp(p.xpForHours(p.plannedHours));
            // 将计划时间计入技能投入
            if (awardHours && p.skillId > 0) {
                addInvestedHours(p.skillId, p.plannedHours);
            }
        }
        updatePlanItem(p);
    }
}

/**
 * @brief 计算总体计划完成率
 * @return 完成百分比
 */
double DataManager::planProgress() const
{
    QList<PlanItem> items = getPlanItems(true);
    if (items.isEmpty()) return 0;
    int done = 0;
    for (const auto &i : items) {
        if (i.status == "completed") ++done;
    }
    return done * 100.0 / items.size();
}

// ============================================================
//  日程数据模块
// ============================================================

/**
 * @brief 计算日程结束时间
 * @return QTime 结束时间
 *
 * 【计算逻辑】
 *   开始时间 + 计划小时数
 *   如果超过24小时，自动取模（但单个日程通常不超过24小时）
 */
QTime ScheduleItem::endTime() const
{
    int totalSecs = startTime.hour() * 3600 + startTime.minute() * 60
                    + static_cast<int>(plannedHours * 3600);
    totalSecs %= 86400; // 一天内的秒数取模
    return QTime(totalSecs / 3600, (totalSecs % 3600) / 60);
}

/**
 * @brief 获取指定日期的所有日程
 * @param date 日期
 * @return QList<ScheduleItem> 日程列表（按开始时间排序）
 *
 * 【数据格式】
 *   schedules_20260730: [
 *     { "id": 1, "planId": 3, "startTime": "09:00", "plannedHours": 2, "completed": false },
 *     { "id": 2, "planId": 5, "startTime": "14:00", "plannedHours": 1.5, "completed": true }
 *   ]
 */
QList<ScheduleItem> DataManager::getSchedules(const QDate &date) const
{
    QList<ScheduleItem> result;
    QString key = QString("schedules_%1").arg(date.toString("yyyyMMdd"));
    if (m_data.contains(key)) {
        QJsonArray arr = m_data[key].toArray();
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            ScheduleItem s;
            s.id           = o["id"].toInt();
            s.planId       = o["planId"].toInt();
            s.date         = date;
            s.startTime    = QTime::fromString(o["startTime"].toString(), "HH:mm");
            s.plannedHours = o["plannedHours"].toDouble();
            s.completed    = o["completed"].toBool();
            result.append(s);
        }
    }
    // 按开始时间排序
    std::sort(result.begin(), result.end(), [](const ScheduleItem &a, const ScheduleItem &b) {
        return a.startTime < b.startTime;
    });
    return result;
}

/**
 * @brief 保存日程（追加模式）
 * @param item 日程对象
 */
void DataManager::saveSchedule(const ScheduleItem &item)
{
    QString key = QString("schedules_%1").arg(item.date.toString("yyyyMMdd"));
    QJsonArray arr = m_data[key].toArray();

    ScheduleItem s = item;
    if (s.id <= 0) {
        s.id = nextId("nextScheduleId");
    }

    QJsonObject o;
    o["id"]           = s.id;
    o["planId"]       = s.planId;
    o["startTime"]    = s.startTime.toString("HH:mm");
    o["plannedHours"] = s.plannedHours;
    o["completed"]    = s.completed;
    arr.append(o);

    m_data[key] = arr;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

/**
 * @brief 删除日程（按ID查找并删除）
 * @param id 日程ID
 *
 * 【实现方式】
 *   遍历所有schedules_开头的键，查找匹配的ID
 *   找到后删除并立即返回
 */
void DataManager::removeSchedule(int id)
{
    for (auto it = m_data.begin(); it != m_data.end(); ++it) {
        if (it.key().startsWith("schedules_")) {
            QJsonArray arr = it.value().toArray();
            for (int i = 0; i < arr.size(); ++i) {
                if (arr[i].toObject()["id"].toInt() == id) {
                    arr.removeAt(i);
                    m_data[it.key()] = arr;
                    save();
                    emit const_cast<DataManager*>(this)->dataChanged();
                    return;
                }
            }
        }
    }
}

/**
 * @brief 标记日程为完成（核心结算逻辑）
 * @param id 日程ID
 *
 * 【完整的完成流程】
 *   1. 查找日程并标记为completed
 *   2. 获取关联的任务模板（PlanItem）
 *   3. 获取关联的技能（SkillItem）
 *   4. 按比例奖励经验值
 *      XP = (日程时长 / xpBaseHours) * xp
 *   5. 将日程时长累加到技能投入时间
 *   6. 检查任务是否应自动完成
 *      - 所有已完成的日程时长之和 >= 计划时长
 *      - 如果是，将任务状态设为"completed"
 *   7. 检查技能是否应自动完成
 *      - 已投入时间 >= 计划时间
 *      - UI通过isCompleted()判断并显示徽章
 *   8. 写入完成记录（用于历史追溯和成就统计）
 *
 * 【经验奖励示例】
 *   任务：计划10小时，每1小时奖10XP
 *   日程：完成2小时 → 奖励 2/1*10 = 20XP
 *
 * 【自动完成任务示例】
 *   任务：计划10小时
 *   已完成的日程：2h + 3h + 5h = 10h
 *   第3个日程完成后 → 自动标记任务为"completed"
 */
void DataManager::markScheduleComplete(int id)
{
    // ============================================================
    // 第1步：查找并标记日程完成
    // ============================================================
    ScheduleItem schedule;
    QString scheduleKey;
    int scheduleIndex = -1;

    for (auto it = m_data.begin(); it != m_data.end(); ++it) {
        if (it.key().startsWith("schedules_")) {
            QJsonArray arr = it.value().toArray();
            for (int i = 0; i < arr.size(); ++i) {
                QJsonObject o = arr[i].toObject();
                if (o["id"].toInt() == id) {
                    o["completed"] = true;
                    arr[i] = o;
                    m_data[it.key()] = arr;
                    scheduleKey = it.key();

                    schedule.id = o["id"].toInt();
                    schedule.planId = o["planId"].toInt();
                    schedule.date = QDate::fromString(scheduleKey.mid(10), "yyyyMMdd");
                    schedule.startTime = QTime::fromString(o["startTime"].toString(), "HH:mm");
                    schedule.plannedHours = o["plannedHours"].toDouble();
                    schedule.completed = true;
                    scheduleIndex = i;
                    break;
                }
            }
            if (scheduleIndex >= 0) break;
        }
    }

    if (schedule.id <= 0) return;

    // ============================================================
    // 第2步：获取关联的任务和技能
    // ============================================================
    PlanItem plan = getPlanItem(schedule.planId);
    if (plan.id <= 0) {
        save();
        emit const_cast<DataManager*>(this)->dataChanged();
        return;
    }

    SkillItem skill;
    bool hasSkill = false;
    if (plan.skillId > 0) {
        skill = getSkill(plan.skillId);
        hasSkill = (skill.id > 0);
    }

    // ============================================================
    // 第3步：按比例奖励经验值
    // ============================================================
    int earnedXp = plan.xpForHours(schedule.plannedHours);
    addXp(earnedXp);

    // ============================================================
    // 第4步：累加技能投入时间
    // ============================================================
    if (hasSkill) {
        addInvestedHours(skill.id, schedule.plannedHours);
        skill = getSkill(skill.id);  // 重新读取更新后的技能
    }

    // ============================================================
    // 第5步：检查任务是否应该自动完成
    // ============================================================
    if (plan.status != "completed") {
        double totalCompletedHours = 0;
        // 遍历所有日期的日程，统计该计划已完成的日程时长总和
        for (auto it = m_data.begin(); it != m_data.end(); ++it) {
            if (it.key().startsWith("schedules_")) {
                QJsonArray arr = it.value().toArray();
                for (const auto &v : arr) {
                    QJsonObject o = v.toObject();
                    if (o["planId"].toInt() == plan.id && o["completed"].toBool()) {
                        totalCompletedHours += o["plannedHours"].toDouble();
                    }
                }
            }
        }
        if (totalCompletedHours >= plan.plannedHours) {
            setPlanStatus(plan.id, "completed");
        } else if (plan.status == "not_started") {
            setPlanStatus(plan.id, "in_progress");
        }
    }

    // ============================================================
    // 第6步：技能自动完成检查（由UI通过isCompleted()判断）
    // ============================================================
    // skill.isCompleted() 会在UI中用于显示达成徽章

    // ============================================================
    // 第7步：写入完成记录
    // ============================================================
    {
        QJsonArray completedArr = m_data["completed_records"].toArray();
        QJsonObject rec;
        rec["completedDate"] = QDate::currentDate().toString("yyyyMMdd");
        rec["scheduleDate"]  = schedule.date.toString("yyyyMMdd");
        rec["planTitle"]     = plan.title;
        rec["skillName"]     = hasSkill ? skill.name : QString();
        rec["plannedHours"]  = schedule.plannedHours;
        rec["xp"]            = earnedXp;
        completedArr.append(rec);
        m_data["completed_records"] = completedArr;
    }

    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

// ============================================================
//  完成记录模块
// ============================================================

/**
 * @brief 获取所有完成记录
 * @return QList<CompletedRecord> 完成记录列表（最新的在前）
 *
 * 【数据格式】
 *   completed_records: [
 *     { "completedDate": "20260730", "scheduleDate": "20260730",
 *       "planTitle": "C++基础学习", "skillName": "C++",
 *       "plannedHours": 2, "xp": 20 },
 *     ...
 *   ]
 *
 * 【用途】
 *   - 成就系统统计
 *   - 连续打卡天数计算
 *   - 历史回顾
 */
QList<CompletedRecord> DataManager::getCompletedRecords() const
{
    QList<CompletedRecord> result;
    QJsonArray arr = m_data["completed_records"].toArray();
    for (const auto &v : arr) {
        QJsonObject o = v.toObject();
        CompletedRecord r;
        r.completedDate = QDate::fromString(o["completedDate"].toString(), "yyyyMMdd");
        r.scheduleDate  = QDate::fromString(o["scheduleDate"].toString(), "yyyyMMdd");
        r.planTitle     = o["planTitle"].toString();
        r.skillName     = o["skillName"].toString();
        r.plannedHours  = o["plannedHours"].toDouble();
        r.xp            = o["xp"].toInt();
        result.append(r);
    }
    // 最新的在前
    std::reverse(result.begin(), result.end());
    return result;
}

// ============================================================
//  数据重置模块
// ============================================================

/**
 * @brief 重置所有数据（危险操作）
 *
 * 【执行步骤】
 *   1. 删除数据文件（物理删除）
 *   2. 清空内存中的JSON对象
 *   3. 保存（创建新文件）
 *   4. 触发dataChanged信号通知界面
 *
 * 【警告】
 *   此操作不可逆！调用前应要求用户二次确认
 */
void DataManager::resetAllData()
{
    // 删除数据文件
    QFile::remove(m_dataPath);
    // 清空内存数据
    m_data = QJsonObject();
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

// ============================================================
//  成就与连续打卡模块
// ============================================================

/**
 * @brief 计算连续打卡天数
 * @return 连续打卡天数
 *
 * 【算法说明】
 *   1. 从今天开始向前追溯
 *   2. 如果今天没有完成记录，从昨天开始算
 *   3. 每天只要有至少一条完成记录就算打卡
 *   4. 遇到没有打卡的日期则停止计数
 *
 * 【示例】
 *   7月30日完成 → 7月29日完成 → 7月28日完成 → 7月27日未完成
 *   返回 3（7月30日、29日、28日）
 *
 * 【实现细节】
 *   使用QSet存储所有有记录的日期字符串
 *   O(n)时间复杂度，n为完成记录总数
 */
int DataManager::getConsecutiveDays() const
{
    auto records = getCompletedRecords();
    if (records.isEmpty()) return 0;

    // 构建有完成记录的日期集合
    QSet<QString> activeDates;
    for (const auto &r : records) {
        activeDates.insert(r.scheduleDate.toString("yyyyMMdd"));
    }

    // 从今天开始向前计数
    QDate today = QDate::currentDate();
    int streak = 0;
    QDate d = today;

    // 如果今天还没有完成记录，从昨天开始算
    if (!activeDates.contains(d.toString("yyyyMMdd"))) {
        d = d.addDays(-1);
    }

    while (activeDates.contains(d.toString("yyyyMMdd"))) {
        ++streak;
        d = d.addDays(-1);
    }

    return streak;
}

/**
 * @brief 获取已完成任务总数
 * @return 完成记录总数
 */
int DataManager::getTotalCompletedTasks() const
{
    return getCompletedRecords().size();
}

// ============================================================
//  复盘数据模块
// ============================================================

/**
 * @brief 获取所有复盘记录
 * @return QList<ReviewEntry> 复盘记录列表（最新的在前）
 *
 * 【数据格式】
 *   reviews: [
 *     { "date": "20260730", "content": "今天学习了C++，感觉不错..." },
 *     ...
 *   ]
 */
QList<ReviewEntry> DataManager::getReviews() const
{
    QList<ReviewEntry> result;
    QJsonArray arr = m_data["reviews"].toArray();
    for (const auto &v : arr) {
        QJsonObject o = v.toObject();
        ReviewEntry e;
        e.date    = QDate::fromString(o["date"].toString(), "yyyyMMdd");
        e.content = o["content"].toString();
        result.append(e);
    }
    std::reverse(result.begin(), result.end());
    return result;
}

/**
 * @brief 获取复盘文件目录
 * @return 目录路径
 *
 * 除了JSON存储外，每个复盘还会生成一个Markdown文件
 * 便于用户使用外部编辑器查看和编辑
 */
QString DataManager::reviewsDir() const
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/reviews";
    QDir().mkpath(dir);
    return dir;
}

/**
 * @brief 获取指定日期的复盘文件路径
 * @param date 日期
 * @return 完整文件路径
 *
 * 【文件命名规则】
 *   YYYY-MM-DD.md
 *   例如：2026-07-30.md
 */
QString DataManager::reviewFilePath(const QDate &date) const
{
    return reviewsDir() + "/" + date.toString("yyyy-MM-dd") + ".md";
}

/**
 * @brief 保存复盘记录
 * @param content 复盘内容
 *
 * 【保存方式】
 *   1. 保存到JSON（用于程序内部查询）
 *   2. 保存为Markdown文件（用于外部查看和编辑）
 *
 * 【Markdown模板】
 *   # 每日复盘 — 2026年7月30日
 *
 *   ---
 *
 *   用户输入的内容...
 */
void DataManager::saveReview(const QString &content)
{
    QDate today = QDate::currentDate();

    // 保存到JSON
    QJsonArray arr = m_data["reviews"].toArray();
    QJsonObject o;
    o["date"]    = today.toString("yyyyMMdd");
    o["content"] = content;
    arr.append(o);
    m_data["reviews"] = arr;
    save();

    // 保存为Markdown文件
    QString filePath = reviewFilePath(today);
    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        stream << "# 每日复盘 — " << today.toString("yyyy年M月d日") << "\n\n";
        stream << "---\n\n";
        stream << content << "\n";
        file.close();
    }

    emit const_cast<DataManager*>(this)->dataChanged();
}

/**
 * @brief 删除复盘记录
 * @param date 日期
 *
 * 【删除方式】
 *   1. 从JSON数组中删除
 *   2. 删除对应的Markdown文件
 */
void DataManager::deleteReview(const QDate &date)
{
    // 从JSON中删除
    QJsonArray arr = m_data["reviews"].toArray();
    QString dateStr = date.toString("yyyyMMdd");
    for (int i = arr.size() - 1; i >= 0; --i) {
        QJsonObject o = arr[i].toObject();
        if (o["date"].toString() == dateStr) {
            arr.removeAt(i);
            break;
        }
    }
    m_data["reviews"] = arr;
    save();

    // 删除Markdown文件
    QString filePath = reviewFilePath(date);
    if (QFile::exists(filePath))
        QFile::remove(filePath);

    emit const_cast<DataManager*>(this)->dataChanged();
}

// ============================================================
//  习惯养成模块
// ============================================================

/**
 * @brief 获取习惯的目标描述
 * @return 目标描述字符串
 *
 * 【示例】
 *   completionMode="count", targetCount=3 → "3 次"
 *   completionMode="hours", targetHours=2.5 → "2.5 小时"
 *   completionMode="duration", targetMinutes=30 → "30 分钟"
 */
QString HabitItem::targetLabel() const
{
    if (completionMode == "hours")
        return QString::fromUtf8("%1 小时").arg(targetHours, 0, 'f', 1);
    else if (completionMode == "duration")
        return QString::fromUtf8("%1 分钟").arg(targetMinutes);
    else
        return QString::fromUtf8("%1 次").arg(targetCount);
}

/**
 * @brief 获取习惯的周期键值
 * @param date 日期
 * @return 周期键值
 *
 * 【周期类型】
 *   - daily: "20260730"（按天）
 *   - weekly: "2026W30"（按周，周一为起始）
 *   - monthly: "202607"（按月）
 *
 * 【用途】
 *   用于生成习惯进度的存储键
 *   例如：habitprog_1_20260730
 */
QString HabitItem::periodKey(const QDate &date) const
{
    if (habitMode == "daily")
        return date.toString("yyyyMMdd");
    else if (habitMode == "weekly") {
        int weekNum = date.weekNumber();             // Qt的周数（周一开始）
        return QString("%1W%2").arg(date.year()).arg(weekNum, 2, 10, QChar('0'));
    } else { // monthly
        return date.toString("yyyyMM");
    }
}

/**
 * @brief 获取所有习惯
 * @return QList<HabitItem> 习惯列表
 *
 * 【数据格式】
 *   habits: [
 *     {
 *       "id": 1,
 *       "name": "晨跑",
 *       "habitMode": "daily",
 *       "completionMode": "duration",
 *       "targetMinutes": 30,
 *       "stars": 2,
 *       "createdDate": "20260701"
 *     }
 *   ]
 */
QList<HabitItem> DataManager::getHabits() const
{
    QList<HabitItem> result;
    QJsonArray arr = m_data["habits"].toArray();
    for (const auto &v : arr) {
        QJsonObject o = v.toObject();
        HabitItem h;
        h.id             = o["id"].toInt();
        h.name           = o["name"].toString();
        h.habitMode      = o["habitMode"].toString("daily");
        h.completionMode = o["completionMode"].toString("count");
        h.targetHours    = o["targetHours"].toDouble(1);
        h.targetCount    = o["targetCount"].toInt(1);
        h.targetMinutes  = o["targetMinutes"].toInt(30);
        h.stars          = o["stars"].toInt(1);
        h.createdDate    = QDate::fromString(o["createdDate"].toString(), "yyyyMMdd");
        if (!h.createdDate.isValid()) h.createdDate = QDate::currentDate();
        result.append(h);
    }
    return result;
}

/**
 * @brief 根据ID获取习惯
 * @param id 习惯ID
 * @return HabitItem 习惯对象（不存在则返回空对象）
 */
HabitItem DataManager::getHabit(int id) const
{
    auto habits = getHabits();
    for (const auto &h : habits) {
        if (h.id == id) return h;
    }
    return HabitItem();
}

/**
 * @brief 保存习惯（新增或更新）
 * @param item 习惯对象
 */
void DataManager::saveHabit(const HabitItem &item)
{
    QJsonArray arr = m_data["habits"].toArray();

    HabitItem h = item;
    if (h.id <= 0) {
        h.id = nextId("nextHabitId");
    }

    bool found = false;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject o = arr[i].toObject();
        if (o["id"].toInt() == h.id) {
            o["id"]             = h.id;
            o["name"]           = h.name;
            o["habitMode"]      = h.habitMode;
            o["completionMode"] = h.completionMode;
            o["targetHours"]    = h.targetHours;
            o["targetCount"]    = h.targetCount;
            o["targetMinutes"]  = h.targetMinutes;
            o["stars"]          = h.stars;
            o["createdDate"]    = h.createdDate.toString("yyyyMMdd");
            arr[i] = o;
            found = true;
            break;
        }
    }
    if (!found) {
        QJsonObject o;
        o["id"]             = h.id;
        o["name"]           = h.name;
        o["habitMode"]      = h.habitMode;
        o["completionMode"] = h.completionMode;
        o["targetHours"]    = h.targetHours;
        o["targetCount"]    = h.targetCount;
        o["targetMinutes"]  = h.targetMinutes;
        o["stars"]          = h.stars;
        o["createdDate"]    = h.createdDate.toString("yyyyMMdd");
        arr.append(o);
    }

    m_data["habits"] = arr;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

/**
 * @brief 删除习惯
 * @param id 习惯ID
 */
void DataManager::removeHabit(int id)
{
    QJsonArray arr = m_data["habits"].toArray();
    QJsonArray filtered;
    for (int i = 0; i < arr.size(); ++i) {
        if (arr[i].toObject()["id"].toInt() != id)
            filtered.append(arr[i]);
    }
    m_data["habits"] = filtered;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

/**
 * @brief 获取习惯进度
 * @param habitId 习惯ID
 * @param date 日期
 * @return 当前进度值（浮点数）
 *
 * 【存储键格式】
 *   habitprog_{habitId}_{periodKey}
 *   例如：habitprog_1_20260730
 *
 * 【进度含义】
 *   - count模式：已完成次数
 *   - hours模式：已完成小时数
 *   - duration模式：已完成分钟数
 */
double DataManager::getHabitProgress(int habitId, const QDate &date) const
{
    HabitItem h = getHabit(habitId);
    if (h.id <= 0) return 0;

    QString pKey = h.periodKey(date);
    QString key = QString("habitprog_%1_%2").arg(habitId).arg(pKey);
    if (m_data.contains(key)) {
        return m_data[key].toObject()["progress"].toDouble(0);
    }
    return 0;
}

/**
 * @brief 设置习惯进度
 * @param habitId 习惯ID
 * @param date 日期
 * @param progress 进度值
 *
 * 【核心逻辑】
 *   1. 更新进度值
 *   2. 检查是否刚完成（之前未完成，现在完成了）
 *   3. 如果是，奖励经验并写入完成记录
 *
 * 【经验奖励】
 *   每个习惯根据星级有不同的XP值：
 *   ⭐ 1星 → 10XP
 *   ⭐⭐ 2星 → 28XP
 *   ⭐⭐⭐ 3星 → 52XP
 *   ⭐⭐⭐⭐ 4星 → 80XP
 *   ⭐⭐⭐⭐⭐ 5星 → 112XP
 *   公式：10 * stars^1.5
 */
void DataManager::setHabitProgress(int habitId, const QDate &date, double progress)
{
    HabitItem h = getHabit(habitId);
    if (h.id <= 0) return;

    // 检查更新前是否已完成
    bool wasCompleted = isHabitCompletedForDate(habitId, date);

    QString pKey = h.periodKey(date);
    QString key = QString("habitprog_%1_%2").arg(habitId).arg(pKey);

    QJsonObject o;
    o["progress"] = progress;
    o["date"]     = date.toString("yyyyMMdd");
    m_data[key] = o;
    save();

    // 如果刚完成，奖励经验
    if (!wasCompleted && isHabitCompletedForDate(habitId, date)) {
        int xp = h.xpValue();
        addXp(xp);

        // 写入完成记录
        QJsonArray completedArr = m_data["completed_records"].toArray();
        QJsonObject rec;
        rec["completedDate"] = QDate::currentDate().toString("yyyyMMdd");
        rec["scheduleDate"]  = date.toString("yyyyMMdd");
        rec["planTitle"]     = QString::fromUtf8("[习惯] %1").arg(h.name);
        rec["skillName"]     = QString::fromUtf8("习惯养成");
        rec["plannedHours"]  = 0;
        rec["xp"]            = xp;
        completedArr.append(rec);
        m_data["completed_records"] = completedArr;
        save();
    }

    emit const_cast<DataManager*>(this)->dataChanged();
}

/**
 * @brief 判断习惯在指定日期是否已完成
 * @param habitId 习惯ID
 * @param date 日期
 * @return true 如果进度达到或超过目标
 */
bool DataManager::isHabitCompletedForDate(int habitId, const QDate &date) const
{
    HabitItem h = getHabit(habitId);
    if (h.id <= 0) return false;

    double prog = getHabitProgress(habitId, date);

    if (h.completionMode == "hours")
        return prog >= h.targetHours;
    else if (h.completionMode == "duration")
        return prog >= h.targetMinutes;
    else // "count"
        return static_cast<int>(prog) >= h.targetCount;
}

/**
 * @brief 获取指定日期有效的所有习惯
 * @param date 日期
 * @return QList<HabitItem> 在指定日期之前创建的活跃习惯
 */
QList<HabitItem> DataManager::getActiveHabits(const QDate &date) const
{
    auto all = getHabits();
    QList<HabitItem> active;
    for (const auto &h : all) {
        if (h.createdDate <= date)
            active.append(h);
    }
    return active;
}

// ============================================================
//  旧版任务模块（Todo — 向后兼容）
// ============================================================

/**
 * @brief 获取指定日期的旧版任务列表
 * @param date 日期
 * @return QList<TaskItem> 任务列表
 *
 * 【说明】
 *   这是早期版本的任务系统，已被日程系统取代
 *   保留是为了向后兼容，新用户应使用日程系统
 *
 * 【数据格式】
 *   tasks_20260730: [
 *     { "id": 1, "time": "09:00", "content": "阅读", "completed": false }
 *   ]
 */
QList<DataManager::TaskItem> DataManager::getTasks(const QDate &date) const
{
    QList<TaskItem> result;
    QString key = QString("tasks_%1").arg(date.toString("yyyyMMdd"));
    if (m_data.contains(key)) {
        QJsonArray arr = m_data[key].toArray();
        for (const auto &v : arr) {
            QJsonObject o = v.toObject();
            TaskItem t;
            t.id        = o["id"].toInt();
            t.date      = date;
            t.time      = o["time"].toString();
            t.content   = o["content"].toString();
            t.completed = o["completed"].toBool();
            result.append(t);
        }
    }
    return result;
}

void DataManager::saveTask(const TaskItem &task)
{
    QString key = QString("tasks_%1").arg(task.date.toString("yyyyMMdd"));
    QJsonArray arr = m_data[key].toArray();

    bool found = false;
    for (int i = 0; i < arr.size(); ++i) {
        QJsonObject o = arr[i].toObject();
        if (o["id"].toInt() == task.id) {
            o["time"]      = task.time;
            o["content"]   = task.content;
            o["completed"] = task.completed;
            arr[i] = o;
            found = true;
            break;
        }
    }
    if (!found) {
        QJsonObject o;
        int newId = arr.size() + 1;
        o["id"]        = newId;
        o["time"]      = task.time;
        o["content"]   = task.content;
        o["completed"] = task.completed;
        arr.append(o);
    }
    m_data[key] = arr;
    save();
    emit const_cast<DataManager*>(this)->dataChanged();
}

void DataManager::removeTask(int id)
{
    for (auto it = m_data.begin(); it != m_data.end(); ++it) {
        if (it.key().startsWith("tasks_")) {
            QJsonArray arr = it.value().toArray();
            for (int i = 0; i < arr.size(); ++i) {
                if (arr[i].toObject()["id"].toInt() == id) {
                    arr.removeAt(i);
                    m_data[it.key()] = arr;
                    save();
                    emit const_cast<DataManager*>(this)->dataChanged();
                    return;
                }
            }
        }
    }
}

int DataManager::tasksCompleted(const QDate &date) const
{
    int count = 0;
    for (const auto &t : getTasks(date)) {
        if (t.completed) ++count;
    }
    return count;
}

int DataManager::tasksTotal(const QDate &date) const
{
    return getTasks(date).size();
}

double DataManager::taskProgress(const QDate &date) const
{
    int total = tasksTotal(date);
    if (total == 0) return 0;
    return tasksCompleted(date) * 100.0 / total;
}
