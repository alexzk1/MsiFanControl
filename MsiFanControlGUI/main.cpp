#include "execonmainthread.h"
#include "mainwindow.h"

#include <boost/program_options.hpp> // IWYU pragma: keep
#include <boost/program_options/detail/parsers.hpp>
#include <boost/program_options/options_description.hpp>
#include <boost/program_options/variables_map.hpp>

#include <QApplication>
#include <QMessageBox>
#include <QObject>
#include <QSharedMemory>

#include <iostream>

namespace po = boost::program_options;

int main(int argc, char *argv[])
{
    // construct static object inside main thread
    ExecOnMainThread::get();

    po::options_description desc("Startup options");

    desc.add_options()("help,h", "Show this help.")("minimize,m", "Minimize to the tray on start.")(
      "gamemode,g", "Enable game mode on start.");

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, desc), vm);
    po::notify(vm);

    if (vm.count("help"))
    {
        std::cout << desc << std::endl;
        return 1;
    }

    const QApplication a(argc, argv);
    a.setApplicationDisplayName("MSI Fans Control");
    a.setApplicationName("MSI Fans Control Gui Application");
    a.setWindowIcon(QIcon(":/images/fan.png"));

    QSharedMemory sharedMemory;
    sharedMemory.setKey(a.applicationName() + "LockOnce");
    if (!sharedMemory.create(1))
    {
        QMessageBox::warning(
          nullptr, a.applicationDisplayName(),
          QObject::tr("GUI control module is already running. Check your status bar."));
        return 0;
    }

    MainWindow w(StartOptions{static_cast<bool>(vm.count("minimize")),
                              static_cast<bool>(vm.count("gamemode"))},
                 nullptr);
    w.show();
    return a.exec();
}
