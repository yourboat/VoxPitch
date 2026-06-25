#include "skipbutton.h"
#include<QPainter>
#include<QtMath>

SkipButton::SkipButton( bool forward, int seconds, QWidget *parent )
    : QAbstractButton( parent ), m_forward( forward ), m_seconds( seconds ) {
    setCursor( Qt::PointingHandCursor );
}

QSize SkipButton::sizeHint() const {
    return QSize( 40, 40 );
}

void SkipButton::paintEvent( QPaintEvent * ) {

    QPainter painter( this );
    painter.setRenderHint( QPainter::Antialiasing );

    QColor color( "#e6e6e6" );
    if( !isEnabled() ) color = QColor( "#5a5f68" );
    else if( isDown() ) color = QColor( "#FFC107" );
    else if( underMouse() ) color = QColor( "#ffe082" );

    double margin = 5.0;
    QRectF rect( margin, margin, width() - 2*margin, height() - 2*margin );
    double r = rect.width() / 2.0;
    QPointF center = rect.center();

    QPen pen( color );
    pen.setWidthF( 1.8 );
    painter.setPen( pen );
    painter.setBrush( Qt::NoBrush );

    // ~300 degree ring, leaving a gap near the top for the arrowhead.
    // QPainter::drawArc angles: 0 = 3 o'clock, positive = counter-clockwise,
    // units of 1/16th of a degree.
    if( m_forward )
        painter.drawArc( rect, -60 * 16, -300 * 16 );          // gap at 1-2 o'clock, sweeps clockwise
    else
        painter.drawArc( rect, (180 + 60) * 16, 300 * 16 );    // gap at 10-11 o'clock, sweeps counter-clockwise

    // arrowhead sitting at the open end, pointing along the ring
    double tipAngleDeg = m_forward ? -60.0 : (180.0 + 60.0);
    double tipRad = qDegreesToRadians( tipAngleDeg );
    QPointF tip( center.x() + r * qCos(tipRad), center.y() - r * qSin(tipRad) );

    double arrowRotationDeg = m_forward ? -30.0 : (180.0 + 30.0);
    QPolygonF arrow;
    arrow << QPointF(5, 0) << QPointF(-3, -4) << QPointF(-3, 4);
    QTransform t;
    t.translate( tip.x(), tip.y() );
    t.rotate( arrowRotationDeg );
    arrow = t.map( arrow );

    painter.setBrush( color );
    painter.setPen( Qt::NoPen );
    painter.drawPolygon( arrow );

    painter.setPen( color );
    QFont f = painter.font();
    f.setPointSizeF( qMax(8.0, height() * 0.30) );
    f.setBold( true );
    painter.setFont( f );
    painter.drawText( rect, Qt::AlignCenter, QString::number(m_seconds) );
}
