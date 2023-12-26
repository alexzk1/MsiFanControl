#include "execonmainthread.h"

//object must be constructed inside main thread, best to do in main
ExecOnMainThread::ExecOnMainThread(QObject *parent) : QObject(parent)
{
    qRegisterMetaType<SimpleVoidFunction>("SimpleVoidFunction");
    connect(this, &ExecOnMainThread::needExec, this, &ExecOnMainThread::doExex, Qt::QueuedConnection);
}

void ExecOnMainThread::exec(SimpleVoidFunction func) const
{
    emit needExec(std::move(func));
}

const ExecOnMainThread &ExecOnMainThread::get()
{
    const static ExecOnMainThread tmp;
    return tmp;
}

void ExecOnMainThread::doExex(SimpleVoidFunction lambda)
{
    lambda();
}
