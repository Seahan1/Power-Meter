#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

#include <QWidget>
#include <vector>

struct PowerData {
    double v; // 电压 (V)
    double i; // 电流 (mA)
    double p; // 功率 (mW)
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

private:
    void drawTrace(class QPainter *p, double PowerData::*member, double range, QColor color, bool visible);

    const std::vector<PowerData> *m_data = nullptr;
    int m_offset = 0;
    double m_zoom = 1.0;
    QColor colorV, colorI, colorP;
};

#endif
