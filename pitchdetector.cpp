#include "pitchdetector.h"
#include<QtMath>
#include<QDebug>

PitchDetector::PitchDetector( QObject *parent )
    : QObject{parent} {

    m_ringBuffer.reserve(m_bufferSize);
}

void PitchDetector::processSamples( const QByteArray &data ) {

    //QByteArray to int16_t
    const int16_t *samples = reinterpret_cast< const int16_t* >( data.constData() );
    int sampleCount = data.size() / sizeof(int16_t);

    //writing into the ring buffer
    m_ringBuffer.append( QList<int16_t>(samples, samples + sampleCount) );

    //get windows and detect
    while( m_ringBuffer.size() >= m_windowSize ) {

        QVector<int16_t> frameSamples = m_ringBuffer.mid( 0 , m_windowSize );
        // consume the whole window (no overlap). The previous 75%-overlap
        // (hop = windowSize/4) re-ran the O(N*maxTau) YIN search ~4x more
        // often than needed and fired a UI update ~86 times/sec, which made
        // the single GUI thread fall behind real-time audio and built up a
        // growing backlog -> the perceived "severe lag".
        m_ringBuffer.remove( 0 , m_windowSize );

        //trans to double : -1.0 ~ 1.0
        QVector<double> frame( m_windowSize );
        const double scale = 1.0 / 32768.0;
        double sumSquares = 0.0;
        for( int i = 0 ; i < m_windowSize ; ++i ) {
            frame[i] = frameSamples[i] * scale;
            sumSquares += frame[i] * frame[i];
        }
        double rms = qSqrt( sumSquares / m_windowSize );

        // Below this level we're hearing a note's release tail, a breath, or
        // room noise rather than real phonation. YIN doesn't know the
        // difference and will happily report a confident (usually wrong,
        // often an octave low) pitch on what's basically silence, which
        // shows up as a sharp downward glitch right at the end of a phrase.
        // Skip detection outright instead of trusting that result.
        double freq = ( rms < m_minRmsEnergy ) ? 0.0 : yinDetect(frame);

        //result
        PitchResult result;
        if( freq > 0.0 && freq < m_sampleRate / 2.0 ) {

            result.isDetected = true;
            result.frequencyHz = freq;
            result.noteName = frequencyToNoteName( freq );
            //clac cent deviation
            //440Hz -> A4
            //midi = 69 + 12 × log2( freq / 440 )
            //cent = 1200 × log2( ratio )
            double closestNoteFreq = 440.0 * qPow( 2.0 , ( qRound( 12.0 * qLn( freq / 440.0 ) / qLn(2.0) ) ) / 12.0 );
            result.centDeviation = 1200.0 * qLn( freq / closestNoteFreq ) / qLn( 2.0 ); //log2
        } 
        else {
            result.isDetected = false;
        }

        emit pitchDetected( result );
    }
}

double PitchDetector::yinDetect( const QVector<double> &frame ) {

    const int N = frame.size();
    if( N < 2 ) return 0.0;

    // cap tau so we never search into the tail of the window, where only a
    // handful of samples remain to compare and d(tau) collapses toward 0
    // regardless of real periodicity (always looks "periodic" at very low
    // fake frequencies). Limit tau to the lowest vocal pitch we care about.
    int maxTau = qMin( N - 1 , static_cast<int>( m_sampleRate / m_minFrequency ) );

    //d(t) = sum_{j} ( x[j] - x[j+t] )^2
    //get the cycle : tau
    QVector<double> d( maxTau + 1, 0.0 );
    for( int tau = 0 ; tau <= maxTau ; ++tau ) {
        double sum = 0.0;
        for( int j = 0 ; j < N - tau ; ++j ) {
            double diff = frame[j] - frame[j + tau];
            sum += diff * diff;
        }
        d[tau] = sum;
    }

    //d'(tau) = d(tau) / [ (1/tau) * sum_{j=1}^{tau} d(j) ]
    //to exclude noise
    QVector<double> dNorm( maxTau + 1, 0.0 );
    dNorm[0] = 1.0;
    double runningSum = 0.0;
    for( int tau = 1 ; tau <= maxTau ; ++tau ) {
        runningSum += d[tau];
        double avg = runningSum / tau;
        dNorm[tau] = (avg > 0.0) ? d[tau] / avg : 1.0;
    }

    //find first dNorm < threshold
    int tauEstimate = -1;
    for( int tau = 2; tau <= maxTau; ++tau ) {
        if( dNorm[tau] < m_yinThreshold ) {
            while( tau + 1 <= maxTau && dNorm[tau + 1] < dNorm[tau] ) {
                tau++;
            }
            tauEstimate = tau;
            break;
        }
    }

    if( tauEstimate == -1 ) return 0.0;

    //parabolic interpolation method
    if( tauEstimate > 1 && tauEstimate < maxTau ) {
        double s0 = dNorm[tauEstimate - 1];
        double s1 = dNorm[tauEstimate];
        double s2 = dNorm[tauEstimate + 1];
        double correction = (s2 - s0) / (2.0 * (2.0 * s1 - s2 - s0));
        double tauFine = tauEstimate + correction;
        return m_sampleRate / tauFine;
    }

    return m_sampleRate / tauEstimate;
}

QString PitchDetector::frequencyToNoteName( double freq ) const {

    if( freq <= 0 ) return "—";
    // A4 -> 440 Hz -> MIDI 69
    double midi = 69.0 + 12.0 * qLn( freq / 440.0 ) / qLn(2.0);
    return noteNameForMidi( qRound( midi ) );

}

QString PitchDetector::noteNameForMidi( int midiNumber ) {

    if( midiNumber < 0 || midiNumber > 127 ) return "—";

    static const QString noteNames[] = {"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    int octave = ( midiNumber / 12 ) - 1;
    int semitone = midiNumber % 12;
    return noteNames[semitone] + QString::number(octave);

}