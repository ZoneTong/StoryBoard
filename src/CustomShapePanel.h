#pragma once

#include <QList>
#include <QPair>
#include <QString>
#include <QWidget>

class QLabel;
class QTabWidget;
class MainWindow;
struct CustomShapeTemplate;

// 需求 7：自定义图形按 shapes 目录下的文件夹自动分组，展开为独立浮动面板，
// 每个文件夹一个标签页；工具栏按钮与菜单项都只是触发这个面板。
// 面板用 Qt::Tool，可拖动、记忆位置与尺寸，点击面板外部不关闭，ESC 关闭，
// 插入模板后收起（需求已确认不做「保持打开」）。
class CustomShapePanel : public QWidget {
    Q_OBJECT
  public:
    explicit CustomShapePanel(MainWindow* window);

    // 每次展开前重扫目录（按 mtime + size 缓存单个模板），并回到上次停留的标签页。
    void showAt(QWidget* anchor);
    QTabWidget* tabWidget() const { return m_tabs; }
    // 当前选中标签页对应的文件夹名（根目录散文件为「其它」）。
    QString currentFolder() const;

  protected:
    void keyPressEvent(QKeyEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

  private:
    struct Folder {
        QString key;
        QString title;
        QList<CustomShapeTemplate> items;
        QStringList errors;
    };

    QList<Folder> scanFolders() const;
    void rebuild(const QList<Folder>& folders);
    void insertTemplate(const QString& filePath);
    // 方向键在模板按钮间移动焦点（左右 ±1，上下 ±列数），回车插入焦点所在模板。
    void moveFocusByStep(int step);
    void restoreGeometryFromSettings();
    void saveGeometryToSettings();

    MainWindow* m_window = nullptr;
    QTabWidget* m_tabs = nullptr;
    QLabel* m_status = nullptr;
    QString m_currentKey;
    bool m_rebuilding = false;
};
