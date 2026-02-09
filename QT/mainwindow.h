#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>
#include <vector>
#include "oscilloscope.h"
#include <QComboBox>

class QLabel;
class QTextEdit;
class QPushButton;
class QSlider;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow(QWidget *parent = nullptr);

private slots:
    void toggleSerial();
    void readData();
    void updatePortList();
    void refreshUI();
    void exportCSV();
    void clearAll();

private:
    void setupUI();
    void parseLine(const QString &line);

    QComboBox *portSelector;
    QSerialPort *serial;
    QByteArray serialBuffer;
    std::vector<PowerData> buf1, buf2;
    Oscilloscope *scope1, *scope2;
    QLabel *ch1V, *ch1I, *ch1P, *ch2V, *ch2I, *ch2P;
    QTextEdit *logWindow;
    QPushButton *btnConnect;
    QSlider *slider;
    double zoom = 1.0;
    int offset = 0;
};

#endif
