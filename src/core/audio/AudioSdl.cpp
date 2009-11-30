/*
 * AudioSdl.cpp - device-class that performs PCM-output via SDL
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

#include "AudioSdl.h"

#ifdef LMMS_HAVE_SDL

#include <QtGui/QLabel>
#include <QtGui/QLineEdit>

#include "engine.h"
#include "debug.h"
#include "gui_templates.h"
#include "templates.h"
#include "Cpu.h"



AudioSdl::AudioSdl( bool & _success_ful, AudioOutputContext * context ) :
	AudioBackend( DEFAULT_CHANNELS, context ),
	m_outBuf( CPU::allocFrames( mixer()->framesPerPeriod() ) ),
	m_convertedBufPos( 0 ),
	m_convertEndian( false ),
	m_stopSemaphore( 1 )
{
	_success_ful = false;

	m_convertedBufSize = mixer()->framesPerPeriod() *
						sizeof( intSampleFrameA );
	m_convertedBuf = (intSampleFrameA *) CPU::memAlloc( m_convertedBufSize );


	if( SDL_Init( SDL_INIT_AUDIO | SDL_INIT_NOPARACHUTE ) < 0 )
	{
		qCritical( "Couldn't initialize SDL: %s\n", SDL_GetError() );
		return;
	}

	m_audioHandle.freq = sampleRate();
	m_audioHandle.format = AUDIO_S16SYS;	// we want it in byte-order
						// of system, so we don't have
						// to convert the buffers
	m_audioHandle.channels = channels();
	m_audioHandle.samples = qMax( 1024, mixer()->framesPerPeriod()*2 );

	m_audioHandle.callback = sdlAudioCallback;
	m_audioHandle.userdata = this;

  	SDL_AudioSpec actual; 

	// open the audio device, forcing the desired format
	if( SDL_OpenAudio( &m_audioHandle, &actual ) < 0 )
	{
		qCritical( "Couldn't open SDL-audio: %s\n", SDL_GetError() );
		return;
	}
	m_convertEndian = ( m_audioHandle.format != actual.format );

	m_stopSemaphore.acquire();

	_success_ful = true;
}




AudioSdl::~AudioSdl()
{
	stopProcessing();
	m_stopSemaphore.release();

	SDL_CloseAudio();
	SDL_Quit();
	CPU::memFree( m_convertedBuf );
	CPU::freeFrames( m_outBuf );
}




void AudioSdl::startProcessing()
{
	m_stopped = false;

	SDL_PauseAudio( 0 );
}




void AudioSdl::stopProcessing()
{
	if( SDL_GetAudioStatus() == SDL_AUDIO_PLAYING )
	{
		m_stopSemaphore.acquire();

		SDL_LockAudio();
		SDL_PauseAudio( 1 );
		SDL_UnlockAudio();
	}
}




void AudioSdl::applyQualitySettings()
{
	if( 0 )//hqAudio() )
	{
		SDL_CloseAudio();

		setSampleRate( mixer()->processingSampleRate() );

		m_audioHandle.freq = sampleRate();

		SDL_AudioSpec actual; 

		// open the audio device, forcing the desired format
		if( SDL_OpenAudio( &m_audioHandle, &actual ) < 0 )
		{
			qCritical( "Couldn't open SDL-audio: %s\n", SDL_GetError() );
		}
	}
}




void AudioSdl::sdlAudioCallback( void * _udata, Uint8 * _buf, int _len )
{
	AudioSdl * _this = static_cast<AudioSdl *>( _udata );

	_this->sdlAudioCallback( _buf, _len );
}




void AudioSdl::sdlAudioCallback( Uint8 * _buf, int _len )
{
	if( m_stopped )
	{
		memset( _buf, 0, _len );
		return;
	}

	while( _len )
	{
		if( m_convertedBufPos == 0 )
		{
			// frames depend on the sample rate
			const fpp_t frames = getNextBuffer( m_outBuf );
			if( !frames )
			{
				m_stopped = true;
				m_stopSemaphore.release();
				memset( _buf, 0, _len );
				return;
			}
			m_convertedBufSize = frames * sizeof( intSampleFrameA );

			CPU::convertToS16( m_outBuf,
						m_convertedBuf,
						frames,
						mixer()->masterGain(),
						m_convertEndian );
		}
		const int min_len = qMin( _len, m_convertedBufSize
							- m_convertedBufPos );
		memcpy( _buf, m_convertedBuf + m_convertedBufPos, min_len );
		_buf += min_len;
		_len -= min_len;
		m_convertedBufPos += min_len;
		m_convertedBufPos %= m_convertedBufSize;
	}
}




AudioSdl::setupWidget::setupWidget( QWidget * _parent ) :
	AudioBackend::setupWidget( AudioSdl::name(), _parent )
{
	QString dev = cfg().value( "Device" );
	m_device = new QLineEdit( dev, this );
	m_device->setGeometry( 10, 20, 160, 20 );

	QLabel * dev_lbl = new QLabel( tr( "DEVICE" ), this );
	dev_lbl->setFont( pointSize<6>( dev_lbl->font() ) );
	dev_lbl->setGeometry( 10, 40, 160, 10 );

}




AudioSdl::setupWidget::~setupWidget()
{
}




void AudioSdl::setupWidget::saveSettings()
{
	cfg().setValue( "Device", m_device->text() );
}


#endif

