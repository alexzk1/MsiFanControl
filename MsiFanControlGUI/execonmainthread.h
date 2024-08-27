#ifndef EXECONMAINTHREAD_H
#define EXECONMAINTHREAD_H

#include <QObject>
#include <functional>
#include <qtmetamacros.h>

using SimpleVoidFunction = std::function<void()>;

class ExecOnMainThread : public QObject
{
    Q_OBJECT
    explicit ExecOnMainThread(QObject* parent = nullptr);
public:
    void exec(SimpleVoidFunction func);
    static ExecOnMainThread& get();
signals:
    void needExec(SimpleVoidFunction lambda);
private slots:
    void doExex(const SimpleVoidFunction& lambda);
};

#endif // EXECONMAINTHREAD_H
