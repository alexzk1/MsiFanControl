#pragma once

#include <QWidget>
#include <QPointer>

#include "device.h"
#include "qcustomplot.h"

namespace Ui
{
    class plotwidget;
}

class plotwidget : public QWidget
{
    Q_OBJECT

public:
    explicit plotwidget(QWidget *parent = nullptr);
    ~plotwidget();

    void SetCurves(CpuGpuFanCurve aLastCurves);
private:
    Ui::plotwidget *ui;
    CpuGpuFanCurve lastIndexedTempCurves;

    QPointer<QCPGraph> iCpuGraph;
    QPointer<QCPGraph> iGpuGraph;
};
