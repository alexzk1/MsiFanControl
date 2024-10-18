#include <algorithm>
#include <bits/chrono.h>
#include <cstddef>
#include <exception>
#include <iostream>
#include <mutex>
#include <optional>
#include <map>
#include <qbuttongroup.h>
#include <qmainwindow.h>
#include <qrgb.h>
#include <stdexcept>
#include <thread>
#include <utility>
#include <filesystem>

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

#include "execonmainthread.h"

#include "reads_period_detector.h"
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
      systemTray(new QSystemTrayIcon(this)),
      batButtons(new QButtonGroup(this))
{
    ui->setupUi(this);
    setFixedSize(size());

    batButtons->addButton(ui->rbBatBalance);
    batButtons->addButton(ui->rbBatMax);
    batButtons->addButton(ui->rbBatMin);
    UncheckAllBatteryButtons();
    batButtons->setExclusive(true);

    if (!std::filesystem::exists("/sys/class/power_supply/BAT1/charge_control_start_threshold"))
    {
        ui->groupBat->setVisible(false);
        std::cerr <<
                  "Driver which could control the charging is not loaded.\n"
                  "Check: https://github.com/BeardOverflow/msi-ec"
                  << std::endl;
    }

    connect(batButtons, QOverload<QAbstractButton *, bool>::of(
                &QButtonGroup::buttonToggled),
            [this](QAbstractButton *button, bool checked)
    {
        BlockReadSetters();
        UpdateRequestToDaemon([&](RequestFromUi& r)
        {
            if (checked)
            {
                if (button == ui->rbBatBalance)
                {
                    r.battery.maxLevel = BatteryLevels::Balanced;
                }
                if (button == ui->rbBatMax)
                {
                    r.battery.maxLevel = BatteryLevels::BestForMobility;
                }
                if (button == ui->rbBatMin)
                {
                    r.battery.maxLevel = BatteryLevels::BestForBattery;
                }
            }
        });
    });

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
            const ReadsPeriodDetector refreshPeriodDetector(pingOk, comm);

            bool hadUserAction = false;
            for (std::size_t loopsCounter = 0; !(*shouldStop); ++loopsCounter)
            {
                std::optional<RequestFromUi> request{std::nullopt};
                {
                    const std::lock_guard grd(requestMutex);
                    std::swap(requestToDaemon, request);
                }

                if (!request)
                {
                    if (hadUserAction || loopsCounter % refreshPeriodDetector() == 0)
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
                    hadUserAction = false;
                }
                else
                {
                    if ((hadUserAction = request->HasUserAction()))
                    {
                        pingOk = comm.SetBooster(request->boosterState);
                        pingOk = comm.SetBattery(request->battery) || pingOk;
                    }
                }

                UpdateUiWithInfo(comm.LastKnownInfo(), !pingOk);
                if (*shouldStop)
                {
                    break;
                }
                using namespace std::chrono_literals;
                std::this_thread::sleep_for(hadUserAction ? 250ms : 1s);
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
        SetUiBattery(info.battery);

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

void MainWindow::SetUiBattery(const Battery &battery)
{
    const auto block = BlockGuard(ui->rbBatBalance, ui->rbBatMax, ui->rbBatMin);
    switch (battery.maxLevel)
    {
    case BatteryLevels::BestForBattery:
        ui->rbBatMin->setChecked(true);
        break;
    case BatteryLevels::Balanced:
        ui->rbBatBalance->setChecked(true);
        break;
    case BatteryLevels::BestForMobility:
        ui->rbBatMax->setChecked(true);
        break;
    case BatteryLevels::NotKnown:
        UncheckAllBatteryButtons();
        break;
    };
}

void MainWindow::UncheckAllBatteryButtons()
{
    if (batButtons)
    {
        if (batButtons->checkedButton())
        {
            batButtons->setExclusive(false);
            batButtons->checkedButton()->setChecked(false);
            batButtons->setExclusive(true);
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
