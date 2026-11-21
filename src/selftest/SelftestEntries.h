#pragma once

namespace selftest {

class Context;

// 各功能段测试入口；调用顺序与历史自检中的断言顺序一致，段内依赖前序段构建的场景状态。
void RunSceneCanvasIoTests(Context& ctx);
void RunLayerModelTests(Context& ctx);
void RunStackingPriorityTests(Context& ctx);
void RunLayerVisibilityLockTests(Context& ctx);
void RunTextItemTests(Context& ctx);
void RunShapeItemTests(Context& ctx);
void RunLayerPanelTests(Context& ctx);
void RunPaintOrderTests(Context& ctx);
void RunViewInteractionTests(Context& ctx);
void RunPropertyDialogFieldGeometryTests(Context& ctx);
void RunPropertyDialogTextNamingTests(Context& ctx);
void RunDuplicateItemTests(Context& ctx);
void RunGroupTests(Context& ctx);
void RunRubberBandTests(Context& ctx);
void RunLockedLayerTests(Context& ctx);
void RunMainWindowFlowTests(Context& ctx);
void RunMainWindowShapeMenuTests(Context& ctx);
void RunSettingsDialogBasicTests(Context& ctx);
void RunLineArtDialogParameterTests(Context& ctx);
void RunLineArtPresetAdaptiveTests(Context& ctx);
void RunFillStyleBasicTests(Context& ctx);
void RunFillStylePatternTests(Context& ctx);
void RunCanvasOcclusionBoundaryTests(Context& ctx);
void RunLayerExportPanelTests(Context& ctx);
void RunLineArtConverterTests(Context& ctx);
void RunAlignmentTests(Context& ctx);
void RunAlignmentMenuTests(Context& ctx);
void RunGroupBoolOpsTests(Context& ctx);
void RunCustomShapeDockTests(Context& ctx);
void RunShapesFolderTests(Context& ctx);
void RunDockPanelMainWindowTests(Context& ctx);
void RunDockOrderPersistenceTests(Context& ctx);
void RunClipboardShortcutTests(Context& ctx);
void RunExcalidrawFormatTests(Context& ctx);
void RunLineSplitTests(Context& ctx);
void RunShapesClusterTests(Context& ctx);
void RunImageFillTests(Context& ctx);

} // namespace selftest
