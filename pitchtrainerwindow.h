#ifndef PITCHTRAINERWINDOW_H
#define PITCHTRAINERWINDOW_H

#include<QWidget>
#include<QLabel>
#include<QPushButton>
#include<QComboBox>
#include "pianokeyboard.h"
#include "audioplayer.h"

// standalone "guess the absolute pitch" trainer: plays a random tone/interval
// /chord (root weighted toward octaves 3-5, the common vocal range) and lets
// the user answer by clicking the 88-key piano, with right/wrong feedback.
class PitchTrainerWindow : public QWidget {

    Q_OBJECT

public:

    explicit PitchTrainerWindow( QWidget *parent = nullptr );

    // lazily plays/generates the first round the first time this tab is
    // actually shown; harmless no-op on later calls
    void ensureStarted();

private slots:

    void onKeyToggled( int midiNote );
    void startNewRound();
    void replayTone();
    void submitChordGuess();
    void onModeChanged( int index );
    void giveHint();

private:

    enum class Mode { SingleNote, TwoNotes, Triad, SeventhChord };

    struct ChordTemplate { QString name; QVector<int> intervals; };

    int pickWeightedRandomMidi() const;
    void updateScoreLabel();
    QString stripOctave( const QString &noteName ) const;
    QString namesForNotes( const QVector<int> &notes ) const; // sorted, comma-joined note names
    bool isMultiSelectMode() const { return m_mode != Mode::SingleNote; }

    PianoKeyboard *m_piano = nullptr;
    AudioPlayer *m_player = nullptr;
    QComboBox *m_modeCombo = nullptr;
    QLabel *m_feedbackLabel = nullptr;
    QLabel *m_scoreLabel = nullptr;
    QPushButton *m_replayButton = nullptr;
    QPushButton *m_hintButton = nullptr;
    QPushButton *m_submitButton = nullptr;
    QPushButton *m_nextButton = nullptr;

    Mode m_mode = Mode::SingleNote;
    QVector<int> m_targetNotes;   // 1 note for SingleNote/TwoNotes-as-list, up to 4 for chords
    QString m_targetLabel;        // interval/chord name, shown only once answered correctly
    int m_correctCount = 0;
    int m_totalCount = 0;
    bool m_roundActive = true; // false once answered correctly, until "下一题"
    bool m_started = false;    // guards the lazy first-round start in ensureStarted()

};

#endif
