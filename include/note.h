/*
 * note.h - declaration of class note which contains all informations about a
 *          note + definitions of several constants and enums
 *
 * Copyright (c) 2004-2010 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#ifndef _NOTE_H
#define _NOTE_H

#include <QtCore/QVector>

#include "volume.h"
#include "panning.h"
#include "midi_time.h"
#include "SerializingObject.h"

class DetuningHelper;


enum Keys
{
	Key_C = 0,
	Key_CIS = 1, Key_DES = 1,
	Key_D = 2,
	Key_DIS = 3, Key_ES = 3,
	Key_E = 4, Key_FES = 4,
	Key_F = 5,
	Key_FIS = 6, Key_GES = 6,
	Key_G = 7,
	Key_GIS = 8, Key_AS = 8,
	Key_A = 9,
	Key_AIS = 10, Key_B = 10,
	Key_H = 11 
} ;


enum Octaves
{
	Octave_0,
	Octave_1,
	Octave_2,
	Octave_3,
	Octave_4, DefaultOctave = Octave_4,
	Octave_5,
	Octave_6,
	Octave_7,
	Octave_8,
	NumOctaves
} ;


const int WhiteKeysPerOctave = 7;
const int BlackKeysPerOctave = 5;
const int KeysPerOctave = WhiteKeysPerOctave + BlackKeysPerOctave;
const int NumKeys = NumOctaves * KeysPerOctave;
const int DefaultKey = DefaultOctave*KeysPerOctave + Key_A;

const float MaxDetuning = 4 * 12.0f;



class EXPORT note : public SerializingObject
{
public:
	note( const midiTime & _length = midiTime( 0 ),
		const midiTime & _pos = midiTime( 0 ),
		int key = DefaultKey,
		volume_t _volume = DefaultVolume,
		panning_t _panning = DefaultPanning,
		DetuningHelper * _detuning = NULL );
	note( const note & _note );
	virtual ~note();
	
	// used by GUI
	inline void setSelected( const bool _selected ){ m_selected = _selected; }
	inline void setOldKey( const int _oldKey ){ m_oldKey = _oldKey; }
	inline void setOldPos( const midiTime & _oldPos ){ m_oldPos = _oldPos; }
	inline void setOldLength( const midiTime & _oldLength )
	{
		m_oldLength = _oldLength;
	}
	inline void setIsPlaying( const bool _isPlaying )
	{
		m_isPlaying = _isPlaying;
	}
	
	
	void setLength( const midiTime & _length );
	void setPos( const midiTime & _pos );
	void setKey( const int _key );
	virtual void setVolume( const volume_t _volume = DefaultVolume );
	void setPanning( const panning_t _panning = DefaultPanning );
	void quantizeLength( const int _q_grid );
	void quantizePos( const int _q_grid );

	static inline bool lessThan( note * &lhs, note * &rhs )
	{
		// function to compare two notes - must be called explictly when
		// using qSort
		return (bool) ((int) ( *lhs ).pos() < (int) ( *rhs ).pos());
	}

	inline bool isSelected() const
	{
		return m_selected;
	}

	inline int oldKey() const
	{
		return m_oldKey;
	}

	inline midiTime oldPos() const
	{
		return m_oldPos;
	}

	inline midiTime oldLength() const
	{
		return m_oldLength;
	}

	inline bool isPlaying() const
	{
		return m_isPlaying;
	}

	inline midiTime endPos() const
	{
		const int l = length();
		return pos() + l;
	}

	inline const midiTime & length() const
	{
		return m_length;
	}

	inline const midiTime & pos() const
	{
		return m_pos;
	}

	inline midiTime pos( midiTime _base_pos ) const
	{
		const int bp = _base_pos;
		return m_pos - bp;
	}

	inline int key() const
	{
		return m_key;
	}

	inline volume_t getVolume() const
	{
		return m_volume;
	}

	inline panning_t getPanning() const
	{
		return m_panning;
	}

	static QString classNodeName()
	{
		return "note";
	}

	inline virtual QString nodeName() const
	{
		return classNodeName();
	}

	static midiTime quantized( const midiTime & _m, const int _q_grid );

	DetuningHelper * detuning() const
	{
		return m_detuning;
	}

	void editDetuningPattern();

	bool hasDetuningInfo() const;


protected:
	virtual void saveSettings( QDomDocument & _doc,
							QDomElement & _parent );
	virtual void loadSettings( const QDomElement & _this );

/*	virtual void undoStep( JournalEntry & _je );
	virtual void redoStep( JournalEntry & _je );*/


private:
/*	enum Actions
	{
		ChangeKey,
		ChangeVolume,
		ChangePanning,
		ChangeLength,
		ChangePosition
	} ;*/

	
	// for piano roll editing
	bool m_selected; 
	int m_oldKey;
	midiTime m_oldPos;
	midiTime m_oldLength;
	bool m_isPlaying;
	
	int m_key;
	volume_t m_volume;
	panning_t m_panning;
	midiTime m_length;
	midiTime m_pos;
	DetuningHelper * m_detuning;

	void createDetuning();

} ;


typedef QVector<note *> NoteVector;


#endif

