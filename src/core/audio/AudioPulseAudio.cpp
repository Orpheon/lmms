/*
 * AudioPulseAudio.cpp - device-class which implements PulseAudio-output
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

#include <QtGui/QLineEdit>
#include <QtGui/QLabel>

#include "AudioPulseAudio.h"

#ifdef LMMS_HAVE_PULSEAUDIO

#include "endian_handling.h"
#include "lcd_spinbox.h"
#include "gui_templates.h"
#include "templates.h"
#include "Cpu.h"


static void stream_write_callback(pa_stream *s, size_t length, void *userdata)
{
	static_cast<AudioPulseAudio *>( userdata )->streamWriteCallback( s, length );
}




AudioPulseAudio::AudioPulseAudio( bool & _success_ful, mixer * _mixer ) :
	AudioDevice( tLimit<ch_cnt_t>( cfg().value( "Channels" ).toInt(),
					DEFAULT_CHANNELS, SURROUND_CHANNELS ),
								_mixer ),
	m_s( NULL ),
	m_convertEndian( false )
{
	_success_ful = false;

	m_sampleSpec.format = PA_SAMPLE_S16LE;
	m_sampleSpec.rate = sampleRate();
	m_sampleSpec.channels = channels();

	_success_ful = true;
}




AudioPulseAudio::~AudioPulseAudio()
{
	stopProcessing();

	if( m_s != NULL )
	{
		pa_stream_unref( m_s );
	}
}




QString AudioPulseAudio::probeDevice()
{
	QString dev = cfg().value( "Device" );
	if( dev.isEmpty() )
	{
		if( getenv( "AUDIODEV" ) != NULL )
		{
			return getenv( "AUDIODEV" );
		}
		return "default";
	}
	return dev;
}




void AudioPulseAudio::startProcessing()
{
	if( !isRunning() )
	{
		start( QThread::HighPriority );
	}
}




void AudioPulseAudio::stopProcessing()
{
	if( isRunning() )
	{
		wait( 1000 );
		terminate();
	}
}




void AudioPulseAudio::applyQualitySettings()
{
	if( hqAudio() )
	{
//		setSampleRate( engine::getMixer()->processingSampleRate() );

	}

	AudioDevice::applyQualitySettings();
}




/* This routine is called whenever the stream state changes */
static void stream_state_callback( pa_stream *s, void * userdata )
{
	switch( pa_stream_get_state( s ) )
	{
		case PA_STREAM_CREATING:
		case PA_STREAM_TERMINATED:
			break;

		case PA_STREAM_READY:
			qDebug( "Stream successfully created\n" );
			break;

		case PA_STREAM_FAILED:
		default:
			qCritical( "Stream errror: %s\n",
					pa_strerror(pa_context_errno(
						pa_stream_get_context( s ) ) ) );
	}
}



/* This is called whenever the context status changes */
static void context_state_callback(pa_context *c, void *userdata)
{
	AudioPulseAudio * _this = static_cast<AudioPulseAudio *>( userdata );
	switch( pa_context_get_state( c ) )
	{
		case PA_CONTEXT_CONNECTING:
		case PA_CONTEXT_AUTHORIZING:
		case PA_CONTEXT_SETTING_NAME:
			break;

		case PA_CONTEXT_READY:
		{
			pa_cvolume cv;
			qDebug( "Connection established.\n" );
			_this->m_s = pa_stream_new( c, "lmms", &_this->m_sampleSpec,  NULL);
			pa_stream_set_state_callback( _this->m_s, stream_state_callback, _this );
			pa_stream_set_write_callback( _this->m_s, stream_write_callback, _this );
			pa_stream_connect_playback( _this->m_s, NULL, NULL,
						(pa_stream_flags) 0,
						pa_cvolume_set( &cv, _this->m_sampleSpec.channels,
											PA_VOLUME_NORM ),
						NULL );
			break;
		}

		case PA_CONTEXT_TERMINATED:
			break;

		case PA_CONTEXT_FAILED:
		default:
			qCritical( "Connection failure: %s\n", pa_strerror( pa_context_errno( c ) ) );
	}
}




void AudioPulseAudio::run()
{
	pa_mainloop * m = NULL;


	if (!(m = pa_mainloop_new())) {
		qCritical( "pa_mainloop_new() failed.\n" );
		return;
	}
	pa_mainloop_api * mainloop_api = pa_mainloop_get_api(m);

	pa_context *context = pa_context_new(mainloop_api, "lmms");
	if ( context == NULL )
	{
        	qCritical( "pa_context_new() failed." );
		return;
	}

	pa_context_set_state_callback(context, context_state_callback, this );
	/* Connect the context */
	pa_context_connect(context, NULL, (pa_context_flags) 0, NULL);

	int ret;
	/* Run the main loop */
	if (pa_mainloop_run(m, &ret) < 0)
	{
		qCritical( "pa_mainloop_run() failed.\n" );
	}
}




void AudioPulseAudio::streamWriteCallback(pa_stream *s, size_t length)
{
	const fpp_t fpp = getMixer()->framesPerPeriod();
	sampleFrameA * temp = CPU::allocFrames( fpp );
	Sint16 * pcmbuf = (Sint16*)CPU::memAlloc( fpp * channels() *
							sizeof(Sint16) );

	size_t fd = 0;
	while( fd < length/4 )
	{
		const fpp_t frames = getNextBuffer( temp );
		if( !frames )
		{
			return;
		}
		int bytes = CPU::convertToS16( temp,
						(intSampleFrameA *) pcmbuf,
						frames,
						getMixer()->masterGain(),
						m_convertEndian );
		if( bytes > 0 )
		{
			pa_stream_write( m_s, pcmbuf, bytes, NULL, 0,
							PA_SEEK_RELATIVE );
		}
		fd += frames;
	}

	CPU::memFree( pcmbuf );
	CPU::freeFrames( temp );
}




AudioPulseAudio::setupWidget::setupWidget( QWidget * _parent ) :
	AudioDevice::setupWidget( AudioPulseAudio::name(), _parent )
{
	m_device = new QLineEdit( AudioPulseAudio::probeDevice(), this );
	m_device->setGeometry( 10, 20, 160, 20 );

	QLabel * dev_lbl = new QLabel( tr( "DEVICE" ), this );
	dev_lbl->setFont( pointSize<6>( dev_lbl->font() ) );
	dev_lbl->setGeometry( 10, 40, 160, 10 );

	lcdSpinBoxModel * m = new lcdSpinBoxModel( /* this */ );
	m->setRange( DEFAULT_CHANNELS, SURROUND_CHANNELS );
	m->setStep( 2 );
	m->setValue( cfg().value( "Channels" ).toInt() );

	m_channels = new lcdSpinBox( 1, this );
	m_channels->setModel( m );
	m_channels->setLabel( tr( "CHANNELS" ) );
	m_channels->move( 180, 20 );

}




AudioPulseAudio::setupWidget::~setupWidget()
{

}




void AudioPulseAudio::setupWidget::saveSettings()
{
	cfg().setValue( "Device", m_device->text() );
	cfg().setValue( "Channels", QString::number( m_channels->value<int>() ) );
}


#endif

