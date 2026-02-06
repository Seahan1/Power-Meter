#include "oscilloscope.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

Oscilloscope::Oscilloscope(QColor vCol, QColor iCol, QColor pCol, QWidget *parent)
    : QWidget(parent), colorV(vCol), colorI(iCol), colorP(pCol) {
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
}

void Oscilloscope::setData(const std::vector<PowerData> *data, int offset, double zoom) {
    m_data = data;
    m_offset = offset;
    m_zoom = zoom;
    update();
}

void Oscilloscope::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::black);

    // 绘制网格
    painter.setPen(QPen(QColor(40, 40, 40), 1, Qt::DotLine));
    for (int x = width(); x > 0; x -= 50) painter.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += height() / 4) painter.drawLine(0, y, width(), y);

    if (!m_data || m_data->size() < 2) return;

    // 绘制三条曲线
    drawTrace(&painter, &PowerData::p, 500.0, colorP, showP);
    drawTrace(&painter, &PowerData::i, 500.0, colorI, showI);
    drawTrace(&painter, &PowerData::v, 3.5, colorV, showV);
}

void Oscilloscope::drawTrace(QPainter *p, double PowerData::*member, double range, QColor color, bool visible) {
    if (!visible) return;
    p->setPen(QPen(color, 2));
    QPainterPath path;

    int rightIdx = m_data->size() - 1 - m_offset;
    double xStep = m_zoom;

    bool first = true;
    for (int i = 0; i < width(); ++i) {
        int idx = rightIdx - i;
        if (idx < 0) break;

        double val = (*m_data)[idx].*member;
        double x = width() - (i * xStep);
        double y = height() - ((val / range) * height());
        y = qBound(0.0, y, (double)height());

        if (first) { path.moveTo(x, y); first = false; }
        else { path.lineTo(x, y); }
        if (x < 0) break;
    }
    p->drawPath(path);
}
