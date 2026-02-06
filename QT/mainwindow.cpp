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
    portSelector = new QComboBox();
    portSelector->setFixedWidth(120);
    updatePortList(); // 初始化时扫描一次串口

    header->addWidget(new QLabel("选择端口:"));
    header->addWidget(portSelector);
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
}
#include <QtSerialPort/QSerialPortInfo>

void MainWindow::updatePortList() {
    portSelector->clear();
    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        portSelector->addItem(info.portName());
    }
}

void MainWindow::toggleSerial() {
    if (serial->isOpen()) {
        // 如果已经打开，则执行关闭
        serial->close();
        btnConnect->setText("连接设备");
        btnConnect->setStyleSheet("background-color: #00e676; color: #000;");
        portSelector->setEnabled(true);
        logWindow->append("<font color='gray'>[系统] 串口已断开</font>");
    } else {
        // 如果是关闭状态，则尝试打开
        QString portName = portSelector->currentText();
        if (portName.isEmpty()) {
            QMessageBox::warning(this, "错误", "未检测到可用串口！");
            return;
        }

        serial->setPortName(portName);
        serial->setBaudRate(QSerialPort::Baud115200); // 需与下位机一致
        serial->setDataBits(QSerialPort::Data8);
        serial->setParity(QSerialPort::NoParity);
        serial->setStopBits(QSerialPort::OneStop);
        serial->setFlowControl(QSerialPort::NoFlowControl);

        if (serial->open(QIODevice::ReadWrite)) {
            btnConnect->setText("断开连接");
            btnConnect->setStyleSheet("background-color: #d32f2f; color: #fff;");
            portSelector->setEnabled(false);
            logWindow->append(QString("<font color='#00e676'>[系统] 成功连接至 %1</font>").arg(portName));
        } else {
            QMessageBox::critical(this, "错误", "无法打开串口：" + serial->errorString());
        }
    }
}
void MainWindow::readData() {
    // 将所有新数据读入缓冲区
    serialBuffer += serial->readAll();

    // 只有当检测到换行符时才处理（保证数据的完整行）
    while (serialBuffer.contains('\n')) {
        int pos = serialBuffer.indexOf('\n');
        QString line = QString::fromUtf8(serialBuffer.left(pos)).trimmed();
        serialBuffer.remove(0, pos + 1);

        if (!line.isEmpty()) {
            parseLine(line); // 这里会调用之前的正则表达式解析逻辑
        }
    }
}
