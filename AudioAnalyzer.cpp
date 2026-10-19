#include "AudioAnalyzer.h"
#include <QtMath>
#include <cmath>

AudioAnalyzer::AudioAnalyzer(QObject *parent) : QObject(parent) {}

void AudioAnalyzer::setBarCount(int n) { m_barCount = qMax(4, n); }
void AudioAnalyzer::setFreqRange(qreal a, qreal b) { m_minHz = a; m_maxHz = b; }

// 单频率能量估计（Goertzel）
static qreal goertzel(const float *s, int n, qreal targetHz, qreal sr)
{
    if (n <= 0 || sr <= 0) return 0;
    const qreal k     = 0.5 + (n * targetHz) / sr;
    const qreal omega = (2.0 * M_PI * k) / n;
    const qreal coeff = 2.0 * std::cos(omega);

    qreal s1 = 0, s2 = 0;
    for (int i = 0; i < n; ++i) {
        const qreal s0 = coeff * s1 - s2 + s[i];
        s2 = s1;
        s1 = s0;
    }
    return s1 * s1 + s2 * s2 - coeff * s1 * s2;
}

void AudioAnalyzer::processBuffer(const QAudioBuffer &buffer)
{
    if (!buffer.isValid()) return;

    const QAudioFormat fmt      = buffer.format();
    const int   channels        = fmt.channelCount();
    const qreal sampleRate      = fmt.sampleRate();
    const auto  sampleFormat    = fmt.sampleFormat();
    if (channels <= 0 || sampleRate <= 0) return;

    const int totalFrames = buffer.sampleCount() / channels;
    const int win         = qMin(totalFrames, 1024);
    if (win < 64) return;

    // 单声道化 + 取前 win 个样本
    QVector<float> mono(win, 0.0f);

    if (sampleFormat == QAudioFormat::Float) {
        const float *src = buffer.constData<float>();
        for (int i = 0; i < win; ++i) {
            float s = 0;
            for (int c = 0; c < channels; ++c)
                s += src[i * channels + c];
            mono[i] = s / channels;
        }
    } else if (sampleFormat == QAudioFormat::Int16) {
        const qint16 *src = buffer.constData<qint16>();
        for (int i = 0; i < win; ++i) {
            float s = 0;
            for (int c = 0; c < channels; ++c)
                s += src[i * channels + c];
            mono[i] = s / (channels * 32768.0f);
        }
    } else if (sampleFormat == QAudioFormat::Int32) {
        const qint32 *src = buffer.constData<qint32>();
        for (int i = 0; i < win; ++i) {
            qint64 s = 0;
            for (int c = 0; c < channels; ++c)
                s += src[i * channels + c];
            mono[i] = float(s / (channels * 2147483648.0));
        }
    } else {
        return;
    }

    // 汉宁窗
    for (int i = 0; i < win; ++i) {
        const float w = 0.5f * (1.0f - std::cos(2.0f * float(M_PI) * i / (win - 1)));
        mono[i] *= w;
    }

    // 对数分布频率 -> 每个柱状条取一个代表频率
    QVector<float> bars(m_barCount, 0.0f);
    const qreal ratio = m_maxHz / m_minHz;

    for (int i = 0; i < m_barCount; ++i) {
        const qreal t0 = qreal(i)     / m_barCount;
        const qreal t1 = qreal(i + 1) / m_barCount;
        const qreal f0 = m_minHz * std::pow(ratio, t0);
        const qreal f1 = m_minHz * std::pow(ratio, t1);
        const qreal fc = (f0 + f1) * 0.5;

        qreal e = goertzel(mono.constData(), win, fc, sampleRate);
        e = std::sqrt(qMax<qreal>(0.0, e)) / win;

        // dB 映射：-70dB .. -10dB → 0..1
        const qreal db = 20.0 * std::log10(e + 1e-9);
        float v = float((db + 70.0) / 60.0);
        bars[i] = qBound(0.0f, v, 1.0f);
    }

    emit spectrumReady(bars);
}