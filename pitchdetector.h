#ifndef PITCHDETECTOR_H
#define PITCHDETECTOR_H

#include<QObject>
#include<QVector>
#include<cstdint>

struct PitchResult {

    double frequencyHz = 0.0;     // frequency
    QString noteName;             // notename (such as C5 )
    double centDeviation = 0.0;   // centDeviation (±50 cent)
    bool isDetected = false;

};

class PitchDetector : public QObject {

    Q_OBJECT

public:


    explicit PitchDetector( QObject *parent = nullptr );

    void setSampleRate(int sr) { m_sampleRate = sr; }
    int sampleRate() const { return m_sampleRate; }
    // samples consumed per pitchDetected emission (hop == window, no overlap);
    // lets callers turn "the Nth result" into an absolute timestamp
    int windowSize() const { return m_windowSize; }

    // shared MIDI-number -> note name (e.g. 60 -> "C4"), used by the UI too
    static QString noteNameForMidi( int midiNumber );


    public slots:
    
    void processSamples(const QByteArray &data);


    signals:

    void pitchDetected(const PitchResult &result);

private:

    double yinDetect(const QVector<double> &frame);
    QString frequencyToNoteName(double freq) const;

    int m_sampleRate = 44100; // for 20Hz a man can hear
    int m_windowSize = 2048;  // detect window (for FFT, YIN)
    int m_bufferSize = 8192;
    double m_minFrequency = 50.0; // lowest vocal pitch to search for (Hz)
    double m_yinThreshold = 0.2;  // absolute threshold for YIN's dip search
    double m_minRmsEnergy = 0.01; // gate out near-silence (note releases, breath, room noise)
    QVector<int16_t> m_ringBuffer;

};

#endif