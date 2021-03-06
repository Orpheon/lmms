/*
 * Piano.cpp - implementation of piano-widget used in instrument-track-window
 *             for testing + according model class
 *
 * Copyright (c) 2004-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

/** \file Piano.cpp
 *  \brief A piano keyboard to play notes on in the instrument plugin window.
 */

/*
 * \mainpage Instrument plugin keyboard display classes
 *
 * \section introduction Introduction
 * 
 * \todo fill this out
 * \todo write isWhite inline function and replace throughout
 */

#include "Piano.h"
#include "MidiEventProcessor.h"


/*! \brief Create a new keyboard display
 *
 *  \param _it the InstrumentTrack window to attach to
 */
Piano::Piano( MidiEventProcessor * _mep ) :
	Model( NULL ),              /*!< base class ctor */
	m_midiEvProc( _mep )        /*!< the InstrumentTrack Model */
{
	for( int i = 0; i < NumKeys; ++i )
	{
		m_pressedKeys[i] = false;
	}

}




/*! \brief Destroy this new keyboard display
 *
 */
Piano::~Piano()
{
}




/*! \brief Turn a key on or off
 *
 *  \param _key the key number to change
 *  \param _on the state to set the key to
 */
void Piano::setKeyState( int _key, bool _on )
{
	m_pressedKeys[tLimit( _key, 0, NumKeys-1 )] = _on;
	emit dataChanged();
}




/*! \brief Handle a note being pressed on our keyboard display
 *
 *  \param _key the key being pressed
 */
void Piano::handleKeyPress( int _key )
{
	m_midiEvProc->processInEvent( midiEvent( MidiNoteOn, 0, _key,
						MidiMaxVelocity ), midiTime() );
	m_pressedKeys[_key] = true;
}





/*! \brief Handle a note being released on our keyboard display
 *
 *  \param _key the key being releassed
 */
void Piano::handleKeyRelease( int _key )
{
	m_midiEvProc->processInEvent( midiEvent( MidiNoteOff, 0, _key, 0 ),
								midiTime() );
	m_pressedKeys[_key] = false;
}




#include "moc_Piano.cxx"

