/*
 * stereoenhancer_controls.cpp - control-dialog for stereoenhancer-effect
 *
 * Copyright (c) 2006-2008 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "stereoenhancer_controls.h"
#include "stereo_enhancer.h"
#include "automatable_model_templates.h"


stereoEnhancerControls::stereoEnhancerControls( stereoEnhancerEffect * _eff ) :
		effectControls( _eff ),
		m_effect( _eff ),
		m_widthModel(0.0f, 0.0f, 180.0f, 1.0f)
{
	connect( &m_widthModel, SIGNAL( dataChanged( void ) ),
			this, SLOT( changeWideCoeff( void ) ) );

	changeWideCoeff();
}



void stereoEnhancerControls::changeWideCoeff( void )
{
	m_effect->m_seFX.setWideCoeff( m_widthModel.value() );
}



void FASTCALL stereoEnhancerControls::loadSettings(
						const QDomElement & _this )
{
	m_widthModel.setValue( _this.attribute( "width" ).toFloat() );
}




void FASTCALL stereoEnhancerControls::saveSettings( QDomDocument & _doc, 
							QDomElement & _this )
{
	_this.setAttribute( "width", m_widthModel.value() );
}



#include "stereoenhancer_controls.moc"
