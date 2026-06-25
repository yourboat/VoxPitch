#ifndef AUDIOPLAYER_H
#define AUDIOPLAYER_H

#include <QObject>
#include <QAudioSink>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QIODevice>
#include <QVector>
#include <QMap>
#include <QTimer>

class AudioPlayer : public QObject{

    Q_OBJECT

public:

    explicit AudioPlayer( QObject *parent = nullptr );
    ~AudioPlayer();

    void start();
    void stop();
    void playSamples( const QByteArray &samples );
    void playTone( int midiNote , int durationMs = 150 );
    // mixes all notes together so they sound simultaneously, like a chord
    void playChord( const QVector<int> &midiNotes , int durationMs );

    // polyphonic note-on/note-off for keys held on screen/keyboard: any
    // number of notes can be held at once (or overlap while one is still
    // ringing out), each mixed together in real time and faded out shortly
    // after its own key is released.
    void noteOn( int midiNote );
    void noteOff( int midiNote );

private slots:

    void feedAudio();

private:

    struct ActiveNote {
        double freqHz;
        qint64 startSample;
        qint64 releaseStartSample = -1; // -1 while still held
    };

    QAudioSink *m_audioSink = nullptr;
    QIODevice *m_audioDevice = nullptr;
    QAudioFormat m_format;

    QTimer *m_synthTimer = nullptr;
    QMap<int, ActiveNote> m_activeNotes; // keyed by midi note
    qint64 m_sampleClock = 0;

};

#endif
