#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

#include <QWidget>
#include <QtGlobal>
#include <vector>
#include <QWheelEvent>

struct PowerData {
    double v; // 电压 (V)
    double i; // 电流 (mA)
    double p; // 功率 (mW)
    quint64 t_ms = 0; // 时间戳 (ms, 上位机)
};

class Oscilloscope : public QWidget {
    Q_OBJECT
public:
    explicit Oscilloscope(QColor vCol, QColor iCol, QColor pCol, QWidget *parent = nullptr);
    void setData(const std::vector<PowerData> *data, int offset, double zoom);

    bool showV = true;
    bool showI = true;
    bool showP = true;

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
private:
private:
    void drawTrace(class QPainter *p, double PowerData::*member, double range, QColor color, bool visible);
    // 辅助函数：自适应量程
    double calculateVisibleMax(double PowerData::*member);
    const std::vector<PowerData> *m_data = nullptr;
    int m_offset = 0;
    double m_zoom = 1.0;
    QColor colorV, colorI, colorP;
};

#endif
