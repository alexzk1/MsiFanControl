#include <chrono>

#include "mainwindow.h"
#include "runners.h"
#include "communicator.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    communicator = utility::startNewRunner([](const auto shouldStop)
    {
        CSharedDevice comm;
        while (!(*shouldStop))
        {
            comm.Communicate();

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(3s);
        }
    });
}

MainWindow::~MainWindow()
{
    communicator.reset();
    delete ui;
}
