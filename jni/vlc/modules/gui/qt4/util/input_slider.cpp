/*****************************************************************************
 * input_slider.cpp : VolumeSlider and SeekSlider
 ****************************************************************************
 * Copyright (C) 2006-2011 the VideoLAN team
 * $Id: d990d9aab5e24ae9c084161450edeb2c259bccb1 $
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "qt4.hpp"

#include "util/input_slider.hpp"

#include <QPaintEvent>
#include <QPainter>
#include <QBitmap>
#include <QPainter>
#include <QStyleOptionSlider>
#include <QLinearGradient>
#include <QTimer>
#include <QRadialGradient>

#define MINIMUM 0
#define MAXIMUM 1000

SeekSlider::SeekSlider( QWidget *_parent ) : QSlider( _parent )
{
    SeekSlider( Qt::Horizontal, _parent );
}

SeekSlider::SeekSlider( Qt::Orientation q, QWidget *_parent )
          : QSlider( q, _parent )
{
    b_isSliding = false;

    /* Timer used to fire intermediate updatePos() when sliding */
    seekLimitTimer = new QTimer( this );
    seekLimitTimer->setSingleShot( true );

    /* Tooltip bubble */
    mTimeTooltip = new TimeTooltip( this );
    mTimeTooltip->setMouseTracking( true );

    /* Properties */
    setRange( MINIMUM, MAXIMUM );
    setSingleStep( 2 );
    setPageStep( 10 );
    setMouseTracking( true );
    setTracking( true );
    setFocusPolicy( Qt::NoFocus );

    /* Init to 0 */
    setPosition( -1.0, 0, 0 );
    secstotimestr( psz_length, 0 );

    CONNECT( this, sliderMoved( int ), this, startSeekTimer( int ) );
    CONNECT( seekLimitTimer, timeout(), this, updatePos() );

    mTimeTooltip->installEventFilter( this );
}

/***
 * \brief Main public method, superseeding setValue. Disabling the slider when neeeded
 *
 * \param pos Position, between 0 and 1. -1 disables the slider
 * \param time Elapsed time. Unused
 * \param legnth Duration time.
 ***/
void SeekSlider::setPosition( float pos, int64_t time, int length )
{
    if( pos == -1.0 )
    {
        setEnabled( false );
        b_isSliding = false;
    }
    else
        setEnabled( true );

    if( !b_isSliding )
        setValue( (int)( pos * 1000.0 ) );

    inputLength = length;
}

void SeekSlider::startSeekTimer( int new_value )
{
    /* Only fire one update, when sliding, every 150ms */
    if( b_isSliding && !seekLimitTimer->isActive() )
        seekLimitTimer->start( 150 );
}

void SeekSlider::updatePos()
{
    float f_pos = (float)( value() ) / 1000.0;
    emit sliderDragged( f_pos ); /* Send new position to VLC's core */
}

void SeekSlider::mouseReleaseEvent( QMouseEvent *event )
{
    event->accept();
    b_isSliding = false;
    seekLimitTimer->stop(); /* We're not sliding anymore: only last seek on release */
    QSlider::mouseReleaseEvent( event );
    updatePos();
}

void SeekSlider::mousePressEvent( QMouseEvent* event )
{
    /* Right-click */
    if( event->button() != Qt::LeftButton &&
        event->button() != Qt::MidButton )
    {
        QSlider::mousePressEvent( event );
        return;
    }

    b_isSliding = true ;
    setValue( QStyle::sliderValueFromPosition( MINIMUM, MAXIMUM, event->x(), width(), false ) );
    event->accept();
}

void SeekSlider::mouseMoveEvent( QMouseEvent *event )
{
    if( b_isSliding )
    {
        setValue( QStyle::sliderValueFromPosition( MINIMUM, MAXIMUM, event->x(), width(), false) );
        emit sliderMoved( value() );
    }

    /* Tooltip */
    if ( inputLength > 0 )
    {
        int posX = qMax( rect().left(), qMin( rect().right(), event->x() ) );

        QPoint p( event->globalX() - ( event->x() - posX ) - ( mTimeTooltip->width() / 2 ),
                  QWidget::mapToGlobal( pos() ).y() - ( mTimeTooltip->height() + 2 ) );


        secstotimestr( psz_length, ( posX * inputLength ) / size().width() );
        mTimeTooltip->setTime( psz_length );
        mTimeTooltip->move( p );
    }
    event->accept();
}

void SeekSlider::wheelEvent( QWheelEvent *event )
{
    /* Don't do anything if we are for somehow reason sliding */
    if( !b_isSliding )
    {
        setValue( value() + event->delta() / 12 ); /* 12 = 8 * 15 / 10
         Since delta is in 1/8 of ° and mouse have steps of 15 °
         and that our slider is in 0.1% and we want one step to be a 1%
         increment of position */
        emit sliderDragged( value() / 1000.0 );
    }
    event->accept();
}

void SeekSlider::enterEvent( QEvent *e )
{
    /* Don't show the tooltip if the slider is disabled */
    if( isEnabled() && inputLength > 0 )
        mTimeTooltip->show();
}

void SeekSlider::leaveEvent( QEvent *e )
{
    if( !rect().contains( mapFromGlobal( QCursor::pos() ) ) )
        mTimeTooltip->hide();
}

void SeekSlider::hideEvent( QHideEvent * )
{
    mTimeTooltip->hide();
}

bool SeekSlider::eventFilter( QObject *obj, QEvent *event )
{
    if( obj == mTimeTooltip )
    {
        if( event->type() == QEvent::Leave ||
            event->type() == QEvent::MouseMove )
        {
            QMouseEvent *e = static_cast<QMouseEvent*>( event );
            if( !rect().contains( mapFromGlobal( e->globalPos() ) ) )
                mTimeTooltip->hide();
        }
        return false;
    }
    else
        return QSlider::eventFilter( obj, event );
}

QSize SeekSlider::sizeHint() const
{
    return ( orientation() == Qt::Horizontal ) ? QSize( 100, 18 )
                                               : QSize( 18, 100 );
}

QSize SeekSlider::handleSize() const
{
    const int size = ( orientation() == Qt::Horizontal ? height() : width() );
    return QSize( size, size );
}

void SeekSlider::paintEvent( QPaintEvent *event )
{
    Q_UNUSED( event );

    QStyleOptionSlider option;
    initStyleOption( &option );

    /* */
    QPainter painter( this );
    painter.setRenderHints( QPainter::Antialiasing );

    // draw bar
    const int barCorner = 3;
    qreal sliderPos     = -1;
    int range           = MAXIMUM;
    QRect barRect       = rect();

    // adjust positions based on the current orientation
    if ( option.sliderPosition != 0 )
    {
        switch ( orientation() )
        {
            case Qt::Horizontal:
                sliderPos = ( ( (qreal)width() ) / (qreal)range )
                        * (qreal)option.sliderPosition;
                break;
            case Qt::Vertical:
                sliderPos = ( ( (qreal)height() ) / (qreal)range )
                        * (qreal)option.sliderPosition;
                break;
        }
    }

    switch ( orientation() )
    {
        case Qt::Horizontal:
            barRect.setHeight( handleSize().height() /2 );
            break;
        case Qt::Vertical:
            barRect.setWidth( handleSize().width() /2 );
            break;
    }

    barRect.moveCenter( rect().center() );

    // set the background color and gradient
    QColor backgroundBase( 135, 135, 135 );
    QLinearGradient backgroundGradient( 0, 0, 0, height() );
    backgroundGradient.setColorAt( 0.0, backgroundBase );
    backgroundGradient.setColorAt( 1.0, backgroundBase.lighter( 150 ) );

    // set the foreground color and gradient
    QColor foregroundBase( 50, 156, 255 );
    QLinearGradient foregroundGradient( 0, 0, 0, height() );
    foregroundGradient.setColorAt( 0.0,  foregroundBase );
    foregroundGradient.setColorAt( 1.0,  foregroundBase.darker( 140 ) );

    // draw a slight 3d effect on the bottom
    painter.setPen( QColor( 230, 230, 230 ) );
    painter.setBrush( Qt::NoBrush );
    painter.drawRoundedRect( barRect.adjusted( 0, 2, 0, 0 ), barCorner, barCorner );

    // draw background
    painter.setPen( Qt::NoPen );
    painter.setBrush( backgroundGradient );
    painter.drawRoundedRect( barRect, barCorner, barCorner );

    // adjusted foreground rectangle
    QRect valueRect = barRect.adjusted( 1, 1, -1, 0 );

    switch ( orientation() )
    {
        case Qt::Horizontal:
            valueRect.setWidth( qMin( width(), int( sliderPos ) ) );
            break;
        case Qt::Vertical:
            valueRect.setHeight( qMin( height(), int( sliderPos ) ) );
            valueRect.moveBottom( rect().bottom() );
            break;
    }

    if ( option.sliderPosition > minimum() && option.sliderPosition <= maximum() )
    {
        // draw foreground
        painter.setPen( Qt::NoPen );
        painter.setBrush( foregroundGradient );
        painter.drawRoundedRect( valueRect, barCorner, barCorner );
    }

    // draw handle
    if ( option.state & QStyle::State_MouseOver )
    {
        if ( sliderPos != -1 )
        {
            const int margin = 0;
            QSize hs = handleSize() - QSize( 5, 5 );
            QPoint pos;

            switch ( orientation() )
            {
                case Qt::Horizontal:
                    pos = QPoint( sliderPos - ( hs.width() / 2 ), 2 );
                    pos.rx() = qMax( margin, pos.x() );
                    pos.rx() = qMin( width() - hs.width() - margin, pos.x() );
                    break;
                case Qt::Vertical:
                    pos = QPoint( 2, height() - ( sliderPos + ( hs.height() / 2 ) ) );
                    pos.ry() = qMax( margin, pos.y() );
                    pos.ry() = qMin( height() - hs.height() - margin, pos.y() );
                    break;
            }

            QRadialGradient buttonGradient( pos.x() + ( hs.width() / 2 ) - 2,
                                            pos.y() + ( hs.height() / 2 ) - 2,
                                            qMax( hs.width(), hs.height() ) );
            buttonGradient.setColorAt( 0.0, QColor(  0,  0,  0 ) );
            buttonGradient.setColorAt( 1.0, QColor( 80, 80, 80 ) );

            painter.setPen( Qt::NoPen );
            painter.setBrush( buttonGradient );
            painter.drawEllipse( pos.x(), pos.y(), hs.width(), hs.height() );
        }
    }
}


/* This work is derived from Amarok's work under GPLv2+
    - Mark Kretschmann
    - Gábor Lehel
   */
#define WLENGTH   80 // px
#define WHEIGHT   22  // px
#define SOUNDMIN  0   // %
#define SOUNDMAX  200 // % OR 400 ?

SoundSlider::SoundSlider( QWidget *_parent, int _i_step, bool b_hard,
                          char *psz_colors )
                        : QAbstractSlider( _parent )
{
    f_step = ( _i_step * 100 ) / AOUT_VOLUME_MAX ;
    setRange( SOUNDMIN, b_hard ? (2 * SOUNDMAX) : SOUNDMAX  );
    setMouseTracking( true );
    b_isSliding = false;
    b_mouseOutside = true;
    b_isMuted = false;

    pixOutside = QPixmap( ":/toolbar/volslide-outside" );

    const QPixmap temp( ":/toolbar/volslide-inside" );
    const QBitmap mask( temp.createHeuristicMask() );

    setFixedSize( pixOutside.size() );

    pixGradient = QPixmap( mask.size() );
    pixGradient2 = QPixmap( mask.size() );

    /* Gradient building from the preferences */
    QLinearGradient gradient( paddingL, 2, WLENGTH + paddingL , 2 );
    QLinearGradient gradient2( paddingL, 2, WLENGTH + paddingL , 2 );

    QStringList colorList = qfu( psz_colors ).split( ";" );
    free( psz_colors );

    /* Fill with 255 if the list is too short */
    if( colorList.size() < 12 )
        for( int i = colorList.size(); i < 12; i++)
            colorList.append( "255" );

    /* Regular colors */
#define c(i) colorList.at(i).toInt()
#define add_color(gradient, range, c1, c2, c3) \
    gradient.setColorAt( range, QColor( c(c1), c(c2), c(c3) ) );

    /* Desaturated colors */
#define desaturate(c) c->setHsvF( c->hueF(), 0.2 , 0.5, 1.0 )
#define add_desaturated_color(gradient, range, c1, c2, c3) \
    foo = new QColor( c(c1), c(c2), c(c3) );\
    desaturate( foo ); gradient.setColorAt( range, *foo );\
    delete foo;

    /* combine the two helpers */
#define add_colors( gradient1, gradient2, range, c1, c2, c3 )\
    add_color( gradient1, range, c1, c2, c3 ); \
    add_desaturated_color( gradient2, range, c1, c2, c3 );

    QColor * foo;
    add_colors( gradient, gradient2, 0.0, 0, 1, 2 );
    add_colors( gradient, gradient2, 0.22, 3, 4, 5 );
    add_colors( gradient, gradient2, 0.5, 6, 7, 8 );
    add_colors( gradient, gradient2, 1.0, 9, 10, 11 );

    QPainter painter( &pixGradient );
    painter.setPen( Qt::NoPen );
    painter.setBrush( gradient );
    painter.drawRect( pixGradient.rect() );
    painter.end();

    painter.begin( &pixGradient2 );
    painter.setPen( Qt::NoPen );
    painter.setBrush( gradient2 );
    painter.drawRect( pixGradient2.rect() );
    painter.end();

    pixGradient.setMask( mask );
    pixGradient2.setMask( mask );
}

void SoundSlider::wheelEvent( QWheelEvent *event )
{
    int newvalue = value() + event->delta() / ( 8 * 15 ) * f_step;
    setValue( __MIN( __MAX( minimum(), newvalue ), maximum() ) );

    emit sliderReleased();
    emit sliderMoved( value() );
}

void SoundSlider::mousePressEvent( QMouseEvent *event )
{
    if( event->button() != Qt::RightButton )
    {
        /* We enter the sliding mode */
        b_isSliding = true;
        i_oldvalue = value();
        emit sliderPressed();
        changeValue( event->x() - paddingL );
        emit sliderMoved( value() );
    }
}

void SoundSlider::mouseReleaseEvent( QMouseEvent *event )
{
    if( event->button() != Qt::RightButton )
    {
        if( !b_mouseOutside && value() != i_oldvalue )
        {
            emit sliderReleased();
            setValue( value() );
            emit sliderMoved( value() );
        }
        b_isSliding = false;
        b_mouseOutside = false;
    }
}

void SoundSlider::mouseMoveEvent( QMouseEvent *event )
{
    if( b_isSliding )
    {
        QRect rect( paddingL - 15,    -1,
                    WLENGTH + 15 * 2 , WHEIGHT + 5 );
        if( !rect.contains( event->pos() ) )
        { /* We are outside */
            if ( !b_mouseOutside )
                setValue( i_oldvalue );
            b_mouseOutside = true;
        }
        else
        { /* We are inside */
            b_mouseOutside = false;
            changeValue( event->x() - paddingL );
            emit sliderMoved( value() );
        }
    }
    else
    {
        int i = ( ( event->x() - paddingL ) * maximum() + 40 ) / WLENGTH;
        i = __MIN( __MAX( 0, i ), maximum() );
        setToolTip( QString("%1  \%" ).arg( i ) );
    }
}

void SoundSlider::changeValue( int x )
{
    setValue( (x * maximum() + 40 ) / WLENGTH );
}

void SoundSlider::setMuted( bool m )
{
    b_isMuted = m;
    update();
}

void SoundSlider::paintEvent( QPaintEvent *e )
{
    QPainter painter( this );
    QPixmap *pixGradient;
    if (b_isMuted)
        pixGradient = &this->pixGradient2;
    else
        pixGradient = &this->pixGradient;

    const int offset = int( ( WLENGTH * value() + 100 ) / maximum() ) + paddingL;

    const QRectF boundsG( 0, 0, offset , pixGradient->height() );
    painter.drawPixmap( boundsG, *pixGradient, boundsG );

    const QRectF boundsO( 0, 0, pixOutside.width(), pixOutside.height() );
    painter.drawPixmap( boundsO, pixOutside, boundsO );

    painter.setPen( palette().color( QPalette::Active, QPalette::Mid ) );
    QFont font; font.setPixelSize( 9 );
    painter.setFont( font );
    const QRect rect( 0, 0, 34, 15 );
    painter.drawText( rect, Qt::AlignRight | Qt::AlignVCenter,
                      QString::number( value() ) + '%' );

    painter.end();
    e->accept();
}

