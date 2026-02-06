#include "mainwindow.h"
#include <QtWidgets>
#include <QTimer>
#include <QRegularExpression>
#include <QFileDialog>
#include <QTextStream>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    serial = new QSerialPort(this);
    connect(serial, &QSerialPort::readyRead, this, &MainWindow::readData);
    
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::refreshUI);
    timer->start(16);
}

void MainWindow::setupUI() {
    setWindowTitle("ProPower Dual Channel Monitor (Qt6)");
    setStyleSheet("QMainWindow { background: #121212; } QLabel { color: #e0e0e0; }");

    QWidget *central = new QWidget();
    QHBoxLayout *mainLayout = new QHBoxLayout(central);

    // 左侧绘图区
    QVBoxLayout *leftLayout = new QVBoxLayout();
    scope1 = new Oscilloscope(QColor("#fdd835"), QColor("#ff9800"), QColor("#ff5252"));
    scope2 = new Oscilloscope(QColor("#00e5ff"), QColor("#2979ff"), QColor("#d05ce3"));
    slider = new QSlider(Qt::Horizontal);
    leftLayout->addWidget(scope1, 3);
    leftLayout->addWidget(scope2, 3);
    leftLayout->addWidget(slider);

    // 右侧面板
    QVBoxLayout *sidePanel = new QVBoxLayout();
    sidePanel->setFixedWidth(280);

    auto createMeter = [&](QString title, QColor col, QLabel*& v, QLabel*& i, QLabel*& p) {
        QGroupBox *gb = new QGroupBox(title);
        gb->setStyleSheet(QString("QGroupBox { color: %1; font-weight: bold; border: 1px solid #333; }").arg(col.name()));
        QVBoxLayout *vbox = new QVBoxLayout(gb);
        v = new QLabel("--.--- V"); i = new QLabel("--.-- mA"); p = new QLabel("--.-- mW");
        vbox->addWidget(v); vbox->addWidget(i); vbox->addWidget(p);
        return gb;
    };

    sidePanel->addWidget(createMeter("CHANNEL 1", QColor("#fdd835"), ch1V, ch1I, ch1P));
    sidePanel->addWidget(createMeter("CHANNEL 2", QColor("#00e5ff"), ch2V, ch2I, ch2P));

    logWindow = new QTextEdit();
    logWindow->setReadOnly(true);
    logWindow->setStyleSheet("background: black; color: #888; font-family: Consolas; font-size: 9pt;");
    sidePanel->addWidget(logWindow);

    btnConnect = new QPushButton("连接设备");
    btnConnect->setFixedHeight(40);
    connect(btnConnect, &QPushButton::clicked, this, &MainWindow::toggleSerial);
    sidePanel->addWidget(btnConnect);

    QPushButton *btnExport = new QPushButton("导出 Excel (CSV)");
    connect(btnExport, &QPushButton::clicked, this, &MainWindow::exportCSV);
    sidePanel->addWidget(btnExport);

    mainLayout->addLayout(leftLayout);
    mainLayout->addLayout(sidePanel);
    setCentralWidget(central);
}

void MainWindow::toggleSerial() {
    if (serial->isOpen()) {
        serial->close();
        btnConnect->setText("连接设备");
    } else {
        serial->setPortName("COM3"); // 实际应通过 QSerialPortInfo 获取
        serial->setBaudRate(QSerialPort::Baud115200);
        if (serial->open(QIODevice::ReadWrite)) btnConnect->setText("断开连接");
    }
}

void MainWindow::readData() {
    serialBuffer += serial->readAll();
    while (serialBuffer.contains('\n')) {
        int pos = serialBuffer.indexOf('\n');
        QString line = QString::fromUtf8(serialBuffer.left(pos)).trimmed();
        serialBuffer.remove(0, pos + 1);
        parseLine(line);
    }
}

void MainWindow::parseLine(const QString &line) {
    static QRegularExpression re("CH:(\\d)\\s+V=([-|\\d\\.]+)\\s+I=([-|\\d\\.]+)\\s+P=([-|\\d\\.]+)");
    QRegularExpressionMatch match = re.match(line);
    if (match.hasMatch()) {
        int ch = match.captured(1).toInt();
        PowerData d = { match.captured(2).toDouble(), match.captured(3).toDouble()*1000, match.captured(4).toDouble()*1000 };
        if (ch == 1) {
            buf1.push_back(d); if(buf1.size() > 5000) buf1.erase(buf1.begin());
            ch1V->setText(QString::number(d.v, 'f', 3) + " V");
        } else {
            buf2.push_back(d); if(buf2.size() > 5000) buf2.erase(buf2.begin());
            ch2V->setText(QString::number(d.v, 'f', 3) + " V");
        }
        logWindow->append(line);
    }
}

void MainWindow::refreshUI() {
    scope1->setData(&buf1, offset, zoom);
    scope2->setData(&buf2, offset, zoom);
}

void MainWindow::exportCSV() {
    QString path = QFileDialog::getSaveFileName(this, "保存 CSV", "", "CSV Files (*.csv)");
    if (path.isEmpty()) return;
    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        QTextStream out(&file);
        out << "Index,CH1_V,CH1_I,CH1_P,CH2_V,CH2_I,CH2_P\n";
        int len = qMax(buf1.size(), buf2.size());
        for (int i=0; i<len; ++i) {
            out << i << "," << (i<buf1.size()?buf1[i].v:0) << "," << (i<buf1.size()?buf1[i].i:0) << "," << (i<buf1.size()?buf1[i].p:0) << ","
                << (i<buf2.size()?buf2[i].v:0) << "," << (i<buf2.size()?buf2[i].i:0) << "," << (i<buf2.size()?buf2[i].p:0) << "\n";
        }
    }
}

void MainWindow::clearAll() {
    buf1.clear(); buf2.clear(); logWindow->clear();
}
