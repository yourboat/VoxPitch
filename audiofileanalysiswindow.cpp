#include "audiofileanalysiswindow.h"
#include<QVBoxLayout>
#include<QHBoxLayout>
#include<QFileDialog>
#include<QAudioBuffer>
#include<QUrl>
#include<QtMath>
#include<QPen>
#include<QFile>
#include<QDataStream>
#include<algorithm>

// QAudioDecoder hands back whatever format the source/codec naturally
// decodes to (could be any channel count or sample type) -- PitchDetector
// only understands a flat mono int16 stream, so downmix+convert here rather
// than risking setAudioFormat() outright failing decode on an unsupported
// request.
static QByteArray toMonoInt16( const QAudioBuffer &buffer ) {

    QAudioFormat fmt = buffer.format();
    int channels = qMax( 1, fmt.channelCount() );
    int frameCount = buffer.frameCount();

    QByteArray result( frameCount * int(sizeof(int16_t)), Qt::Uninitialized );
    int16_t *out = reinterpret_cast<int16_t*>( result.data() );
    const char *raw = buffer.constData<char>();

    auto sampleAt = [&]( int frame, int channel ) -> double {
        int index = frame * channels + channel;
        switch( fmt.sampleFormat() ) {
        case QAudioFormat::UInt8:
            return ( int(reinterpret_cast<const quint8*>(raw)[index]) - 128 ) / 128.0;
        case QAudioFormat::Int16:
            return reinterpret_cast<const qint16*>(raw)[index] / 32768.0;
        case QAudioFormat::Int32:
            return reinterpret_cast<const qint32*>(raw)[index] / 2147483648.0;
        case QAudioFormat::Float:
            return double( reinterpret_cast<const float*>(raw)[index] );
        default:
            return 0.0;
        }
    };

    for( int frame = 0 ; frame < frameCount ; ++frame ) {
        double sum = 0.0;
        for( int ch = 0 ; ch < channels ; ++ch )
            sum += sampleAt( frame, ch );
        double mono = sum / channels;
        out[frame] = static_cast<int16_t>( qBound(-1.0, mono, 1.0) * 32767 );
    }
    return result;
}

AudioFileAnalysisWindow::AudioFileAnalysisWindow( QWidget *parent )
    : QWidget( parent ) {

    setStyleSheet( "background-color: #14181f;" );

    QVBoxLayout *layout = new QVBoxLayout( this );

    QHBoxLayout *topLayout = new QHBoxLayout();
    m_openButton = new QPushButton( "打开音频文件", this );
    m_openButton -> setStyleSheet(
        "QPushButton { background-color: #2c3440; color: #e6e6e6; padding: 4px 10px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #3a4456; }" );
    m_statusLabel = new QLabel( "请选择一个音频文件开始分析", this );
    m_statusLabel -> setStyleSheet( "color: #9aa0a6; font-size: 13px;" );

    m_recordButton = new QPushButton( "● 录制", this );
    m_recordButton -> setEnabled( false );
    m_recordButton -> setStyleSheet(
        "QPushButton { background-color: #2c3440; color: #F44336; padding: 4px 10px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #3a4456; }"
        "QPushButton:disabled { color: #5a5f68; }" );

    m_exportButton = new QPushButton( "导出录音", this );
    m_exportButton -> setEnabled( false );
    m_exportButton -> setStyleSheet(
        "QPushButton { background-color: #2c3440; color: #e6e6e6; padding: 4px 10px; border-radius: 4px; }"
        "QPushButton:hover { background-color: #3a4456; }"
        "QPushButton:disabled { color: #5a5f68; }" );

    m_comparisonLabel = new QLabel( "", this );
    m_comparisonLabel -> setStyleSheet( "color: #9aa0a6; font-size: 13px; font-weight: bold;" );
    m_comparisonLabel -> setMinimumWidth( 140 );
    m_comparisonLabel -> setAlignment( Qt::AlignCenter );

    topLayout -> addWidget( m_openButton );
    topLayout -> addWidget( m_statusLabel , 1 );
    topLayout -> addWidget( m_comparisonLabel );
    topLayout -> addWidget( m_recordButton );
    topLayout -> addWidget( m_exportButton );
    layout -> addLayout( topLayout );

    // chart
    m_chart = new QChart();
    m_chart -> setTheme( QChart::ChartThemeDark );
    m_chart -> setTitle("音频音高曲线");
    m_chart -> legend() -> hide();
    m_chart -> setAnimationOptions( QChart::NoAnimation );
    m_chart -> setBackgroundRoundness( 8 );

    m_series = new QLineSeries();
    QPen pen( QColor("#FFC107") );
    pen.setWidth( 2 );
    m_series -> setPen( pen );
    m_chart -> addSeries( m_series );

    // the user's sung-along pitch, overlaid in a contrasting colour
    m_recordedSeries = new QLineSeries();
    QPen recordedPen( QColor("#26C6DA") );
    recordedPen.setWidth( 2 );
    m_recordedSeries -> setPen( recordedPen );
    m_chart -> addSeries( m_recordedSeries );

    // thin vertical line marking the timestamp currently held at screen centre
    m_centerLineSeries = new QLineSeries();
    QPen centerPen( QColor("#e6e6e6") );
    centerPen.setWidth( 1 );
    centerPen.setStyle( Qt::DashLine );
    m_centerLineSeries -> setPen( centerPen );
    m_chart -> addSeries( m_centerLineSeries );

    // dot highlighting whichever note sits at that centred timestamp
    m_selectionDot = new QScatterSeries();
    m_selectionDot -> setMarkerSize( 12 );
    m_selectionDot -> setColor( QColor("#ffffff") );
    m_selectionDot -> setBorderColor( QColor("#FFC107") );
    m_chart -> addSeries( m_selectionDot );

    m_axisY = new QCategoryAxis();
    m_axisY -> setLabelsPosition( QCategoryAxis::AxisLabelsPositionOnValue );
    m_chart -> addAxis( m_axisY , Qt::AlignLeft );
    m_series -> attachAxis( m_axisY );
    m_recordedSeries -> attachAxis( m_axisY );
    m_centerLineSeries -> attachAxis( m_axisY );
    m_selectionDot -> attachAxis( m_axisY );
    buildCenteredYAxis( 60 ); // placeholder window until something is analysed

    m_axisX = new QValueAxis();
    m_axisX -> setTitleText("时间 (s)");
    m_axisX -> setRange( 0, kVisibleSeconds );
    m_axisX -> setLabelFormat( "%.0f" );
    m_chart -> addAxis( m_axisX, Qt::AlignBottom );
    m_series -> attachAxis( m_axisX );
    m_recordedSeries -> attachAxis( m_axisX );
    m_centerLineSeries -> attachAxis( m_axisX );
    m_selectionDot -> attachAxis( m_axisX );

    connect( m_axisX, &QValueAxis::rangeChanged, this, &AudioFileAnalysisWindow::updateSelectionFromCenter );

    m_chartView = new PannableChartView( m_chart, this );
    m_chartView -> setRenderHint( QPainter::Antialiasing );
    m_chartView -> setVerticalPanEnabled( false ); // the Y window is managed automatically
    layout -> addWidget( m_chartView , 1 );

    m_pitchTipLabel = new QLabel( m_chartView );
    m_pitchTipLabel -> setStyleSheet(
        "background-color: #FFC107; color: #14181f;"
        "border-radius: 4px; padding: 2px 6px; font-weight: bold;" );
    m_pitchTipLabel -> hide();

    // transport bar, bottom-centre like a media player: skip-back / play-pause
    // / skip-forward icons, then current-time -- seek slider -- total-time
    QHBoxLayout *transportLayout = new QHBoxLayout();

    m_skipBackButton = new SkipButton( false, int(kSkipSeconds), this );
    m_skipBackButton -> setEnabled( false );

    m_playPauseButton = new QPushButton( "▶", this );
    m_playPauseButton -> setFixedSize( 40, 40 );
    m_playPauseButton -> setEnabled( false );
    m_playPauseButton -> setFocusPolicy( Qt::NoFocus ); // avoid double-toggling with the Space shortcut below
    m_playPauseButton -> setStyleSheet(
        "QPushButton { background-color: transparent; color: #e6e6e6; border: none; font-size: 18px; }"
        "QPushButton:hover { color: #FFC107; }"
        "QPushButton:disabled { color: #5a5f68; }" );

    m_skipForwardButton = new SkipButton( true, int(kSkipSeconds), this );
    m_skipForwardButton -> setEnabled( false );

    m_currentTimeLabel = new QLabel( "00:00", this );
    m_totalTimeLabel = new QLabel( "00:00", this );
    for( QLabel *l : { m_currentTimeLabel, m_totalTimeLabel } )
        l -> setStyleSheet( "color: #9aa0a6; font-size: 12px;" );

    m_seekSlider = new QSlider( Qt::Horizontal, this );
    m_seekSlider -> setRange( 0, 0 );
    m_seekSlider -> setEnabled( false );
    m_seekSlider -> setStyleSheet(
        "QSlider::groove:horizontal { height: 4px; background: #3a4456; border-radius: 2px; }"
        "QSlider::handle:horizontal { width: 14px; height: 14px; margin: -5px 0; border-radius: 7px; background: #e6e6e6; }"
        "QSlider::sub-page:horizontal { background: #FFC107; border-radius: 2px; }" );

    transportLayout -> addSpacing( 8 );
    transportLayout -> addWidget( m_skipBackButton );
    transportLayout -> addWidget( m_playPauseButton );
    transportLayout -> addWidget( m_skipForwardButton );
    transportLayout -> addSpacing( 8 );
    transportLayout -> addWidget( m_currentTimeLabel );
    transportLayout -> addWidget( m_seekSlider , 1 );
    transportLayout -> addWidget( m_totalTimeLabel );
    transportLayout -> addSpacing( 8 );
    layout -> addLayout( transportLayout );

    QShortcut *spaceShortcut = new QShortcut( QKeySequence(Qt::Key_Space), this );
    connect( spaceShortcut, &QShortcut::activated, this, &AudioFileAnalysisWindow::onPlayPauseClicked );

    // offline analysis pipeline: decode the whole file as fast as possible
    // through the same YIN detector used live, building a full pitch curve
    // before any audible playback starts
    m_detector = new PitchDetector( this );
    m_decoder = new QAudioDecoder( this );
    connect( m_decoder, &QAudioDecoder::bufferReady, this, &AudioFileAnalysisWindow::onDecoderBufferReady );
    connect( m_decoder, &QAudioDecoder::finished, this, &AudioFileAnalysisWindow::onDecodeFinished );
    connect( m_decoder, QOverload<QAudioDecoder::Error>::of(&QAudioDecoder::error),
             this, &AudioFileAnalysisWindow::onDecodeError );
    connect( m_detector, &PitchDetector::pitchDetected, this, &AudioFileAnalysisWindow::onPitchResult );

    // playback: a separate, ordinary QMediaPlayer just for listening
    m_audioOutput = new QAudioOutput( this );
    m_mediaPlayer = new QMediaPlayer( this );
    m_mediaPlayer -> setAudioOutput( m_audioOutput );
    connect( m_mediaPlayer, &QMediaPlayer::playbackStateChanged, this, &AudioFileAnalysisWindow::onPlaybackStateChanged );
    connect( m_mediaPlayer, &QMediaPlayer::durationChanged, this, &AudioFileAnalysisWindow::onDurationChanged );

    m_syncTimer = new QTimer( this );
    m_syncTimer -> setInterval( 40 ); // ~25 ticks/sec, matches the curve's own ~46ms point spacing
    connect( m_syncTimer, &QTimer::timeout, this, &AudioFileAnalysisWindow::onSyncTick );

    connect( m_openButton, &QPushButton::clicked, this, &AudioFileAnalysisWindow::onOpenFile );
    connect( m_playPauseButton, &QPushButton::clicked, this, &AudioFileAnalysisWindow::onPlayPauseClicked );
    connect( m_skipBackButton, &QAbstractButton::clicked, this, &AudioFileAnalysisWindow::skipBackward );
    connect( m_skipForwardButton, &QAbstractButton::clicked, this, &AudioFileAnalysisWindow::skipForward );
    connect( m_seekSlider, &QSlider::sliderPressed, this, &AudioFileAnalysisWindow::onSeekSliderPressed );
    connect( m_seekSlider, &QSlider::sliderMoved, this, &AudioFileAnalysisWindow::onSeekSliderMoved );
    connect( m_seekSlider, &QSlider::sliderReleased, this, &AudioFileAnalysisWindow::onSeekSliderReleased );

    // sing-along recording: a second, independent capture+detector pair so
    // it never interferes with the offline file analysis above
    m_micCapturer = new AudioCapturer( this );
    m_micDetector = new PitchDetector( this );
    connect( m_micCapturer, &AudioCapturer::audioDataReady, m_micDetector, &PitchDetector::processSamples );
    connect( m_micCapturer, &AudioCapturer::audioDataReady, this, &AudioFileAnalysisWindow::onMicAudioCaptured );
    connect( m_micDetector, &PitchDetector::pitchDetected, this, &AudioFileAnalysisWindow::onMicPitchResult );

    connect( m_recordButton, &QPushButton::clicked, this, &AudioFileAnalysisWindow::onRecordClicked );
    connect( m_exportButton, &QPushButton::clicked, this, &AudioFileAnalysisWindow::onExportClicked );
}

void AudioFileAnalysisWindow::onOpenFile() {

    QString path = QFileDialog::getOpenFileName( this, "选择音频文件", QString(),
        "音频文件 (*.mp3 *.wav *.m4a *.flac *.aac *.ogg);;所有文件 (*)" );
    if( path.isEmpty() ) return;

    resetForNewFile();
    m_statusLabel -> setText( "正在分析音频，请稍候..." );

    m_decoder -> setSource( QUrl::fromLocalFile(path) );
    m_decoder -> start();

    m_mediaPlayer -> setSource( QUrl::fromLocalFile(path) );
}

void AudioFileAnalysisWindow::resetForNewFile() {

    if( m_recording ) stopRecording();

    m_mediaPlayer -> stop();
    m_syncTimer -> stop();

    m_curve.clear();
    m_analyzedSamples = 0;
    m_analysisDone = false;
    m_selectedSeconds = 0.0;
    m_axisCenterMidi = INT_MIN;

    m_recordedSamples.clear();
    m_recordedCurve.clear();
    m_recordSampleRate = 0;
    m_recordedSeries -> clear();
    m_comparisonLabel -> setText( "" );

    m_series -> clear();
    m_centerLineSeries -> clear();
    m_selectionDot -> clear();
    m_pitchTipLabel -> hide();
    m_axisX -> setRange( 0, kVisibleSeconds );
    buildCenteredYAxis( 60 );

    m_playPauseButton -> setEnabled( false );
    m_playPauseButton -> setText( "▶" );
    m_skipBackButton -> setEnabled( false );
    m_skipForwardButton -> setEnabled( false );
    m_recordButton -> setEnabled( false );
    m_exportButton -> setEnabled( false );

    m_seekSlider -> setEnabled( false );
    m_seekSlider -> setRange( 0, 0 );
    m_currentTimeLabel -> setText( "00:00" );
    m_totalTimeLabel -> setText( "00:00" );
}

void AudioFileAnalysisWindow::onDecoderBufferReady() {

    QAudioBuffer buffer = m_decoder -> read();
    if( !buffer.isValid() ) return;

    m_detector -> setSampleRate( buffer.format().sampleRate() );
    QByteArray mono = toMonoInt16( buffer );
    m_detector -> processSamples( mono );
}

void AudioFileAnalysisWindow::onPitchResult( const PitchResult &result ) {

    m_analyzedSamples += m_detector -> windowSize();
    double seconds = double(m_analyzedSamples) / m_detector -> sampleRate();
    double midi = result.isDetected ? frequencyToMidi( result.frequencyHz ) : 0.0;
    m_curve.append( { seconds, midi, result.isDetected } );
}

void AudioFileAnalysisWindow::onDecodeFinished() {

    m_analysisDone = true;
    buildFullCurveChart();

    m_recordedCurve.resize( m_curve.size() );
    for( int i = 0 ; i < m_curve.size() ; ++i )
        m_recordedCurve[i] = { m_curve[i].seconds, 0.0, false };

    m_statusLabel -> setText( QString("分析完成，共 %1 个音高点，可以拖动音高线选择位置后播放").arg( m_curve.size() ) );
    m_playPauseButton -> setEnabled( true );
    m_skipBackButton -> setEnabled( true );
    m_skipForwardButton -> setEnabled( true );
    m_seekSlider -> setEnabled( true );
    m_recordButton -> setEnabled( true );
}

void AudioFileAnalysisWindow::buildFullCurveChart() {

    m_series -> clear();
    for( const CurvePoint &p : m_curve )
        if( p.detected )
            m_series -> append( p.seconds, p.midi );

    // start the view at the beginning of the file; updateSelectionFromCenter()
    // fires from this setRange() and picks up whatever note is nearest 0s
    m_axisX -> setRange( 0, kVisibleSeconds );
}

void AudioFileAnalysisWindow::onDecodeError( QAudioDecoder::Error error ) {

    Q_UNUSED( error );
    m_statusLabel -> setText( QString("音频解析失败：%1").arg( m_decoder->errorString() ) );
}

void AudioFileAnalysisWindow::onPlayPauseClicked() {

    if( m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState ) {
        m_mediaPlayer -> pause();
    } else {
        m_mediaPlayer -> setPosition( qint64(m_selectedSeconds * 1000) );
        m_seekSlider -> setValue( int(m_selectedSeconds * 1000) );
        m_mediaPlayer -> play();
    }
}

void AudioFileAnalysisWindow::skipBackward() { skipBy( -kSkipSeconds ); }
void AudioFileAnalysisWindow::skipForward() { skipBy( kSkipSeconds ); }

void AudioFileAnalysisWindow::skipBy( double deltaSeconds ) {

    double upperBound = m_mediaPlayer->duration() > 0
        ? double(m_mediaPlayer->duration()) / 1000.0
        : ( m_curve.isEmpty() ? 1e9 : m_curve.last().seconds );
    double target = qBound( 0.0, m_selectedSeconds + deltaSeconds, upperBound );
    m_selectedSeconds = target;

    if( m_mediaPlayer->playbackState() == QMediaPlayer::PlayingState ) {
        // just seek; the running sync timer will recentre the view (and the
        // slider) on the next tick using the real playback position
        m_mediaPlayer -> setPosition( qint64(target * 1000) );
    } else {
        double half = ( m_axisX->max() - m_axisX->min() ) / 2.0;
        m_axisX -> setRange( target - half, target + half ); // triggers updateSelectionFromCenter
        m_seekSlider -> setValue( int(target * 1000) );
        m_currentTimeLabel -> setText( formatTime( qint64(target * 1000) ) );
    }
}

void AudioFileAnalysisWindow::onPlaybackStateChanged( QMediaPlayer::PlaybackState state ) {

    if( state == QMediaPlayer::PlayingState ) {
        m_playPauseButton -> setText( "⏸" );
        m_syncTimer -> start();
    } else {
        m_playPauseButton -> setText( "▶" );
        m_syncTimer -> stop();
    }
}

void AudioFileAnalysisWindow::onSyncTick() {

    qint64 positionMs = m_mediaPlayer -> position();

    // re-centre the view on actual playback position; updateSelectionFromCenter()
    // (connected to rangeChanged) picks up the dot/line/tip-label update
    double seconds = positionMs / 1000.0;
    double half = ( m_axisX->max() - m_axisX->min() ) / 2.0;
    m_axisX -> setRange( seconds - half, seconds + half );

    if( !m_sliderBeingDragged ) {
        m_seekSlider -> setValue( int(positionMs) );
        m_currentTimeLabel -> setText( formatTime(positionMs) );
    }
}

void AudioFileAnalysisWindow::onDurationChanged( qint64 durationMs ) {
    m_seekSlider -> setRange( 0, int(durationMs) );
    m_totalTimeLabel -> setText( formatTime(durationMs) );
}

void AudioFileAnalysisWindow::onSeekSliderPressed() {
    m_sliderBeingDragged = true;
}

void AudioFileAnalysisWindow::onSeekSliderMoved( int valueMs ) {
    // live-preview the position on the chart while dragging, without
    // spamming QMediaPlayer::setPosition() on every pixel of the drag
    m_currentTimeLabel -> setText( formatTime(valueMs) );
    double target = valueMs / 1000.0;
    double half = ( m_axisX->max() - m_axisX->min() ) / 2.0;
    m_axisX -> setRange( target - half, target + half );
}

void AudioFileAnalysisWindow::onSeekSliderReleased() {
    m_sliderBeingDragged = false;
    m_mediaPlayer -> setPosition( m_seekSlider->value() );
}

QString AudioFileAnalysisWindow::formatTime( qint64 ms ) {
    qint64 totalSeconds = ms / 1000;
    return QString( "%1:%2" )
        .arg( totalSeconds / 60, 2, 10, QChar('0') )
        .arg( totalSeconds % 60, 2, 10, QChar('0') );
}

int AudioFileAnalysisWindow::nearestCurveIndex( double seconds ) const {

    if( m_curve.isEmpty() ) return -1;

    auto it = std::lower_bound( m_curve.begin(), m_curve.end(), seconds,
        []( const CurvePoint &p, double s ) { return p.seconds < s; } );

    if( it == m_curve.begin() ) return 0;
    if( it == m_curve.end() ) return m_curve.size() - 1;

    int idx = int( it - m_curve.begin() );
    if( qAbs(m_curve[idx].seconds - seconds) < qAbs(m_curve[idx - 1].seconds - seconds) )
        return idx;
    return idx - 1;
}

void AudioFileAnalysisWindow::updateSelectionFromCenter() {

    double centerSeconds = ( m_axisX->min() + m_axisX->max() ) / 2.0;

    int idx = nearestCurveIndex( centerSeconds );
    if( idx < 0 ) return;
    const CurvePoint &point = m_curve[idx];
    m_selectedSeconds = point.seconds;

    if( !point.detected ) {
        m_selectionDot -> clear();
        m_centerLineSeries -> clear();
        m_pitchTipLabel -> hide();
        return;
    }

    int newCenter = qRound( point.midi );
    if( newCenter != m_axisCenterMidi )
        buildCenteredYAxis( newCenter );

    m_selectionDot -> clear();
    m_selectionDot -> append( point.seconds, point.midi );

    m_centerLineSeries -> clear();
    m_centerLineSeries -> append( point.seconds, m_axisY->min() );
    m_centerLineSeries -> append( point.seconds, m_axisY->max() );

    QPointF tipPos = m_chart -> mapToPosition( QPointF(point.seconds, point.midi), m_series );
    m_pitchTipLabel -> setText( PitchDetector::noteNameForMidi( qRound(point.midi) ) );
    m_pitchTipLabel -> adjustSize();
    m_pitchTipLabel -> move( qRound(tipPos.x()) - m_pitchTipLabel->width() / 2,
                              qRound(tipPos.y()) - m_pitchTipLabel->height() - 12 );
    m_pitchTipLabel -> show();
    m_pitchTipLabel -> raise();
}

void AudioFileAnalysisWindow::buildCenteredYAxis( int centerMidi ) {

    m_axisCenterMidi = centerMidi;

    const QStringList oldLabels = m_axisY -> categoriesLabels();
    for( const QString &label : oldLabels )
        m_axisY -> remove( label );

    static const bool isNatural[12] = {
        true, false, true, false, true, true, false, true, false, true, false, true
    };
    auto natural = []( int midi ) {
        int n = midi % 12;
        if( n < 0 ) n += 12;
        return isNatural[n];
    };

    int low = centerMidi - 6;
    int high = low + kVisibleSemitones - 1;
    while( !natural(low) ) --low;
    while( !natural(high) ) ++high;

    m_axisY -> setStartValue( low - 1 );
    for( int midi = low ; midi <= high ; ++midi ) {
        if( natural(midi) )
            m_axisY -> append( PitchDetector::noteNameForMidi( midi ) , midi );
    }

    m_axisY -> setRange( low - 1, high );
}

qreal AudioFileAnalysisWindow::frequencyToMidi( double freq ) const {
    if( freq <= 0 ) return 0;
    return 69.0 + 12.0 * qLn(freq / 440.0) / qLn(2.0);
}

void AudioFileAnalysisWindow::onRecordClicked() {
    if( m_recording ) stopRecording();
    else startRecording();
}

void AudioFileAnalysisWindow::startRecording() {

    if( !m_analysisDone ) return;

    m_recordStartSeconds = m_selectedSeconds;
    m_micAnalyzedSamples = 0;

    m_micCapturer -> start();
    int micRate = m_micCapturer->format().sampleRate();
    m_micDetector -> setSampleRate( micRate );

    // (re)allocate the recording buffer the first time we know both the mic's
    // sample rate and the track's duration; later takes reuse it as-is so
    // sections outside the new take keep whatever was recorded before
    if( m_recordedSamples.isEmpty() ) {
        m_recordSampleRate = micRate;
        double durationSeconds = m_mediaPlayer->duration() > 0
            ? double(m_mediaPlayer->duration()) / 1000.0
            : ( m_curve.isEmpty() ? 0.0 : m_curve.last().seconds + 1.0 );
        m_recordedSamples.fill( 0, qMax(0, int(durationSeconds * m_recordSampleRate)) );
    }
    m_recordWriteSample = qint64( m_recordStartSeconds * m_recordSampleRate );

    m_recording = true;
    m_recordButton -> setText( "■ 停止录制" );

    // keep the track running for timing/comparison purposes, but mute its
    // speaker output so it doesn't bleed into the microphone recording
    m_audioOutput -> setMuted( true );
    if( m_mediaPlayer->playbackState() != QMediaPlayer::PlayingState ) {
        m_mediaPlayer -> setPosition( qint64(m_recordStartSeconds * 1000) );
        m_mediaPlayer -> play();
    }
}

void AudioFileAnalysisWindow::stopRecording() {

    if( !m_recording ) return;
    m_recording = false;
    m_micCapturer -> stop();
    m_audioOutput -> setMuted( false );
    m_recordButton -> setText( "● 录制" );
    m_exportButton -> setEnabled( !m_recordedSamples.isEmpty() );
}

void AudioFileAnalysisWindow::onMicAudioCaptured( const QByteArray &data ) {

    if( !m_recording || m_recordedSamples.isEmpty() ) return;

    const int16_t *samples = reinterpret_cast<const int16_t*>( data.constData() );
    int count = data.size() / int(sizeof(int16_t));

    for( int i = 0 ; i < count ; ++i ) {
        qint64 idx = m_recordWriteSample + i;
        if( idx >= 0 && idx < m_recordedSamples.size() )
            m_recordedSamples[idx] = samples[i];
    }
    m_recordWriteSample += count;
}

void AudioFileAnalysisWindow::onMicPitchResult( const PitchResult &result ) {

    if( !m_recording ) return;

    m_micAnalyzedSamples += m_micDetector -> windowSize();
    double seconds = m_recordStartSeconds + double(m_micAnalyzedSamples) / m_micDetector->sampleRate();

    int idx = nearestCurveIndex( seconds );
    if( idx < 0 || idx >= m_recordedCurve.size() ) return;

    double midi = result.isDetected ? frequencyToMidi( result.frequencyHz ) : 0.0;
    m_recordedCurve[idx] = { m_curve[idx].seconds, midi, result.isDetected };

    rebuildRecordedSeries();
    updateComparisonFeedback( idx );
}

void AudioFileAnalysisWindow::rebuildRecordedSeries() {

    QList<QPointF> points;
    points.reserve( m_recordedCurve.size() );
    for( const CurvePoint &p : m_recordedCurve )
        if( p.detected )
            points.append( QPointF(p.seconds, p.midi) );
    m_recordedSeries -> replace( points );
}

void AudioFileAnalysisWindow::updateComparisonFeedback( int idx ) {

    const CurvePoint &recorded = m_recordedCurve[idx];
    const CurvePoint &reference = m_curve[idx];

    if( !recorded.detected || !reference.detected ) {
        m_comparisonLabel -> setText( "" );
        return;
    }

    double deviation = recorded.midi - reference.midi; // semitones; +ve = sung too high
    if( qAbs(deviation) < 0.25 ) {
        m_comparisonLabel -> setText( "✓ 准确" );
        m_comparisonLabel -> setStyleSheet( "color: #4CAF50; font-size: 13px; font-weight: bold;" );
    } else if( deviation > 0 ) {
        m_comparisonLabel -> setText( QString("音高了 (+%1 音分)").arg( qRound(deviation * 100) ) );
        m_comparisonLabel -> setStyleSheet( "color: #F44336; font-size: 13px; font-weight: bold;" );
    } else {
        m_comparisonLabel -> setText( QString("音低了 (%1 音分)").arg( qRound(deviation * 100) ) );
        m_comparisonLabel -> setStyleSheet( "color: #2196F3; font-size: 13px; font-weight: bold;" );
    }
}

void AudioFileAnalysisWindow::onExportClicked() {

    if( m_recordedSamples.isEmpty() ) return;

    QString path = QFileDialog::getSaveFileName( this, "导出录音", "recording.wav", "WAV 音频 (*.wav)" );
    if( path.isEmpty() ) return;

    exportRecordingToWav( path );
}

void AudioFileAnalysisWindow::exportRecordingToWav( const QString &path ) {

    QFile file( path );
    if( !file.open( QIODevice::WriteOnly ) ) {
        m_statusLabel -> setText( QString("导出失败：无法写入 %1").arg(path) );
        return;
    }

    qint32 dataSize = m_recordedSamples.size() * int(sizeof(int16_t));
    qint32 sampleRate = m_recordSampleRate;
    qint16 numChannels = 1;
    qint16 bitsPerSample = 16;
    qint32 byteRate = sampleRate * numChannels * bitsPerSample / 8;
    qint16 blockAlign = numChannels * bitsPerSample / 8;
    qint32 riffChunkSize = 36 + dataSize;

    QDataStream out( &file );
    out.setByteOrder( QDataStream::LittleEndian );

    out.writeRawData( "RIFF", 4 );
    out << riffChunkSize;
    out.writeRawData( "WAVE", 4 );
    out.writeRawData( "fmt ", 4 );
    out << qint32(16);            // PCM fmt chunk size
    out << qint16(1);             // PCM format
    out << numChannels;
    out << sampleRate;
    out << byteRate;
    out << blockAlign;
    out << bitsPerSample;
    out.writeRawData( "data", 4 );
    out << dataSize;
    out.writeRawData( reinterpret_cast<const char*>(m_recordedSamples.constData()), dataSize );

    file.close();
    m_statusLabel -> setText( QString("已导出到 %1").arg(path) );
}
