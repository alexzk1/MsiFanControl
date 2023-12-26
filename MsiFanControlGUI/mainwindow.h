#pragma once

#include <QMainWindow>
#include <memory>
#include <thread>
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

    Ui::MainWindow *ui;
    std::shared_ptr<std::thread> communicator;
};
