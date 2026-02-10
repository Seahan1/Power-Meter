#include "serialworker.h"

SerialWorker::SerialWorker(QObject* parent) : QObject(parent),
    m_re(R"(CH:(\d)\s+V=\s*([-\d\.]+)\s+V\s+\|\s+I=\s*([-\d\.]+)\s+A\s+\|\s+P=\s*([-\d\.]+)\s+W)")
{
}

void SerialWorker::openPort(const QString& portName, int baud) {
    if (m_serial.isOpen()) m_serial.close();

    m_serial.setPortName(portName);
    m_serial.setBaudRate(baud);
    m_serial.setDataBits(QSerialPort::Data8);
    m_serial.setParity(QSerialPort::NoParity);
    m_serial.setStopBits(QSerialPort::OneStop);
    m_serial.setFlowControl(QSerialPort::NoFlowControl);

    if (!m_serial.open(QIODevice::ReadOnly)) {
        emit errorOccured("无法打开串口：" + m_serial.errorString());
        emit connectedChanged(false);
        return;
    }

    connect(&m_serial, &QSerialPort::readyRead, this, &SerialWorker::onReadyRead, Qt::DirectConnection);
    emit connectedChanged(true);
}

void SerialWorker::closePort() {
    if (m_serial.isOpen()) m_serial.close();
    emit connectedChanged(false);
}

void SerialWorker::onReadyRead() {
    m_buf += m_serial.readAll();

    while (true) {
        int pos = m_buf.indexOf('\n');
        if (pos < 0) break;
        QByteArray one = m_buf.left(pos);
        m_buf.remove(0, pos + 1);

        QString line = QString::fromUtf8(one).trimmed();
        if (line.isEmpty()) continue;

        ParsedSample s;
        if (tryParse(line, s)) {
            emit sampleReady(s);
        } else {
            // 启动信息/错误信息可以选择性发到 UI 日志（低频）
            if (line.startsWith("OK:") || line.startsWith("ERR:") || line.startsWith("System")) {
                emit logLine(line);
            }
        }
    }
}

bool SerialWorker::tryParse(const QString& line, ParsedSample& out) {
    auto match = m_re.match(line);
    if (!match.hasMatch()) return false;

    bool ok=false;
    int ch = match.captured(1).toInt(&ok);
    if (!ok || (ch!=1 && ch!=2)) return false;

    float v = match.captured(2).toFloat();
    float i_ma = match.captured(3).toFloat() * 1000.0f;
    float p_mw = match.captured(4).toFloat() * 1000.0f;

    out = { ch, v, i_ma, p_mw };
    return true;
}
