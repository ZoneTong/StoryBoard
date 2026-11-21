#include "UiBuilderController.h"

#include "MainWindow.h"
#include "UiActionFactory.h"
#include "UiMenuBuilder.h"
#include "UiStatusBarBuilder.h"
#include "UiToolBarBuilder.h"

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
