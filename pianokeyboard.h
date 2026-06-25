#ifndef PIANOKEYBOARD_H
#define PIANOKEYBOARD_H

#include<QWidget>
#include<QSet>
#include<QMap>

class PianoKeyboard : public QWidget {

    Q_OBJECT

public:

    explicit PianoKeyboard( QWidget *parent = nullptr );

    // Midi to keyboard
    void highlightNote( int midiNote );
    void clearHighlight();

    // multi-select mode: clicking a key toggles it on/off (for picking out
    // a chord) instead of the normal momentary press/release behaviour
    void setMultiSelectMode( bool enabled );
    QSet<int> selectedNotes() const { return m_selectedNotes; }
    void clearSelection();
    // forcibly add (not toggle) a note to the selection, used for hints
    void selectNote( int midiNote );

    void keyPressEvent( QKeyEvent *event ) override;
    void keyReleaseEvent( QKeyEvent *event ) override;

    signals:
    void noteOn(int midiNote);
    void noteOff(int midiNote);

protected:

    void paintEvent( QPaintEvent *event ) override;
    void mousePressEvent( QMouseEvent *event ) override;
    void mouseReleaseEvent( QMouseEvent *event ) override;

private:
    
    // calc geomiteric info
    void computeKeyRects();
    // mouse to MIDI ; -1 -> no
    int noteAtPos( const QPoint &pos ) const;
    // key to MIDI
    int keyToMidiNote( int qtKey ) const;

    // MIDI -> keyboard (21~108)
    QMap<int, QRectF> m_whiteKeyRects;
    QMap<int, QRectF> m_blackKeyRects;
    
    // current highlighted and pressed
    int m_highlightedNote = -1;
    QSet<int> m_pressedKeys;
    bool m_multiSelectMode = false;
    QSet<int> m_selectedNotes;

    // qtKey -> midiNote
    QMap<int, int> m_keyboardMap; 
};

#endif