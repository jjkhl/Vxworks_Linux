#include "Qt_Plot.h"
#include "shared_queue.h"
#include <QPixmap>
#include <QMessageBox>
#include <QPushButton>
#include <QTimer>
#include <QDateTime>
#include <QDir>
#include <QTime>

// ==================== 新增这些头文件（解决 QPen 错误） ====================
#include <QPen>
#include <QColor>

// Qwt 头文件
#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_legend.h>
#include <qwt_text.h>
#include <qwt_plot_picker.h>
#include <qwt_picker_machine.h>

Qt_Plot::Qt_Plot(QWidget *parent) : QMainWindow(parent), ui(new Ui::Qt_Plot)
{
    ui->setupUi(this);

    connect(ui->pb_snapshot, &QPushButton::clicked, this, &Qt_Plot::savePNG_clicked);
    connect(ui->pb_pause, &QPushButton::clicked, this, &Qt_Plot::pause_clicked);

    // ==================== QwtPlot 初始化 ====================
    plot = ui->widget;

    plot->setTitle("实时曲线");
    plot->setCanvasBackground(Qt::black);

    // 曲线
    curve = new QwtPlotCurve("Signal");
    curve->setPen(QPen(Qt::green, 2.0));  // 已修复
    curve->setRenderHint(QwtPlotItem::RenderAntialiased, true);
    curve->attach(plot);

    // 网格
    grid = new QwtPlotGrid();
    grid->setPen(QPen(QColor(60, 60, 60), 1, Qt::DotLine));  // 已修复
    grid->attach(plot);

    // 坐标轴
    plot->setAxisTitle(QwtPlot::xBottom, "Time");
    plot->setAxisTitle(QwtPlot::yLeft, "Value");

    QwtText titleX("Time"), titleY("Value");
    titleX.setColor(Qt::white);
    titleY.setColor(Qt::white);
    plot->setAxisTitle(QwtPlot::xBottom, titleX);
    plot->setAxisTitle(QwtPlot::yLeft, titleY);

    plot->setAxisScale(QwtPlot::xBottom, 0, 10);
    plot->setAxisScale(QwtPlot::yLeft, -10, 10);

    plot->insertLegend(new QwtLegend(), QwtPlot::RightLegend);

    // ==================== CSV 文件 ====================
    QString rootDir = "Result";
    QDir dir;
    if (!dir.exists(rootDir))
        dir.mkdir(rootDir);

    QString dateDir = rootDir + "/" + QDate::currentDate().toString("yyyy-MM-dd");
    if (!dir.exists(dateDir))
        dir.mkdir(dateDir);

    QString timeStr = QTime::currentTime().toString("hh-mm-ss");
    QString csvPath = dateDir + "/data_" + timeStr + ".csv";

    file.open(csvPath.toStdString().c_str());
    if (file.is_open())
        file << "x,y\n";

    // ==================== 定时器 ====================
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &Qt_Plot::updateplot);
    timer->start(30);

    // 采样器
    picker = new MyPlotPicker(QwtPlot::xBottom, QwtPlot::yLeft, plot->canvas(), &plotXs, &plotYs);
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
    // plot->canvas()->grab().save(path);
    QPixmap pixmap(plot->size());
    plot->render(&pixmap);

    pixmap.save(path);
}

void Qt_Plot::pause_clicked()
{
    paused = !paused;
    ui->pb_pause->setText(paused ? "Continue" : "Pause");
}

void Qt_Plot::updateplot()
{
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        while (!g_queue.empty())
        {
            Event e = g_queue.front();
            g_queue.pop();

            plotXs.append(e.x);
            plotYs.append(e.y);

            if (file.is_open())
                file << e.x << "," << e.y << "\n";
        }
    }

    if (paused)
        return;

    // 限制点数
    const int maxPoints = 3000;
    if (plotXs.size() > maxPoints)
    {
        int removeCount = plotXs.size() - maxPoints;
        plotXs.remove(0, removeCount);
        plotYs.remove(0, removeCount);
    }

    curve->setSamples(plotXs, plotYs);

    if (!plotXs.isEmpty())
    {
        double latest = plotXs.last();
        plot->setAxisScale(QwtPlot::xBottom, latest - 10.0, latest);
    }

    plot->setAxisAutoScale(QwtPlot::yLeft);
    plot->replot();
}