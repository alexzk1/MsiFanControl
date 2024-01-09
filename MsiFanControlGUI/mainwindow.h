#pragma once

#include <QMainWindow>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <mutex>
#include <tuple>
#include <chrono>
#include <QSystemTrayIcon>
#include <QPointer>

#include "device.h"
#include <QAction>
#include <QPoint>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

struct StartOptions
{
    bool minimized{false};
    bool game_mode{false};
};

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(StartOptions options, QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void CreateCommunicator();
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    enum class ConnState {GREEN, RED, YELLOW};
    void UpdateUiWithInfo(FullInfoBlock info, bool possiblyBrokenConn);

    //must be called on GUI thread!
    void SetDaemonConnectionStateOnGuiThread(const ConnState state);

    template <typename taCallable>
    void UpdateRequestToDaemon(const taCallable& callback)
    {
        std::lock_guard grd(requestMutex);
        if (!requestToDaemon)
        {
            requestToDaemon = RequestFromUi{};
        }
        callback(*requestToDaemon);
    }

    void SetUiBooster(BoosterState state);

    void BlockReadSetters()
    {
        allowedUpdate = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    }

    bool IsReadSettingBlocked()
    {
        return std::chrono::steady_clock::now() < allowedUpdate;
    }

    void LaunchGameMode();

    void SetImageIcon(std::optional<int> value, const QColor& color = qRgba(0, 0, 0, 0));

    Ui::MainWindow *ui;
    std::shared_ptr<std::thread> communicator;

    std::shared_ptr<std::thread> gameModeThread;

    std::optional<RequestFromUi> requestToDaemon;
    std::mutex requestMutex;

    std::chrono::time_point<std::chrono::steady_clock> allowedUpdate{std::chrono::steady_clock::now()};

    std::optional<FullInfoBlock> lastReadInfo;
    std::mutex lastReadInfoMutex;

    QPointer<QSystemTrayIcon> systemTray;
    bool closing{false};
};
