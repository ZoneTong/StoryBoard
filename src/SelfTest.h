#pragma once

#include <QString>

class CanvasScene;

int RunSelfTest(const QString& reportPath = QString());
void PopulateDemoScene(CanvasScene& scene);
bool RenderDemoCanvas(const QString& outputPath);
bool WriteDemoCanvas(const QString& outputPath);
