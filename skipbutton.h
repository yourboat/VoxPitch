#ifndef SKIPBUTTON_H
#define SKIPBUTTON_H

#include<QAbstractButton>

// custom-painted "skip back/forward N seconds" control: a circular arrow
// with the second count in the middle, like a typical media player's
// skip-15/skip-10 button.
class SkipButton : public QAbstractButton {

    Q_OBJECT

public:

    explicit SkipButton( bool forward, int seconds, QWidget *parent = nullptr );

protected:

    void paintEvent( QPaintEvent *event ) override;
    QSize sizeHint() const override;

private:

    bool m_forward;
    int m_seconds;

};

#endif
