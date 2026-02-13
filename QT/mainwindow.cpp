#include "mainwindow.h"
#include "serialworker.h"

#include <QtWidgets>
#include <QTimer>
#include <QFileDialog>
#include <QTextStream>
#include <QSerialPortInfo>
#include <QThread>
#include <QElapsedTimer>
#include <cmath>
#include <limits>

static constexpr int kMaxChannels = 6;
static QColor kChColorsV[kMaxChannels] = {
    QColor("#fdd835"), QColor("#00e5ff"), QColor("#66bb6a"),
    QColor("#ab47bc"), QColor("#ffa726"), QColor("#ef5350")
};
static QColor kChColorsI[kMaxChannels] = {
    QColor("#ff9800"), QColor("#2979ff"), QColor("#43a047"),
    QColor("#7e57c2"), QColor("#fb8c00"), QColor("#e53935")
};
static QColor kChColorsP[kMaxChannels] = {
    QColor("#ff5252"), QColor("#d05ce3"), QColor("#26c6da"),
    QColor("#ec407a"), QColor("#ffd54f"), QColor("#8d6e63")
};

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
    m_clock = new QElapsedTimer();
    m_clock->start();

    setupUI();

    // ---- Serial worker thread
    qRegisterMetaType<ParsedSample>("ParsedSample");

    ioThread = new QThread(this);
    worker = new SerialWorker();
    worker->moveToThread(ioThread);

    connect(ioThread, &QThread::finished, worker, &QObject::deleteLater);

    connect(worker, &SerialWorker::sampleReady, this, &MainWindow::onSampleReady, Qt::QueuedConnection);
    connect(worker, &SerialWorker::logLine, this, [this](const QString& s){
        // 低频日志：只显示重要行
        logWindow->append(s.toHtmlEscaped());
    }, Qt::QueuedConnection);
    connect(worker, &SerialWorker::errorOccured, this, [this](const QString& s){
        QMessageBox::critical(this, "串口错误", s);
    }, Qt::QueuedConnection);

    ioThread->start();

    // ---- UI refresh timer (30fps)
    auto *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::refreshUI);
    timer->start(33);
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        // 尝试从 watched 或其 parent 链上找 chIndex
        QObject* obj = watched;
        while (obj) {
            QVariant v = obj->property("chIndex");
            if (v.isValid()) {
                int idx = v.toInt();
                setSelectedChannel(idx);
                if (m_tabs) m_tabs->setCurrentIndex(1); // Focus tab
                return true;
            }
            obj = obj->parent();
        }
    }
    return QMainWindow::eventFilter(watched, event);
}


void MainWindow::setupUI() {
    setWindowTitle("ProPower Multi-Channel Monitor");
    resize(1200, 780);

    setStyleSheet(
        "QMainWindow { background-color: #121212; }"
        "QWidget { background-color: #121212; color: #e0e0e0; font-family: 'Segoe UI', 'Consolas'; }"
        "QGroupBox { border: 1px solid #333; border-radius: 8px; margin-top: 10px; padding-top: 10px; font-weight: bold; }"
        "QPushButton { background-color: #333; border: none; border-radius: 4px; padding: 6px 12px; font-weight: bold; min-height: 26px; }"
        "QPushButton:hover { background-color: #444; }"
        "QTextEdit { background-color: #000; border: 1px solid #333; border-radius: 8px; font-family: 'Courier New'; }"
        "QSlider::handle:horizontal { background: #00e676; width: 18px; }"
        );

    QWidget *central = new QWidget(this);
    setCentralWidget(central);
    auto *rootLayout = new QVBoxLayout(central);

    // ---------------- Header ----------------
    auto *header = new QHBoxLayout();

    portSelector = new QComboBox(this);
    portSelector->setFixedWidth(220);

    btnConnect = new QPushButton("连接设备", this);
    btnConnect->setStyleSheet("background-color: #00e676; color: #000;");
    connect(btnConnect, &QPushButton::clicked, this, &MainWindow::toggleSerial);

    auto *btnRefresh = new QPushButton("刷新", this);
    btnRefresh->setFixedWidth(60);
    connect(btnRefresh, &QPushButton::clicked, this, &MainWindow::updatePortList);

    header->addWidget(new QLabel("端口:", this));
    header->addWidget(portSelector);
    header->addWidget(btnRefresh);
    header->addWidget(btnConnect);
    header->addStretch(1);


    rootLayout->addLayout(header);

    // ---------------- Main body ----------------
    auto *mainBody = new QHBoxLayout();

    // Left: tabs (Overview / Focus)
    m_tabs = new QTabWidget(this);

    // ---- Overview tab
    QWidget* overviewPage = new QWidget(this);
    auto *overviewGrid = new QGridLayout(overviewPage);
    overviewGrid->setContentsMargins(8, 8, 8, 8);
    overviewGrid->setHorizontalSpacing(10);
    overviewGrid->setVerticalSpacing(10);

    for (int i = 0; i < kMaxChannels; ++i) {
        auto *cell = new QWidget(this);
        auto *cellLayout = new QVBoxLayout(cell);
        cellLayout->setContentsMargins(0,0,0,0);
        cellLayout->setSpacing(4);

        auto *title = new QLabel(QString("CHANNEL %1").arg(i+1), this);
        title->setStyleSheet("font-weight:bold; color:#bdbdbd;");
        title->setProperty("chIndex", i);
        title->installEventFilter(this);
        cellLayout->addWidget(title, 0, Qt::AlignLeft);

        m_overviewScopes[i] = new Oscilloscope(kChColorsV[i], kChColorsI[i], kChColorsP[i], this);
        m_overviewScopes[i]->setMinimumHeight(160);
        m_overviewScopes[i]->setProperty("chIndex", i);
        m_overviewScopes[i]->installEventFilter(this);
        cell->setProperty("chIndex", i);
        cell->installEventFilter(this);

        cellLayout->addWidget(m_overviewScopes[i], 1);



        int row = i / 2; // 3 rows, 2 cols
        int col = i % 2;
        overviewGrid->addWidget(cell, row, col);
    }

    m_tabs->addTab(overviewPage, "Overview");

    // ---- Focus tab
    QWidget* focusPage = new QWidget(this);
    auto *focusLayout = new QVBoxLayout(focusPage);
    focusLayout->setContentsMargins(8,8,8,8);

    m_focusScope = new Oscilloscope(kChColorsV[0], kChColorsI[0], kChColorsP[0], this);
    focusLayout->addWidget(m_focusScope, 1);

    auto *bottomBar = new QHBoxLayout();
    slider = new QSlider(Qt::Horizontal, this);
    slider->setRange(0, 0);
    connect(slider, &QSlider::valueChanged, this, [this](int v){
        offset = v;
        dirty = true;
    });

    bottomBar->addWidget(new QLabel("历史回放", this));
    bottomBar->addWidget(slider, 1);
    focusLayout->addLayout(bottomBar);

    m_tabs->addTab(focusPage, "Focus");

    // Right: side panel
    QWidget *sidePanel = new QWidget(this);
    sidePanel->setFixedWidth(340);
    auto *sideLayout = new QVBoxLayout(sidePanel);
    sideLayout->setContentsMargins(8,8,8,8);
    sideLayout->setSpacing(10);

    // ---- Channel cards in scroll area
    auto *scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    QWidget *cardsHost = new QWidget(this);
    auto *cardsLayout = new QGridLayout(cardsHost);
    cardsLayout->setContentsMargins(0,0,0,0);
    cardsLayout->setHorizontalSpacing(8);
    cardsLayout->setVerticalSpacing(8);

    for (int i = 0; i < kMaxChannels; ++i) {
        auto *card = new QGroupBox(QString("● CH%1").arg(i+1), this);
        card->setStyleSheet(QString("QGroupBox { border-left: 3px solid %1; }")
                                .arg(kChColorsV[i].name()));

        auto *vbox = new QVBoxLayout(card);
        vbox->setSpacing(2);

        m_chV[i] = new QLabel("--.--- V", this);
        m_chI[i] = new QLabel("--.-- mA", this);
        m_chP[i] = new QLabel("--.-- mW", this);

        // 字体小一点
        m_chV[i]->setStyleSheet(QString("font-size: 15px; color: %1;").arg(kChColorsV[i].name()));
        m_chI[i]->setStyleSheet(QString("font-size: 15px; color: %1;").arg(kChColorsI[i].name()));
        m_chP[i]->setStyleSheet(QString("font-size: 15px; color: %1;").arg(kChColorsP[i].name()));

        vbox->addWidget(m_chV[i]);
        vbox->addWidget(m_chI[i]);
        vbox->addWidget(m_chP[i]);

        // Focus 按钮做成小图标/小按钮（可选）
        auto *btnFocus = new QPushButton("Focus", this);
        btnFocus->setFixedHeight(24);
        connect(btnFocus, &QPushButton::clicked, this, [this, i](){
            setSelectedChannel(i);
            if (m_tabs) m_tabs->setCurrentIndex(1);
        });
        vbox->addWidget(btnFocus);

        int row = i / 2;
        int col = i % 2;
        cardsLayout->addWidget(card, row, col);
    }


    cardsLayout->setRowStretch(cardsLayout->rowCount(), 1);
    scroll->setWidget(cardsHost);

    sideLayout->addWidget(scroll, 3);

    // ---- Stats panel
    auto *statsBox = new QGroupBox("统计（最近 N 秒）", this);
    auto *statsLayout = new QGridLayout(statsBox);

    m_statsChSelector = new QComboBox(this);
    for (int i = 0; i < kMaxChannels; ++i) m_statsChSelector->addItem(QString("CH%1").arg(i+1), i);
    connect(m_statsChSelector, &QComboBox::currentIndexChanged, this, [this](int){
    dirty = true;
});

m_statsWindowSec = new QSpinBox(this);
m_statsWindowSec->setRange(1, 300);
m_statsWindowSec->setValue(10);
connect(m_statsWindowSec, qOverload<int>(&QSpinBox::valueChanged), this, [this](int){
    dirty = true;
});

statsLayout->addWidget(new QLabel("通道", this), 0, 0);
statsLayout->addWidget(m_statsChSelector, 0, 1);
statsLayout->addWidget(new QLabel("窗口(s)", this), 0, 2);
statsLayout->addWidget(m_statsWindowSec, 0, 3);

// header row
QStringList cols = {"Min", "Max", "Avg", "RMS"};
statsLayout->addWidget(new QLabel("", this), 1, 0);
for (int c=0; c<4; ++c) statsLayout->addWidget(new QLabel(cols[c], this), 1, c+1);

QStringList rows = {"V (V)", "I (mA)", "P (mW)"};
for (int r=0; r<3; ++r) {
    statsLayout->addWidget(new QLabel(rows[r], this), r+2, 0);
    for (int c=0; c<4; ++c) {
        m_statLabel[r][c] = new QLabel("--", this);
        m_statLabel[r][c]->setStyleSheet("color:#e0e0e0;");
        statsLayout->addWidget(m_statLabel[r][c], r+2, c+1);
    }
}

// Energy
m_energyMWh = new QLabel("E: -- mWh", this);
m_energyWh  = new QLabel("E: -- Wh", this);
m_energyMWh->setStyleSheet("font-weight:bold; color:#00e676;");
m_energyWh->setStyleSheet("font-weight:bold; color:#00e676;");
statsLayout->addWidget(m_energyMWh, 5, 0, 1, 2);
statsLayout->addWidget(m_energyWh, 5, 2, 1, 2);

sideLayout->addWidget(statsBox, 0);

// ---- Log window
logWindow = new QTextEdit(this);
logWindow->setReadOnly(true);
logWindow->setPlaceholderText("系统就绪...");
logWindow->document()->setMaximumBlockCount(300);

auto* dock = new QDockWidget("Output", this);
dock->setWidget(logWindow);
dock->setAllowedAreas(Qt::BottomDockWidgetArea);
addDockWidget(Qt::BottomDockWidgetArea, dock);
dock->setMinimumHeight(140);


// ---- Bottom buttons
auto *btnExport = new QPushButton("导出 CSV", this);
connect(btnExport, &QPushButton::clicked, this, &MainWindow::exportCSV);

auto *btnClear = new QPushButton("清空", this);
connect(btnClear, &QPushButton::clicked, this, &MainWindow::clearAll);

sideLayout->addWidget(btnExport);
sideLayout->addWidget(btnClear);

// assemble
mainBody->addWidget(m_tabs, 3);
mainBody->addWidget(sidePanel, 1);
rootLayout->addLayout(mainBody);

updatePortList();
setSelectedChannel(0);
}

void MainWindow::updatePortList() {
    QString current = portSelector->currentData().toString();
    portSelector->clear();

    const auto infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &info : infos) {
        QString label = info.portName() + " (" + info.description() + ")";
        portSelector->addItem(label, info.portName());
    }

    // restore selection if possible
    int idx = portSelector->findData(current);
    if (idx >= 0) portSelector->setCurrentIndex(idx);

    if (infos.isEmpty()) {
        logWindow->append("<font color='gray'>[系统] 未检测到串口设备</font>");
    }
}

void MainWindow::toggleSerial() {
    if (connected) {
        QMetaObject::invokeMethod(worker, "closePort", Qt::QueuedConnection);
        connected = false;

        btnConnect->setText("连接设备");
        btnConnect->setStyleSheet("background-color: #00e676; color: #000;");
        portSelector->setEnabled(true);
        logWindow->append("<font color='gray'>[系统] 已断开</font>");
        return;
    }

    QString portName = portSelector->currentData().toString();
    if (portName.isEmpty()) {
        QMessageBox::warning(this, "错误", "未检测到可用串口！");
        return;
    }

    QMetaObject::invokeMethod(worker, "openPort", Qt::QueuedConnection,
                              Q_ARG(QString, portName),
                              Q_ARG(int, 115200));

    connected = true;
    btnConnect->setText("断开连接");
    btnConnect->setStyleSheet("background-color: #d32f2f; color: #fff;");
    portSelector->setEnabled(false);
    logWindow->append(QString("<font color='#00e676'>[系统] 连接至 %1</font>").arg(portName));
}

void MainWindow::setSelectedChannel(int chIndex) {
    if (chIndex < 0 || chIndex >= kMaxChannels) return;
    m_selectedCh = chIndex;

    // update focus scope colors to match channel
    if (m_focusScope) {
        // easiest: recreate scope pen colors by constructing a new widget? too heavy.
        // We keep the same scope widget but its internal colors are fixed in constructor.
        // So for visual consistency, we just keep focus scope as is; traces remain readable.
        // If you want, we can re-create focus scope later.
    }

    dirty = true;
}

void MainWindow::onSampleReady(const ParsedSample& s) {
    int chIndex = s.ch - 1;
    if (chIndex < 0 || chIndex >= kMaxChannels) return;

    PowerData pt;
    pt.v = s.v;
    pt.i = s.i;
    pt.p = s.p;
    pt.t_ms = (quint64)m_clock->elapsed();

    auto &buf = m_bufs[chIndex];
    buf.push_back(pt);

    // Batch trim
    static const int kMax = 10000;
    static const int kMargin = 2000;
    if ((int)buf.size() > kMax + kMargin) {
        buf.erase(buf.begin(), buf.end() - kMax);
    }

    // Update dashboard labels (latest value)
    m_chV[chIndex]->setText(QString::number(pt.v, 'f', 3) + " V");
    m_chI[chIndex]->setText(QString::number(pt.i, 'f', 1) + " mA");
    m_chP[chIndex]->setText(QString::number(pt.p, 'f', 1) + " mW");

    dirty = true;
}

static void computeStatsWindow(
    const std::vector<PowerData>& buf,
    quint64 windowMs,
    double PowerData::*member,
    double& outMin, double& outMax, double& outAvg, double& outRms)
{
    outMin = std::numeric_limits<double>::infinity();
    outMax = -std::numeric_limits<double>::infinity();
    outAvg = 0.0;
    outRms = 0.0;

    if (buf.empty()) return;

    quint64 tEnd = buf.back().t_ms;
    quint64 tStart = (tEnd > windowMs) ? (tEnd - windowMs) : 0;

    double sum = 0.0, sumSq = 0.0;
    int n = 0;

    // scan from back until out of window (fast for recent window)
    for (int idx = (int)buf.size() - 1; idx >= 0; --idx) {
        if (buf[idx].t_ms < tStart) break;
        double v = buf[idx].*member;
        outMin = std::min(outMin, v);
        outMax = std::max(outMax, v);
        sum += v;
        sumSq += v * v;
        n++;
    }

    if (n == 0) return;
    outAvg = sum / n;
    outRms = std::sqrt(sumSq / n);
}

static double computeEnergyMWh(const std::vector<PowerData>& buf, quint64 windowMs) {
    if (buf.size() < 2) return 0.0;

    quint64 tEnd = buf.back().t_ms;
    quint64 tStart = (tEnd > windowMs) ? (tEnd - windowMs) : 0;

    // find first index >= tStart
    int startIdx = (int)buf.size() - 1;
    while (startIdx > 0 && buf[startIdx-1].t_ms >= tStart) startIdx--;

    double e_mWh = 0.0;
    for (int i = startIdx + 1; i < (int)buf.size(); ++i) {
        const auto& a = buf[i-1];
        const auto& b = buf[i];
        if (b.t_ms < tStart) continue;

        double dt_h = (double)(b.t_ms - a.t_ms) / 3600000.0;
        double p_avg_mW = (a.p + b.p) / 2.0;
        e_mWh += p_avg_mW * dt_h;
    }
    return e_mWh;
}

void MainWindow::updateStatsUI() {
    int chIndex = m_statsChSelector ? m_statsChSelector->currentData().toInt() : 0;
    if (chIndex < 0 || chIndex >= kMaxChannels) chIndex = 0;

    const auto& buf = m_bufs[chIndex];
    quint64 windowMs = (quint64)(m_statsWindowSec ? m_statsWindowSec->value() : 10) * 1000ULL;

    auto setRow = [&](int r, double PowerData::*member) {
        double mn, mx, avg, rms;
        computeStatsWindow(buf, windowMs, member, mn, mx, avg, rms);
        if (!std::isfinite(mn) || !std::isfinite(mx)) {
            for (int c=0;c<4;++c) m_statLabel[r][c]->setText("--");
            return;
        }
        m_statLabel[r][0]->setText(QString::number(mn, 'f', 3));
        m_statLabel[r][1]->setText(QString::number(mx, 'f', 3));
        m_statLabel[r][2]->setText(QString::number(avg, 'f', 3));
        m_statLabel[r][3]->setText(QString::number(rms, 'f', 3));
    };

    setRow(0, &PowerData::v);
    setRow(1, &PowerData::i);
    setRow(2, &PowerData::p);

    double e_mWh = computeEnergyMWh(buf, windowMs);
    m_energyMWh->setText(QString("E: %1 mWh").arg(e_mWh, 0, 'f', 4));
    m_energyWh->setText(QString("E: %1 Wh").arg(e_mWh/1000.0, 0, 'f', 6));
}

void MainWindow::refreshUI() {
    if (!dirty) return;
    dirty = false;

    // update slider range based on selected channel
    const auto& focusBuf = m_bufs[m_selectedCh];
    int maxOffset = focusBuf.empty() ? 0 : (int)focusBuf.size() - 1;
    slider->setRange(0, maxOffset);
    if (offset > maxOffset) offset = maxOffset;

    // update plots based on current tab
    int tabIdx = m_tabs ? m_tabs->currentIndex() : 0;
    if (tabIdx == 0) {
        for (int i=0;i<kMaxChannels;++i) {
            m_overviewScopes[i]->setData(&m_bufs[i], 0, zoom);
            m_overviewScopes[i]->update();
        }
    } else {
        m_focusScope->setData(&m_bufs[m_selectedCh], offset, zoom);
        m_focusScope->update();
    }

    updateStatsUI();
}

void MainWindow::exportCSV() {
    QString path = QFileDialog::getSaveFileName(this, "保存 CSV", "", "CSV Files (*.csv)");
    if (path.isEmpty()) return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return;

    QTextStream out(&file);
    out << "Index";
    for (int ch=0; ch<kMaxChannels; ++ch) {
        out << QString(",CH%1_V,CH%1_I,CH%1_P").arg(ch+1);
    }
    out << "\n";

    // export aligned by index (simple)
    int len = 0;
    for (int ch=0; ch<kMaxChannels; ++ch) len = std::max(len, (int)m_bufs[ch].size());

    for (int i=0; i<len; ++i) {
        out << i;
        for (int ch=0; ch<kMaxChannels; ++ch) {
            const auto& b = m_bufs[ch];
            if (i < (int)b.size()) out << "," << b[i].v << "," << b[i].i << "," << b[i].p;
            else out << ",0,0,0";
        }
        out << "\n";
    }
}

void MainWindow::clearAll() {
    for (auto& b : m_bufs) b.clear();
    logWindow->clear();
    dirty = true;
}
