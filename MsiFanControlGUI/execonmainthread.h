#ifndef EXECONMAINTHREAD_H
#define EXECONMAINTHREAD_H

#include <QObject>
#include <functional>

using SimpleVoidFunction = std::function<void()>;

class ExecOnMainThread : public QObject
{
    Q_OBJECT
    explicit ExecOnMainThread(QObject *parent = nullptr);
public:
    void exec(SimpleVoidFunction func) const;
    static const ExecOnMainThread &get();
signals:
    void needExec(SimpleVoidFunction lambda) const;
private slots:
    void doExex(SimpleVoidFunction lambda);
};

#endif // EXECONMAINTHREAD_H
