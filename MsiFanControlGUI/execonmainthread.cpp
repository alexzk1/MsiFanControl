#include "execonmainthread.h"

#include <QObject>
#include <qnamespace.h>
#include <qtmetamacros.h>
#include <utility>

//object must be constructed inside main thread, best to do in main
ExecOnMainThread::ExecOnMainThread(QObject* parent) : QObject(parent)
{
    qRegisterMetaType<SimpleVoidFunction>("SimpleVoidFunction");
    connect(this, &ExecOnMainThread::needExec, this, &ExecOnMainThread::doExex,
            Qt::QueuedConnection);
}

void ExecOnMainThread::exec(SimpleVoidFunction func)
{
    emit needExec(std::move(func));
}

ExecOnMainThread& ExecOnMainThread::get()
{
    static ExecOnMainThread tmp;
    return tmp;
}

void ExecOnMainThread::doExex(const SimpleVoidFunction& lambda)
{
    lambda();
}
