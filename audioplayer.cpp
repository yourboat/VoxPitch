#include "audioplayer.h"
#include<QMediaDevices>
#include<QtMath>
#include<QDebug>
#include<QRandomGenerator>
#include<iterator>

AudioPlayer::AudioPlayer(QObject *parent)
    : QObject{parent} {

    m_format.setSampleRate(44100);
    m_format.setChannelCount(1);
    m_format.setSampleFormat(QAudioFormat::Int16);

    m_synthTimer = new QTimer( this );
    m_synthTimer -> setInterval( 20 ); // ~50 ticks/sec keeps held-note mixing responsive
    connect( m_synthTimer, &QTimer::timeout, this, &AudioPlayer::feedAudio );
}

AudioPlayer::~AudioPlayer() {
    stop();
}

void AudioPlayer::start() {

    // get default audio device
    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();
    if (!outputDevice.isFormatSupported(m_format)) {
        qWarning() << "播放格式默认不支持";
        return;
    }

    // create QAudioSink and play
    m_audioSink = new QAudioSink( outputDevice , m_format , this);
    // Default buffer size is only a couple hundred ms; playTone() writes a
    // whole tone in one write() call, and whatever doesn't fit in the buffer
    // is silently dropped (not queued) -- so long tones just got truncated
    // to whatever the default could hold, regardless of durationMs. Size it
    // for the longest tone we ever generate (a few seconds) with headroom.
    m_audioSink -> setBufferSize( m_format.sampleRate() * m_format.bytesPerSample() * m_format.channelCount() * 8 );
    m_audioDevice = m_audioSink -> start();
    if( m_audioDevice ) {
        qDebug() << "音频播放已就绪";
    }
    else {
        qCritical() << "无法打开音频输出设备";
    }
}

void AudioPlayer::stop() {

    if (m_audioSink) {
        m_synthTimer -> stop();
        m_activeNotes.clear();
        m_audioSink -> stop();
        m_audioDevice = nullptr;
        delete m_audioSink;
        m_audioSink = nullptr;
        qDebug() << "音频播放已停止";
    }
}

void AudioPlayer::playSamples( const QByteArray &samples ) {

    if( m_audioDevice ) {
        m_audioDevice -> write( samples );
    }
}

// A bare sine wave -- or even a clean stack of perfectly-integer harmonics --
// reads as "electronic" because real piano strings aren't quite that tidy:
// higher partials run slightly sharp (string stiffness/inharmonicity), each
// note is actually 2-3 unison strings beating gently against each other, and
// the hammer strike adds a brief noise transient before the tone settles.
// None of that is expensive to fake, and together it goes a long way toward
// sounding like a struck string instead of a synth. Shared by both the
// one-shot tones (playTone/playChord) and the live polyphonic mixer below,
// parameterised by elapsed time so either can sample it at any instant.
static double pianoWaveSample( double freqHz, double t, int sampleRate, double amplitude ) {

    static const double harmonicWeights[] = { 1.0, 0.55, 0.30, 0.16, 0.09, 0.05 };
    const int harmonicCount = 6;
    const double inharmonicity = 0.0005; // higher harmonics run a bit sharp, like a real stiff string
    const double detune = 0.0015;        // two near-unison "strings" beating against each other
    const double attackMs = 4.0;         // quick ramp-up avoids a click at note-on
    const double noiseBurstMs = 35.0;    // brief hammer-strike transient

    static double weightSum = 0.0;
    if( weightSum == 0.0 )
        for( double w : harmonicWeights ) weightSum += w;

    double value = 0.0;
    for( int h = 0 ; h < harmonicCount ; ++h ) {
        int n = h + 1;
        double stretched = freqHz * n * qSqrt( 1.0 + inharmonicity * n * n );
        if( stretched >= sampleRate / 2.0 ) break; // would alias past Nyquist

        // two-stage decay: a quick initial "thump" plus a longer ring-out,
        // with upper harmonics dying out faster than the fundamental
        double fast = qExp( -8.0 * n * t );
        double slow = qExp( -0.9 * n * t );
        double envelope = 0.4 * fast + 0.6 * slow;

        double tone = 0.5 * sin( 2 * M_PI * stretched * (1.0 - detune) * t )
                    + 0.5 * sin( 2 * M_PI * stretched * (1.0 + detune) * t );

        value += (harmonicWeights[h] / weightSum) * envelope * tone;
    }

    if( t * 1000.0 < noiseBurstMs ) {
        double noiseEnvelope = qExp( -t * 120.0 );
        double noise = ( QRandomGenerator::global()->bounded(2000) - 1000 ) / 1000.0;
        value += 0.12 * noiseEnvelope * noise;
    }

    double attackEnvelope = qMin( 1.0, t * 1000.0 / attackMs );
    return value * amplitude * attackEnvelope;
}

static QByteArray generatePianoTone( double freqHz , int sampleRate , int durationMs, double amplitude = 0.5) {

    int totalSamples = sampleRate * durationMs / 1000;
    QByteArray data( totalSamples * sizeof(int16_t), Qt::Uninitialized );
    int16_t *samples = reinterpret_cast<int16_t*>( data.data() );

    for( int i = 0 ; i < totalSamples ; ++i ) {
        double t = static_cast<double>(i) / sampleRate;
        double value = pianoWaveSample( freqHz, t, sampleRate, amplitude );
        samples[i] = static_cast<int16_t>( qBound(-1.0, value, 1.0) * 32767 );
    }
    return data;

}

void AudioPlayer::playTone( int midiNote , int durationMs ) {

    if (!m_audioDevice) {
        qWarning() << "AudioPlayer not started";
        return;
    }

    double freqHz = 440.0 * qPow( 2.0, (midiNote - 69) / 12.0 );
    QByteArray toneData = generatePianoTone( freqHz , m_format.sampleRate() , durationMs );
    m_audioDevice -> write(toneData);
}

void AudioPlayer::noteOn( int midiNote ) {

    if (!m_audioDevice) {
        qWarning() << "AudioPlayer not started";
        return;
    }

    double freqHz = 440.0 * qPow( 2.0, (midiNote - 69) / 12.0 );
    // re-striking an already-held note (e.g. a quick double press) just
    // restarts its envelope from t=0
    m_activeNotes[midiNote] = ActiveNote{ freqHz, m_sampleClock, -1 };

    if( !m_synthTimer->isActive() )
        m_synthTimer->start();
}

void AudioPlayer::noteOff( int midiNote ) {

    auto it = m_activeNotes.find( midiNote );
    if( it != m_activeNotes.end() && it->releaseStartSample < 0 )
        it->releaseStartSample = m_sampleClock;
}

void AudioPlayer::feedAudio() {

    if( !m_audioDevice || !m_audioSink ) return;

    const int sampleRate = m_format.sampleRate();
    const double releaseSeconds = 0.12; // fade-out length once a key is released

    // Only ever generate a small lookahead chunk, never "however much free
    // space the buffer reports" -- the buffer is sized for several seconds
    // (for the one-shot playTone/playChord calls), and committing that far
    // ahead here would make new/released notes take seconds to take effect.
    int freeSamples = m_audioSink->bytesFree() / int(sizeof(int16_t));
    int maxChunk = sampleRate * m_synthTimer->interval() * 2 / 1000;
    freeSamples = qMin( freeSamples, maxChunk );
    if( freeSamples <= 0 ) return;

    double perNoteAmplitude = 0.5 / qSqrt( double(qMax(1, m_activeNotes.size())) );

    QVector<double> mix( freeSamples, 0.0 );

    for( auto it = m_activeNotes.begin() ; it != m_activeNotes.end() ; ) {
        ActiveNote &note = it.value();
        bool expired = false;

        for( int i = 0 ; i < freeSamples ; ++i ) {
            qint64 sampleIndex = m_sampleClock + i;
            double t = double( sampleIndex - note.startSample ) / sampleRate;
            double amp = pianoWaveSample( note.freqHz, t, sampleRate, perNoteAmplitude );

            if( note.releaseStartSample >= 0 ) {
                double tRelease = double( sampleIndex - note.releaseStartSample ) / sampleRate;
                if( tRelease >= releaseSeconds ) amp = 0.0;
                else amp *= 1.0 - tRelease / releaseSeconds;
            }
            mix[i] += amp;
        }

        if( note.releaseStartSample >= 0 ) {
            double tReleaseEnd = double( m_sampleClock + freeSamples - note.releaseStartSample ) / sampleRate;
            if( tReleaseEnd >= releaseSeconds ) expired = true;
        }

        it = expired ? m_activeNotes.erase(it) : std::next(it);
    }

    m_sampleClock += freeSamples;

    QByteArray buffer( freeSamples * sizeof(int16_t), Qt::Uninitialized );
    int16_t *out = reinterpret_cast<int16_t*>( buffer.data() );
    for( int i = 0 ; i < freeSamples ; ++i )
        out[i] = static_cast<int16_t>( qBound(-1.0, mix[i], 1.0) * 32767 );

    m_audioDevice -> write( buffer );

    if( m_activeNotes.isEmpty() )
        m_synthTimer -> stop(); // nothing held; stop polling until the next noteOn
}

void AudioPlayer::playChord( const QVector<int> &midiNotes , int durationMs ) {

    if( !m_audioDevice || midiNotes.isEmpty() ) {
        qWarning() << "AudioPlayer not started or empty chord";
        return;
    }

    int sampleRate = m_format.sampleRate();
    int totalSamples = sampleRate * durationMs / 1000;
    // scale each note down by sqrt(N) instead of N so a chord doesn't end up
    // much quieter than a single note, while the final qBound below still
    // catches any constructive-interference peaks so nothing clips
    double perNoteAmplitude = 0.5 / qSqrt( midiNotes.size() );

    QVector<double> mixBuffer( totalSamples , 0.0 );
    for( int midi : midiNotes ) {
        double freqHz = 440.0 * qPow( 2.0, (midi - 69) / 12.0 );
        QByteArray tone = generatePianoTone( freqHz , sampleRate , durationMs , perNoteAmplitude );
        const int16_t *toneSamples = reinterpret_cast<const int16_t*>( tone.constData() );
        int sampleCount = tone.size() / sizeof(int16_t);
        for( int i = 0 ; i < sampleCount && i < totalSamples ; ++i )
            mixBuffer[i] += toneSamples[i] / 32768.0;
    }

    QByteArray mixed( totalSamples * sizeof(int16_t), Qt::Uninitialized );
    int16_t *out = reinterpret_cast<int16_t*>( mixed.data() );
    for( int i = 0 ; i < totalSamples ; ++i )
        out[i] = static_cast<int16_t>( qBound(-1.0, mixBuffer[i], 1.0) * 32767 );

    m_audioDevice -> write( mixed );
}
