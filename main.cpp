// ============================================================
// Life Manager — 个人生活综合管理软件
// 入口文件：初始化 Qt 应用、加载全局样式、启动主窗口
// ============================================================

#include <QApplication>
#include <QFont>
#include <QFile>
#include <QIcon>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    // 创建 Qt 应用程序实例
    QApplication app(argc, argv);

    // 基础应用信息
    app.setApplicationName("Life Manager");
    app.setWindowIcon(QIcon(":/resources/style/cat.png"));
    app.setApplicationVersion("1.0.0");
    app.setOrganizationName("LifeManager");

    // 设置全局默认字体（微软雅黑，10pt）
    QFont defaultFont("Microsoft YaHei", 10);
    app.setFont(defaultFont);

    // 加载全局 QSS 样式表
    QFile styleFile(":/resources/style/global.qss");
    if (styleFile.open(QFile::ReadOnly | QFile::Text)) {
        QString style = QString::fromUtf8(styleFile.readAll());
        app.setStyleSheet(style);
        styleFile.close();
    }

    // 创建并显示主窗口
    MainWindow window;
    window.show();

    // 进入事件循环
    return app.exec();
}
