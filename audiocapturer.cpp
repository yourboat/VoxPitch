#include "audiocapturer.h"
#include<QMediaDevices>
#include<QDebug>

AudioCapturer::AudioCapturer(QObject *parent)
    : QObject{parent} {
    // set audio format
    m_format.setSampleRate(44100); // 44.1kHz
    m_format.setChannelCount(1); // single channel 
    m_format.setSampleFormat(QAudioFormat::Int16); // 16bit
}

AudioCapturer::~AudioCapturer() {
    stop();
}

void AudioCapturer::start() {

    // get default audio device
    QAudioDevice inputDevice = QMediaDevices::defaultAudioInput();

    if( !inputDevice.isFormatSupported( m_format ) ) {
        qWarning() << "默认格式不支持，使用设备首选格式";
        m_format = inputDevice.preferredFormat();
    }

    // create QAudioSource and read 
    m_audioSource = new QAudioSource( inputDevice , m_format , this );
    m_audioSource -> setBufferSize( m_format.sampleRate() * m_format.bytesPerSample() * 0.04 ); // 40ms

    m_audioDevice = m_audioSource -> start();
    if( m_audioDevice ) {
        // connect to the readyRead signal
        connect( m_audioDevice , &QIODevice::readyRead , this , &AudioCapturer::onReadyRead );
        qDebug() << "音频采集已开启";
    } 
    else {
        qCritical() << "无法打开音频输入设备";
    }
}

void AudioCapturer::stop(){

    if( m_audioSource ) {
        m_audioSource -> stop();
        m_audioDevice = nullptr;
        delete m_audioSource;
        m_audioSource = nullptr;
        qDebug() << "音频采集已停止";
    }
}

void AudioCapturer::onReadyRead(){
    
    if( m_audioDevice ) {
        QByteArray data = m_audioDevice -> readAll();
        if( !data.isEmpty() ) {
            emit audioDataReady( data );
        }
    }
}