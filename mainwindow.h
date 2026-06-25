#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include<QtCharts/QChart>
#include<QtCharts/QChartView>
#include<QtCharts/QLineSeries>
#include<QtCharts/QValueAxis>
#include<QtCharts/QCategoryAxis>

#include<QMainWindow>
#include<QLabel>
#include<QTimer>
#include<QListWidget>
#include<QStackedWidget>
#include<QSet>
#include<climits>
#include "audiocapturer.h"
#include "pitchdetector.h"
#include "pianokeyboard.h"
#include "audioplayer.h"
#include "pitchtrainerwindow.h"
#include "audiofileanalysiswindow.h"

class MainWindow : public QMainWindow {

    Q_OBJECT

public:

    explicit MainWindow( QWidget *parent = nullptr );
    ~MainWindow();

    public slots:

    void onPitchDetected( const PitchResult &result );

    private slots:

    void clearPianoHighlight();
    void onSidebarRowChanged( int row );

protected:

    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;

private:
    
    void setupUI();
    void setupAudio();
    void updatePitchDisplay( const PitchResult &result );
    // rebuilds the 12-semitone-tall Y axis (note name "lanes") around centerMidi,
    // only touching the axis when the rounded center actually moves
    void updatePitchAxis( double centerMidi );

    QListWidget *m_sidebar = nullptr;
    QStackedWidget *m_stack = nullptr;
    QWidget *m_realtimeTab = nullptr; // which page owns the real-time piano/keyboard shortcuts

    QTimer *m_highlightTimer = nullptr;
    QChartView *m_chartView = nullptr;
    QChart *m_chart = nullptr;
    QLineSeries *m_series = nullptr;
    QCategoryAxis *m_axisY = nullptr;
    QValueAxis *m_axisX = nullptr;

    static constexpr int kVisibleSemitones = 12;
    int m_axisCenterMidi = INT_MIN; // sentinel: axis not built yet
    double m_smoothedMidi = 60.0;   // exponentially-smoothed pitch the axis follows

    QLabel *m_pitchTipLabel = nullptr; // floating note-name tag at the line's leading edge

    QLabel *m_labelNote = nullptr;
    QLabel *m_labelFreq = nullptr;
    QLabel *m_labelDeviation = nullptr;

    AudioCapturer *m_capturer = nullptr;
    PitchDetector *m_detector = nullptr;
    AudioPlayer *m_player = nullptr;
    PianoKeyboard *m_piano = nullptr;
    PitchTrainerWindow *m_trainerWindow = nullptr;
    AudioFileAnalysisWindow *m_fileAnalysisWindow = nullptr;

    qint64 m_startTime = 0;
    bool m_userKeyPressed = false;
    QSet<int> m_pressedKeys; // every key currently held, for multi-key playing
    qreal frequencyToMidi( double freq ) const;

};

#endif