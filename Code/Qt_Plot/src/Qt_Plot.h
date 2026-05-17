#pragma once
#include "ui_Qt_Plot.h"
#include <QMainWindow>
#include <QTimer>
#include <fstream>

class Qt_Plot : public QMainWindow
{
    Q_OBJECT

public:
    Qt_Plot(QWidget *parent = nullptr);
    ~Qt_Plot();

    void savePNG_clicked();
    void pause_clicked();
private slots:
    void updateplot();

private:
    Ui_Qt_Plot *ui;
    QTimer *timer;
    double x = 0;
    std::ofstream file;
    bool paused = false;
    //////////////////////////////////////////////////
    // 显示缓存
    //////////////////////////////////////////////////
    QVector<double> plotXs;
    QVector<double> plotYs;
};