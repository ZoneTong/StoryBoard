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
        QStringList filePaths;
        QList<int> layerIds;
    };

    explicit ExportDialog(CanvasScene* scene, const QString& startDirectory, QWidget* parent = nullptr,
                          CanvasView* view = nullptr);

    Options options() const;

  private:
    QRectF SelectedArea() const;
    // 勾选「自适应尺寸」时按视图出图：范围取当前可见的场景区域，比例取当前缩放，
    // 于是输出像素恰好等于全景视图在屏幕上的尺寸；没拿到视图时回退到下拉选的范围。
    QRectF ExportArea() const;
    qreal ExportScale() const;
    bool FitViewSelected() const;
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
