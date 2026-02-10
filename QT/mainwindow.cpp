#include "mainwindow.h"
#include <QtWidgets>
#include <QTimer>
#include <QRegularExpression>
#include <QFileDialog>
#include <QTextStream>
#include <QSerialPort>
#include <QSerialPortInfo>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    setupUI();
    //serial = new QSerialPort(this);
    //connect(serial, &QSerialPort::readyRead, this, &MainWindow::readData);
    
    //新增线程与worker
    qRegisterMetaType<ParsedSample>("ParsedSample");

    ioThread = new QThread(this);
    worker = new SerialWorker();
    worker->moveToThread(ioThread);

    connect(ioThread, &QThread::finished, worker, &QObject::deleteLater);

    // worker -> UI
    connect(worker, &SerialWorker::sampleReady, this, &MainWindow::onSampleReady, Qt::QueuedConnection);
    connect(worker, &SerialWorker::logLine, this, [this](const QString& s){
        logWindow->append(s);
    }, Qt::QueuedConnection);
    connect(worker, &SerialWorker::errorOccured, this, [this](const QString& s){
        QMessageBox::critical(this, "串口错误", s);
    }, Qt::QueuedConnection);

    ioThread->start();
    /////

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::refreshUI);
    timer->start(16);
}

void MainWindow::setupUI() {
    this->setWindowTitle("ProPower Dual Channel Monitor");
    this->resize(1100, 750);

    // 全局样式表：模拟 HTML 的深色背景和扁平化风格
    this->setStyleSheet(
        "QMainWindow { background-color: #121212; }"
        "QWidget { background-color: #121212; color: #e0e0e0; font-family: 'Segoe UI', 'Consolas'; }"
        "QGroupBox { border: 1px solid #333; border-radius: 8px; margin-top: 10px; padding-top: 10px; font-weight: bold; }"
        "QPushButton { background-color: #333; border: none; border-radius: 4px; padding: 5px 12px; font-weight: bold; min-height: 25px; }"
        "QPushButton:hover { background-color: #444; }"
        "QTextEdit { background-color: #000; border: 1px solid #333; border-radius: 8px; font-family: 'Courier New'; }"
        "QSlider::handle:horizontal { background: #00e676; width: 18px; }"
        );

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    QVBoxLayout *rootLayout = new QVBoxLayout(central);

    // --- 1. Header (连接区域) ---
    QHBoxLayout *header = new QHBoxLayout();
    // 1. 创建下拉框
    portSelector = new QComboBox(this);
    portSelector->setFixedWidth(120);

    //连接按钮
    btnConnect = new QPushButton("连接设备", this);
    btnConnect->setStyleSheet("background-color: #00e676; color: #000;");
    // 绑定点击事件到 toggleSerial 函数
    connect(btnConnect, &QPushButton::clicked, this, &MainWindow::toggleSerial);


    // 刷新按钮
    QPushButton *btnRefresh = new QPushButton("刷新", this);
    btnRefresh->setFixedWidth(50);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::updatePortList);

    // 布局添加
    header->addWidget(new QLabel("端口:", this));
    header->addWidget(portSelector);
    header->addWidget(btnRefresh); // 加入刷新按钮
    header->addWidget(btnConnect);


    // --- 2. Main Body (左侧示波器 + 右侧仪表盘) ---
    QHBoxLayout *mainBody = new QHBoxLayout();

    // 左侧：示波器堆叠
    QVBoxLayout *chartsColumn = new QVBoxLayout();
    scope1 = new Oscilloscope(QColor("#fdd835"), QColor("#ff9800"), QColor("#ff5252"), this);
    scope2 = new Oscilloscope(QColor("#00e5ff"), QColor("#2979ff"), QColor("#d05ce3"), this);

    chartsColumn->addWidget(new QLabel("CHANNEL 1 LIVE"), 0, Qt::AlignLeft);
    chartsColumn->addWidget(scope1, 1);
    chartsColumn->addWidget(new QLabel("CHANNEL 2 LIVE"), 0, Qt::AlignLeft);
    chartsColumn->addWidget(scope2, 1);

    // 历史控制条
    QHBoxLayout *bottomBar = new QHBoxLayout();
    slider = new QSlider(Qt::Horizontal);
    bottomBar->addWidget(new QLabel("历史回放"));
    bottomBar->addWidget(slider);
    chartsColumn->addLayout(bottomBar);

    // 右侧：控制面板
    QWidget *sidePanel = new QWidget();
    sidePanel->setFixedWidth(300);
    QVBoxLayout *sideLayout = new QVBoxLayout(sidePanel);

    // 通道 1 卡片
    QGroupBox *card1 = new QGroupBox("● CHANNEL 1");
    card1->setStyleSheet("QGroupBox { border-left: 3px solid #fdd835; color: #fdd835; }");
    QVBoxLayout *vbox1 = new QVBoxLayout(card1);
    ch1V = new QLabel("--.--- V"); ch1V->setStyleSheet("font-size: 20px; color: #fdd835;");
    ch1I = new QLabel("--.-- mA"); ch1I->setStyleSheet("font-size: 20px; color: #ff9800;");
    ch1P = new QLabel("--.-- mW"); ch1P->setStyleSheet("font-size: 20px; color: #ff5252;");
    vbox1->addWidget(new QLabel("VOLT")); vbox1->addWidget(ch1V);
    vbox1->addWidget(new QLabel("CURR")); vbox1->addWidget(ch1I);
    vbox1->addWidget(new QLabel("PWR")); vbox1->addWidget(ch1P);

    // 通道 2 卡片
    QGroupBox *card2 = new QGroupBox("● CHANNEL 2");
    card2->setStyleSheet("QGroupBox { border-left: 3px solid #00e5ff; color: #00e5ff; }");
    QVBoxLayout *vbox2 = new QVBoxLayout(card2);
    ch2V = new QLabel("--.--- V"); ch2V->setStyleSheet("font-size: 20px; color: #00e5ff;");
    ch2I = new QLabel("--.-- mA"); ch2I->setStyleSheet("font-size: 20px; color: #2979ff;");
    ch2P = new QLabel("--.-- mW"); ch2P->setStyleSheet("font-size: 20px; color: #d05ce3;");
    vbox2->addWidget(new QLabel("VOLT")); vbox2->addWidget(ch2V);
    vbox2->addWidget(new QLabel("CURR")); vbox2->addWidget(ch2I);
    vbox2->addWidget(new QLabel("PWR")); vbox2->addWidget(ch2P);

    // 日志窗口
    logWindow = new QTextEdit();
    logWindow->setPlaceholderText("系统就绪...");

    // 底部按钮
    QPushButton *btnExport = new QPushButton("导出 Excel (CSV)");
    connect(btnExport, &QPushButton::clicked, this, &MainWindow::exportCSV);
    QPushButton *btnClear = new QPushButton("清空");
    connect(btnClear, &QPushButton::clicked, this, &MainWindow::clearAll);

    sideLayout->addWidget(card1);
    sideLayout->addWidget(card2);
    sideLayout->addWidget(logWindow);
    sideLayout->addWidget(btnExport);
    sideLayout->addWidget(btnClear);

    mainBody->addLayout(chartsColumn, 3);
    mainBody->addWidget(sidePanel);
    rootLayout->addLayout(mainBody);
    this->updatePortList();
}

void MainWindow::updatePortList() {
    QString current = portSelector->currentText(); // 记住当前选的
    portSelector->clear();

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        // 显示 端口号 + 描述，方便你确认是不是你的设备
        QString label = info.portName() + " (" + info.description() + ")";
        portSelector->addItem(label, info.portName()); // itemData 存纯端口号
    }

    // 调试打印：看看 Qt 到底识别到了什么
    if (infos.isEmpty()) {
        logWindow->append("未检测到任何串口设备！");
    } else {
        logWindow->append(QString("检测到 %1 个设备").arg(infos.count()));
    }
}

void MainWindow::toggleSerial()
{
    if (connected) {
        // 关闭串口（发给 worker 线程）
        QMetaObject::invokeMethod(worker, "closePort", Qt::QueuedConnection);

        connected = false;
        btnConnect->setText("连接设备");
        btnConnect->setStyleSheet("background-color: #00e676; color: #000;");
        portSelector->setEnabled(true);
        logWindow->append("<font color='gray'>[系统] 串口已断开</font>");
    } 
    else {
        QString portName = portSelector->currentData().toString();
        if (portName.isEmpty()) {
            QMessageBox::warning(this, "错误", "未检测到可用串口！");
            return;
        }

        // 打开串口（发给 worker 线程）
        QMetaObject::invokeMethod(
            worker,
            "openPort",
            Qt::QueuedConnection,
            Q_ARG(QString, portName),
            Q_ARG(int, 115200)
        );

        connected = true;
        btnConnect->setText("断开连接");
        btnConnect->setStyleSheet("background-color: #d32f2f; color: #fff;");
        portSelector->setEnabled(false);
        logWindow->append(
            QString("<font color='#00e676'>[系统] 连接请求已发送：%1</font>").arg(portName)
        );
    }
}


void MainWindow::onSampleReady(const ParsedSample& s) {
    PowerData pt;
    pt.v = s.v;
    pt.i = s.i;
    pt.p = s.p;

    if (s.ch == 1) {
        buf1.push_back(pt);
        ch1V->setText(QString::number(pt.v, 'f', 3) + " V");
        ch1I->setText(QString::number(pt.i, 'f', 1) + " mA");
        ch1P->setText(QString::number(pt.p, 'f', 1) + " mW");
    } else {
        buf2.push_back(pt);
        ch2V->setText(QString::number(pt.v, 'f', 3) + " V");
        ch2I->setText(QString::number(pt.i, 'f', 1) + " mA");
        ch2P->setText(QString::number(pt.p, 'f', 1) + " mW");
    }

    // 批量裁剪（或你后续改 deque/环形）
    static const int kMax=10000, kMargin=2000;
    if ((int)buf1.size() > kMax + kMargin) buf1.erase(buf1.begin(), buf1.end() - kMax);
    if ((int)buf2.size() > kMax + kMargin) buf2.erase(buf2.begin(), buf2.end() - kMax);

    dirty = true;
}
/*
void MainWindow::readData() {
    QByteArray data = serial->readAll();
    serialBuffer += data;

    // 【调试代码】只要收到字节，就打印到日志窗口！
    // 这样你就能确认：1. 连接通没通；2. 收到的格式到底是什么
    if (!data.isEmpty()) {
        // 为了防止刷屏太快，只打印前20个字符示例，或者全部打印
        // logWindow->append("RX: " + QString::fromUtf8(data));
    }

    while (serialBuffer.contains('\n')) {
        int pos = serialBuffer.indexOf('\n');
        // 处理 \r\n 或 \n
        QString line = QString::fromUtf8(serialBuffer.left(pos)).trimmed();
        serialBuffer.remove(0, pos + 1);

        if (!line.isEmpty()) {
            // 【关键】打印解析前的原始行，看看长什么样
            // 如果这里有输出，说明连接成功，是正则写错了
            logWindow->append("Raw Line: " + line);

            parseLine(line);
        }
    }
}

void MainWindow::parseLine(const QString &line) {
    // 忽略一些启动时的干扰信息
    if (line.startsWith("Found") || line.contains("Addr")) {
        return;
    }
    // 正则表达式：匹配带单位的数据
    static QRegularExpression re(R"(CH:(\d)\s+V=\s*([-\d\.]+)\s+V\s+\|\s+I=\s*([-\d\.]+)\s+A\s+\|\s+P=\s*([-\d\.]+)\s+W)");
    QRegularExpressionMatch match = re.match(line);
    if (match.hasMatch()) {
        bool ok;
        int ch = match.captured(1).toInt(&ok);
        double v = match.captured(2).toDouble();       // 电压保持 V
        double i = match.captured(3).toDouble() * 1000.0; // 【修改】 A -> mA
        double p = match.captured(4).toDouble() * 1000.0; // 【修改】 W -> mW
        if (ok) {
            PowerData pt;
            pt.v = v;
            pt.i = i; // 存入的是 mA
            pt.p = p; // 存入的是 mW
            if (ch == 1) {
                buf1.push_back(pt);
                // 【修改】标签显示单位改为 mA 和 mW
                ch1V->setText(QString::number(v, 'f', 3) + " V");
                ch1I->setText(QString::number(i, 'f', 1) + " mA");
                ch1P->setText(QString::number(p, 'f', 1) + " mW");
            } else if (ch == 2) {
                buf2.push_back(pt);
                ch2V->setText(QString::number(v, 'f', 3) + " V");
                ch2I->setText(QString::number(i, 'f', 1) + " mA");
                ch2P->setText(QString::number(p, 'f', 1) + " mW");
            }
            // 【修改点2】 std::vector 不支持 removeFirst()，要用 erase
            if (buf1.size() > 10000) {
                buf1.erase(buf1.begin());∏
            }
            if (buf2.size() > 10000) {
                buf2.erase(buf2.begin());
            }
        }
    } else {
        // 调试用：如果不想看“忽略数据”的刷屏，可以注释掉下面这行
        // if (!line.isEmpty() && line.length() > 10) logWindow->append("忽略: " + line);
    }
}
*/


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
