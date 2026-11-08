cmake_minimum_required(VERSION 3.21)

project(StoryBoard VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)

set(STORYBOARD_TS_FILES
    translations/storyboard_zh.ts
)

set(STORYBOARD_SOURCES
    src/main.cpp
    src/BoardTypes.h
    src/Settings.h
    src/Settings.cpp
    src/I18n.h
    src/I18n.cpp
    src/ItemGeometry.h
    src/IconFactory.h
    src/IconFactory.cpp
    src/BoardItem.h
    src/BoardItem.cpp
    src/StrokeItem.h
    src/StrokeItem.cpp
    src/ImageItem.h
    src/ImageItem.cpp
    src/TextItem.h
    src/TextItem.cpp
    src/ShapeItem.h
    src/ShapeItem.cpp
    src/Commands.h
    src/Commands.cpp
    src/BoardScene.h
    src/BoardScene.cpp
    src/BoardView.h
    src/BoardView.cpp
    src/LayerPanel.h
    src/LayerPanel.cpp
    src/ExportDialog.h
    src/ExportDialog.cpp
    src/SettingsDialog.h
    src/SettingsDialog.cpp
    src/TextDialog.h
    src/TextDialog.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/SelfTest.h
    src/SelfTest.cpp
    resources/resources.qrc
)

qt_add_executable(StoryBoard WIN32 ${STORYBOARD_SOURCES})

# Qt 官方 i18n 流水线：.ts -> .qm，并把 .qm 与 .ts 一起嵌进资源。
# 运行时 QTranslator 从 :/i18n/storyboard_en.qm 装载英文翻译。
qt_add_lrelease(StoryBoard
    TS_FILES ${STORYBOARD_TS_FILES}
    QM_FILES_OUTPUT_VARIABLE STORYBOARD_QM_FILES
)
# .qm 生成在构建目录里，是绝对路径；给资源一个稳定的相对别名。
set_source_files_properties(${STORYBOARD_QM_FILES} PROPERTIES
    QT_RESOURCE_ALIAS "storyboard_zh.qm"
)
set_source_files_properties(${STORYBOARD_TS_FILES} PROPERTIES
    QT_RESOURCE_ALIAS "storyboard_zh.ts"
)
qt_add_resources(StoryBoard "storyboard_translations"
    PREFIX "/i18n"
    FILES ${STORYBOARD_TS_FILES} ${STORYBOARD_QM_FILES}
)

if(WIN32)
    target_sources(StoryBoard PRIVATE resources/app.rc)
endif()

target_include_directories(StoryBoard PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(StoryBoard PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

if(MSVC)
    target_compile_options(StoryBoard PRIVATE /utf-8 /W4)
else()
    target_compile_options(StoryBoard PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()

# Copy the Qt runtime next to the executable so it can be launched by double click.
find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt windeployqt6
    HINTS "${QT6_INSTALL_PREFIX}/bin" "${Qt6_DIR}/../../../bin"
)
if(WIN32 AND WINDEPLOYQT_EXECUTABLE)
    set(STORYBOARD_DEPLOY_EXTRA)
    if(EXISTS "${QT6_INSTALL_PREFIX}/plugins/platforms/qoffscreen.dll")
        list(APPEND STORYBOARD_DEPLOY_EXTRA
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${QT6_INSTALL_PREFIX}/plugins/platforms/qoffscreen.dll"
                    "$<TARGET_FILE_DIR:StoryBoard>/platforms/qoffscreen.dll"
        )
    endif()

    add_custom_target(deploy
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                "$<TARGET_FILE:StoryBoard>"
        ${STORYBOARD_DEPLOY_EXTRA}
        DEPENDS StoryBoard
        COMMENT "Deploying Qt runtime next to StoryBoard.exe"
        VERBATIM
    )
endif()

install(TARGETS StoryBoard RUNTIME DESTINATION .)
