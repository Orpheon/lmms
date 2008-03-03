/*
 *
 * Copyright (c) 2008 Danny McRae <khjklujn/at/users.sourceforge.net>
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


#include "engine.h"
#include "note_play_handle.h"

#include "rhodey_instrument.h"
#include "rhodey_instrument_view.h"
#include "rhodey_processor.h"

#undef SINGLE_SOURCE_COMPILE
#include "embedded_resources.h"


extern "C"
{

plugin::descriptor rhodey_plugin_descriptor =
{
	STRINGIFY_PLUGIN_NAME( PLUGIN_NAME ),
	"Rhodey",
	QT_TRANSLATE_NOOP( "pluginBrowser",
				"Rhodes noises" ),
	"Danny McRae <khjklujn/at/users.sf.net>",
	0x0100,
	plugin::Instrument,
	new QPixmap( PLUGIN_NAME::getIconPixmap( "logo" ) ),
	NULL
} ;

}


rhodeyInstrument::rhodeyInstrument( instrumentTrack * _channel_track ):
	stkInstrument<rhodeyProcessor, rhodeyModel>( _channel_track, &rhodey_plugin_descriptor )
{
	model()->index()->setTrack( _channel_track );
	model()->crossfade()->setTrack( _channel_track );
	model()->lfoSpeed()->setTrack( _channel_track );
	model()->lfoDepth()->setTrack( _channel_track );
	model()->adsrTarget()->setTrack( _channel_track );
}




rhodeyInstrument::~rhodeyInstrument()
{
}




QString rhodeyInstrument::nodeName( void ) const
{
	return( rhodey_plugin_descriptor.name );
}




pluginView * rhodeyInstrument::instantiateView( QWidget * _parent )
{
	return( new rhodeyInstrumentView( this, _parent ) );
}




extern "C"
{

// neccessary for getting instance out of shared lib
plugin * lmms_plugin_main( model * _model, void * _data )
{
	return( new rhodeyInstrument( static_cast<instrumentTrack *>( _data ) ) );
}


}

