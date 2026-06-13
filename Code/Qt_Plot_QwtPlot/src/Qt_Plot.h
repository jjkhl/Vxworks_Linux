#pragma once

#include "ui_Qt_Plot.h"
#include <QMainWindow>
#include <QTimer>
#include <QLabel>
#include <QPen>
#include <QBrush>
#include <QVector>
#include <fstream>

#include <qwt_plot.h>
#include <qwt_plot_curve.h>
#include <qwt_plot_grid.h>
#include <qwt_legend.h>
#include <qwt_plot_picker.h>
#include <qwt_text.h>

//////////////////////////////////////////////////////////////
// 自定义 Picker
//////////////////////////////////////////////////////////////
class MyPlotPicker : public QwtPlotPicker
{
public:
    MyPlotPicker(int xAxis, int yAxis, QWidget *canvas, const QVector<double> *xs, const QVector<double> *ys)
            : QwtPlotPicker(xAxis, yAxis, QwtPicker::CrossRubberBand, QwtPicker::AlwaysOn, canvas), m_xs(xs), m_ys(ys)
    {
        setTrackerMode(QwtPicker::AlwaysOn);
        setRubberBandPen(QPen(Qt::red, 1.5));
        setTrackerPen(QPen(Qt::cyan));
    }

protected:
    virtual QwtText trackerTextF(const QPointF &pos) const override
    {
        if (!m_xs || m_xs->isEmpty())
            return QwtText();

        double x = pos.x();

        int closestIndex = 0;
        double minDist = std::abs((*m_xs)[0] - x);

        for (int i = 1; i < m_xs->size(); ++i)
        {
            double dist = std::abs((*m_xs)[i] - x);
            if (dist < minDist)
            {
                minDist = dist;
                closestIndex = i;
            }
        }

        double closestX = (*m_xs)[closestIndex];
        double closestY = (*m_ys)[closestIndex];

        QString text = QString("X: %1\nY: %2").arg(closestX, 0, 'f', 3).arg(closestY, 0, 'f', 3);

        QwtText label(text);
        label.setColor(Qt::white);
        label.setBackgroundBrush(QBrush(QColor(0, 0, 0, 220)));
        label.setBorderPen(QPen(Qt::yellow));
        label.setBorderRadius(4);
        // label.setPadding(6);

        return label;
    }

private:
    const QVector<double> *m_xs;
    const QVector<double> *m_ys;
};

//////////////////////////////////////////////////////////////
// 主窗口
//////////////////////////////////////////////////////////////

class Qt_Plot : public QMainWindow
{
    Q_OBJECT

public:
    explicit Qt_Plot(QWidget *parent = nullptr);
    ~Qt_Plot();

private slots:
    void updateplot();
    void savePNG_clicked();
    void pause_clicked();

private:
    Ui::Qt_Plot *ui;

    QwtPlot *plot = nullptr;
    QwtPlotCurve *curve = nullptr;
    QwtPlotGrid *grid = nullptr;
    MyPlotPicker *picker = nullptr;

    QVector<double> plotXs;
    QVector<double> plotYs;

    QTimer *timer = nullptr;
    std::ofstream file;
    bool paused = false;
};