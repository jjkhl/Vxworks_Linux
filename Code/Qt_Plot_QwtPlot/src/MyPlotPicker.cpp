#include "MyPlotPicker.h"
#include <QPen>
#include <QBrush>
#include <cmath>

MyPlotPicker::MyPlotPicker(QwtPlot* plot,
                           int xAxis,
                           int yAxis,
                           QWidget* canvas,
                           const QVector<double>* xs,
                           const QVector<double>* ys,
                           bool* paused,
                           bool* channelVisible)
    : QwtPlotPicker(xAxis, yAxis,
                    QwtPicker::CrossRubberBand,
                    QwtPicker::AlwaysOn,
                    canvas),
      m_plot(plot),
      m_xs(xs),
      m_paused(paused),
      m_channelVisible(channelVisible)
{
    for (int i = 0; i < CHANNEL_NUM; i++)
        m_ys[i] = &ys[i];

    setTrackerMode(QwtPicker::AlwaysOn);
    setRubberBandPen(QPen(Qt::red, 1.5));
    setTrackerPen(QPen(Qt::cyan));

    m_marker = new QwtPlotMarker();

    m_symbol = new QwtSymbol(
        QwtSymbol::Ellipse,
        QBrush(Qt::red),
        QPen(Qt::white, 2),
        QSize(10, 10));

    m_marker->setSymbol(m_symbol);
    m_marker->attach(m_plot);
}

void MyPlotPicker::moveLeft(int step)
{
    if (!m_xs || m_xs->isEmpty())
        return;

    m_bestIndex -= step;
    if (m_bestIndex < 0)
        m_bestIndex = 0;

    refreshMarker();
}

void MyPlotPicker::moveRight(int step)
{
    if (!m_xs || m_xs->isEmpty())
        return;

    m_bestIndex += step;
    int maxIndex = m_xs->size() - 1;

    if (m_bestIndex > maxIndex)
        m_bestIndex = maxIndex;

    refreshMarker();
}

void MyPlotPicker::refreshMarker()
{
    if (!m_xs || m_xs->isEmpty())
        return;

    double x = (*m_xs)[m_bestIndex];
    double y = (*m_ys[m_bestChannel])[m_bestIndex];

    m_marker->setValue(x, y);
    m_plot->replot();
}

void MyPlotPicker::updateMarker(double mouseX, double mouseY) const
{
    if (!m_xs || m_xs->isEmpty())
        return;

    int closestIndex = 0;
    double minXDist = std::abs((*m_xs)[0] - mouseX);

    for (int i = 1; i < m_xs->size(); i++)
    {
        double dist = std::abs((*m_xs)[i] - mouseX);
        if (dist < minXDist)
        {
            minXDist = dist;
            closestIndex = i;
        }
    }

    int bestChannel = -1;
    double minYDist = 1e100;

    for (int ch = 0; ch < CHANNEL_NUM; ch++)
    {
        if (!m_channelVisible[ch])
            continue;

        double dist = std::abs((*m_ys[ch])[closestIndex] - mouseY);

        if (dist < minYDist)
        {
            minYDist = dist;
            bestChannel = ch;
        }
    }

    if (bestChannel < 0)
        return;

    m_bestChannel = bestChannel;
    m_bestIndex = closestIndex;

    double x = (*m_xs)[closestIndex];
    double y = (*m_ys[bestChannel])[closestIndex];

    QColor colors[CHANNEL_NUM] =
    {
        Qt::green,
        Qt::red,
        Qt::yellow,
        Qt::cyan
    };

    m_symbol->setBrush(QBrush(colors[bestChannel]));

    m_marker->setValue(x, y);
    m_plot->replot();
}

QwtText MyPlotPicker::trackerTextF(const QPointF& pos) const
{
    if (!m_xs || m_xs->isEmpty())
        return QwtText();

    if (!m_locked)
        updateMarker(pos.x(), pos.y());

    double x = (*m_xs)[m_bestIndex];
    double y = (*m_ys[m_bestChannel])[m_bestIndex];

    QString text = QString("CH%1\nX:%2\nY:%3")
                       .arg(m_bestChannel + 1)
                       .arg(x, 0, 'f', 3)
                       .arg(y, 0, 'f', 3);

    QwtText label(text);
    label.setColor(Qt::white);
    label.setBackgroundBrush(QBrush(QColor(0, 0, 0, 220)));
    label.setBorderPen(QPen(Qt::yellow));
    label.setBorderRadius(4);

    return label;
}

void MyPlotPicker::widgetMousePressEvent(QMouseEvent* e)
{
    if (*m_paused)
    {
        if (e->button() == Qt::LeftButton)
        {
            m_locked = !m_locked;
        }
    }

    QwtPlotPicker::widgetMousePressEvent(e);
}