/*
 * spectrumanaylzer_controls.cpp - controls for spectrum analyzer
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


#include "spectrum_analyzer.h"



spectrumAnalyzerControls::spectrumAnalyzerControls( spectrumAnalyzer * _eff ) :
	EffectControls( _eff ),
	m_effect( _eff ),
	m_linearSpec( FALSE, this, tr( "Linear spectrum" ) ),
	m_linearYAxis( FALSE, this, tr( "Linear Y-axis" ) ),
	m_channelMode( spectrumAnalyzer::MergeChannels,
			spectrumAnalyzer::MergeChannels,
			spectrumAnalyzer::RightChannel,
						this, tr( "Channel mode" ) )
{
}




void spectrumAnalyzerControls::loadSettings( const QDomElement & _this )
{
//	m_freqModel.setValue( _this.attribute( "freq" ).toFloat() );
}




void spectrumAnalyzerControls::saveSettings( QDomDocument & _doc, 
							QDomElement & _this )
{
//	_this.setAttribute( "freq", m_freqModel.value() );
}



#include "moc_spectrumanalyzer_controls.cxx"

