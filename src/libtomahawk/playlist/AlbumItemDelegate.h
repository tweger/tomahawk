/* === This file is part of Tomahawk Player - <http://tomahawk-player.org> ===
 *
 *   Copyright 2010-2011, Christian Muehlhaeuser <muesli@tomahawk-player.org>
 *   Copyright 2011-2012, Leo Franchi            <lfranchi@kde.org>
 *
 *   Tomahawk is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 3 of the License, or
 *   (at your option) any later version.
 *
 *   Tomahawk is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with Tomahawk. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef ALBUMITEMDELEGATE_H
#define ALBUMITEMDELEGATE_H

#include <QStyledItemDelegate>
#include <QTimeLine>

#include "DllMacro.h"

class QTimeLine;
namespace Tomahawk {
    class PixmapDelegateFader;
}

class QEvent;
class AlbumProxyModel;
class ImageButton;

class DLLEXPORT AlbumItemDelegate : public QStyledItemDelegate
{
Q_OBJECT

public:
    AlbumItemDelegate( QAbstractItemView* parent = 0, AlbumProxyModel* proxy = 0 );

protected:
    void paint( QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index ) const;
    QSize sizeHint( const QStyleOptionViewItem& option, const QModelIndex& index ) const;

    bool editorEvent( QEvent* event, QAbstractItemModel* model, const QStyleOptionViewItem& option, const QModelIndex& index );
    bool eventFilter( QObject* obj, QEvent* event );

signals:
    void updateIndex( const QModelIndex& idx );

private slots:
    void modelChanged();
    void doUpdateIndex( const QPersistentModelIndex& idx );

    void onScrolled( int dx, int dy );
    void onPlaybackStarted( const QPersistentModelIndex& index );
    void onPlaybackFinished();

    void onPlayClicked( const QPersistentModelIndex& index );
    void onPlaylistChanged( const QPersistentModelIndex& index );

    void fadingFrameChanged( const QPersistentModelIndex& );
    void fadingFrameFinished( const QPersistentModelIndex& );
private:
    QTimeLine* createTimeline( QTimeLine::Direction direction );

    QAbstractItemView* m_view;
    AlbumProxyModel* m_model;

    mutable QHash< QPersistentModelIndex, QRect > m_artistNameRects;
    mutable QHash< QPersistentModelIndex, QSharedPointer< Tomahawk::PixmapDelegateFader > > m_covers;

    QPersistentModelIndex m_hoverIndex;
    QPersistentModelIndex m_hoveringOver;
    mutable QRect m_playButtonRect;

    QPixmap m_shadowPixmap;
    mutable QHash< QPersistentModelIndex, QWidget* > m_spinner;
    mutable QHash< QPersistentModelIndex, ImageButton* > m_playButton;
    mutable QHash< QPersistentModelIndex, ImageButton* > m_pauseButton;

    mutable QHash< QPersistentModelIndex, QTimeLine* > m_hoverFaders;
};

#endif // ALBUMITEMDELEGATE_H
