cmake_minimum_required(VERSION 3.16)
project(LifeManager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# 优先查找 Qt6，找不到则回退到 Qt5
find_package(Qt6 COMPONENTS Widgets Sql Network QUIET)
if(NOT Qt6_FOUND)
    find_package(Qt5 5.15 REQUIRED COMPONENTS Widgets Sql Network)
    set(QT_VERSION_MAJOR 5)
else()
    set(QT_VERSION_MAJOR 6)
endif()

set(SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    leftpanel.cpp
    leftpanel.h
    centerpanel.cpp
    centerpanel.h
    rightpanel.cpp
    rightpanel.h
    datamanager.cpp
    datamanager.h
    taskdialog.cpp
    taskdialog.h
    dietdialog.cpp
    dietdialog.h
    scheduledialog.cpp
    scheduledialog.h
)

set(FORMS
    mainwindow.ui
    leftpanel.ui
    centerpanel.ui
    rightpanel.ui
)

set(RESOURCES
    resources.qrc
)

if(WIN32)
    set(APP_ICON_RESOURCE app_icon.rc)
endif()

add_executable(${PROJECT_NAME} ${SOURCES} ${FORMS} ${RESOURCES} ${APP_ICON_RESOURCE})

if(QT_VERSION_MAJOR EQUAL 6)
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt6::Widgets
        Qt6::Sql
        Qt6::Network
    )
else()
    target_link_libraries(${PROJECT_NAME} PRIVATE
        Qt5::Widgets
        Qt5::Sql
        Qt5::Network
    )
endif()

# 将资源目录复制到构建目录
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/resources
    $<TARGET_FILE_DIR:${PROJECT_NAME}>/resources
)
