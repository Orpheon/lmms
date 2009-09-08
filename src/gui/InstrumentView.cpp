/*
 * InstrumentView.cpp - base-class for views of all instruments
 *
 * Copyright (c) 2008-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "InstrumentView.h"
#include "embed.h"
#include "Instrument.h"
#include "InstrumentTrack.h"
#include "ResourceDB.h"
#include "string_pair_drag.h"


InstrumentView::InstrumentView( Instrument * _instrument, QWidget * _parent ) :
	PluginView( _instrument, _parent )
{
	setModel( _instrument );
	setFixedSize( 250, 250 );
	setAttribute( Qt::WA_DeleteOnClose, true );
	setAcceptDrops( true );
}




InstrumentView::~InstrumentView()
{
	/* TODO{TNG}: Reimplement
	if( instrumentTrackWindow() )
	{
		instrumentTrackWindow()->m_instrumentView = NULL;
	}
	*/
}




void InstrumentView::setModel( Model * _model, bool )
{
	if( dynamic_cast<Instrument *>( _model ) != NULL )
	{
		ModelView::setModel( _model );
		/* TODO{TNG}: Reimplement
		instrumentTrackWindow()->setWindowIcon(
				model()->descriptor()->logo->pixmap() );
		*/
		connect( model(), SIGNAL( destroyed( QObject * ) ),
					this, SLOT( close() ) );
	}
}




InstrumentTrackWindow * InstrumentView::instrumentTrackWindow()
{
	/* TODO{TNG}: Reimplement
	return dynamic_cast<InstrumentTrackWindow *>(
			parentWidget()->parentWidget() );
	*/
}




void InstrumentView::dragEnterEvent( QDragEnterEvent * _dee )
{
	if( stringPairDrag::decodeKey( _dee ) == ResourceItem::mimeKey() )
	{
		const ResourceItem * item =
			engine::mergedResourceDB()->
				itemByHash( stringPairDrag::decodeValue( _dee ) );
		if( item &&
			model()->descriptor()->supportsFileType(
						item->nameExtension() ) )
		{
			_dee->acceptProposedAction();
		}
	}
}




void InstrumentView::dropEvent( QDropEvent * _de )
{
	if( stringPairDrag::decodeKey( _de ) == ResourceItem::mimeKey() )
	{
		const ResourceItem * item =
			engine::mergedResourceDB()->
				itemByHash( stringPairDrag::decodeValue( _de ) );
		model()->loadResource( item );
		_de->accept();
	}
}





