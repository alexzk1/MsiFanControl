#pragma once

#include "cm_ctors.h"

#include <QObject>

#include <qtmetamacros.h>

#include <functional>
#include <iostream>

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

/// @brief Execs callable in destructor of this object.
template <typename taCallable>
class ExecOnExitScope
{
  public:
    explicit ExecOnExitScope(const taCallable &callable) :
        callable(callable) {};
    ExecOnExitScope() = delete;
    NO_COPYMOVE(ExecOnExitScope);
    ~ExecOnExitScope()
    {
        try
        {
            callable();
        }
        catch (...)
        {
            std::cerr << "ExecOnExitScope callable throwed exception." << std::endl;
        }
    }

  private:
    const taCallable &callable;
};
