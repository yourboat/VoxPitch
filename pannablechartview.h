#ifndef PANNABLECHARTVIEW_H
#define PANNABLECHARTVIEW_H

#include<QtCharts/QChartView>

// QChartView with no built-in way to drag the visible plot area around;
// this adds plain click-and-drag panning (QChart::scroll), independent of
// any rubber-band zoom selection.
class PannableChartView : public QChartView {

    Q_OBJECT

public:

    explicit PannableChartView( QChart *chart, QWidget *parent = nullptr );

    // some pages only want left/right scrubbing (e.g. when the Y window is
    // managed automatically and a manual vertical drag would just get
    // overridden); on by default to keep free panning elsewhere.
    void setVerticalPanEnabled( bool enabled ) { m_verticalPanEnabled = enabled; }

protected:

    void mousePressEvent( QMouseEvent *event ) override;
    void mouseMoveEvent( QMouseEvent *event ) override;
    void mouseReleaseEvent( QMouseEvent *event ) override;

private:

    bool m_panning = false;
    bool m_verticalPanEnabled = true;
    QPoint m_lastPos;

};

#endif
