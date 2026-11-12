#pragma once

#include <QString>

class QWidget;

enum class UiLanguage { System = 0, Chinese = 1, English = 2 };

// 读取/保存设置项 ui/language（system / zh / en），默认跟随系统语言。
UiLanguage uiLanguageSetting();
void setUiLanguageSetting(UiLanguage language);

// 运行时强制语言（测试与截图用），不写入设置；UiLanguage::System 表示取消强制。
void uiOverrideLanguage(UiLanguage language);
UiLanguage uiEffectiveLanguage();
bool uiIsEnglish();

// 在创建主窗口之前调用：装载 Qt 翻译资源（:/i18n/storyboard_zh.qm）。
// 源语言是英文，中文等其它语种放在对应 .ts 中。
void uiInitializeLanguage();

// 用 Qt 的 QTranslator 把英文源串翻成当前语言；找不到译文时原样返回。
QString uiText(const QString& english);

// 中英双向翻译（用于把已经显示出来的文本切回另一种语言）。
QString uiTranslateEitherWay(const QString& text);

// 递归刷新一个界面树里的文字（动作、菜单、按钮、标签、下拉项等）。
// 只处理翻译资源里出现过的整串文本，图元/图层等用户数据不会被改写。
void uiRetranslateWidgetTree(QWidget* root);
