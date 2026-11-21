#include "AppModes.h"
#include "AppStartup.h"
#include "CommandLineOptions.h"

#include <QApplication>

#ifndef STORYBOARD_VERSION
#error "STORYBOARD_VERSION must be defined by the build system (see CMakeLists.txt project VERSION)"
#endif

int main(int argc, char** argv)
{
    QStringList arguments;
    arguments.reserve(argc);
    for (int i = 0; i < argc; ++i) {
        arguments.append(QString::fromLocal8Bit(argv[i]));
    }

    const CommandLineOptions options = CommandLineOptions::Parse(arguments);
    AppStartup::PrepareEnvironment(options);

    QApplication app(argc, argv);
    AppStartup::Initialize(app);

    return AppModes::Run(app, options);
}
