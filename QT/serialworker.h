#pragma once
#include <QObject>
#include <QSerialPort>
#include <QRegularExpression>

struct ParsedSample {
    int ch;
    float v; // V
    float i; // mA
    float p; // mW
};
Q_DECLARE_METATYPE(ParsedSample)

class SerialWorker : public QObject {
    Q_OBJECT
public:
    explicit SerialWorker(QObject* parent=nullptr);

public slots:
    void openPort(const QString& portName, int baud);
    void closePort();

signals:
    void sampleReady(const ParsedSample& s);
    void logLine(const QString& s);
    void connectedChanged(bool ok);
    void errorOccured(const QString& s);

private slots:
    void onReadyRead();

private:
    bool tryParse(const QString& line, ParsedSample& out);

    QSerialPort m_serial;
    QByteArray m_buf;
    QRegularExpression m_re;
};
