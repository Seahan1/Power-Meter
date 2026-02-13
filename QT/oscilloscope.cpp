#include "oscilloscope.h"
#include <QPainter>
#include <QPainterPath>
#include <QtMath>
#include <cmath>
#include <algorithm> // for std::max
Oscilloscope::Oscilloscope(QColor vCol, QColor iCol, QColor pCol, QWidget *parent)
    : QWidget(parent), colorV(vCol), colorI(iCol), colorP(pCol)
{
    setBackgroundRole(QPalette::Base);
    setAutoFillBackground(true);
    showV = true; showI = true; showP = true;
    m_zoom = 5.0;
}
void Oscilloscope::setData(const std::vector<PowerData> *data, int offset, double zoom) {
    m_data = data;
    m_offset = offset;
    //m_zoom = zoom;acul
}
// 【新增】处理鼠标滚轮事件
void Oscilloscope::wheelEvent(QWheelEvent *event) {
    // 获取滚轮滚动的角度 delta
    QPoint numPixels = event->pixelDelta();
    QPoint numDegrees = event->angleDelta() / 8;
    int step = 0;
    if (!numPixels.isNull()) {
        step = numPixels.y();
    } else if (!numDegrees.isNull()) {
        QPoint numSteps = numDegrees / 15;
        step = numSteps.y();
    }
    // 滚轮向上滚（正数）：放大（Zoom In），间距变大
    // 滚轮向下滚（负数）：缩小（Zoom Out），间距变小
    if (step > 0) {
        m_zoom *= 1.2; // 每次放大 20%
    } else {
        m_zoom /= 1.2; // 每次缩小 20%
    }
    // 限制缩放范围，防止无限放大或缩小
    // 0.1 表示 10个点挤在1像素里（看宏观趋势）
    // 50.0 表示 1个点占50像素（看极细微变化）
    if (m_zoom < 0.1) m_zoom = 0.1;
    if (m_zoom > 50.0) m_zoom = 50.0;
    // 触发重绘
    update();
}

// 【新增函数】计算当前视图内数据的最大值
double Oscilloscope::calculateVisibleMax(double PowerData::*member) {
    if (!m_data || m_data->empty()) return 10.0;
    double maxVal = 0.0;
    int rightIdx = m_data->size() - 1 - m_offset;
    // 使用当前的 m_zoom 计算屏幕内能容纳多少个点
    // 如果 m_zoom 很大，i * m_zoom 增长很快，循环次数其实变少了（因为很快超出 width）
    // 为了准确遍历屏幕上的像素，我们还是按像素循环
    for (int i = 0; i < width(); ++i) {
        double dataIndexStep = i / m_zoom; // 计算当前像素对应第几个数据点
        int idx = rightIdx - (int)dataIndexStep;
        if(idx<0) break;
        double val = std::abs((*m_data)[idx].*member);
        if(val>maxVal) maxVal = val;
    }
    if(maxVal<0.1) maxVal = 1.0;
    return maxVal * 1.2;
}
double Oscilloscope::calculateVisibleRms(double PowerData::*member) const {
    if (!m_data || m_data->empty()) return 0.0;

    double sumSq = 0.0;
    int n = 0;
    int rightIdx = (int)m_data->size() - 1 - m_offset;

    for (int px = 0; px < width(); ++px) {
        int dataDist = (int)(px / m_zoom);
        int idx = rightIdx - dataDist;
        if (idx < 0) break;

        double v = (*m_data)[idx].*member;
        sumSq += v * v;
        n++;
    }

    return (n > 0) ? std::sqrt(sumSq / n) : 0.0;
}

void Oscilloscope::paintEvent(QPaintEvent *) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.fillRect(rect(), Qt::black);
    // 网格绘制
    painter.setPen(QPen(QColor(60, 60, 60), 1, Qt::DotLine));
    for (int x = width(); x > 0; x -= 50) painter.drawLine(x, 0, x, height());
    for (int y = 0; y < height(); y += height() / 4) painter.drawLine(0, y, width(), y);
    if (!m_data || m_data->size() < 2) return;
    double rangeV = calculateVisibleMax(&PowerData::v);
    double rangeI = calculateVisibleMax(&PowerData::i);
    double rangeP = calculateVisibleMax(&PowerData::p);
    drawTrace(&painter, &PowerData::p, rangeP, colorP, showP);
    drawTrace(&painter, &PowerData::i, rangeI, colorI, showI);
    drawTrace(&painter, &PowerData::v, rangeV, colorV, showV);
    // 显示当前量程和缩放倍率
    double rmsV = calculateVisibleRms(&PowerData::v);
    double rmsI = calculateVisibleRms(&PowerData::i);
    double rmsP = calculateVisibleRms(&PowerData::p);

    painter.setPen(Qt::white);
    painter.drawText(10, 20,
                     QString("RMS: V=%1 V  I=%2 mA  P=%3 mW")
                         .arg(rmsV, 0, 'f', 3)
                         .arg(rmsI, 0, 'f', 1)
                         .arg(rmsP, 0, 'f', 1)
                     );

    // 显示横轴缩放信息
    painter.drawText(width() - 100, 20, QString("Zoom: x%1").arg(m_zoom, 0, 'f', 1));
}
void Oscilloscope::drawTrace(QPainter *p, double PowerData::*member, double range, QColor color, bool visible) {
    if (!visible) return;
    p->setPen(QPen(color, 2));
    QPainterPath path;
    int rightIdx = m_data->size() - 1 - m_offset;
    bool first = true;
    // 这里的绘图逻辑也需要稍微适配 zoom
    // 我们按照屏幕像素 x 从右向左遍历
    for (int i = 0; i < width(); ++i) {
        // 计算当前像素对应的数据索引
        // i 是距离右边的像素数，除以 zoom 得到距离最新的数据点个数
        int dataDist = (int)(i / m_zoom);
        int idx = rightIdx - dataDist;
        if (idx < 0) break;
        double val = std::abs((*m_data)[idx].*member);
        double x = width() - i; // x 坐标就是当前像素位置
        double y = height() - ((val / range) * height());
        y = qBound(0.0, y, (double)height());
        if (first) { path.moveTo(x, y); first = false; }
        else { path.lineTo(x, y); }
    }
    p->drawPath(path);
}
