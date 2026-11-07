#pragma once

#include <QString>

class BoardScene;

int runSelfTest(const QString& reportPath = QString());
void populateDemoScene(BoardScene& scene);
bool renderDemoBoard(const QString& outputPath);
bool writeDemoTemplate(const QString& outputPath);
