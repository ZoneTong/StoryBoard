#include "ImagePickerDialog.h"

#include "CanvasScene.h"
#include "ShapeItem.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QIcon>
#include <QListWidget>
#include <QPixmap>
#include <QPushButton>
#include <QSet>
#include <QVBoxLayout>

namespace {

constexpr int THUMB_SIDE = 96;

QPixmap thumbnail(const QPixmap& image)
{
    return image.scaled(THUMB_SIDE, THUMB_SIDE, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

} // namespace

ImagePickerDialog::ImagePickerDialog(const QVector<QPixmap>& images, QWidget* parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("ImagePickerDialog"));
    setMinimumSize(420, 340);

    auto* layout = new QVBoxLayout(this);
    m_list = new QListWidget(this);
    m_list->setObjectName(QStringLiteral("ImagePickerList"));
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(THUMB_SIDE, THUMB_SIDE));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setGridSize(QSize(THUMB_SIDE + 24, THUMB_SIDE + 28));
    layout->addWidget(m_list, 1);

    for (const QPixmap& image : images) {
        AddImage(image);
    }
    if (m_list->count() > 0) {
        m_list->setCurrentRow(0);
    }

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    auto* fromFile = buttons->addButton(tr("From file…"), QDialogButtonBox::ActionRole);
    connect(fromFile, &QPushButton::clicked, this, &ImagePickerDialog::ChooseLocalFile);
    // 双击缩略图等同于确定。
    connect(m_list, &QListWidget::itemDoubleClicked, this, &QDialog::accept);
}

void ImagePickerDialog::AddImage(const QPixmap& image)
{
    if (image.isNull()) {
        return;
    }
    auto* item = new QListWidgetItem(QIcon(thumbnail(image)), m_localName.isEmpty() ? tr("Canvas image") : m_localName);
    item->setData(Qt::UserRole, QVariant::fromValue(image));
    m_list->addItem(item);
    m_list->setCurrentItem(item);
}

QPixmap ImagePickerDialog::SelectedImage() const
{
    const QListWidgetItem* item = m_list->currentItem();
    if (!item) {
        return QPixmap();
    }
    return item->data(Qt::UserRole).value<QPixmap>();
}

void ImagePickerDialog::ChooseLocalFile()
{
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Choose image"), QString(), tr("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp);;All files (*)"));
    if (path.isEmpty()) {
        return;
    }
    const QPixmap pixmap(path);
    if (pixmap.isNull()) {
        return;
    }
    m_localName = QFileInfo(path).completeBaseName();
    AddImage(pixmap);
}

QPixmap ImagePickerDialog::Pick(QWidget* parent, const QVector<QPixmap>& images, const QString& title)
{
    ImagePickerDialog dialog(images, parent);
    dialog.setWindowTitle(title);
    if (dialog.exec() != QDialog::Accepted) {
        return QPixmap();
    }
    return dialog.SelectedImage();
}

QVector<QPixmap> CollectCanvasImages(const CanvasScene* scene)
{
    QVector<QPixmap> result;
    QSet<qint64> seen;
    if (!scene) {
        return result;
    }
    for (CanvasItem* item : scene->CanvasItems()) {
        QPixmap candidate;
        if (auto* shape = dynamic_cast<ShapeItem*>(item); shape && shape->HasFillImage()) {
            candidate = shape->FillImage();
        }
        if (candidate.isNull() || seen.contains(candidate.cacheKey())) {
            continue;
        }
        seen.insert(candidate.cacheKey());
        result.append(candidate);
    }
    return result;
}
