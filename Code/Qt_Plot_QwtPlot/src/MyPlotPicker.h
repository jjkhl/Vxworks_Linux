#pragma once

#include <qwt_plot_picker.h>
#include <qwt_plot_marker.h>
#include <qwt_symbol.h>
#include <qwt_text.h>
#include <qwt_plot.h>

#include <QVector>
#include <QMouseEvent>

class MyPlotPicker : public QwtPlotPicker
{
    Q_OBJECT

public:
    static constexpr int CHANNEL_NUM = 4;

    MyPlotPicker(QwtPlot* plot,
                 int xAxis,
                 int yAxis,
                 QWidget* canvas,
                 const QVector<double>* xs,
                 const QVector<double>* ys,
                 bool* paused,
                 bool* channelVisible);

    void moveLeft(int step = 1);
    void moveRight(int step = 1);
    void refreshMarker();

    int currentIndex() const { return m_bestIndex; }
    int currentChannel() const { return m_bestChannel; }

protected:
    virtual QwtText trackerTextF(const QPointF& pos) const override;
    virtual void widgetMousePressEvent(QMouseEvent* e) override;

private:
    void updateMarker(double mouseX, double mouseY) const;

private:
    QwtPlot* m_plot;

    const QVector<double>* m_xs;
    const QVector<double>* m_ys[CHANNEL_NUM];

    bool* m_paused;
    bool* m_channelVisible;

    mutable QwtPlotMarker* m_marker;
    mutable QwtSymbol* m_symbol;

    mutable bool m_locked = false;
    mutable int m_bestChannel = 0;
    mutable int m_bestIndex = 0;
};