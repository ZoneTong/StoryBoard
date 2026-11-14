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

void UiBuilderController::buildActions()
{
    m_actions->build();
}

void UiBuilderController::buildShapeMenu()
{
    m_menus->buildShapeMenu();
}

void UiBuilderController::buildMenus()
{
    m_menus->buildMenus();
}

void UiBuilderController::buildToolBars()
{
    m_toolBars->buildToolBars();
}

void UiBuilderController::buildStatusBar()
{
    m_statusBar->buildStatusBar();
}
