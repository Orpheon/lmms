/*
 * stereo_enhancer.cpp - stereo-enhancer-effect-plugin
 *
 * Copyright (c) 2006-2007 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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


#include "stereo_enhancer.h"


#undef SINGLE_SOURCE_COMPILE
#include "embed.cpp"


extern "C"
{

plugin::descriptor stereoenhancer_plugin_descriptor =
{
	STRINGIFY_PLUGIN_NAME( PLUGIN_NAME ),
	"StereoEnhancer Effect",
	QT_TRANSLATE_NOOP( "pluginBrowser",
				"Plugin for enhancing stereo separation of a stereo input file" ),
	"Lou Herard <lherard/at/gmail.com>",
	0x0100,
	plugin::Effect,
	new QPixmap( PLUGIN_NAME::getIconPixmap( "logo" ) ),
	NULL
} ;

}



stereoEnhancerEffect::stereoEnhancerEffect(
			const descriptor::subPluginFeatures::key * _key ) :
	effect( &stereoenhancer_plugin_descriptor, _key ),
	m_seFX( effectLib::stereoEnhancer<>( 0.0f ) ),
	m_delayBuffer( new surroundSampleFrame[DEFAULT_BUFFER_SIZE] ),
	m_currFrame( 0 )
{
	// TODO:  Make m_delayBuffer customizable?
}




stereoEnhancerEffect::~stereoEnhancerEffect()
{
	if( m_delayBuffer )
	{
		//delete [] m_delayBuffer;
		delete m_delayBuffer;
		
	}
	m_currFrame = 0;
}




bool FASTCALL stereoEnhancerEffect::processAudioBuffer( surroundSampleFrame * _buf,
							const fpp_t _frames )
{
	
	// This appears to be used for determining whether or not to continue processing
	// audio with this effect	
	double out_sum = 0.0;
	
	float width;
	int frameIndex = 0;
	
	
	if( isBypassed() || !isRunning () )
	{
		return( FALSE );
	}

	for( fpp_t f = 0; f < _frames; ++f )
	{
		
		// copy samples into the delay buffer
		m_delayBuffer[m_currFrame][0] = _buf[f][0];
		m_delayBuffer[m_currFrame][1] = _buf[f][1];
		
		// Get the width knob value from the Stereo Enhancer effect
		width = m_seFX.getWideCoeff();
		
		// Calculate the correct sample frame for processing
		frameIndex = m_currFrame - width;
		
		if( frameIndex < 0 )
		{
			// e.g. difference = -10, frameIndex = DBS - 10
			frameIndex += DEFAULT_BUFFER_SIZE;
		}

		//sample_t s[2] = { _buf[f][0], _buf[f][1] };	//Vanilla
		sample_t s[2] = { _buf[f][0], m_delayBuffer[frameIndex][1] };	//Chocolate
		
		m_seFX.nextSample( s[0], s[1] );

		for( ch_cnt_t ch = 0; ch < SURROUND_CHANNELS; ++ch )
		{
			_buf[f][ch] = getDryLevel() * _buf[f][ch] +
				getWetLevel() *
					s[ch%DEFAULT_CHANNELS];
			out_sum += _buf[f][ch]*_buf[f][ch];			
		}
		
		// Update currFrame
		m_currFrame += 1;
		m_currFrame %= DEFAULT_BUFFER_SIZE;
	}

	if( out_sum <= getGate() )
	{
		incrementBufferCount();
		if( getBufferCount() > getTimeout() )
		{
			stopRunning();
			resetBufferCount();
			clearMyBuffer();
		}
	}
	else
	{
		resetBufferCount();
		//clearMyBuffer();
	}
	return( isRunning() );
}


void stereoEnhancerEffect::clearMyBuffer()
{
	int i;
	for (i = 0; i < DEFAULT_BUFFER_SIZE; i++)
	{
		m_delayBuffer[i][0] = 0.0f;
		m_delayBuffer[i][1] = 0.0f;
	}
	
	m_currFrame = 0;
}





extern "C"
{

// neccessary for getting instance out of shared lib
plugin * lmms_plugin_main( void * _data )
{
	return( new stereoEnhancerEffect(
		static_cast<const plugin::descriptor::subPluginFeatures::key *>(
								_data ) ) );
}

}
