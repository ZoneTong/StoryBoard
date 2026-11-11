cmake_minimum_required(VERSION 3.21)

project(StoryBoard VERSION 0.1.1 LANGUAGES CXX)

set(STORYBOARD_VERSION "${PROJECT_VERSION}")
set(STORYBOARD_VERSION_TWEAK "${PROJECT_VERSION_TWEAK}")
if(STORYBOARD_VERSION_TWEAK STREQUAL "")
    set(STORYBOARD_VERSION_TWEAK 0)
endif()
set(STORYBOARD_VERSION_COMMA
    "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},${STORYBOARD_VERSION_TWEAK}")
set(STORYBOARD_APP_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.ico")

# Windows 版本资源从 CMake 的 PROJECT_VERSION 生成，避免 app.rc / main.cpp /
# CMakeLists 各维护一份版本号。
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/resources/app.rc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/app.rc"
    @ONLY
)

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

# Qt 内置对话框（如 QFontDialog）的标签由 qtbase 自己的翻译目录提供；
# 这里把 Qt 6.12 的中文基础翻译一并嵌入，避免部署时依赖额外 translations 目录。
set(STORYBOARD_QT_TRANSLATION_FILE
    translations/qtbase_zh_CN.qm
)

set(STORYBOARD_SOURCES
    src/main.cpp
    src/CanvasTypes.h
    src/Settings.h
    src/Settings.cpp
    src/I18n.h
    src/I18n.cpp
    src/ItemGeometry.h
    src/IconFactory.h
    src/IconFactory.cpp
    src/CanvasItem.h
    src/CanvasItem.cpp
    src/StrokeItem.h
    src/StrokeItem.cpp
    src/ImageItem.h
    src/ImageItem.cpp
    src/TextItem.h
    src/TextItem.cpp
    src/ShapeItem.h
    src/ShapeItem.cpp
    src/LineArtConverter.h
    src/LineArtConverter.cpp
    src/LineArtPresets.h
    src/LineArtDialog.h
    src/LineArtDialog.cpp
    src/Commands.h
    src/Commands.cpp
    src/CanvasScene.h
    src/CanvasScene.cpp
    src/CanvasView.h
    src/CanvasView.cpp
    src/LayerPanel.h
    src/LayerPanel.cpp
    src/ExportDialog.h
    src/ExportDialog.cpp
    src/SettingsDialog.h
    src/SettingsDialog.cpp
    src/ItemPropertiesDialog.h
    src/ItemPropertiesDialog.cpp
    src/MainWindow.h
    src/MainWindow.cpp
    src/SelfTest.h
    src/SelfTest.cpp
    resources/resources.qrc
)

qt_add_executable(StoryBoard WIN32 ${STORYBOARD_SOURCES})

target_compile_definitions(StoryBoard PRIVATE STORYBOARD_VERSION="${STORYBOARD_VERSION}")

# Qt 官方 i18n 流水线：.ts -> .qm，并把 .qm 与 .ts 一起嵌进资源。
# 运行时 QTranslator 从 :/i18n/storyboard_zh.qm 装载中文翻译。
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
set_source_files_properties(${STORYBOARD_QT_TRANSLATION_FILE} PROPERTIES
    QT_RESOURCE_ALIAS "qtbase_zh_CN.qm"
)
qt_add_resources(StoryBoard "storyboard_translations"
    PREFIX "/i18n"
    FILES ${STORYBOARD_TS_FILES} ${STORYBOARD_QM_FILES} ${STORYBOARD_QT_TRANSLATION_FILE}
)

if(WIN32)
    target_sources(StoryBoard PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
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
