#pragma once

#include <QString>

class CanvasScene;

int runSelfTest(const QString& reportPath = QString());
void populateDemoScene(CanvasScene& scene);
bool renderDemoCanvas(const QString& outputPath);
bool writeDemoCanvas(const QString& outputPath);
