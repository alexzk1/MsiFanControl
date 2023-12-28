#include <stdexcept>
#include <map>

#include <QTimer>
#include <QString>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QCloseEvent>

#include "execonmainthread.h"
#include "mainwindow.h"
#include "qcheckbox.h"
#include "qnamespace.h"
#include "qradiobutton.h"
#include "qtimer.h"
#include "runners.h"
#include "communicator.h"
#include "./ui_mainwindow.h"
#include "gui_helpers.h"

#include "delayed_buttons.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setFixedSize(size());

    connect(ui->btnOn, &QRadioButton::toggled, this, [this](bool on)
    {
        if (on)
        {
            BlockReadSetters();
            UpdateRequestToDaemon([&](RequestFromUi& r)
            {
                r.boosterState = BoosterState::ON;
            });

        }
    });

    connect(ui->btnOff, &QRadioButton::toggled, this, [this](bool on)
    {
        if (on)
        {
            BlockReadSetters();
            UpdateRequestToDaemon([&](RequestFromUi& r)
            {
                r.boosterState = BoosterState::OFF;
            });
        }
    });

    connect(ui->action_Game_Mode, &QAction::triggered, this, [this](bool checked)
    {
        ui->boostGroup->setEnabled(!checked);
        if (checked)
        {
            LaunchGameMode();
        }
        else
        {
            gameModeThread.reset();
        }

        //sync checkbox to the action
        auto block = BlockGuard(ui->cbGameMode, ui->action_Game_Mode);
        ui->cbGameMode->setCheckState(checked ? Qt::CheckState::Checked : Qt::CheckState::Unchecked);
        ui->action_Game_Mode->setChecked(checked);
    });

    //trigger action by checkbox
    connect(ui->cbGameMode, &QCheckBox::stateChanged, ui->action_Game_Mode, &QAction::triggered);

    connect(ui->actionQuit, &QAction::triggered, [this]()
    {
        closing = true;
        show(); //event is not sent to hidden
        close();
    });

    auto trayIconMenu = new QMenu(this);
    trayIconMenu->addAction(ui->action_Game_Mode);
    trayIconMenu->addSeparator();
    trayIconMenu->addAction(ui->actionQuit);
    auto sysTrayIcon = new QSystemTrayIcon(this);
    sysTrayIcon->setContextMenu(trayIconMenu);
    sysTrayIcon->setIcon(QIcon(":/images/fan.png"));
    sysTrayIcon->show();

    connect(sysTrayIcon, &QSystemTrayIcon::activated, [this](auto reason)
    {
        if(reason == QSystemTrayIcon::Trigger)
        {
            if(isVisible())
            {
                hide();
            }
            else
            {
                show();
                activateWindow();
            }
        }
    });

    SetDaemonConnectionStateOnGuiThread(ConnState::RED);
    QTimer::singleShot(2000, this, &MainWindow::CreateCommunicator);
}

MainWindow::~MainWindow()
{
    gameModeThread.reset();
    communicator.reset();
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if(closing)
    {
        event->accept();
    }
    else
    {
        this->hide();
        event->ignore();
    }
}

void MainWindow::LaunchGameMode()
{
    static constexpr int kDegreeLimit = 72;//celsium
    static constexpr int kCpuOnlyDegree = 94; //if cpu is such hot - boost, even if gpu is off
    static_assert(kDegreeLimit < kCpuOnlyDegree, "Revise here.");

    static constexpr std::chrono::seconds kWait(20);

    using namespace std::chrono_literals;

    //If booster is OFF and both temps are above kDegreeLimit -> turn boost ON
    //While is HOT update timestamp.
    //If it is NOT hot, wait timestamp + kWait then turn boost off

    gameModeThread = utility::startNewRunner([this](auto shouldStop)
    {
        FullInfoBlock info;
        std::chrono::time_point<std::chrono::steady_clock> allowToOffAt{std::chrono::steady_clock::now()};

        const auto setBooster = [this](BoosterState state)
        {
            UpdateRequestToDaemon([&](RequestFromUi& r)
            {
                r.boosterState = state;
            });
            std::this_thread::sleep_for(1500ms);
        };

        while (! *(shouldStop))
        {
            std::optional<FullInfoBlock> opt_info;
            {
                std::lock_guard grd(lastReadInfoMutex);
                std::swap(opt_info, lastReadInfo);
            }
            if (opt_info)
            {
                info = std::move(*opt_info);
            }

            const bool isHot = info.info.cpu.temperature > kCpuOnlyDegree
                               || (info.info.cpu.temperature > kDegreeLimit
                                   && info.info.gpu.temperature > kDegreeLimit);

            if (isHot)
            {
                allowToOffAt = std::chrono::steady_clock::now() + kWait;
            }

            if (info.boosterState == BoosterState::OFF)
            {
                if (isHot)
                {
                    setBooster(BoosterState::ON);
                }
            }
            else
            {
                if (!isHot && std::chrono::steady_clock::now() > allowToOffAt)
                {
                    setBooster(BoosterState::OFF);
                }
            }
            std::this_thread::sleep_for(500ms);
        };
    });
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
                std::optional<RequestFromUi> request{std::nullopt};
                {
                    std::lock_guard grd(requestMutex);
                    std::swap(requestToDaemon, request);
                }
                auto info = comm.Communicate(request);
                auto broken = comm.PossiblyBroken();

                UpdateUiWithInfo(std::move(info), broken);

                using namespace std::chrono_literals;
                std::this_thread::sleep_for(1500ms);
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

        SetUiBooster(info.boosterState);
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

        std::lock_guard grd(lastReadInfoMutex);
        lastReadInfo = std::move(info);
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

void MainWindow::SetUiBooster(BoosterState state)
{
    if (!IsReadSettingBlocked())
    {
        auto block = BlockGuard(ui->btnOff, ui->btnOn);
        switch (state)
        {
        case BoosterState::ON:
            ui->btnOn->setChecked(true);
            break;
        case BoosterState::OFF:
            ui->btnOff->setChecked(true);
            break;
        default:
            break;
        }
    }
}
