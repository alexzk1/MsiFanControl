#include <algorithm>
#include <bits/chrono.h>
#include <cstddef>
#include <exception>
#include <iostream>
#include <mutex>
#include <optional>
#include <map>
#include <qmainwindow.h>
#include <qrgb.h>
#include <stdexcept>
#include <thread>
#include <utility>

#include <QTimer>
#include <QString>
#include <QSystemTrayIcon>
#include <QMenu>
#include <QAction>
#include <QMessageBox>
#include <QCloseEvent>
#include <QPainter>
#include <QIcon>
#include <QImage>
#include <QPixmap>
#include <vector>

#include "execonmainthread.h"

#include "qcheckbox.h"
#include "qnamespace.h"
#include "qradiobutton.h"
#include "qtimer.h"
#include "runners.h"
#include "communicator.h"
#include "gui_helpers.h"
#include "booster_onoff_decider.h"

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "delayed_buttons.h"

MainWindow::MainWindow(StartOptions options, QWidget* parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow),
      systemTray(new QSystemTrayIcon(this))
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
        ui->cbGameMode->setCheckState(checked ? Qt::CheckState::Checked :
                                      Qt::CheckState::Unchecked);
        ui->action_Game_Mode->setChecked(checked);
    });

    //trigger action by checkbox
    connect(ui->cbGameMode, &QCheckBox::stateChanged, ui->action_Game_Mode,
            &QAction::triggered);

    connect(ui->actionQuit, &QAction::triggered, this, [this]()
    {
        closing = true;
        show(); //event is not sent to hidden
        close();
    });

    {
        //NOLINTNEXTLINE
        auto trayIconMenu = new QMenu(this);
        trayIconMenu->addAction(ui->action_Game_Mode);
        trayIconMenu->addSeparator();
        trayIconMenu->addAction(ui->actionQuit);
        systemTray->setContextMenu(trayIconMenu);
    }

    SetImageIcon(std::nullopt);
    systemTray->show();

    connect(systemTray, &QSystemTrayIcon::activated, this, [this](auto reason)
    {
        if (reason == QSystemTrayIcon::Trigger)
        {
            if (isVisible())
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
    QTimer::singleShot(500, this, [this, options]()
    {
        CreateCommunicator();
        if (options.minimized)
        {
            hide();
        }

        if (options.game_mode)
        {
            ui->action_Game_Mode->setChecked(true);
            ui->cbGameMode->setCheckState(Qt::CheckState::Checked);
            ui->boostGroup->setEnabled(false);
            LaunchGameMode();
        }
    });
}

MainWindow::~MainWindow()
{
    gameModeThread.reset();
    communicator.reset();
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (closing)
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
    using namespace std::chrono_literals;

    //If booster is OFF and both temps are above kDegreeLimit -> turn boost ON
    //While is HOT update timestamp.
    //If it is NOT hot, wait timestamp + kWait then turn boost off

    gameModeThread = utility::startNewRunner([this](auto shouldStop)
    {
        BoosterOnOffDecider<3> decider;

        while (! *(shouldStop))
        {
            std::optional<FullInfoBlock> optInfo;
            {
                const std::lock_guard grd(lastReadInfoForGameModeThreadMutex);
                std::swap(optInfo, lastReadInfoForGameModeThread);
            }
            const auto state = decider.GetUpdatedState(std::move(optInfo));
            if (state != BoosterState::NO_CHANGE)
            {
                UpdateRequestToDaemon([&state](RequestFromUi& r)
                {
                    r.boosterState = state;
                });
            }
            std::this_thread::sleep_for(kMinimumServiceDelay + 500ms);
        };
    });
}

void MainWindow::CreateCommunicator()
{
    communicator = utility::startNewRunner([this](const auto shouldStop) mutable
    {
        try
        {
            CSharedDevice comm(shouldStop);
            bool pingOk = true;

            // We're trying to read data is less as possible because each read triggers IRQ9 which
            // leads to more power consumption eventually. However, when CPU is hot, it means it is loaded,
            // so IRQ-9 will not add too much and we can call check more often.
            // So what we do here is dynamic range - higher the temp - more often we do
            // update from the daemon.
            const auto selectRefreshPeriod = [&comm, &pingOk]()
            {
                //This must be ordered by the temp.
                using temp_div_pair = std::pair<std::uint16_t, std::size_t>;
                static const std::vector<temp_div_pair> tempDivPairs =
                {
                    {0, 1}, //it was no reads yet, avoid delays.
                    {39, 35}, // no user around ?
                    {42, 23},
                    {45, 17},
                    {47, 15},
                    {50, 13},
                    {60, 10},
                    {65, 7},
                    {70, 2},
                };

                //If ping failed, request updates as fast as possible.
                if (!pingOk)
                {
                    return tempDivPairs.back().second;
                }

                const auto it = std::lower_bound(tempDivPairs.begin(), tempDivPairs.end(),
                                                 comm.LastKnownInfo().info.cpu.temperature,
                                                 [](const temp_div_pair& pair, std::uint16_t temp)
                {
                    return pair.first < temp;
                });
                if (it == tempDivPairs.end())
                {
                    return tempDivPairs.back().second;
                };
                return it->second;
            };

            for (std::size_t loopsCounter = 0; !(*shouldStop); ++loopsCounter)
            {
                std::optional<RequestFromUi> request{std::nullopt};
                {
                    const std::lock_guard grd(requestMutex);
                    std::swap(requestToDaemon, request);
                }

                if (!request)
                {
                    if (loopsCounter % selectRefreshPeriod() == 0)
                    {
                        pingOk = comm.RefreshData();
                    }
                    else
                    {
                        if (loopsCounter % 3 == 0)
                        {
                            if (!comm.PingDaemon())
                            {
                                throw std::runtime_error("Possibly daemon was stopped.");
                            }
                        }
                    }
                }
                else
                {
                    if (request->boosterState != BoosterState::NO_CHANGE)
                    {
                        pingOk = comm.SetBooster(request->boosterState);
                    }
                }

                UpdateUiWithInfo(comm.LastKnownInfo(), !pingOk);
                if (*shouldStop)
                {
                    break;
                }
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(1s);
            }
        }
        catch (std::exception& ex)
        {
            std::cerr << "Exception in communication with daemon, retry soon: "
                      << ex.what() << std::endl;
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
    ExecOnMainThread::get().exec([this, info = std::move(info),
                                  possiblyBrokenConn]() mutable
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
        ui->outHwProfile->setText(info.behaveAndCurve.behaveState == BehaveState::AUTO ?
                                  tr("Auto") :
                                  tr("Advanced"));

        if (info.daemonDeviceException.empty())
        {
            SetDaemonConnectionStateOnGuiThread(possiblyBrokenConn ? ConnState::YELLOW :
                                                ConnState::GREEN);
        }
        else
        {
            ui->statusbar->showMessage(tr("Device error: ") + QString::fromStdString(
                                           info.daemonDeviceException));
        }

        SetImageIcon(info.info.cpu.temperature, Qt::green);
        ReadCurvesFromDaemon(std::move(info.behaveAndCurve));

        const std::lock_guard grd(lastReadInfoForGameModeThreadMutex);
        lastReadInfoForGameModeThread = std::move(info);
    });
}

void MainWindow::SetDaemonConnectionStateOnGuiThread(const ConnState state)
{
    //must be called on GUI thread!
    static const std::map<ConnState, QString> states =
    {
        {ConnState::GREEN, tr("Daemon is OK.")},
        {ConnState::YELLOW, tr("Daemon is not responding...")},
        {ConnState::RED, tr("No connection to the daemon, retrying...")},
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

void MainWindow::SetImageIcon(std::optional<int> value, const QColor& color)
{
    static const QIcon icon(":/images/fan.png");
    if (!value || isVisible())
    {
        systemTray->setIcon(icon);
    }
    else
    {
        QImage image(25, 25, QImage::Format_RGBA8888);
        image.fill(qRgba(0, 0, 0, 0));
        QPainter p;
        if (!p.begin(&image))
        {
            systemTray->setIcon(icon);
            return;
        }

        p.setPen(QPen(color));
        p.setFont(QFont("Times", 14, QFont::Bold));
        p.drawText(image.rect(), Qt::AlignCenter, QString("%1°").arg(*value));
        p.end();

        systemTray->setIcon(QIcon(QPixmap::fromImage(image)));
    }
}

void MainWindow::ReadCurvesFromDaemon(BehaveWithCurve curves)
{
    //TODO: Parse BehaveState and send curve to the dedicated widget
    //{AUTO, ADVANCED, NO_CHANGE};
    ui->curvesWidget->SetCurves(std::move(curves.curve));
}
