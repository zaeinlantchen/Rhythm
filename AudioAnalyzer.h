#ifndef AUDIOANALYZER_H
#define AUDIOANALYZER_H

#include <QObject>
#include <QVector>
#include <QAudioBuffer>

class AudioAnalyzer : public QObject
{
    Q_OBJECT
public:
    explicit AudioAnalyzer(QObject *parent = nullptr);

    void setBarCount(int n);
    void setFreqRange(qreal minHz, qreal maxHz);

public slots:
    void processBuffer(const QAudioBuffer &buffer);

signals:
    void spectrumReady(const QVector<float> &bars);   // 0.0 ~ 1.0

private:
    int   m_barCount = 64;
    qreal m_minHz    = 40.0;
    qreal m_maxHz    = 16000.0;
};

#endif