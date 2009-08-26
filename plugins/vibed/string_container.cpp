/*
 * string_container.cpp - contains a collection of strings
 *
 * Copyright (c) 2006 Danny McRae <khjklujn/at/yahoo/com>
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

#include "string_container.h"


stringContainer::stringContainer(const float _pitch, 
				const sample_rate_t _sample_rate,
				const Uint32 _buffer_length,
				const Uint8 _strings ) :
	m_pitch( _pitch ),
	m_sampleRate( _sample_rate ),
	m_bufferLength( _buffer_length )
{
	for( Uint8 i = 0; i < _strings; i++ )
	{
		m_exists.append( false );
	}
}




void stringContainer::addString(Uint8 _harm,
				const float _pick,
				const float _pickup,
				const float * _impulse,
				const float _randomize,
				const float _string_loss,
				const float _detune,
				const Uint8 _oversample,
				const bool _state,
				const Uint8 _id )
{
	float harm;
	switch( _harm )
	{
		case 0:
			harm = 0.25f;
			break;
		case 1:
			harm = 0.5f;
			break;
		case 2:
			harm = 1.0f;
			break;
		case 3:
			harm = 2.0f;
			break;
		case 4:
			harm = 3.0f;
			break;
		case 5:
			harm = 4.0f;
			break;
		case 6:
			harm = 5.0f;
			break;
		case 7:
			harm = 6.0f;
			break;
		case 8:
			harm = 7.0f;
			break;
		default:
			harm = 1.0f;
	}

	m_strings.append( new vibratingString(	m_pitch * harm,
						_pick, 
						_pickup,
						const_cast<float*>(_impulse),
						m_bufferLength,
						m_sampleRate,
						_oversample,
						_randomize,
						_string_loss,
						_detune,
						_state ) );
	m_exists[_id] = true;
}
