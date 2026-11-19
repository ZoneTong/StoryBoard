#pragma once

namespace selftest {

class Context;

// 各功能段测试入口；调用顺序与历史自检中的断言顺序一致，段内依赖前序段构建的场景状态。
void runSceneCanvasIoTests(Context& ctx);
void runLayerModelTests(Context& ctx);
void runStackingPriorityTests(Context& ctx);
void runLayerVisibilityLockTests(Context& ctx);
void runTextItemTests(Context& ctx);
void runShapeItemTests(Context& ctx);
void runLayerPanelTests(Context& ctx);
void runPaintOrderTests(Context& ctx);
void runViewInteractionTests(Context& ctx);
void runPropertyDialogFieldGeometryTests(Context& ctx);
void runPropertyDialogTextNamingTests(Context& ctx);
void runDuplicateItemTests(Context& ctx);
void runGroupTests(Context& ctx);
void runRubberBandTests(Context& ctx);
void runLockedLayerTests(Context& ctx);
void runMainWindowFlowTests(Context& ctx);
void runMainWindowShapeMenuTests(Context& ctx);
void runSettingsDialogBasicTests(Context& ctx);
void runLineArtDialogParameterTests(Context& ctx);
void runLineArtPresetAdaptiveTests(Context& ctx);
void runFillStyleBasicTests(Context& ctx);
void runFillStylePatternTests(Context& ctx);
void runCanvasOcclusionBoundaryTests(Context& ctx);
void runLayerExportPanelTests(Context& ctx);
void runLineArtConverterTests(Context& ctx);
void runAlignmentTests(Context& ctx);
void runAlignmentMenuTests(Context& ctx);
void runGroupBoolOpsTests(Context& ctx);
void runCustomShapeDockTests(Context& ctx);
void runShapesFolderTests(Context& ctx);
void runDockPanelMainWindowTests(Context& ctx);
void runDockOrderPersistenceTests(Context& ctx);
void runClipboardShortcutTests(Context& ctx);
void runExcalidrawFormatTests(Context& ctx);
void runLineSplitTests(Context& ctx);
void runShapesClusterTests(Context& ctx);
void runImageFillTests(Context& ctx);

} // namespace selftest
