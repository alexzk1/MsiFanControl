#include <chrono>
#include <stdexcept>
#include <map>

#include <QTimer>
#include <QString>

#include "execonmainthread.h"
#include "mainwindow.h"
#include "qtimer.h"
#include "runners.h"
#include "communicator.h"
#include "./ui_mainwindow.h"
#include "delayed_buttons.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setFixedSize(size());

    SetDaemonConnectionStateOnGuiThread(ConnState::RED);
    QTimer::singleShot(2000, this, &MainWindow::CreateCommunicator);
}

MainWindow::~MainWindow()
{
    communicator.reset();
    delete ui;
}

void MainWindow::CreateCommunicator()
{
    communicator = utility::startNewRunner([this](const auto shouldStop)
    {
        CleanSharedMemory cleaner;
        try
        {
            CSharedDevice comm;
            while (!(*shouldStop))
            {
                auto info = comm.Communicate();
                auto broken = comm.PossiblyBroken();

                UpdateUiWithInfo(std::move(info), broken);

                using namespace std::chrono_literals;
                std::this_thread::sleep_for(2s);
            }
        }
        catch(std::exception& ex)
        {
            std::cerr << "Exception in communication with daemon, retry soon: " << ex.what() << std::endl;
            //Re-try again from the main thread later.
            ExecOnMainThread::get().exec([this]()
            {
                SetDaemonConnectionStateOnGuiThread(ConnState::RED);
                QTimer::singleShot(5000, this, &MainWindow::CreateCommunicator);
            });
        }
    });
}

void MainWindow::UpdateUiWithInfo(FullInfoBlock info, bool possiblyBrokenConn)
{
    ExecOnMainThread::get().exec([this, info = std::move(info), possiblyBrokenConn]()
    {
        static const QString fmtNum("%1");
        ui->outCpuT->setText(QString(fmtNum).arg(info.info.cpu.temperature));
        ui->outCpuR->setText(QString(fmtNum).arg(info.info.cpu.fanRPM));
        if (0 == info.info.gpu.temperature)
        {
            static const QString offline(tr("Offline"));
            ui->outGpuT->setText(offline);
            ui->outGpuR->setText(offline);
        }
        else
        {
            ui->outGpuT->setText(QString(fmtNum).arg(info.info.gpu.temperature));
            ui->outGpuR->setText(QString(fmtNum).arg(info.info.gpu.fanRPM));
        }

        ui->outBoost->setText(info.boosterState == BoosterState::ON ? tr("On") : tr("Off"));
        ui->outHwProfile->setText(info.behaveState == BehaveState::AUTO ? tr("Auto") : tr("Advanced"));

        if (info.daemonDeviceException.empty())
        {
            SetDaemonConnectionStateOnGuiThread(possiblyBrokenConn ? ConnState::YELLOW : ConnState::GREEN);
        }
        else
        {
            ui->statusbar->showMessage(tr("Device error: ") + QString::fromStdString(
                                           info.daemonDeviceException));
        }
    });
}

void MainWindow::SetDaemonConnectionStateOnGuiThread(const ConnState state)
{
    //must be called on GUI thread!
    static const std::map<ConnState, QString> states =
    {
        {ConnState::GREEN, tr("Daemon is OK.")},
        {ConnState::YELLOW, tr("Daemon is not responding...")},
        {ConnState::RED, tr("No connection to the daemon, retrying each 5s.")},
    };
    ui->statusbar->showMessage(states.at(state));
    setEnabled(state != ConnState::RED);
}
