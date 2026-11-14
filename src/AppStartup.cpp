#include "AppStartup.h"

#include "CommandLineOptions.h"
#include "I18n.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QIcon>
#include <QStandardPaths>
#include <QStyleFactory>

namespace {

void applyApplicationStyle(QApplication& app)
{
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    QFont font(QStringLiteral("Microsoft YaHei UI"));
    font.setPointSizeF(9.0);
    app.setFont(font);

    app.setStyleSheet(QStringLiteral(R"(
QMainWindow { background: #F4F6F9; }
QMenuBar { background: #FFFFFF; border-bottom: 1px solid #E3E7EE; padding: 2px 8px; }
QMenuBar::item { background: transparent; padding: 6px 11px; border-radius: 6px; }
QMenuBar::item:selected { background: #EDF1F8; }
QMenuBar::item:pressed { background: #E3E9F4; }
QMenu { background: #FFFFFF; border: 1px solid #E0E4EB; border-radius: 8px; padding: 6px; }
QMenu::item { padding: 7px 28px 7px 26px; border-radius: 5px; }
QMenu::item:selected { background: #EBF0FF; color: #1F2937; }
QMenu::item:disabled { color: #A8AFBB; }
QMenu::separator { height: 1px; background: #EDF0F4; margin: 5px 8px; }
QToolBar#MainToolBar { background: #FFFFFF; border: none; border-bottom: 1px solid #E3E7EE; spacing: 3px; padding: 4px 8px; }
QToolBar#ModeToolBar { background: #FFFFFF; border: none; border-right: 1px solid #E3E7EE; spacing: 8px; padding: 12px 7px; }
QToolBar::separator { background: #E4E8EF; width: 1px; margin: 4px 6px; }
QToolButton { border: 1px solid transparent; border-radius: 8px; padding: 5px; color: #2B2F36; }
QToolButton:hover { background: #F1F4F9; }
QToolButton:pressed { background: #E7ECF5; }
QToolButton:checked { background: #E9EEFF; border-color: #C9D6FF; color: #2F52C7; }
QToolButton:disabled { color: #B4BAC5; }
/* 主工具栏图标 20px、按钮 30×30（尺寸在 MainWindow 里钉死），与 30px 高的下拉框同基线。 */
QToolBar#MainToolBar QToolButton { padding: 0px; }
/* 画笔颜色按钮是分裂按钮：色块区保持 20px 图标，右侧箭头单独占一条窄区，
   画一条分隔线而不是叠一个白框，避免看起来像两个控件压在一起。 */
QToolButton#ColorButton::menu-button { width: 8px; border: none; border-left: 1px solid #E3E7EE; }
QToolButton#ColorButton::menu-button:hover { background: #EDF1F8; }
/* 下拉框的箭头子控件本身就是 24px，再留右内边距会把文字挤成省略，
   所以只保留左内边距；数字框的加减按钮画在右内边距里，右侧留 22px。 */
QToolBar#MainToolBar QComboBox { padding: 3px 0px 3px 8px; }
QToolBar#MainToolBar QSpinBox { padding: 3px 22px 3px 8px; }
QToolBar#ModeToolBar QToolButton { padding: 9px 4px; min-width: 58px; }
QLabel#ToolBarLabel { color: #6B7280; padding: 0 4px 0 8px; }
/* 紧跟文字按钮后的标签：按钮自身已有内边距，这里只留一点点空隙，避免叠成一条长缝。 */
QLabel#ToolBarLabelTight { color: #6B7280; padding: 0 4px 0 2px; }
QLabel#Badge { background: #F0F3F8; border-radius: 9px; padding: 1px 10px; color: #4B5563; }
QLabel#StatusHint { color: #6B7280; padding-left: 8px; }
QStatusBar { background: #FFFFFF; border-top: 1px solid #E3E7EE; min-height: 28px; }
QStatusBar QLabel { color: #5B6472; }
QStatusBar::item { border: none; }
QSpinBox, QDoubleSpinBox { background: #FFFFFF; border: 1px solid #DDE2EA; border-radius: 6px; padding: 4px 24px 4px 8px; min-height: 20px; color: #2B2F36; }
QSpinBox:focus, QDoubleSpinBox:focus { border-color: #7C9BFF; }
QSpinBox::up-button, QDoubleSpinBox::up-button { subcontrol-origin: border; subcontrol-position: top right; width: 22px; height: 15px; border: none; background: transparent; }
QSpinBox::down-button, QDoubleSpinBox::down-button { subcontrol-origin: border; subcontrol-position: bottom right; width: 22px; height: 15px; border: none; background: transparent; }
QSpinBox::up-arrow, QDoubleSpinBox::up-arrow { image: url(:/icons/chevron-up.png); width: 8px; height: 8px; }
QSpinBox::down-arrow, QDoubleSpinBox::down-arrow { image: url(:/icons/chevron-down.png); width: 8px; height: 8px; }
QScrollBar:vertical { background: transparent; width: 10px; margin: 2px; }
QScrollBar::handle:vertical { background: #C9CFDA; min-height: 30px; border-radius: 4px; }
QScrollBar::handle:vertical:hover { background: #AEB7C6; }
QScrollBar:horizontal { background: transparent; height: 10px; margin: 2px; }
QScrollBar::handle:horizontal { background: #C9CFDA; min-width: 30px; border-radius: 4px; }
QScrollBar::handle:horizontal:hover { background: #AEB7C6; }
QScrollBar::add-line, QScrollBar::sub-line { width: 0px; height: 0px; }
QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
QDockWidget { color: #374151; font-weight: 600; }
QDockWidget::title { background: #FFFFFF; padding: 7px 10px; border-bottom: 1px solid #E3E7EE; }
QWidget#LayerPanel { background: #FFFFFF; }
QLabel#LayerCaption { color: #8A909C; font-size: 9pt; font-weight: 600; }
QLabel#LayerHint { color: #8A909C; font-size: 9pt; }
QListWidget#LayerList { background: #F8F9FC; border: 1px solid #E6E9F0; border-radius: 8px; padding: 4px; outline: none; }
QListWidget#LayerList::item { padding: 7px 8px; border-radius: 6px; margin: 2px 1px; color: #374151; }
QListWidget#LayerList::item:hover { background: #EEF2F9; }
/* 图层行的高亮由选中状态驱动，用动态属性而不是逐行写内联样式。 */
QWidget#LayerRow { background-color: transparent; border-left: 3px solid transparent; border-radius: 4px; }
QWidget#LayerRow[selected="true"] { background-color: #E8EEFB; border-left: 3px solid #2563EB; }
QLabel#LayerRowLabel[state="background"] { color: #787F8C; }
QLabel#LayerRowLabel[state="locked"] { color: #606878; }
QLabel#LayerRowLabel[state="hidden"] { color: #969DA9; }
QListWidget#LayerList::item:selected { background: #E9EEFF; color: #2F52C7; }
QListWidget#LayerList::item:disabled { color: #A8AFBB; }
QToolButton#LayerToolButton { border: 1px solid #DDE2EA; border-radius: 6px; background: #FFFFFF; color: #4B5563; font-size: 13px; font-weight: 700; padding: 0; }
QToolButton#LayerToolButton:hover { background: #F3F6FB; }
QToolButton#LayerToolButton:disabled { color: #C4CAD4; background: #F7F8FA; }
QWidget#ItemPanel, QWidget#ItemPanelContent, QWidget#ItemPanelScroll { background: #FFFFFF; }
QLabel#ItemTitle { font-size: 13px; font-weight: 700; color: #1F2937; }
QLabel#ItemMeta { color: #6B7280; font-size: 9pt; }
QToolButton#ItemShapeButton { border: 1px solid transparent; border-radius: 6px; background: #F8F9FC; }
QToolButton#ItemShapeButton:hover { background: #EEF2F9; }
QToolButton#ItemShapeButton:checked { background: #E8EEFB; border-color: #2563EB; }
QToolButton#ItemCustomShapeButton { border: 1px solid #DDE2EA; border-radius: 6px; background: #FFFFFF; padding: 4px 6px; color: #4B5563; }
QToolButton#ItemCustomShapeButton:hover { background: #F3F6FB; }
QToolButton#ItemCustomShapeButton:checked { border-color: #2563EB; color: #1D4ED8; }
QToolButton#ItemRowIcon { border: none; background: transparent; padding: 0; }
QLabel#ItemColorLabel { color: #6B7280; font-size: 9pt; }
QLabel#ItemColorHint { color: #8A909C; font-size: 9pt; }
QToolButton#ItemColorSwatch { border: 1px solid #CBD5E1; border-radius: 4px; }
QToolButton#ItemColorButton { border: 1px solid #DDE2EA; border-radius: 4px; background: #FFFFFF; color: #4B5563; }
QToolButton#ItemFillStyleButton, QToolButton#ItemBorderStyleButton { border: 1px solid transparent; border-radius: 5px; background: #F8F9FC; padding: 2px; }
QToolButton#ItemFillStyleButton:hover, QToolButton#ItemBorderStyleButton:hover { background: #EEF2F9; }
QToolButton#ItemFillStyleButton:checked, QToolButton#ItemBorderStyleButton:checked { background: #E8EEFB; border-color: #2563EB; }
QToolButton#ItemLayerUpButton, QToolButton#ItemLayerDownButton, QToolButton#ItemHeightUpButton, QToolButton#ItemHeightDownButton { border: 1px solid transparent; border-radius: 5px; background: #F8F9FC; padding: 2px; }
QToolButton#ItemLayerUpButton:hover, QToolButton#ItemLayerDownButton:hover, QToolButton#ItemHeightUpButton:hover, QToolButton#ItemHeightDownButton:hover { background: #EEF2F9; }
QToolButton#ItemLayerUpButton:checked, QToolButton#ItemLayerDownButton:checked, QToolButton#ItemHeightUpButton:checked, QToolButton#ItemHeightDownButton:checked { background: #E8EEFB; border-color: #2563EB; }
QSpinBox#ItemValueSpin, QDoubleSpinBox#ItemValueSpin { background: #FFFFFF; border: 1px solid #DDE2EA; border-radius: 5px; padding: 2px 4px; min-height: 16px; color: #2B2F36; }
QDialog { background: #F7F8FA; }
QGroupBox { background: #FFFFFF; border: 1px solid #E3E7EE; border-radius: 10px; margin-top: 10px; padding-top: 6px; font-weight: 600; }
QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; color: #374151; }
QLineEdit, QComboBox, QFontComboBox { background: #FFFFFF; border: 1px solid #DDE2EA; border-radius: 6px; padding: 5px 8px; min-height: 20px; color: #2B2F36; }
QComboBox, QFontComboBox { padding-right: 0px; }
QLineEdit:focus, QComboBox:focus, QFontComboBox:focus { border-color: #7C9BFF; }
QPlainTextEdit { background: #FFFFFF; border: 1px solid #DDE2EA; border-radius: 6px; padding: 7px 8px; color: #2B2F36; selection-background-color: #DCE6FF; }
QPlainTextEdit:focus { border-color: #7C9BFF; }
QComboBox::drop-down { subcontrol-origin: padding; subcontrol-position: top right; width: 24px; border: none; background: transparent; }
QComboBox::down-arrow { image: url(:/icons/chevron-down.png); width: 10px; height: 10px; }
QComboBox QAbstractItemView { border: 1px solid #E0E4EB; background: #FFFFFF; selection-background-color: #EBF0FF; selection-color: #1F2937; }
QPushButton { background: #FFFFFF; border: 1px solid #D9DFE9; border-radius: 7px; padding: 6px 14px; min-height: 20px; color: #2B2F36; }
QPushButton:hover { background: #F3F6FB; }
QPushButton:pressed { background: #E8EDF6; }
QPushButton:default { background: #3E6BFF; border-color: #3E6BFF; color: #FFFFFF; }
QPushButton:default:hover { background: #4E78FF; }
QDialogButtonBox QPushButton { min-width: 82px; }
QLabel#DialogTitle { font-size: 17px; font-weight: 700; color: #1F2937; }
/* 次级标题与表格标题共用 15px/600，提示文字统一走正文 9pt，字号只留三档。 */
QLabel#ItemPropertiesHeading { font-size: 15px; font-weight: 600; }
QLabel#LayerNameHeader, QLabel#LayerLockHeader { color: #8A909C; font-size: 9pt; font-weight: 600; }
QLabel#DialogHint, QLabel#ItemPropertiesReadOnlyHint { color: #6B7280; }
/* 设置页整页可滚动，滚动区本身不画底，避免和对话框背景割裂。 */
QScrollArea#SettingsScroll { background: transparent; border: none; }
QScrollArea#SettingsScroll > QWidget { background: transparent; }
/* “怎么调”折叠按钮做成文字按钮，不要工具按钮的边框。 */
QToolButton#LineArtHintToggle { border: none; background: transparent; color: #3E6BFF; font-weight: 600; padding: 0px; }
QToolButton#LineArtHintToggle:hover { color: #2F52C7; }
QCheckBox { spacing: 7px; color: #374151; }
QToolTip { background: #1F2937; color: #F9FAFB; border: none; padding: 5px 8px; border-radius: 4px; }
)"));
}

} // namespace

namespace AppStartup {

void prepareEnvironment(const CommandLineOptions& options)
{
    if (options.selfTest && qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
    }
    if (options.selfTest && qEnvironmentVariableIsEmpty("STORYBOARD_CONFIG")) {
        // 自检会反复写设置，把配置指到自检工作目录，绝不碰用户自己的 StoryBoard.ini。
        const QString reportFile = options.selfTestPath.isEmpty()
                                       ? QDir(QStandardPaths::writableLocation(QStandardPaths::TempLocation))
                                             .filePath(QStringLiteral("storyboard_selftest.txt"))
                                       : options.selfTestPath;
        const QString workDirectory =
            QDir(QFileInfo(reportFile).absolutePath()).filePath(QStringLiteral("storyboard-selftest-work"));
        QDir().mkpath(workDirectory);
        qputenv("STORYBOARD_CONFIG", QDir(workDirectory).filePath(QStringLiteral("StoryBoard.ini")).toUtf8());
    }
}

void initialize(QApplication& app)
{
    QCoreApplication::setOrganizationName(QStringLiteral("ZoneTong"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("zonetong.github.io"));
    QCoreApplication::setApplicationName(QStringLiteral("StoryBoard"));
    QCoreApplication::setApplicationVersion(QString::fromLatin1(STORYBOARD_VERSION));
    uiInitializeLanguage();
    app.setApplicationDisplayName(QCoreApplication::tr("StoryBoard Canvas"));
    app.setWindowIcon(QIcon(QStringLiteral(":/icons/app.png")));
    applyApplicationStyle(app);
}

} // namespace AppStartup
