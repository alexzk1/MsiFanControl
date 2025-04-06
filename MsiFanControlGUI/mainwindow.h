#pragma once

#include "cm_ctors.h" // IWYU pragma: keep
#include "device.h"   // IWYU pragma: keep

#include <QAction>
#include <QButtonGroup>
#include <QMainWindow>
#include <QPoint>
#include <QPointer>
#include <QSystemTrayIcon>

#include <qcolor.h>
#include <qpointer.h>
#include <qrgb.h>
#include <qtconfigmacros.h>
#include <qtmetamacros.h>
#include <qwidget.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

struct StartOptions
{
    bool minimized{false};
    bool game_mode{false};
};

class MainWindow final : public QMainWindow
{
    Q_OBJECT

  public:
    NO_COPYMOVE(MainWindow);
    explicit MainWindow(StartOptions options, QWidget *parent = nullptr);
    ~MainWindow() final;

  private slots:
    void CreateCommunicator();

  protected:
    void closeEvent(QCloseEvent *event) override;

  private:
    enum class ConnState : std::uint8_t {
        GREEN,
        RED,
        YELLOW
    };
    void UpdateUiWithInfo(FullInfoBlock info, bool possiblyBrokenConn);

    // must be called on GUI thread!
    void SetDaemonConnectionStateOnGuiThread(const ConnState state);

    template <typename taCallable>
    void UpdateRequestToDaemon(const taCallable &callback)
    {
        const std::lock_guard grd(requestMutex);
        if (!requestToDaemon)
        {
            requestToDaemon = RequestFromUi{};
        }
        callback(*requestToDaemon);
    }

    void SetUiBooster(const BoostersStates &state);
    void SetUiBattery(const Battery &battery);
    void UncheckAllBatteryButtons();

    void BlockReadSetters()
    {
        allowedUpdate = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    }

    bool IsReadSettingBlocked()
    {
        return std::chrono::steady_clock::now() < allowedUpdate;
    }

    void LaunchGameMode();

    void SetImageIcon(std::optional<int> value, const QColor &color = qRgba(0, 0, 0, 0));

    void ReadCurvesFromDaemon(BehaveWithCurve curves);

    Ui::MainWindow *ui;
    std::shared_ptr<std::thread> communicator;

    std::shared_ptr<std::thread> gameModeThread;

    std::optional<RequestFromUi> requestToDaemon;
    std::mutex requestMutex;

    std::chrono::time_point<std::chrono::steady_clock> allowedUpdate{
      std::chrono::steady_clock::now()};

    std::optional<FullInfoBlock> lastReadInfoForGameModeThread;
    std::mutex lastReadInfoForGameModeThreadMutex;

    QPointer<QSystemTrayIcon> systemTray;
    QPointer<QButtonGroup> batButtons;
    bool closing{false};
};
