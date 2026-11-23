#pragma once

#include <QString>

class CanvasScene;

int RunSelfTest(const QString& reportPath = QString());
namespace Demo {

void Populate(CanvasScene& scene);
bool Render(const QString& outputPath);
bool Write(const QString& outputPath);

} // namespace Demo
