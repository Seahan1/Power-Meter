#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <array>
#include <vector>
#include <QtGlobal>
#include "oscilloscope.h"

class QLabel;
class QTextEdit;
class QPushButton;
class QSlider;
class QComboBox;
class QSpinBox;
class QTabWidget;
class QThread;
class SerialWorker;
struct ParsedSample;
class QElapsedTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void toggleSerial();
    void updatePortList();
    void refreshUI();
    void exportCSV();
    void clearAll();
    void onSampleReady(const ParsedSample& s);

private:
    void setupUI();
    void setSelectedChannel(int chIndex);
    void updateStatsUI();

    static constexpr int kMaxChannels = 6;

    // ---- Top / Connection
    QComboBox *portSelector = nullptr;
    QPushButton *btnConnect = nullptr;

    // ---- Buffers (per-channel)
    std::array<std::vector<PowerData>, kMaxChannels> m_bufs{};

    // ---- Plotting
    QTabWidget* m_tabs = nullptr;
    std::array<Oscilloscope*, kMaxChannels> m_overviewScopes{};
    Oscilloscope* m_focusScope = nullptr;
    int m_selectedCh = 0; // 0..5

    // ---- Channel cards (right panel)
    std::array<QLabel*, kMaxChannels> m_chV{};
    std::array<QLabel*, kMaxChannels> m_chI{};
    std::array<QLabel*, kMaxChannels> m_chP{};
    QTextEdit *logWindow = nullptr;

    // ---- History / zoom
    QSlider *slider = nullptr;
    double zoom = 1.0;
    int offset = 0;

    // ---- Stats panel
    QComboBox* m_statsChSelector = nullptr;
    QSpinBox*  m_statsWindowSec = nullptr;
    QLabel* m_statLabel[3][4]{}; // 0=V,1=I,2=P x (min,max,avg,rms)
    QLabel* m_energyMWh = nullptr;
    QLabel* m_energyWh  = nullptr;

    // ---- IO thread
    QThread* ioThread = nullptr;
    SerialWorker* worker = nullptr;
    bool connected = false;

    // ---- Timing & repaint
    bool dirty = false;
    QElapsedTimer* m_clock = nullptr;
};

#endif
