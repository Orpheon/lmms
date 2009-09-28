/*
 * SampleSegmentItem.h - SampleSegment QGraphicsItem used in the song editor
 *
 * Copyright (c) 2009 Paul Giblock <pgib/at/users.sourceforge.net>
 *
 * This file is part of Linux MultiMedia Studio - http://lmms.sourceforge.net
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program (see COPYING); if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 *
 */


#include <QtGui/QStyleOptionGraphicsItem>

#include "gui/tracks/SampleSegmentItem.h"
#include "gui/tracks/track_content_object_item.h"
#include "gui/tracks/track_container_scene.h"
#include "gui/tracks/track_item.h"
#include "engine.h"
#include "lmms_style.h"
#include "Track.h"



SampleSegmentItem::SampleSegmentItem( TrackItem * _track, TrackSegment * _object ) :
	TrackSegmentItem( _track, _object )
{
}

void SampleSegmentItem::paint(
		QPainter * _painter,
		const QStyleOptionGraphicsItem * _option,
		QWidget * _widget )
{

	QRectF rc = boundingRect();

	qreal xscale = _option->matrix.m11();
	_painter->save();
	_painter->scale( 1.0f/xscale, 1.0f );
	rc.setWidth( rc.width() * xscale );

	// TODO: Use a proxy class
	LmmsStyleOptionTCO * options = new LmmsStyleOptionTCO();
	options->type = LmmsStyleOptionTCO::Pattern;
	options->rect = rc;
	options->selected = isSelected();
	options->hovered = m_hover;
	options->duration = 1;

	engine::getLmmsStyle()->drawTrackSegment( _painter, m_tco, options );

	_painter->restore();
	delete options;
	return;
}



void SampleSegmentItem::mousePressEvent( QGraphicsSceneMouseEvent * event )
{
	TrackSegmentItem::mousePressEvent( event );
}



void SampleSegmentItem::mouseReleaseEvent( QGraphicsSceneMouseEvent * event )
{
	TrackSegmentItem::mouseReleaseEvent( event );
} 
 

#include "gui/tracks/moc_SampleSegmentItem.cxx"
