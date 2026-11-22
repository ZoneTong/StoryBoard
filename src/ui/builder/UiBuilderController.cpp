#include "ui/builder/UiBuilderController.h"

#include "ui/builder/UiActionFactory.h"
#include "ui/builder/UiMenuBuilder.h"
#include "ui/builder/UiStatusBarBuilder.h"
#include "ui/builder/UiToolBarBuilder.h"
#include "ui/mainwindow/MainWindow.h"

UiBuilderController::UiBuilderController(MainWindow* window)
    : m_window(window), m_actions(std::make_unique<UiActionFactory>(window)),
      m_menus(std::make_unique<UiMenuBuilder>(window)), m_toolBars(std::make_unique<UiToolBarBuilder>(window)),
      m_statusBar(std::make_unique<UiStatusBarBuilder>(window))
{
}

UiBuilderController::~UiBuilderController() = default;

void UiBuilderController::BuildActions()
{
    m_actions->Build();
}

void UiBuilderController::BuildShapeMenu()
{
    m_menus->BuildShapeMenu();
}

void UiBuilderController::BuildMenus()
{
    m_menus->BuildMenus();
}

void UiBuilderController::BuildToolBars()
{
    m_toolBars->BuildToolBars();
}

void UiBuilderController::BuildStatusBar()
{
    m_statusBar->BuildStatusBar();
}
