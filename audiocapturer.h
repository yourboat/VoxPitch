#ifndef AUDIOCAPTURER_H
#define AUDIOCAPTURER_H

#include<QObject>
#include<QAudioSource>
#include<QAudioDevice>
#include<QAudioFormat>
#include<QIODevice>

class AudioCapturer : public QObject {

    Q_OBJECT

public:

    explicit AudioCapturer( QObject *parent = nullptr );
    ~AudioCapturer();

    void start();
    void stop();
    QAudioFormat format() const { return m_format; }

    signals:
    // when a new audio data prepared, put this signal
    // a QByteArray with primitive PCM data (int16_t single channel)
    void audioDataReady(const QByteArray &data);

    private slots:
    
    void onReadyRead();

private:

    QAudioSource *m_audioSource = nullptr;
    QIODevice *m_audioDevice = nullptr;
    QAudioFormat m_format;

};

#endif