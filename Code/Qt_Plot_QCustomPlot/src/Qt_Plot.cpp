#include "Qt_Plot.h"
#include <qmessagebox.h>
#include <qpushbutton.h>
#include <QMessageBox>
#include "qcustomplot.h"
#include "shared_queue.h"
#include <cmath>
#include <QDateTime>
#include <QDir>

Qt_Plot::Qt_Plot(QWidget *parent) : QMainWindow(parent), ui(new Ui_Qt_Plot)
{
    ui->setupUi(this);
    connect(ui->pb_snapshot, &QPushButton::clicked, this, &Qt_Plot::savePNG_clicked);
    connect(ui->pb_pause, &QPushButton::clicked, this, &Qt_Plot::pause_clicked);
    ui->widget->addGraph();
    //////////////////////////////////////////////////
    // 示波器风格
    //////////////////////////////////////////////////
    ui->widget->setBackground(Qt::black);

    ui->widget->graph(0)->setPen(QPen(Qt::green));

    ui->widget->xAxis->setBasePen(QPen(Qt::white));
    ui->widget->yAxis->setBasePen(QPen(Qt::white));

    ui->widget->xAxis->setTickPen(QPen(Qt::white));
    ui->widget->yAxis->setTickPen(QPen(Qt::white));

    ui->widget->xAxis->setSubTickPen(QPen(Qt::white));
    ui->widget->yAxis->setSubTickPen(QPen(Qt::white));

    ui->widget->xAxis->setTickLabelColor(Qt::white);
    ui->widget->yAxis->setTickLabelColor(Qt::white);

    ui->widget->xAxis->setLabelColor(Qt::white);
    ui->widget->yAxis->setLabelColor(Qt::white);

    //////////////////////////////////////////////////
    // 坐标轴
    //////////////////////////////////////////////////
    ui->widget->xAxis->setLabel("Time");
    ui->widget->yAxis->setLabel("Value");

    ui->widget->xAxis->setRange(0, 10);
    ui->widget->yAxis->setRange(-10, 10);

    //////////////////////////////////////////////////
    // 日期目录
    //////////////////////////////////////////////////

    //////////////////////////////////////////////////
    // Result目录
    //////////////////////////////////////////////////

    QString rootDir = "Result";

    QDir dir;

    if (!dir.exists(rootDir))
    {
        dir.mkdir(rootDir);
    }

    //////////////////////////////////////////////////
    // 日期目录
    //////////////////////////////////////////////////

    QString dateDir = rootDir + "/" + QDate::currentDate().toString("yyyy-MM-dd");

    if (!dir.exists(dateDir))
    {
        dir.mkdir(dateDir);
    }

    //////////////////////////////////////////////////
    // 时间文件名
    //////////////////////////////////////////////////

    QString timeStr = QTime::currentTime().toString("hh-mm-ss");

    //////////////////////////////////////////////////
    // CSV路径
    //////////////////////////////////////////////////

    QString csvPath = dateDir + "/data_" + timeStr + ".csv";

    //////////////////////////////////////////////////
    // 打开CSV
    //////////////////////////////////////////////////

    file.open(csvPath.toStdString());

    file << "x,y\n";

    //////////////////////////////////////////////////
    // 定时器
    //////////////////////////////////////////////////
    timer = new QTimer(this);

    connect(timer, &QTimer::timeout, this, &Qt_Plot::updateplot);

    timer->start(30);
}

Qt_Plot::~Qt_Plot()
{
    if (file.is_open())
    {
        file.close();
    }
    delete ui;
}

void Qt_Plot::savePNG_clicked()
{
    QString dateDir = "Result/" + QDate::currentDate().toString("yyyy-MM-dd");

    QString timeStr = QDateTime::currentDateTime().toString("hh-mm-ss");

    QString path = dateDir + "/plot_" + timeStr + ".png";

    ui->widget->savePng(path);
}

void Qt_Plot::pause_clicked()
{
    paused = !paused;

    if (paused)
    {
        ui->pb_pause->setText("Continue");
    }
    else
    {
        ui->pb_pause->setText("Pause");
    }
}

void Qt_Plot::updateplot()
{
    //////////////////////////////////////////////////
    // 后台继续收数据
    //////////////////////////////////////////////////
    {
        std::lock_guard<std::mutex> lock(g_mutex);

        while (!g_queue.empty())
        {
            auto e = g_queue.front();

            //////////////////////////////////////////////////
            // 永远缓存
            //////////////////////////////////////////////////
            plotXs.push_back(e.x);

            plotYs.push_back(e.y);

            //////////////////////////////////////////////////
            // CSV记录
            //////////////////////////////////////////////////
            if (file.is_open())
            {
                file << e.x << "," << e.y << "\n";
            }

            g_queue.pop();
        }
    }

    //////////////////////////////////////////////////
    // 暂停时：
    // 不刷新UI
    //////////////////////////////////////////////////
    if (paused)
    {
        return;
    }

    //////////////////////////////////////////////////
    // 限制缓存大小
    //////////////////////////////////////////////////
    const int maxPoints = 3000;

    if (plotXs.size() > maxPoints)
    {
        int removeCount = plotXs.size() - maxPoints;

        plotXs.remove(0, removeCount);

        plotYs.remove(0, removeCount);
    }

    //////////////////////////////////////////////////
    // 整体刷新
    //////////////////////////////////////////////////
    ui->widget->graph(0)->setData(plotXs, plotYs);
    ui->widget->graph(0)->rescaleValueAxis();

    //////////////////////////////////////////////////
    // X轴滚动
    //////////////////////////////////////////////////
    if (!plotXs.isEmpty())
    {
        ui->widget->xAxis->setRange(plotXs.last(), 10, Qt::AlignRight);
    }

    //////////////////////////////////////////////////
    // 重绘
    //////////////////////////////////////////////////
    ui->widget->replot();

    //////////////////////////////////////////////////
    // 定时截图
    //////////////////////////////////////////////////
    // static int saveCount = 0;

    // saveCount++;

    // if (saveCount % 1000 == 0)
    // {
    //     QString dateDir = QDate::currentDate().toString("yyyy-MM-dd");

    //     QString timeStr = QDateTime::currentDateTime().toString("hh-mm-ss");

    //     QString name = dateDir + "/plot_" + timeStr + ".png";

    //     ui->widget->savePng(name);
    // }
}