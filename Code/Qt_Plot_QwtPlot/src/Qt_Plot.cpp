#include "Qt_Plot.h"
#include "shared_queue.h"

#include <QPixmap>
#include <QPushButton>
#include <QCheckBox>
#include <QTimer>
#include <QDateTime>
#include <QDir>
#include <QTime>
#include <QPen>
#include <QColor>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_legend.h>
#include <qwt_text.h>

Qt_Plot::Qt_Plot(QWidget* parent) : QMainWindow(parent), ui(new Ui::Qt_Plot)
{
    ui->setupUi(this);

    connect(ui->pb_snapshot, &QPushButton::clicked, this, &Qt_Plot::savePNG_clicked);

    connect(ui->pb_pause, &QPushButton::clicked, this, &Qt_Plot::pause_clicked);

    plot = ui->widget;

    plot->setTitle("实时曲线");
    plot->setCanvasBackground(Qt::black);

    QColor colors[CHANNEL_NUM] = {Qt::green, Qt::red, Qt::yellow, Qt::cyan};

    QString names[CHANNEL_NUM] = {"CH1", "CH2", "CH3", "CH4"};

    ////////////////////////////////////////////////////
    // Curves
    ////////////////////////////////////////////////////
    for (int i = 0; i < CHANNEL_NUM; i++)
    {
        curves[i] = new QwtPlotCurve(names[i]);
        curves[i]->setPen(QPen(colors[i], 2.0));
        curves[i]->setRenderHint(QwtPlotItem::RenderAntialiased, true);
        curves[i]->attach(plot);
    }

    ////////////////////////////////////////////////////
    // Grid
    ////////////////////////////////////////////////////
    grid = new QwtPlotGrid();
    grid->setPen(QPen(QColor(60, 60, 60), 1, Qt::DotLine));
    grid->attach(plot);

    plot->setAxisTitle(QwtPlot::xBottom, "Time");
    plot->setAxisTitle(QwtPlot::yLeft, "Value");

    plot->setAxisScale(QwtPlot::xBottom, 0, 10);
    plot->setAxisScale(QwtPlot::yLeft, -10, 10);

    plot->insertLegend(new QwtLegend(), QwtPlot::RightLegend);

    ////////////////////////////////////////////////////
    // CSV
    ////////////////////////////////////////////////////
    QString rootDir = "Result";
    QDir().mkpath(rootDir);

    QString dateDir = rootDir + "/" + QDate::currentDate().toString("yyyy-MM-dd");
    QDir().mkpath(dateDir);

    QString timeStr = QTime::currentTime().toString("hh-mm-ss");
    QString csvPath = dateDir + "/data_" + timeStr + ".csv";

    file.open(csvPath.toStdString().c_str());

    if (file.is_open())
    {
        file << "x,ch1,ch2,ch3,ch4\n";
    }

    ////////////////////////////////////////////////////
    // Picker
    ////////////////////////////////////////////////////
    picker = new MyPlotPicker(
            plot, QwtPlot::xBottom, QwtPlot::yLeft, plot->canvas(), &plotXs, plotYs, &paused, channelVisible);

    ////////////////////////////////////////////////////
    // CheckBox
    ////////////////////////////////////////////////////
    connect(ui->cb_ch1,
            &QCheckBox::toggled,
            this,
            [=](bool checked)
            {
                channelVisible[0] = checked;
                curves[0]->setVisible(checked);
                plot->replot();
            });

    connect(ui->cb_ch2,
            &QCheckBox::toggled,
            this,
            [=](bool checked)
            {
                channelVisible[1] = checked;
                curves[1]->setVisible(checked);
                plot->replot();
            });

    connect(ui->cb_ch3,
            &QCheckBox::toggled,
            this,
            [=](bool checked)
            {
                channelVisible[2] = checked;
                curves[2]->setVisible(checked);
                plot->replot();
            });

    connect(ui->cb_ch4,
            &QCheckBox::toggled,
            this,
            [=](bool checked)
            {
                channelVisible[3] = checked;
                curves[3]->setVisible(checked);
                plot->replot();
            });

    ////////////////////////////////////////////////////
    // Timer
    ////////////////////////////////////////////////////
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Qt_Plot::updateplot);
    timer->start(30);

    statusBar()->showMessage("Ready");

    zoomer = new QwtPlotZoomer(plot->canvas());

    zoomer->setRubberBand(QwtPicker::RectRubberBand);
    zoomer->setRubberBandPen(QPen(Qt::green));
    zoomer->setTrackerPen(QPen(Qt::white));

    zoomer->setMousePattern(QwtEventPattern::MouseSelect1, Qt::LeftButton);
    zoomer->setMousePattern(QwtEventPattern::MouseSelect2, Qt::NoButton);
    zoomer->setMousePattern(QwtEventPattern::MouseSelect3, Qt::NoButton);

    panner = new QwtPlotPanner(plot->canvas());
    panner->setMouseButton(Qt::MiddleButton);

    plot->canvas()->installEventFilter(this);

    connect(zoomer, &QwtPlotZoomer::zoomed, this, [=](const QRectF&) { autoScroll = false; });
    connect(ui->pb_reset, &QPushButton::clicked, this, &Qt_Plot::resetView);
}

bool Qt_Plot::eventFilter(QObject* obj, QEvent* event)
{
    if (obj == plot->canvas())
    {
        if (event->type() == QEvent::MouseButtonPress)
        {
            QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);

            if (mouseEvent->button() == Qt::RightButton)
            {
                resetView();
                return true;
            }
        }
    }

    return QMainWindow::eventFilter(obj, event);
}

void Qt_Plot::resetView()
{
    if (plotXs.isEmpty())
        return;

    //////////////////////////////////////
    // Pause模式：恢复暂停时窗口
    //////////////////////////////////////
    if (paused && pausedViewValid)
    {
        plot->setAxisScale(QwtPlot::xBottom, pausedXMin, pausedXMax);
        plot->setAxisAutoScale(QwtPlot::yLeft);

        plot->replot();
        zoomer->setZoomBase();
        return;
    }

    //////////////////////////////////////
    // Live模式：恢复最新窗口
    //////////////////////////////////////
    double latest = rawXs.isEmpty() ? plotXs.last() : rawXs.last();

    plot->setAxisScale(QwtPlot::xBottom, latest - 10.0, latest);
    plot->setAxisAutoScale(QwtPlot::yLeft);

    plot->replot();
    zoomer->setZoomBase();

    autoScroll = true;
}

void Qt_Plot::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::MiddleButton)
    {
        resetView();
    }

    QMainWindow::mouseDoubleClickEvent(event);
}

Qt_Plot::~Qt_Plot()
{
    if (file.is_open())
        file.close();

    delete ui;
}

void Qt_Plot::savePNG_clicked()
{
    QString dateDir = "Result/" + QDate::currentDate().toString("yyyy-MM-dd");

    QDir().mkpath(dateDir);

    QString timeStr = QDateTime::currentDateTime().toString("hh-mm-ss");

    QString path = dateDir + "/plot_" + timeStr + ".png";

    plot->replot();

    QPixmap pixmap(plot->size());
    plot->render(&pixmap);
    pixmap.save(path);
}

void Qt_Plot::pause_clicked()
{
    paused = !paused;

    if (paused)
    {
        autoScroll = false;

        pausedXMin = plot->axisScaleDiv(QwtPlot::xBottom).lowerBound();
        pausedXMax = plot->axisScaleDiv(QwtPlot::xBottom).upperBound();

        pausedViewValid = true;

        ui->pb_pause->setText("Continue");
    }
    else
    {
        rebuildPlotBuffer();
        autoScroll = true;
        pausedViewValid = false;

        ui->pb_pause->setText("Pause");
    }
}

void Qt_Plot::rebuildPlotBuffer()
{
    plotXs.clear();

    for (int i = 0; i < CHANNEL_NUM; i++)
        plotYs[i].clear();

    if (rawXs.isEmpty())
        return;

    const int maxPoints = 3000;

    int start = 0;

    if (rawXs.size() > maxPoints)
        start = rawXs.size() - maxPoints;

    for (int i = start; i < rawXs.size(); i++)
    {
        plotXs.append(rawXs[i]);

        for (int ch = 0; ch < CHANNEL_NUM; ch++)
            plotYs[ch].append(rawYs[ch][i]);
    }
}

void Qt_Plot::updateplot()
{
    //////////////////////////////////////////////
    // 1. 永远接收数据 -> raw buffer
    //////////////////////////////////////////////
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        while (!g_queue.empty())
        {
            Event e = g_queue.front();
            g_queue.pop();

            rawXs.append(e.x);

            for (int i = 0; i < CHANNEL_NUM; i++)
                rawYs[i].append(e.y[i]);

            ////////////////////////////////////////////
            // CSV保存
            ////////////////////////////////////////////
            if (file.is_open())
            {
                file << e.x;

                for (int i = 0; i < CHANNEL_NUM; i++)
                    file << "," << e.y[i];

                file << "\n";
            }
        }
    }

    //////////////////////////////////////////////
    // 2. 非暂停状态更新显示 buffer
    //////////////////////////////////////////////
    if (!paused)
    {
        rebuildPlotBuffer();
    }

    //////////////////////////////////////////////
    // 3. 更新曲线
    //////////////////////////////////////////////
    for (int i = 0; i < CHANNEL_NUM; i++)
    {
        curves[i]->setSamples(plotXs, plotYs[i]);
    }

    //////////////////////////////////////////////
    // 4. 自动滚动
    //////////////////////////////////////////////
    if (!paused && autoScroll && !plotXs.isEmpty())
    {
        double latest = plotXs.last();

        plot->setAxisScale(QwtPlot::xBottom, latest - 10.0, latest);
        plot->setAxisAutoScale(QwtPlot::yLeft);
    }

    plot->replot();
}

void Qt_Plot::keyPressEvent(QKeyEvent* event)
{
    int step = 1;

    if (event->modifiers() & Qt::ShiftModifier)
        step = 10;

    if (event->modifiers() & Qt::ControlModifier)
        step = 100;

    switch (event->key())
    {
        case Qt::Key_Left:
            picker->moveLeft(step);
            updateStatus();
            break;

        case Qt::Key_Right:
            picker->moveRight(step);
            updateStatus();
            break;

        case Qt::Key_Home:
            picker->moveLeft(999999);
            updateStatus();
            break;

        case Qt::Key_End:
            picker->moveRight(999999);
            updateStatus();
            break;

        default:
            QMainWindow::keyPressEvent(event);
    }
}

void Qt_Plot::updateStatus()
{
    if (plotXs.isEmpty())
        return;

    int index = picker->currentIndex();
    int ch = picker->currentChannel();

    if (index < 0 || index >= plotXs.size())
        return;

    double x = plotXs[index];
    double y = plotYs[ch][index];

    QString msg = QString("CH%1 | Index=%2/%3 | X=%4 | Y=%5")
                          .arg(ch + 1)
                          .arg(index)
                          .arg(plotXs.size())
                          .arg(x, 0, 'f', 3)
                          .arg(y, 0, 'f', 3);

    statusBar()->showMessage(msg);
}