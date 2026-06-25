#include "pitchtrainerwindow.h"
#include "pitchdetector.h"
#include<QVBoxLayout>
#include<QHBoxLayout>
#include<QRandomGenerator>
#include<QVariant>
#include<algorithm>

PitchTrainerWindow::PitchTrainerWindow( QWidget *parent )
    : QWidget( parent ) {

    setStyleSheet( "background-color: #14181f;" );

    m_player = new AudioPlayer( this );
    m_player -> start();

    QVBoxLayout *layout = new QVBoxLayout( this );

    m_modeCombo = new QComboBox( this );
    m_modeCombo -> addItem( "单音" , QVariant::fromValue( int(Mode::SingleNote) ) );
    m_modeCombo -> addItem( "双音" , QVariant::fromValue( int(Mode::TwoNotes) ) );
    m_modeCombo -> addItem( "三音和弦" , QVariant::fromValue( int(Mode::Triad) ) );
    m_modeCombo -> addItem( "四音和弦" , QVariant::fromValue( int(Mode::SeventhChord) ) );
    m_modeCombo -> setStyleSheet( "color: #e6e6e6; background-color: #2c3440; padding: 3px 8px;" );
    layout -> addWidget( m_modeCombo , 0 , Qt::AlignHCenter );

    m_feedbackLabel = new QLabel( this );
    m_feedbackLabel -> setAlignment( Qt::AlignCenter );
    m_feedbackLabel -> setStyleSheet( "color: #e6e6e6; font-size: 16px;" );
    layout -> addWidget( m_feedbackLabel );

    m_scoreLabel = new QLabel( this );
    m_scoreLabel -> setAlignment( Qt::AlignCenter );
    m_scoreLabel -> setStyleSheet( "color: #9aa0a6; font-size: 13px;" );
    layout -> addWidget( m_scoreLabel );

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_replayButton = new QPushButton( "重新播放", this );
    m_hintButton = new QPushButton( "提示", this );
    m_submitButton = new QPushButton( "提交答案", this );
    m_nextButton = new QPushButton( "下一题", this );
    m_nextButton -> setEnabled( false );
    m_hintButton -> setVisible( false );
    m_submitButton -> setVisible( false );
    buttonLayout -> addStretch();
    buttonLayout -> addWidget( m_replayButton );
    buttonLayout -> addWidget( m_hintButton );
    buttonLayout -> addWidget( m_submitButton );
    buttonLayout -> addWidget( m_nextButton );
    buttonLayout -> addStretch();
    layout -> addLayout( buttonLayout );

    m_piano = new PianoKeyboard( this );
    m_piano -> setMinimumHeight( 160 );
    layout -> addWidget( m_piano , 1 );

    connect( m_piano, &PianoKeyboard::noteOn, this, &PitchTrainerWindow::onKeyToggled );
    connect( m_replayButton, &QPushButton::clicked, this, &PitchTrainerWindow::replayTone );
    connect( m_hintButton, &QPushButton::clicked, this, &PitchTrainerWindow::giveHint );
    connect( m_submitButton, &QPushButton::clicked, this, &PitchTrainerWindow::submitChordGuess );
    connect( m_nextButton, &QPushButton::clicked, this, &PitchTrainerWindow::startNewRound );
    connect( m_modeCombo, &QComboBox::currentIndexChanged, this, &PitchTrainerWindow::onModeChanged );
}

void PitchTrainerWindow::ensureStarted() {
    if( m_started ) return;
    m_started = true;
    startNewRound();
}

int PitchTrainerWindow::pickWeightedRandomMidi() const {

    const int kLow = 21, kHigh = 108;       // full 88 keys
    const int kCommonLow = 48, kCommonHigh = 83; // C3~B5: weighted higher

    int totalWeight = 0;
    for( int midi = kLow ; midi <= kHigh ; ++midi )
        totalWeight += ( midi >= kCommonLow && midi <= kCommonHigh ) ? 4 : 1;

    int r = QRandomGenerator::global() -> bounded( totalWeight );
    int cumulative = 0;
    for( int midi = kLow ; midi <= kHigh ; ++midi ) {
        cumulative += ( midi >= kCommonLow && midi <= kCommonHigh ) ? 4 : 1;
        if( r < cumulative ) return midi;
    }
    return 60; // unreachable fallback
}

QString PitchTrainerWindow::stripOctave( const QString &noteName ) const {
    QString result = noteName;
    while( !result.isEmpty() && result.back().isDigit() )
        result.chop(1);
    return result;
}

QString PitchTrainerWindow::namesForNotes( const QVector<int> &notes ) const {
    QStringList names;
    for( int midi : notes ) names << PitchDetector::noteNameForMidi( midi );
    return names.join( ", " );
}

void PitchTrainerWindow::onModeChanged( int index ) {
    m_mode = static_cast<Mode>( m_modeCombo -> itemData(index).toInt() );
    startNewRound();
}

void PitchTrainerWindow::startNewRound() {

    m_piano -> clearSelection();
    m_piano -> setMultiSelectMode( isMultiSelectMode() );
    m_submitButton -> setVisible( isMultiSelectMode() );
    m_hintButton -> setVisible( isMultiSelectMode() );
    m_hintButton -> setEnabled( true );

    int root = pickWeightedRandomMidi();
    m_targetNotes.clear();
    m_targetLabel.clear();

    switch( m_mode ) {

    case Mode::SingleNote:
        m_targetNotes = { root };
        break;

    case Mode::TwoNotes: {
        static const QString intervalNames[] = {
            "", "小二度", "大二度", "小三度", "大三度", "纯四度", "三全音",
            "纯五度", "小六度", "大六度", "小七度", "大七度", "纯八度"
        };
        int interval = QRandomGenerator::global() -> bounded( 1, 13 ); // 1~12 semitones
        int second = root + interval;
        if( second > 108 ) second = root - interval;
        m_targetNotes = { root, second };
        m_targetLabel = intervalNames[interval];
        break;
    }

    case Mode::Triad:
    case Mode::SeventhChord: {
        static const QVector<ChordTemplate> triads = {
            { "大三和弦", {0,4,7} }, { "小三和弦", {0,3,7} },
            { "减三和弦", {0,3,6} }, { "增三和弦", {0,4,8} },
        };
        static const QVector<ChordTemplate> sevenths = {
            { "大七和弦", {0,4,7,11} }, { "属七和弦", {0,4,7,10} },
            { "小七和弦", {0,3,7,10} }, { "半减七和弦", {0,3,6,10} },
            { "减七和弦", {0,3,6,9} },
        };
        const QVector<ChordTemplate> &templates = ( m_mode == Mode::Triad ) ? triads : sevenths;
        ChordTemplate tmpl = templates[ QRandomGenerator::global() -> bounded( templates.size() ) ];

        for( int attempt = 0 ; attempt < 30 && root + tmpl.intervals.last() > 108 ; ++attempt )
            root = pickWeightedRandomMidi();

        for( int iv : tmpl.intervals )
            m_targetNotes.append( root + iv );
        m_targetLabel = stripOctave( PitchDetector::noteNameForMidi(root) ) + tmpl.name;
        break;
    }
    }

    m_roundActive = true;
    m_totalCount++;

    m_feedbackLabel -> setText( isMultiSelectMode()
        ? "请听音，在钢琴上选出对应的音后点击提交"
        : "请听音，在钢琴上点出你听到的音" );
    m_feedbackLabel -> setStyleSheet( "color: #e6e6e6; font-size: 16px;" );
    m_nextButton -> setEnabled( false );
    updateScoreLabel();

    replayTone();
}

void PitchTrainerWindow::replayTone() {
    const int durationMs = 1000;
    if( m_targetNotes.size() == 1 )
        m_player -> playTone( m_targetNotes.first() , durationMs );
    else
        m_player -> playChord( m_targetNotes , durationMs );
}

void PitchTrainerWindow::onKeyToggled( int midiNote ) {

    if( !m_roundActive ) return;

    if( m_mode != Mode::SingleNote ) {
        // multi-note modes: play back everything selected so far (not just
        // the newest key) so the chord is heard building up as a whole
        QVector<int> selected( m_piano->selectedNotes().begin(), m_piano->selectedNotes().end() );
        std::sort( selected.begin(), selected.end() );
        if( selected.size() == 1 )
            m_player -> playTone( selected.first() , 1000 );
        else if( !selected.isEmpty() )
            m_player -> playChord( selected , 1000 );
        return; // judged on 提交, not per click
    }

    m_player -> playTone( midiNote , 1000 ); // let them hear exactly what they clicked

    if( midiNote == m_targetNotes.first() ) {
        m_correctCount++;
        m_roundActive = false;
        m_feedbackLabel -> setText( QString("✓ 正确！是 %1").arg( PitchDetector::noteNameForMidi(midiNote) ) );
        m_feedbackLabel -> setStyleSheet( "color: #4CAF50; font-size: 16px; font-weight: bold;" );
        m_nextButton -> setEnabled( true );
        updateScoreLabel();
    } else {
        QString clickedName = PitchDetector::noteNameForMidi( midiNote );
        QString direction = ( midiNote < m_targetNotes.first() ) ? "偏低" : "偏高";
        m_feedbackLabel -> setText( QString("✗ 你点的是 %1，比答案%2，再试一下").arg( clickedName, direction ) );
        m_feedbackLabel -> setStyleSheet( "color: #F44336; font-size: 16px; font-weight: bold;" );
    }
}

void PitchTrainerWindow::submitChordGuess() {

    if( !m_roundActive ) return;

    QSet<int> selected = m_piano -> selectedNotes();
    if( selected.isEmpty() ) {
        m_feedbackLabel -> setText( "请先在钢琴上选好音，再点提交" );
        m_feedbackLabel -> setStyleSheet( "color: #e6e6e6; font-size: 16px;" );
        return;
    }

    QVector<int> selectedSorted( selected.begin(), selected.end() );
    std::sort( selectedSorted.begin(), selectedSorted.end() );
    QString selectedNames = namesForNotes( selectedSorted );

    if( selectedSorted.size() == 1 )
        m_player -> playTone( selectedSorted.first() , 1000 );
    else
        m_player -> playChord( selectedSorted , 1000 );

    QSet<int> target( m_targetNotes.begin(), m_targetNotes.end() );
    if( selected == target ) {
        m_correctCount++;
        m_roundActive = false;
        m_feedbackLabel -> setText( QString("✓ 正确！你选的是 %1，这是 %2").arg( selectedNames, m_targetLabel ) );
        m_feedbackLabel -> setStyleSheet( "color: #4CAF50; font-size: 16px; font-weight: bold;" );
        m_nextButton -> setEnabled( true );
        updateScoreLabel();
    } else {
        m_feedbackLabel -> setText( QString("✗ 你选的是 %1，不对，再试一下").arg( selectedNames ) );
        m_feedbackLabel -> setStyleSheet( "color: #F44336; font-size: 16px; font-weight: bold;" );
    }
}

void PitchTrainerWindow::giveHint() {

    if( !m_roundActive || !isMultiSelectMode() ) return;

    QSet<int> selected = m_piano -> selectedNotes();

    int hintNote = -1;
    for( int midi : m_targetNotes ) {
        if( !selected.contains(midi) ) { hintNote = midi; break; }
    }
    if( hintNote == -1 ) return; // already fully revealed

    m_piano -> selectNote( hintNote );
    m_player -> playTone( hintNote , 1000 );
    selected.insert( hintNote );

    bool allRevealed = std::all_of( m_targetNotes.begin(), m_targetNotes.end(),
        [&]( int midi ) { return selected.contains(midi); } );

    if( allRevealed ) {
        m_roundActive = false;
        m_hintButton -> setEnabled( false );
        m_feedbackLabel -> setText( QString("已提示完整个答案：%1（%2）")
            .arg( namesForNotes(m_targetNotes), m_targetLabel ) );
        m_feedbackLabel -> setStyleSheet( "color: #FFC107; font-size: 16px; font-weight: bold;" );
        m_nextButton -> setEnabled( true );
    } else {
        int remaining = m_targetNotes.size() - selected.size();
        m_feedbackLabel -> setText( QString("提示：%1 是其中一个音，还差 %2 个")
            .arg( PitchDetector::noteNameForMidi(hintNote) ).arg( remaining ) );
        m_feedbackLabel -> setStyleSheet( "color: #FFC107; font-size: 16px;" );
    }
}

void PitchTrainerWindow::updateScoreLabel() {
    m_scoreLabel -> setText( QString("正确 %1 / 共 %2").arg( m_correctCount ).arg( m_totalCount ) );
}
