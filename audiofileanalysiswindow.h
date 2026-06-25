#ifndef AUDIOFILEANALYSISWINDOW_H
#define AUDIOFILEANALYSISWINDOW_H

#include<QWidget>
#include<QLabel>
#include<QPushButton>
#include<QSlider>
#include<QAudioDecoder>
#include<QMediaPlayer>
#include<QAudioOutput>
#include<QTimer>
#include<QShortcut>
#include<QtCharts/QChart>
#include<QtCharts/QLineSeries>
#include<QtCharts/QScatterSeries>
#include<QtCharts/QValueAxis>
#include<QtCharts/QCategoryAxis>
#include<climits>
#include "pannablechartview.h"
#include "skipbutton.h"
#include "pitchdetector.h"
#include "audiocapturer.h"

// third page: open an audio file, analyse its pitch curve offline with the
// same YIN detector used for the microphone. The chart shows a fixed ~1
// octave Y window and a fixed centre point on screen; dragging scrubs the
// curve left/right underneath that centre (like a needle), and whichever
// note currently sits there gets a highlighted dot + note-name tag.
// Playback starts from wherever is centred, and auto-scrolls the same way
// while it plays.
class AudioFileAnalysisWindow : public QWidget {

    Q_OBJECT

public:

    explicit AudioFileAnalysisWindow( QWidget *parent = nullptr );

private slots:

    void onOpenFile();
    void onDecoderBufferReady();
    void onDecodeFinished();
    void onDecodeError( QAudioDecoder::Error error );
    void onPitchResult( const PitchResult &result );
    void onPlayPauseClicked();
    void onSyncTick();
    void onPlaybackStateChanged( QMediaPlayer::PlaybackState state );
    void updateSelectionFromCenter();
    void skipBackward();
    void skipForward();
    void onDurationChanged( qint64 durationMs );
    void onSeekSliderPressed();
    void onSeekSliderMoved( int valueMs );
    void onSeekSliderReleased();
    void onRecordClicked();
    void onMicAudioCaptured( const QByteArray &data );
    void onMicPitchResult( const PitchResult &result );
    void onExportClicked();

private:

    struct CurvePoint { double seconds; double midi; bool detected; };

    static constexpr int kVisibleSemitones = 12;
    static constexpr double kVisibleSeconds = 10.0;
    static constexpr double kSkipSeconds = 10.0;

    void resetForNewFile();
    void buildFullCurveChart();
    void buildCenteredYAxis( int centerMidi );
    int nearestCurveIndex( double seconds ) const;
    qreal frequencyToMidi( double freq ) const;
    void skipBy( double deltaSeconds );
    static QString formatTime( qint64 ms );
    void startRecording();
    void stopRecording();
    void rebuildRecordedSeries();
    void updateComparisonFeedback( int idx );
    void exportRecordingToWav( const QString &path );

    // controls
    QPushButton *m_openButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    SkipButton *m_skipBackButton = nullptr;
    QPushButton *m_playPauseButton = nullptr;
    SkipButton *m_skipForwardButton = nullptr;
    QLabel *m_currentTimeLabel = nullptr;
    QLabel *m_totalTimeLabel = nullptr;
    QSlider *m_seekSlider = nullptr;
    bool m_sliderBeingDragged = false;
    QPushButton *m_recordButton = nullptr;
    QPushButton *m_exportButton = nullptr;
    QLabel *m_comparisonLabel = nullptr;

    // chart
    PannableChartView *m_chartView = nullptr;
    QChart *m_chart = nullptr;
    QLineSeries *m_series = nullptr;          // the whole song's pitch curve
    QLineSeries *m_recordedSeries = nullptr;   // the user's sung-along pitch curve
    QLineSeries *m_centerLineSeries = nullptr; // thin vertical line at the selected moment
    QScatterSeries *m_selectionDot = nullptr;  // highlights the note currently centred
    QCategoryAxis *m_axisY = nullptr;
    QValueAxis *m_axisX = nullptr;
    QLabel *m_pitchTipLabel = nullptr;

    int m_axisCenterMidi = INT_MIN;
    double m_selectedSeconds = 0.0; // timestamp currently centred on screen

    // offline analysis
    QAudioDecoder *m_decoder = nullptr;
    PitchDetector *m_detector = nullptr;
    qint64 m_analyzedSamples = 0; // total samples consumed by completed windows so far
    QVector<CurvePoint> m_curve;  // full precomputed pitch-over-time curve
    bool m_analysisDone = false;

    // playback
    QMediaPlayer *m_mediaPlayer = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QTimer *m_syncTimer = nullptr;

    // sing-along recording: mic input captured in sync with playback,
    // overdub-style (re-recording overwrites only the re-recorded range)
    AudioCapturer *m_micCapturer = nullptr;
    PitchDetector *m_micDetector = nullptr;
    bool m_recording = false;
    int m_recordSampleRate = 0;
    double m_recordStartSeconds = 0.0;   // timeline position the current take began at
    qint64 m_recordWriteSample = 0;      // running write cursor into m_recordedSamples
    qint64 m_micAnalyzedSamples = 0;     // running sample count for the mic's own detector
    QVector<int16_t> m_recordedSamples;  // raw mono PCM, aligned to the track's timeline
    QVector<CurvePoint> m_recordedCurve; // one slot per m_curve entry; latest take wins

};

#endif
