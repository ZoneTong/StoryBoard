#pragma once

#include <QDialog>
#include <QList>
#include <QRectF>
#include <QString>
#include <QStringList>

class CanvasScene;
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

    explicit ExportDialog(CanvasScene* scene, const QString& startDirectory, QWidget* parent = nullptr);

    Options options() const;

  private:
    QRectF SelectedArea() const;
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
    QComboBox* m_scopeCombo = nullptr;
    QComboBox* m_layerModeCombo = nullptr;
    QDoubleSpinBox* m_scaleSpin = nullptr;
    QComboBox* m_formatCombo = nullptr;
    QLineEdit* m_pathEdit = nullptr;
    QLabel* m_sizeLabel = nullptr;
};
