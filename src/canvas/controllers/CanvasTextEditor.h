#pragma once

#include <QObject>
#include <QPointer>

class CanvasScene;
class CanvasView;
class QTextEdit;
class TextItem;

// 画布内联文字编辑：在选中的文字图元上盖一个输入框，正文随图元字体与当前缩放显示，
// 打开时焦点直接落在正文末尾；回车提交、Shift+Enter 换行、Esc 放弃修改。
class CanvasTextEditor : public QObject {
  public:
    CanvasTextEditor(CanvasView* view, CanvasScene* scene);

    // 打开内联编辑；已有编辑先提交。返回是否确实打开了。
    bool BeginEdit(TextItem* item);
    void Commit();
    void Cancel();
    bool IsEditing() const { return m_edit != nullptr && m_item != nullptr; }

  protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void CloseEditor();

    CanvasView* m_view = nullptr;
    CanvasScene* m_scene = nullptr;
    QPointer<TextItem> m_item;
    QTextEdit* m_edit = nullptr;
    bool m_closing = false;
};
