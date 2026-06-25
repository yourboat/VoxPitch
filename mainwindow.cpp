#include "mainwindow.h"
#include<QVBoxLayout>
#include<QHBoxLayout>
#include<QDateTime>
#include<QPen>
#include<QDebug>
#include<QPushButton>

MainWindow::MainWindow( QWidget *parent )
    : QMainWindow( parent ) {
    
    setupUI();
    setupAudio();
    
    m_startTime = QDateTime::currentDateTime().toMSecsSinceEpoch();
    setWindowTitle( "VoxPitch - 声乐辅助训练" );
    resize( 800, 500 );

    setFocusPolicy(Qt::StrongFocus);
    setFocus();
}

MainWindow::~MainWindow() {}

void MainWindow::keyPressEvent( QKeyEvent *event ) {
    // only the real-time page's piano listens for computer-keyboard play;
    // otherwise typing while on the training page would silently trigger
    // notes on a piano you can't even see
    if( m_piano && m_stack && m_stack->currentWidget() == m_realtimeTab )
        m_piano -> keyPressEvent( event );
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent( QKeyEvent *event ) {
    if( m_piano && m_stack && m_stack->currentWidget() == m_realtimeTab )
        m_piano -> keyReleaseEvent( event );
    QMainWindow::keyReleaseEvent(event);
}

void MainWindow::onSidebarRowChanged( int row ) {
    m_stack -> setCurrentIndex( row );
    if( row == 1 )
        m_trainerWindow -> ensureStarted();
}


void MainWindow::setupAudio() {

    m_capturer = new AudioCapturer(this);
    m_detector = new PitchDetector(this);
    m_player = new AudioPlayer(this);
    m_player -> start();

    connect(m_capturer, &AudioCapturer::audioDataReady,
            m_detector, &PitchDetector::processSamples);

    connect(m_detector, &PitchDetector::pitchDetected,
                  this, &MainWindow::onPitchDetected);

    // clean highlight if no effective note last 800ms
    m_highlightTimer = new QTimer(this);
    m_highlightTimer -> setSingleShot(true);
    m_highlightTimer -> setInterval( 800 ); 
    connect(m_highlightTimer, &QTimer::timeout,
                        this, &MainWindow::clearPianoHighlight);

    // note on: any number of keys can be held/overlap at once, each mixed
    // together and sustained for as long as that key stays held
    connect(m_piano, &PianoKeyboard::noteOn,
        this, [this](int midi) {
            m_pressedKeys.insert( midi );
            m_userKeyPressed = true;
            m_piano -> highlightNote( midi );
            m_highlightTimer -> stop();
            m_player -> noteOn( midi );
        });

    // note off
    connect(m_piano, &PianoKeyboard::noteOff,
        this, [this](int midi) {
            m_pressedKeys.remove( midi );
            m_userKeyPressed = !m_pressedKeys.isEmpty();
            m_player -> noteOff( midi );
            if (!m_highlightTimer -> isActive())
                m_highlightTimer -> start();
        });

    // detector -> highlight
    connect(m_detector, &PitchDetector::pitchDetected,
         this, [this](const PitchResult &res) {

            if (m_userKeyPressed) return;

            if( res.isDetected ) {
                int midi = qRound( frequencyToMidi( res.frequencyHz ) );
                m_piano -> highlightNote( midi );
                m_highlightTimer -> stop();
            }
            else {
                if( !m_highlightTimer -> isActive() )
                    m_highlightTimer -> start();
            }
        });

    m_capturer->start();

    // AudioCapturer may fall back to the device's preferred format (e.g. a
    // different sample rate) if 44.1kHz mono isn't directly supported; keep
    // PitchDetector in sync so frequency math stays correct.
    const QAudioFormat capturedFormat = m_capturer->format();
    m_detector->setSampleRate( capturedFormat.sampleRate() );
    qDebug() << "实际音频采集格式：" << capturedFormat.sampleRate() << "Hz,"
             << capturedFormat.channelCount() << "声道, sampleFormat="
             << capturedFormat.sampleFormat();
}

void MainWindow::setupUI() {

    // Claude-style layout: a narrow left sidebar list picks the page shown
    // in a QStackedWidget on the right, instead of a top tab bar.
    QWidget *shell = new QWidget( this );
    QHBoxLayout *shellLayout = new QHBoxLayout( shell );
    shellLayout -> setContentsMargins( 0, 0, 0, 0 );
    shellLayout -> setSpacing( 0 );
    setCentralWidget( shell );

    m_sidebar = new QListWidget( shell );
    m_sidebar -> setFixedWidth( 160 );
    m_sidebar -> setFrameShape( QFrame::NoFrame );
    m_sidebar -> setStyleSheet(
        "QListWidget { background-color: #1b1f27; border: 0; outline: 0; padding-top: 8px; }"
        "QListWidget::item { color: #c7cad1; padding: 10px 14px; }"
        "QListWidget::item:selected { background-color: #2c3440; color: #FFC107; }" );
    m_sidebar -> addItem( "实时音高" );
    m_sidebar -> addItem( "绝对音高训练" );
    m_sidebar -> addItem( "音频文件分析" );
    shellLayout -> addWidget( m_sidebar );

    m_stack = new QStackedWidget( shell );
    shellLayout -> addWidget( m_stack , 1 );

    connect( m_sidebar, &QListWidget::currentRowChanged, this, &MainWindow::onSidebarRowChanged );

    //central
    m_realtimeTab = new QWidget( m_stack );

    QVBoxLayout *mainLayout = new QVBoxLayout( m_realtimeTab );

    m_realtimeTab -> setStyleSheet( "background-color: #14181f;" );

    //chart start
    m_chart = new QChart();
    m_chart -> setTheme( QChart::ChartThemeDark );
    m_chart -> setTitle("实时音高");
    m_chart -> legend() -> hide();
    m_chart -> setAnimationOptions( QChart::NoAnimation );
    m_chart -> setBackgroundRoundness( 8 );

    //series
    m_series = new QLineSeries();
    QPen pen( QColor("#FFC107") ); // amber, matches the moving tip label
    pen.setWidth( 2 );
    m_series -> setPen( pen );
    m_chart -> addSeries( m_series );

    // Y: rolling ~12-semitone window, recentred on the singer's current pitch.
    // Every semitone advances by the same amount (it's a plain linear pitch
    // axis) but only the natural notes (white keys) get a text label, so
    // consecutive labels look 1 or 2 semitones apart depending on whether a
    // sharp/flat sits between them -- same convention as the reference image.
    m_axisY = new QCategoryAxis();
    m_axisY -> setLabelsPosition( QCategoryAxis::AxisLabelsPositionOnValue );
    m_chart -> addAxis( m_axisY , Qt::AlignLeft );
    m_series -> attachAxis( m_axisY );
    updatePitchAxis( m_smoothedMidi ); // build the initial C4-centred window

    // X: time nearest 10s
    m_axisX = new QValueAxis();
    m_axisX -> setTitleText("时间 (s)");
    m_axisX -> setRange( 0, 10 );
    m_axisX -> setLabelFormat( "%.0f" );
    m_chart -> addAxis( m_axisX, Qt::AlignBottom );
    m_series -> attachAxis( m_axisX );

    // chart end
    m_chartView = new QChartView( m_chart );
    m_chartView -> setRenderHint( QPainter::Antialiasing );
    mainLayout -> addWidget( m_chartView , 1 );

    // floating note-name tag that rides along the leading edge of the curve
    m_pitchTipLabel = new QLabel( m_chartView );
    m_pitchTipLabel -> setStyleSheet(
        "background-color: #FFC107; color: #14181f;"
        "border-radius: 4px; padding: 2px 6px; font-weight: bold;" );
    m_pitchTipLabel -> hide();

    // infoLayout
    QHBoxLayout *infoLayout = new QHBoxLayout();
    m_labelNote = new QLabel("音名: —");
    m_labelFreq = new QLabel("频率: 0.00 Hz");
    m_labelDeviation = new QLabel("偏差: 0.0 音分");
    for( QLabel *label : { m_labelNote, m_labelFreq, m_labelDeviation } )
        label -> setStyleSheet( "color: #e6e6e6; font-size: 13px;" );
    infoLayout -> addWidget( m_labelNote );
    infoLayout -> addWidget( m_labelFreq );
    infoLayout -> addWidget( m_labelDeviation );
    infoLayout -> addStretch();

    mainLayout -> addLayout( infoLayout );

    // pianoLayout
    m_piano = new PianoKeyboard( m_realtimeTab );
    m_piano -> setMinimumHeight( 120 );
    mainLayout -> addWidget( m_piano, 0 );// fixed

    m_stack -> addWidget( m_realtimeTab );

    // 绝对音高训练 lives as a peer page, not a separate pop-up window
    m_trainerWindow = new PitchTrainerWindow( m_stack );
    m_stack -> addWidget( m_trainerWindow );

    m_fileAnalysisWindow = new AudioFileAnalysisWindow( m_stack );
    m_stack -> addWidget( m_fileAnalysisWindow );

    m_sidebar -> setCurrentRow( 0 );

}

void MainWindow::onPitchDetected( const PitchResult &result ) {

    updatePitchDisplay( result );

    if( !result.isDetected ) {
        m_pitchTipLabel -> hide();
        return;
    }

    qreal midi = frequencyToMidi( result.frequencyHz );

    // smooth the axis-following target so the note "lanes" glide rather than
    // snap/jitter every time the pitch wobbles near a semitone boundary
    m_smoothedMidi = m_smoothedMidi * 0.7 + midi * 0.3;
    updatePitchAxis( m_smoothedMidi );

    //calc current time (s)
    qint64 currentMs = QDateTime::currentDateTime().toMSecsSinceEpoch();
    qreal seconds = (currentMs - m_startTime) / 1000.0;

    m_series -> append( seconds , midi );

    if( seconds > 10 ) {
        m_axisX -> setRange( seconds - 10 , seconds );
    }

    //data restriction : 1000
    if( m_series -> count() > 1000 ) {
        m_series -> removePoints( 0 , m_series -> count() - 500 );
    }

    // park the floating note-name tag right on the newest (right-most) point
    QPointF tipPos = m_chart -> mapToPosition( QPointF( seconds, midi ), m_series );
    m_pitchTipLabel -> setText( result.noteName );
    m_pitchTipLabel -> adjustSize();
    m_pitchTipLabel -> move( qRound( tipPos.x() ) - m_pitchTipLabel->width() - 6,
                              qRound( tipPos.y() ) - m_pitchTipLabel->height() / 2 );
    m_pitchTipLabel -> show();
    m_pitchTipLabel -> raise();

}

void MainWindow::updatePitchAxis( double centerMidi ) {

    int newCenter = qRound( centerMidi );
    if( newCenter == m_axisCenterMidi ) return; // already showing this window
    m_axisCenterMidi = newCenter;

    const QStringList oldLabels = m_axisY -> categoriesLabels();
    for( const QString &label : oldLabels )
        m_axisY -> remove( label );

    // a note is "natural" (a white key, no sharp/flat) at these semitones
    static const bool isNatural[12] = {
        true, false, true, false, true, true, false, true, false, true, false, true
    };
    auto natural = []( int midi ) {
        int n = midi % 12;
        if( n < 0 ) n += 12;
        return isNatural[n];
    };

    int low = newCenter - 6;
    int high = low + kVisibleSemitones - 1; // ~12 semitones total

    // snap both edges out to the nearest natural note so every labelled
    // lane's boundary lines up with a real gridline instead of cutting one
    // off mid-semitone
    while( !natural(low) ) --low;
    while( !natural(high) ) ++high;

    m_axisY -> setStartValue( low - 1 );
    for( int midi = low ; midi <= high ; ++midi ) {
        if( natural(midi) )
            m_axisY -> append( PitchDetector::noteNameForMidi( midi ) , midi );
    }

    m_axisY -> setRange( low - 1 , high );
}

void MainWindow::updatePitchDisplay( const PitchResult &result ) {

    if ( result.isDetected ) {
        m_labelNote -> setText( QString("音名: %1").arg(result.noteName));
        m_labelFreq -> setText( QString("频率: %1 Hz").arg(result.frequencyHz, 6, 'f', 2));
        m_labelDeviation -> setText( QString("偏差: %1 音分").arg(result.centDeviation, 5, 'f', 1));
    }
    else {
        m_labelNote -> setText("音名: —");
        m_labelFreq -> setText("频率: —");
        m_labelDeviation -> setText("偏差: —");
    }

}

void MainWindow::clearPianoHighlight() {

    m_piano->clearHighlight();
}

qreal MainWindow::frequencyToMidi( double freq ) const {
    if(freq <= 0) return 0;
    return 69.0 + 12.0 * qLn(freq / 440.0) / qLn(2.0);
}