#include "execonmainthread.h"
#include "mainwindow.h"

#include <QApplication>

#include <boost/program_options.hpp>

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

    QApplication a(argc, argv);
    MainWindow w(StartOptions{static_cast<bool>(vm.count("minimize")),
                              static_cast<bool>(vm.count("gamemode"))},
                 nullptr);
    w.show();
    return a.exec();
}
