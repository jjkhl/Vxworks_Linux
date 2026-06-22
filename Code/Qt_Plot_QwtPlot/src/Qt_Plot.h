#pragma once

#include "shared_queue.h"
#include "ui_Qt_Plot.h"
#include "MyPlotPicker.h"

#include <QMainWindow>
#include <QTimer>
#include <QEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <fstream>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_plot_zoomer.h>
#include <qwt_plot_panner.h>
#include <QwtScaleDiv>

class Qt_Plot : public QMainWindow
{
    Q_OBJECT

public:
    explicit Qt_Plot(QWidget* parent = nullptr);
    ~Qt_Plot();

    void updateStatus();
    void rebuildPlotBuffer();

private slots:
    void updateplot();
    void savePNG_clicked();
    void pause_clicked();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    bool eventFilter(QObject* obj, QEvent* event) override;

private slots:
    void resetView();

private:
    Ui::Qt_Plot* ui;

    QwtPlot* plot = nullptr;
    QwtPlotCurve* curves[CHANNEL_NUM];
    QwtPlotGrid* grid = nullptr;
    MyPlotPicker* picker = nullptr;

    QVector<double> plotXs;
    QVector<double> plotYs[CHANNEL_NUM];

    QTimer* timer = nullptr;
    std::ofstream file;

    bool paused = false;
    bool channelVisible[CHANNEL_NUM] = {true, true, true, true};

    QwtPlotZoomer* zoomer = nullptr;
    QwtPlotPanner* panner = nullptr;

    bool autoScroll = true;

    QVector<double> rawXs;
    QVector<double> rawYs[CHANNEL_NUM];

    double pausedXMin = 0;
    double pausedXMax = 10;
    bool pausedViewValid = false;

protected:
    void keyPressEvent(QKeyEvent* event) override;
};