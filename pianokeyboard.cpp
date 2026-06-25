#include "pianokeyboard.h"
#include<QPainter>
#include<QMouseEvent>
#include<QKeyEvent>
#include<QtMath>
#include<QDebug>

PianoKeyboard::PianoKeyboard(QWidget *parent)
    : QWidget( parent ) {

    setMinimumHeight( 120 );
    setFocusPolicy( Qt::StrongFocus ); // allow qtkey event

    // qtkey to MIDI
    // C3 -> G5
    m_keyboardMap = {
        {Qt::Key_Z, 48}, {Qt::Key_S, 49}, {Qt::Key_X, 50}, {Qt::Key_D, 51},
        {Qt::Key_C, 52}, {Qt::Key_V, 53}, {Qt::Key_G, 54}, {Qt::Key_B, 55},
        {Qt::Key_H, 56}, {Qt::Key_N, 57}, {Qt::Key_J, 58}, {Qt::Key_M, 59},
        {Qt::Key_Q, 60}, {Qt::Key_2, 61}, {Qt::Key_W, 62}, {Qt::Key_3, 63},
        {Qt::Key_E, 64}, {Qt::Key_R, 65}, {Qt::Key_5, 66}, {Qt::Key_T, 67},
        {Qt::Key_6, 68}, {Qt::Key_Y, 69}, {Qt::Key_7, 70}, {Qt::Key_U, 71},
        {Qt::Key_I, 72}, {Qt::Key_9, 73}, {Qt::Key_O, 74}, {Qt::Key_0, 75},
        {Qt::Key_P, 76}, {Qt::Key_BracketLeft, 77}, {Qt::Key_Equal, 78}, {Qt::Key_BracketRight, 79}
    };
}

void PianoKeyboard::computeKeyRects() {

    m_whiteKeyRects.clear();
    m_blackKeyRects.clear();
    if( width() <= 0 ) return;

    // white -> 52  (MIDI 21~108)
    // tot -> 88
    int startMidi = 21;
    int endMidi = 108;
    int whiteCount = 0;
    for( int midi = startMidi ; midi <= endMidi ; ++midi ) {
        int noteInOctave = midi % 12;
        // white: 0,2,4,5,7,9,11
        if( noteInOctave != 1 && noteInOctave != 3 && noteInOctave != 6 && noteInOctave != 8 && noteInOctave != 10 )
            whiteCount++;
    }

    double whiteKeyWidth = static_cast<double>(width()) / whiteCount;
    double blackKeyWidth = whiteKeyWidth * 0.6;
    double blackKeyHeight = height() * 0.6;
    double whiteKeyHeight = height();

    int whiteIndex = 0;
    for( int midi = startMidi ; midi <= endMidi ; ++midi ) {
        int note = midi % 12;
        bool isBlack = (note == 1 || note == 3 || note == 6 || note == 8 || note == 10);
        if(isBlack) {
            // pre white right - half black
            double left = whiteIndex * whiteKeyWidth - blackKeyWidth / 2.0;
            QRectF rect( left , 0 , blackKeyWidth , blackKeyHeight );
            m_blackKeyRects.insert( midi , rect );
        } else {
            double left = whiteIndex * whiteKeyWidth ;
            QRectF rect( left , 0 , whiteKeyWidth , whiteKeyHeight );
            m_whiteKeyRects.insert( midi , rect );
            whiteIndex++;
        }
    }
}

void PianoKeyboard::paintEvent( QPaintEvent * ) {

    QPainter painter(this);
    painter.setRenderHint( QPainter::Antialiasing, false );

    computeKeyRects();

    //white
    for( auto it = m_whiteKeyRects.begin() ; it != m_whiteKeyRects.end() ; ++it ) {
        int midi = it.key();
        QRectF rect = it.value();
        QColor color = Qt::white;
        if( midi == m_highlightedNote ) color = QColor(255, 200, 100);
        else if( m_selectedNotes.contains(midi) ) color = QColor(100, 180, 255);
        painter.fillRect( rect , color );
        painter.setPen( Qt::black );
        painter.drawRect( rect );
    }

    //black
    for( auto it = m_blackKeyRects.begin() ; it != m_blackKeyRects.end() ; ++it ) {
        int midi = it.key();
        QRectF rect = it.value();
        QColor color = QColor(40, 40, 40);
        if( midi == m_highlightedNote ) color = QColor(255, 150, 50);
        else if( m_selectedNotes.contains(midi) ) color = QColor(30, 110, 200);
        painter.fillRect( rect , color );
        painter.setPen( Qt::black );
        painter.drawRect( rect );
    }
}

int PianoKeyboard::noteAtPos( const QPoint &pos ) const {
    
    //black
    for( auto it = m_blackKeyRects.begin() ; it != m_blackKeyRects.end() ; ++it)
        if( it.value().contains(pos) )
            return it.key();
    
    //white
    for( auto it = m_whiteKeyRects.begin() ; it != m_whiteKeyRects.end() ; ++it)
        if( it.value().contains(pos) )
            return it.key();

    return -1;
}

int PianoKeyboard::keyToMidiNote( int qtKey ) const {
    return m_keyboardMap.value( qtKey , -1 );
}

void PianoKeyboard::mousePressEvent( QMouseEvent *event ) {
    int midi = noteAtPos( event->pos() );
    if( midi == -1 ) return;

    if( m_multiSelectMode ) {
        if( m_selectedNotes.contains(midi) ) {
            m_selectedNotes.remove( midi );
            emit noteOff( midi );
        } else {
            m_selectedNotes.insert( midi );
            emit noteOn( midi );
        }
        update();
        return;
    }

    m_pressedKeys.insert( midi );
    m_highlightedNote = midi;
    emit noteOn( midi );
    update();
}

void PianoKeyboard::mouseReleaseEvent( QMouseEvent *event ) {
    if( m_multiSelectMode ) return; // toggling is handled entirely on press

    int midi = noteAtPos( event->pos() );
    if( midi != -1 && m_pressedKeys.contains( midi ) ) {
        m_pressedKeys.remove( midi );
        emit noteOff( midi );
        if( m_highlightedNote == midi )
            m_highlightedNote = -1;
        update();
    }
}

void PianoKeyboard::setMultiSelectMode( bool enabled ) {
    m_multiSelectMode = enabled;
    clearSelection();
}

void PianoKeyboard::clearSelection() {
    m_selectedNotes.clear();
    update();
}

void PianoKeyboard::selectNote( int midiNote ) {
    if( midiNote < 21 || midiNote > 108 ) return;
    m_selectedNotes.insert( midiNote );
    update();
}

void PianoKeyboard::keyPressEvent( QKeyEvent *event ) {

    // qDebug() << "Key press:" << event->key();
    int midi = keyToMidiNote( event->key() );
    if( midi != -1 && !event->isAutoRepeat() ) {
        m_pressedKeys.insert(midi);
        m_highlightedNote = midi;
        emit noteOn(midi);
        update();
    } 
    else {
        QWidget::keyPressEvent( event );
    }
}

void PianoKeyboard::keyReleaseEvent( QKeyEvent *event ) {
    int midi = keyToMidiNote( event->key() );
    if ( midi != -1 && !event->isAutoRepeat() ) {
        m_pressedKeys.remove(midi);
        emit noteOff(midi);
        if( m_highlightedNote == midi )
            m_highlightedNote = -1;
        update();
    } 
    else {
        QWidget::keyReleaseEvent(event);
    }
}

void PianoKeyboard::highlightNote( int midiNote ) {
    if( midiNote >= 21 && midiNote <= 108 ) {
        m_highlightedNote = midiNote;
        update();
    }
}

void PianoKeyboard::clearHighlight() {
    m_highlightedNote = -1;
    update();
}