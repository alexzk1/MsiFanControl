#pragma once

#include <QMainWindow>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>
#include <mutex>
#include <tuple>
#include <chrono>

#include "device.h"
#include <QAction>

QT_BEGIN_NAMESPACE
namespace Ui
{
    class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void CreateCommunicator();

private:
    enum class ConnState {GREEN, RED, YELLOW};
    void UpdateUiWithInfo(FullInfoBlock info, bool possiblyBrokenConn);

    //must be called on GUI thread!
    void SetDaemonConnectionStateOnGuiThread(const ConnState state);

    template <typename taCallable>
    void UpdateRequestToDaemonOnGuiThread(const taCallable& callback)
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
        allowedUpdate = std::chrono::steady_clock::now() + std::chrono::seconds(10);;
    }

    bool IsReadSettingBlocked()
    {
        return std::chrono::steady_clock::now() < allowedUpdate;
    }

    Ui::MainWindow *ui;
    std::shared_ptr<std::thread> communicator;

    std::optional<RequestFromUi> requestToDaemon;
    std::mutex requestMutex;

    std::chrono::time_point<std::chrono::steady_clock> allowedUpdate{std::chrono::steady_clock::now()};
};
