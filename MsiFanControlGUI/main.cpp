#include "mainwindow.h"
#include "execonmainthread.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    //construct static object inside main thread
    ExecOnMainThread::get();

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
