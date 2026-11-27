cmake_minimum_required(VERSION 3.21)

# 项目名取自仓库根目录的文件夹名，重命名目录或在工作树中构建时无需改配置。
# 仅把不能安全用于目标名的字符替换为下划线，普通目录名保持原样。
get_filename_component(STORYBOARD_ROOT_FOLDER_NAME "${CMAKE_CURRENT_SOURCE_DIR}" NAME)
string(REGEX REPLACE "[^A-Za-z0-9_.+-]" "_" STORYBOARD_PROJECT_NAME "${STORYBOARD_ROOT_FOLDER_NAME}")
if(STORYBOARD_PROJECT_NAME STREQUAL "")
    set(STORYBOARD_PROJECT_NAME "StoryBoard")
elseif(STORYBOARD_PROJECT_NAME MATCHES "^[^A-Za-z_]")
    set(STORYBOARD_PROJECT_NAME "_${STORYBOARD_PROJECT_NAME}")
endif()

project(${STORYBOARD_PROJECT_NAME} VERSION 0.3.0 LANGUAGES CXX)
message(STATUS "Project name from folder: ${STORYBOARD_ROOT_FOLDER_NAME} -> ${PROJECT_NAME}")

set(STORYBOARD_VERSION "${PROJECT_VERSION}")
set(STORYBOARD_VERSION_TWEAK "${PROJECT_VERSION_TWEAK}")
if(STORYBOARD_VERSION_TWEAK STREQUAL "")
    set(STORYBOARD_VERSION_TWEAK 0)
endif()
set(STORYBOARD_VERSION_COMMA
    "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},${STORYBOARD_VERSION_TWEAK}")
set(STORYBOARD_APP_NAME "${PROJECT_NAME}")
set(STORYBOARD_APP_EXE_NAME "${PROJECT_NAME}.exe")
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

# 项目的验证编译链是 GCC 15.1（Qt 官方 mingw1510_64）。版本偏低仍可尝试构建，
# 但给出警告，避免把未验证的二进制当成发布包。
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 15.0)
    message(WARNING "${PROJECT_NAME} 的验证编译链为 GCC 15.1，当前 GCC ${CMAKE_CXX_COMPILER_VERSION} 未经验证。")
endif()

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

# 源文件按文件夹组织：目录内新增 / 删除文件由 CONFIGURE_DEPENDS 自动同步，
# 不再逐个文件维护清单。新增子目录时只在这里追加一行文件夹路径。
function(storyboard_add_source_folder target folder)
    if(NOT IS_DIRECTORY "${folder}")
        return()
    endif()
    file(GLOB STORYBOARD_FOLDER_SOURCES CONFIGURE_DEPENDS
        "${folder}/*.cpp"
        "${folder}/*.cxx"
        "${folder}/*.cc"
        "${folder}/*.h"
        "${folder}/*.hpp"
        "${folder}/*.hxx"
        "${folder}/*.qrc"
    )
    if(STORYBOARD_FOLDER_SOURCES)
        target_sources(${target} PRIVATE ${STORYBOARD_FOLDER_SOURCES})
        # IDE 里仍按真实目录层级展示，避免所有文件挤在 Source Files 根下。
        source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" PREFIX "" FILES ${STORYBOARD_FOLDER_SOURCES})
    endif()
endfunction()

set(STORYBOARD_SOURCE_FOLDERS
    src
    src/app
    src/canvas
    src/canvas/controllers
    src/io
    src/items
    src/items/geometry
    src/items/lines
    src/items/polygon
    src/items/properties
    src/items/shapes
    src/items/text
    src/lineart
    src/ops
    src/scene
    src/ui
    src/ui/builder
    src/ui/dialogs
    src/ui/icons
    src/ui/mainwindow
    src/ui/panels/customshape
    src/ui/panels/layers
    src/ui/panels/properties
    src/ui/settings
    src/ui/widgets
    src/ui/widgets/rows
    src/selftest
    src/selftest/core
    src/selftest/groups
    src/selftest/items
    src/selftest/layers
    src/selftest/lineart
    src/selftest/merge
    src/selftest/properties
    src/selftest/scene
    src/selftest/ui
    resources
)

set(STORYBOARD_EXECUTABLE_KEYWORDS)
if(WIN32)
    list(APPEND STORYBOARD_EXECUTABLE_KEYWORDS WIN32)
elseif(APPLE)
    list(APPEND STORYBOARD_EXECUTABLE_KEYWORDS MACOSX_BUNDLE)
endif()
qt_add_executable(${PROJECT_NAME} ${STORYBOARD_EXECUTABLE_KEYWORDS})
foreach(source_folder IN LISTS STORYBOARD_SOURCE_FOLDERS)
    storyboard_add_source_folder(${PROJECT_NAME} "${CMAKE_CURRENT_SOURCE_DIR}/${source_folder}")
endforeach()

if(APPLE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        MACOSX_BUNDLE_BUNDLE_NAME "${PROJECT_NAME}"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.zonetong.${PROJECT_NAME}"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE STORYBOARD_VERSION="${STORYBOARD_VERSION}"
                                                         STORYBOARD_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")

# Qt 官方 i18n 流水线：.ts -> .qm，并把 .qm 与 .ts 一起嵌进资源。
# 运行时 QTranslator 从 :/i18n/storyboard_zh.qm 装载中文翻译。
qt_add_lrelease(${PROJECT_NAME}
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
qt_add_resources(${PROJECT_NAME} "storyboard_translations"
    PREFIX "/i18n"
    FILES ${STORYBOARD_TS_FILES} ${STORYBOARD_QM_FILES} ${STORYBOARD_QT_TRANSLATION_FILE}
)

if(WIN32)
    target_sources(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/app.rc")
    # 需求 31：打开 shapes 目录改走 Shell.Application（CLSID_Shell）COM 通道，
    # 需要显式链接 ole32 / oleaut32；shell32 与 uuid 供外壳与 CLSID 使用。
    target_link_libraries(${PROJECT_NAME} PRIVATE ole32 oleaut32 shell32 uuid)
endif()

target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /utf-8 /W4)
else()
    target_compile_options(${PROJECT_NAME} PRIVATE -Wall -Wextra -Wno-unused-parameter)
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
                    "$<TARGET_FILE_DIR:${PROJECT_NAME}>/platforms/qoffscreen.dll"
        )
    endif()

    add_custom_target(deploy
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                "$<TARGET_FILE:${PROJECT_NAME}>"
        ${STORYBOARD_DEPLOY_EXTRA}
        DEPENDS ${PROJECT_NAME}
        COMMENT "Deploying Qt runtime next to ${PROJECT_NAME}"
        VERBATIM
    )
endif()

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION .)

# 需求 8：把示例模板（含四个子目录与顺序.txt）放到可执行文件旁的 shapes/，
# 自定义图形面板按子目录分页显示；用户自己往这个目录里加文件同样会被扫到。
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/examples/shapes")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_directory
                "${CMAKE_CURRENT_SOURCE_DIR}/examples/shapes"
                "$<TARGET_FILE_DIR:${PROJECT_NAME}>/shapes"
        COMMENT "Copying example shapes next to ${PROJECT_NAME}"
    )
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/examples/shapes/" DESTINATION shapes)
endif()
