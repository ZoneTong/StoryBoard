#pragma once

#include <QDialog>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

class CanvasScene;
class CanvasView;
class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QLineEdit;

class ExportDialog : public QDialog {
    Q_OBJECT
  public:
    struct Options {
        QRectF area;
        qreal scale = 1.0;
        QString format;
        QString filePath;
        bool separateLayers = false;
        // 勾选过「自适应尺寸」时记下来，调用方写回设置，下次打开弹窗保持同一状态。
        bool fitViewSize = false;
        QStringList filePaths;
        QList<int> layerIds;
    };

    explicit ExportDialog(CanvasScene* scene, const QString& startDirectory, QWidget* parent = nullptr,
                          CanvasView* view = nullptr, bool fitViewSize = false);

    Options options() const;

  private:
    QRectF SelectedArea() const;
    // 勾选「自适应尺寸」时按画布内容出图：范围取可见图元并集的最小包围框，比例取 1，
    // 于是输出像素恰好等于包围框的场景尺寸；没拿到场景时回退到下拉选的范围。
    QRectF ExportArea() const;
    qreal ExportScale() const;
    bool FitViewSelected() const;
    // 勾选状态变化后同步两个被接管的控件与尺寸标签。
    void ApplyFitViewState();
    QString SelectedFormat() const;
    QString FileFilter() const;
    bool SeparateLayersSelected() const;
    QStringList OutputFilePaths() const;
    static QString LayerFileComponent(const QString& name, int layerId);
    static QString FormatSuffix(const QString& format);
    void UpdateEstimatedSize();
    void UpdatePathExtension();
    void BrowseForFile();
    void ValidateAndAccept();

    CanvasScene* m_scene = nullptr;
    CanvasView* m_view = nullptr;
    QComboBox* m_scopeCombo = nullptr;
    QComboBox* m_layerModeCombo = nullptr;
    QDoubleSpinBox* m_scaleSpin = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QCheckBox* m_fitViewCheck = nullptr;
};
