#include <algorithm>
#include <cstdint>

#include "plotwidget.h"
#include "ui_plotwidget.h"

namespace {
constexpr std::size_t kFirstSpeedIndexOnGraph = 1;

//Given X from the graph converts to array's index.
std::size_t GraphXToArrayIndex(const std::size_t aGraphIndex)
{
    return aGraphIndex - kFirstSpeedIndexOnGraph;
}

//There are N levels of RPM, without exact values (like speed in the car), N is slower than N+1.
//Number at this level defines temperature when it must be enabled.
void ShowIndexedTempOnGraph(const AddressedValueAnyList& aCurve,
                            const QPointer<QCPGraph>& aGraph)
{
    if (aGraph)
    {
        QVector<double> tempPerRpm;
        tempPerRpm.reserve(aCurve.size());
        std::transform(aCurve.begin(), aCurve.end(), std::back_inserter(tempPerRpm),
                       [](const auto& aVarinat) -> double
        {
            return Info::parseTemp(aVarinat);
        });

        QVector<double> rpms;
        rpms.resize(tempPerRpm.size());
        std::iota(rpms.begin(), rpms.end(), static_cast<double>(kFirstSpeedIndexOnGraph));

        aGraph->setData(rpms, tempPerRpm, true);
    }
}
}

plotwidget::plotwidget(QWidget* parent)
    : QWidget(parent)
    , ui(new Ui::plotwidget)
{
    ui->setupUi(this);

    ui->drawer->xAxis->setLabel(tr("Speed Number"));
    ui->drawer->yAxis->setLabel(tr("Enable at Deg"));

    ui->drawer->yAxis->setRange(0, 110);

    iCpuGraph = ui->drawer->addGraph();
    iGpuGraph = ui->drawer->addGraph();
}

plotwidget::~plotwidget()
{
    delete ui;
}

void plotwidget::SetCurves(CpuGpuFanCurve aLastCurves)
{
    if (aLastCurves != lastIndexedTempCurves)
    {
        lastIndexedTempCurves = std::move(aLastCurves);

        ShowIndexedTempOnGraph(lastIndexedTempCurves.cpu, iCpuGraph);
        ShowIndexedTempOnGraph(lastIndexedTempCurves.gpu, iGpuGraph);

        ui->drawer->xAxis->setRange(kFirstSpeedIndexOnGraph,
                                    lastIndexedTempCurves.cpu.size() - 1 + kFirstSpeedIndexOnGraph);
        ui->drawer->replot();
    }
}
