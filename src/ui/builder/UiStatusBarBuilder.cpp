#include "ui/builder/UiStatusBarBuilder.h"

#include "ui/mainwindow/MainWindow.h"

#include <QLabel>
#include <QStatusBar>
#include <QWidget>

namespace {

QLabel* MakeBadge(const QString& objectName, QWidget* parent)
{
    auto* label = new QLabel(parent);
    label->setObjectName(objectName);
    label->setAlignment(Qt::AlignCenter);
    return label;
}

} // namespace

UiStatusBarBuilder::UiStatusBarBuilder(MainWindow* window) : m_window(window) {}

void UiStatusBarBuilder::BuildStatusBar()
{
    m_window->statusBar()->setSizeGripEnabled(true);
    m_window->m_hintLabel = new QLabel(m_window);
    m_window->m_hintLabel->setObjectName(QStringLiteral("StatusHint"));
    m_window->m_modeBadge = MakeBadge(QStringLiteral("Badge"), m_window);
    m_window->m_zoomBadge = MakeBadge(QStringLiteral("Badge"), m_window);
    m_window->m_countBadge = MakeBadge(QStringLiteral("Badge"), m_window);

    m_window->statusBar()->addWidget(m_window->m_hintLabel, 1);
    m_window->statusBar()->addPermanentWidget(m_window->m_modeBadge);
    m_window->statusBar()->addPermanentWidget(m_window->m_zoomBadge);
    m_window->statusBar()->addPermanentWidget(m_window->m_countBadge);
}
