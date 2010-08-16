/*
 * LadspaEffect.cpp - class for processing LADSPA effects
 *
 * Copyright (c) 2006-2008 Danny McRae <khjklujn/at/users.sourceforge.net>
 * Copyright (c) 2009-2010 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include <QtGui/QMessageBox>

#include "AudioBackend.h"
#include "AudioOutputContext.h"
#include "LadspaEffect.h"
#include "mmp.h"
#include "ladspa_2_lmms.h"
#include "LadspaControl.h"
#include "LadspaSubPluginFeatures.h"
#include "Mixer.h"
#include "EffectChain.h"
#include "Cpu.h"
#include "AutomationPattern.h"
#include "ControllerConnection.h"

#include "embed.cpp"


extern "C"
{

Plugin::Descriptor PLUGIN_EXPORT ladspaeffect_plugin_descriptor =
{
	STRINGIFY( PLUGIN_NAME ),
	"LADSPA Effect",
	QT_TRANSLATE_NOOP( "pluginBrowser",
				"plugin for using arbitrary LADSPA-effects "
				"inside LMMS." ),
	"Danny McRae <khjklujn/at/users.sourceforge.net>",
	0x0100,
	Plugin::Effect,
	new PluginPixmapLoader( "logo" ),
	NULL,
	new LadspaSubPluginFeatures( Plugin::Effect )
} ;

}


LadspaEffect::LadspaEffect( Model * _parent,
			const Descriptor::SubPluginFeatures::Key * _key ) :
	Effect( &ladspaeffect_plugin_descriptor, _parent, _key ),
	m_controls( NULL ),
	m_maxSampleRate( 0 ),
	m_key( LadspaSubPluginFeatures::subPluginKeyToLadspaKey( _key ) )
{
	ladspa2LMMS * manager = engine::getLADSPAManager();
	if( manager->getDescription( m_key ) == NULL )
	{
		if( !engine::suppressMessages() )
		{
			QMessageBox::warning( 0, tr( "Effect" ), 
				tr( "Unknown LADSPA plugin %1 requested." ).
							arg( m_key.second ),
				QMessageBox::Ok, QMessageBox::NoButton );
		}
		setOkay( false );
		return;
	}

	setDisplayName( manager->getShortName( m_key ) );

	pluginInstantiation();

	connect( engine::mixer(), SIGNAL( sampleRateChanged() ),
					this, SLOT( changeSampleRate() ) );
}




LadspaEffect::~LadspaEffect()
{
	pluginDestruction();
}




void LadspaEffect::changeSampleRate()
{
	multimediaProject mmp( multimediaProject::EffectSettings );
	m_controls->saveState( mmp, mmp.content() );

	LadspaControls * controls = m_controls;
	m_controls = NULL;

	m_pluginMutex.lock();
	pluginDestruction();
	pluginInstantiation();
	m_pluginMutex.unlock();

	controls->effectModelChanged( m_controls );
	delete controls;

	m_controls->restoreState( mmp.content().firstChild().toElement() );

	// the IDs of re-created controls have been saved and now need to be
	// resolved again
	AutomationPattern::resolveAllIDs();

	// make sure, connections are ok
	ControllerConnection::finalizeConnections();
}




bool LadspaEffect::processAudioBuffer( sampleFrame * _buf, 
							const fpp_t _frames )
{
	m_pluginMutex.lock();
	if( !isOkay() || dontRun() || !isRunning() || !isEnabled() )
	{
		m_pluginMutex.unlock();
		return false;
	}

	int frames = _frames;
	sampleFrame * o_buf = NULL;

	if( m_maxSampleRate < engine::mixer()->processingSampleRate() )
	{
		o_buf = _buf;
		_buf = CPU::allocFrames( _frames );
		sampleDown( o_buf, _buf, m_maxSampleRate );
		frames = _frames * m_maxSampleRate /
				engine::mixer()->processingSampleRate();
	}

	// Copy the LMMS audio buffer to the LADSPA input buffer and initialize
	// the control ports.  Need to change this to handle non-in-place-broken
	// plugins--would speed things up to use the same buffer for both
	// LMMS and LADSPA.
	ch_cnt_t channel = 0;
	for( ch_cnt_t proc = 0; proc < processorCount(); ++proc )
	{
		for( int port = 0; port < m_portCount; ++port )
		{
			port_desc_t * pp = m_ports.at( proc ).at( port );
			switch( pp->rate )
			{
				case CHANNEL_IN:
					for( fpp_t frame = 0; 
						frame < frames; ++frame )
					{
						pp->buffer[frame] = 
							_buf[frame][channel];
					}
					++channel;
					break;
				case AUDIO_RATE_INPUT:
					pp->value = static_cast<LADSPA_Data>( 
										pp->control->value() / pp->scale );
					// This only supports control rate ports, so the audio rates are
					// treated as though they were control rate by setting the
					// port buffer to all the same value.
					for( fpp_t frame = 0; 
						frame < frames; ++frame )
					{
						pp->buffer[frame] = 
							pp->value;
					}
					break;
				case CONTROL_RATE_INPUT:
					if( pp->control == NULL )
					{
						break;
					}
					pp->value = static_cast<LADSPA_Data>( 
										pp->control->value() / pp->scale );
					pp->buffer[0] = 
						pp->value;
					break;
				case CHANNEL_OUT:
				case AUDIO_RATE_OUTPUT:
				case CONTROL_RATE_OUTPUT:
					break;
				default:
					break;
			}
		}
	}

	// Process the buffers.
	for( ch_cnt_t proc = 0; proc < processorCount(); ++proc )
	{
		(m_descriptor->run)( m_handles[proc], frames );
	}

	// Copy the LADSPA output buffers to the LMMS buffer.
	double out_sum = 0.0;
	channel = 0;
	float * buffers[2];

	for( ch_cnt_t proc = 0; proc < processorCount(); ++proc )
	{
		for( int port = 0; port < m_portCount; ++port )
		{
			port_desc_t * pp = m_ports.at( proc ).at( port );
			switch( pp->rate )
			{
				case CHANNEL_IN:
				case AUDIO_RATE_INPUT:
				case CONTROL_RATE_INPUT:
					break;
				case CHANNEL_OUT:
					if( channel < DEFAULT_CHANNELS )
					{
						buffers[channel] = pp->buffer;
					}
					++channel;
					break;
				case AUDIO_RATE_OUTPUT:
				case CONTROL_RATE_OUTPUT:
					break;
				default:
					break;
			}
		}
	}

	if( channel == 1 )
	{
		buffers[1] = buffers[0];
	}
	if( channel >= 1 && channel <= DEFAULT_CHANNELS )
	{
		CPU::bufWetDryMixSplitted( _buf, buffers[0], buffers[1],
					wetLevel(), dryLevel(), frames );
	}

	for( int i = 0; i < frames; ++i )
	{
		out_sum += _buf[i][0]*_buf[i][0];
		out_sum += _buf[i][1]*_buf[i][1];

	}

	if( o_buf != NULL )
	{
		sampleBack( _buf, o_buf, m_maxSampleRate );
		CPU::freeFrames( _buf );
	}

	checkGate( out_sum / frames );


	bool is_running = isRunning();
	m_pluginMutex.unlock();
	return is_running;
}




void LadspaEffect::setControl( int _control, LADSPA_Data _value )
{
	if( !isOkay() )
	{
		return;
	}
	m_portControls[_control]->value = _value;
}




void LadspaEffect::pluginInstantiation()
{
	m_maxSampleRate = maxSamplerate( displayName() );

	ladspa2LMMS * manager = engine::getLADSPAManager();

	// Calculate how many processing units are needed.
	const ch_cnt_t lmms_chnls = engine::mixer()->audioOutputContext()->
													audioBackend()->channels();
	int effect_channels = manager->getDescription( m_key )->inputChannels;
	setProcessorCount( lmms_chnls / effect_channels );

	// Categorize the ports, and create the buffers.
	m_portCount = manager->getPortCount( m_key );

	for( ch_cnt_t proc = 0; proc < processorCount(); proc++ )
	{
		multi_proc_t ports;
		for( int port = 0; port < m_portCount; port++ )
		{
			port_desc_t * p = new PortDescription;

			p->name = manager->getPortName( m_key, port );
			p->proc = proc;
			p->port_id = port;
			p->control = NULL;

			// Determine the port's category.
			if( manager->isPortAudio( m_key, port ) )
			{
		// Nasty manual memory management--was having difficulty
		// with some prepackaged plugins that were segfaulting
		// during cleanup.  It was easier to troubleshoot with the
		// memory management all taking place in one file.
				p->buffer = 
		new LADSPA_Data[engine::mixer()->framesPerPeriod()];

				if( p->name.toUpper().contains( "IN" ) &&
					manager->isPortInput( m_key, port ) )
				{
					p->rate = CHANNEL_IN;
				}
				else if( p->name.toUpper().contains( "OUT" ) &&
					manager->isPortOutput( m_key, port ) )
				{
					p->rate = CHANNEL_OUT;
				}
				else if( manager->isPortInput( m_key, port ) )
				{
					p->rate = AUDIO_RATE_INPUT;
				}
				else
				{
					p->rate = AUDIO_RATE_OUTPUT;
				}
			}
			else
			{
				p->buffer = new LADSPA_Data[1];

				if( manager->isPortInput( m_key, port ) )
				{
					p->rate = CONTROL_RATE_INPUT;
				}
				else
				{
					p->rate = CONTROL_RATE_OUTPUT;
				}
			}

			p->scale = 1.0f;
			if( manager->isPortToggled( m_key, port ) )
			{
				p->data_type = TOGGLED;
			}
			else if( manager->isInteger( m_key, port ) )
			{
				p->data_type = INTEGER;
			}
			else if( p->name.toUpper().contains( "(SECONDS)" ) )
			{
				p->data_type = TIME;
				p->scale = 1000.0f;
				int loc = p->name.toUpper().indexOf(
								"(SECONDS)" );
				p->name.replace( loc, 9, "(ms)" );
			}
			else if( p->name.toUpper().contains( "(S)" ) )
			{
				p->data_type = TIME;
				p->scale = 1000.0f;
				int loc = p->name.toUpper().indexOf( "(S)" );
				p->name.replace( loc, 3, "(ms)" );
			}
			else if( p->name.toUpper().contains( "(MS)" ) )
			{
				p->data_type = TIME;
				int loc = p->name.toUpper().indexOf( "(MS)" );
				p->name.replace( loc, 4, "(ms)" );
			}
			else
			{
				p->data_type = FLOATING;
			}

			// Get the range and default values.
			p->max = manager->getUpperBound( m_key, port );
			if( p->max == NOHINT )
			{
				p->max = p->name.toUpper() == "GAIN" ? 10.0f :
					1.0f;
			}

			if( manager->areHintsSampleRateDependent(
								m_key, port ) )
			{
				p->max *= m_maxSampleRate;
			}

			p->min = manager->getLowerBound( m_key, port );
			if( p->min == NOHINT )
			{
				p->min = 0.0f;
			}

			if( manager->areHintsSampleRateDependent(
								m_key, port ) )
			{
				p->min *= m_maxSampleRate;
			}

			p->def = manager->getDefaultSetting( m_key, port );
			if( p->def == NOHINT )
			{
				if( p->data_type != TOGGLED )
				{
					p->def = ( p->min + p->max ) / 2.0f;
				}
				else
				{
					p->def = 1.0f;
				}
			}

			p->max *= p->scale;
			p->min *= p->scale;
			p->def *= p->scale;

			p->value = p->def;


			ports.append( p );

	// For convenience, keep a separate list of the ports that are used 
	// to control the processors.
			if( p->rate == AUDIO_RATE_INPUT || 
					p->rate == CONTROL_RATE_INPUT )
			{
				p->control_id = m_portControls.count();
				m_portControls.append( p );
			}
		}
		m_ports.append( ports );
	}

	// Instantiate the processing units.
	m_descriptor = manager->getDescriptor( m_key );
	if( m_descriptor == NULL )
	{
		QMessageBox::warning( 0, "Effect", 
			"Can't get LADSPA descriptor function: " + m_key.second,
			QMessageBox::Ok, QMessageBox::NoButton );
		setOkay( false );
		return;
	}
	if( m_descriptor->run == NULL )
	{
		QMessageBox::warning( 0, "Effect",
			"Plugin has no processor: " + m_key.second,
			QMessageBox::Ok, QMessageBox::NoButton );
		setDontRun( true );
	}
	for( ch_cnt_t proc = 0; proc < processorCount(); proc++ )
	{
		LADSPA_Handle effect = manager->instantiate( m_key,
							m_maxSampleRate );
		if( effect == NULL )
		{
			QMessageBox::warning( 0, "Effect",
				"Can't get LADSPA instance: " + m_key.second,
				QMessageBox::Ok, QMessageBox::NoButton );
			setOkay( false );
			return;
		}
		m_handles.append( effect );
	}

	// Connect the ports.
	for( ch_cnt_t proc = 0; proc < processorCount(); proc++ )
	{
		for( int port = 0; port < m_portCount; port++ )
		{
			port_desc_t * pp = m_ports.at( proc ).at( port );
			if( !manager->connectPort( m_key,
						m_handles[proc],
						port,
						pp->buffer ) )
			{
				QMessageBox::warning( 0, "Effect", 
				"Failed to connect port: " + m_key.second, 
				QMessageBox::Ok, QMessageBox::NoButton );
				setDontRun( true );
				return;
			}
		}
	}

	// Activate the processing units.
	for( ch_cnt_t proc = 0; proc < processorCount(); proc++ )
	{
		manager->activate( m_key, m_handles[proc] );
	}
	m_controls = new LadspaControls( this );
}




void LadspaEffect::pluginDestruction()
{
	if( !isOkay() )
	{
		return;
	}

	delete m_controls;

	for( ch_cnt_t proc = 0; proc < processorCount(); proc++ )
	{
		ladspa2LMMS * manager = engine::getLADSPAManager();
		manager->deactivate( m_key, m_handles[proc] );
		manager->cleanup( m_key, m_handles[proc] );
		for( int port = 0; port < m_portCount; port++ )
		{
			port_desc_t * pp = m_ports.at( proc ).at( port );
			delete[] pp->buffer;
			delete pp;
		}
		m_ports[proc].clear();
	}
	m_ports.clear();
	m_handles.clear();
	m_portControls.clear();
}






static QMap<QString, sample_rate_t> __buggy_plugins;

sample_rate_t LadspaEffect::maxSamplerate( const QString & _name )
{
	if( __buggy_plugins.isEmpty() )
	{
		__buggy_plugins["C* AmpVTS"] = 88200;
		__buggy_plugins["Chorus2"] = 44100;
		__buggy_plugins["Notch Filter"] = 96000;
		__buggy_plugins["Freeverb"] = 44100;
		__buggy_plugins["TAP Reflector"] = 192000;
	}
	if( __buggy_plugins.contains( _name ) )
	{
		return __buggy_plugins[_name];
	}
	return engine::mixer()->processingSampleRate();
}




extern "C"
{

// necessary for getting instance out of shared lib
Plugin * PLUGIN_EXPORT lmms_plugin_main( Model * _parent, void * _data )
{
	return new LadspaEffect( _parent,
		static_cast<const Plugin::Descriptor::SubPluginFeatures::Key *>(
								_data ) );
}

}


#include "moc_LadspaEffect.cxx"

