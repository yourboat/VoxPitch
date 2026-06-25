#include "pannablechartview.h"
#include<QMouseEvent>

PannableChartView::PannableChartView( QChart *chart, QWidget *parent )
    : QChartView( chart, parent ) {
    setDragMode( QGraphicsView::NoDrag ); // we drive panning ourselves below
}

void PannableChartView::mousePressEvent( QMouseEvent *event ) {
    if( event->button() == Qt::LeftButton ) {
        m_panning = true;
        m_lastPos = event->pos();
        setCursor( Qt::ClosedHandCursor );
        event->accept();
        return;
    }
    QChartView::mousePressEvent( event );
}

void PannableChartView::mouseMoveEvent( QMouseEvent *event ) {
    if( m_panning ) {
        QPoint delta = event->pos() - m_lastPos;
        m_lastPos = event->pos();
        // drag-to-follow: content moves with the cursor, so the scroll
        // direction is the inverse of the horizontal mouse delta
        chart() -> scroll( -delta.x(), m_verticalPanEnabled ? delta.y() : 0 );
        event->accept();
        return;
    }
    QChartView::mouseMoveEvent( event );
}

void PannableChartView::mouseReleaseEvent( QMouseEvent *event ) {
    if( m_panning ) {
        m_panning = false;
        setCursor( Qt::ArrowCursor );
        event->accept();
        return;
    }
    QChartView::mouseReleaseEvent( event );
}
