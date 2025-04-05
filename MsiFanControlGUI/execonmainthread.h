#pragma once

#include <QObject>

#include <qtmetamacros.h>

#include <functional>

using SimpleVoidFunction = std::function<void()>;

/// @brief Execs callable on main (GUI) thread. This should be used if thread needs to call Qt/gui's
/// methods.
class ExecOnMainThread : public QObject
{
    Q_OBJECT
    explicit ExecOnMainThread(QObject *parent = nullptr);

  public:
    void exec(SimpleVoidFunction func);
    static ExecOnMainThread &get();
  signals:
    void needExec(SimpleVoidFunction lambda);
  private slots:
    void doExex(const SimpleVoidFunction &lambda);
};
