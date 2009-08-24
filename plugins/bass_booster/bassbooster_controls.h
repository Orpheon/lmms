/*
 * bassbooster_controls.h - controls for bassbooster-effect
 *
 * Copyright (c) 2008 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#ifndef _BASSBOOSTER_CONTROLS_H
#define _BASSBOOSTER_CONTROLS_H

#include "EffectControls.h"
#include "bassbooster_control_dialog.h"
#include "knob.h"


class bassBoosterEffect;


class bassBoosterControls : public EffectControls
{
	Q_OBJECT
public:
	bassBoosterControls( bassBoosterEffect * _eff );
	virtual ~bassBoosterControls()
	{
	}

	virtual void saveSettings( QDomDocument & _doc, QDomElement & _parent );
	virtual void loadSettings( const QDomElement & _this );
	inline virtual QString nodeName( void ) const
	{
		return( "bassboostercontrols" );
	}

	virtual int controlCount( void )
	{
		return( 3 );
	}

	virtual EffectControlDialog * createView( void )
	{
		return( new bassBoosterControlDialog( this ) );
	}


private slots:
	void changeFrequency( void );
	void changeGain( void );
	void changeRatio( void );


private:
	bassBoosterEffect * m_effect;
	FloatModel m_freqModel;
	FloatModel m_gainModel;
	FloatModel m_ratioModel;

	friend class bassBoosterControlDialog;

} ;

#endif
