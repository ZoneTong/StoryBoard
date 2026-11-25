#ifndef EXCALIDRAWFORMAT_H
#define EXCALIDRAWFORMAT_H

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>

// 需求 9：Excalidraw 兼容。
// 只做「场景 JSON <-> 画板 JSON」的纯转换，不碰场景对象，方便单独自检。
namespace Excalidraw {
constexpr char SCENE_TYPE[] = "excalidraw";
constexpr char CLIPBOARD_TYPE[] = "excalidraw/clipboard";

bool IsScene(const QJsonObject& root);
bool IsClipboard(const QJsonObject& root);

// 元素数组 -> StoryBoard items 数组（粘贴用）。不支持的元素跳过并写 warning。
// layers 非空时回填每个 frame 对应的一层；files 提供 image 元素的 dataURL；
// keepImages 决定 image 元素是转成位图图元还是跳过；groupParents 非空时回填组嵌套关系。
QJsonArray ElementsToItems(const QJsonArray& elements, QStringList* warnings = nullptr, QJsonArray* layers = nullptr,
                           const QJsonObject& files = QJsonObject(), bool keepImages = false,
                           QJsonObject* groupParents = nullptr);

// .excalidraw 场景 -> StoryBoard 画板根 JSON（导入用）。
QJsonObject SceneToStoryBoard(const QJsonObject& scene, QStringList* warnings = nullptr, bool keepImages = false);

// StoryBoard 画板根 JSON -> .excalidraw 场景（导出用）。
QJsonObject StoryBoardToScene(const QJsonObject& document, QStringList* warnings = nullptr);

// StoryBoard 画板根 JSON -> Excalidraw 剪贴板 JSON（复制到 excalidraw.com 用）。
QJsonObject StoryBoardToClipboard(const QJsonObject& document, QStringList* warnings = nullptr);
} // namespace Excalidraw

#endif // EXCALIDRAWFORMAT_H
