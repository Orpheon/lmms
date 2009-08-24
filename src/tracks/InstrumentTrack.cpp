/*
 * InstrumentTrack.cpp - implementation of instrument-track-class
 *                        (window + data-structures)
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

#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QQueue>
#include <QtGui/QApplication>
#include <QtGui/QCloseEvent>
#include <QtGui/QFileDialog>
#include <QtGui/QLabel>
#include <QtGui/QLayout>
#include <QtGui/QLineEdit>
#include <QtGui/QMdiArea>
#include <QtGui/QMenu>
#include <QtGui/QMessageBox>
#include <QtGui/QMdiSubWindow>
#include <QtGui/QPainter>

#include "ResourceAction.h"
#include "ResourceDB.h"

#include "InstrumentTrack.h"
#include "AudioPort.h"
#include "automation_pattern.h"
#include "bb_track.h"
#include "config_mgr.h"
#include "debug.h"
#include "EffectChain.h"
#include "EffectRackView.h"
#include "embed.h"
#include "engine.h"
#include "FxMixerView.h"
#include "InstrumentSoundShaping.h"
#include "InstrumentSoundShapingView.h"
#include "fade_button.h"
#include "gui_templates.h"
#include "Instrument.h"
#include "InstrumentFunctionViews.h"
#include "InstrumentMidiIOView.h"
#include "knob.h"
#include "lcd_spinbox.h"
#include "led_checkbox.h"
#include "MainWindow.h"
#include "MidiClient.h"
#include "MidiPortMenu.h"
#include "mmp.h"
#include "note_play_handle.h"
#include "pattern.h"
#include "PluginView.h"
#include "sample_play_handle.h"
#include "song.h"
#include "string_pair_drag.h"
#include "surround_area.h"
#include "tab_widget.h"
#include "tooltip.h"
#include "track_label_button.h"



const char * volume_help = QT_TRANSLATE_NOOP( "instrumentTrack",
						"With this knob you can set "
						"the volume of the opened "
						"channel.");

const int INSTRUMENT_WIDTH	= 254;
const int INSTRUMENT_HEIGHT	= INSTRUMENT_WIDTH;
const int INSTRUMENT_WINDOW_CACHE_SIZE = 8;


// #### IT:
InstrumentTrack::InstrumentTrack( trackContainer * _tc ) :
	track( track::InstrumentTrack, _tc ),
	MidiEventProcessor(),
	m_audioPort( tr( "unnamed_track" ) ),
	m_midiPort( tr( "unnamed_track" ), engine::getMixer()->midiClient(),
								this, this ),
	m_notes(),
        m_volumeModel( DefaultVolume, MinVolume, MaxVolume, 0.1f, this,
							tr( "Volume" ) ),
        m_panningModel( DefaultPanning, PanningLeft, PanningRight, 0.1f,
							this, tr( "Panning" ) ),
	m_pitchModel( 0, -100, 100, 1, this, tr( "Pitch" ) ),
	m_pitchRangeModel( 1, 1, 24, this, tr( "Pitch range" ) ),
	m_effectChannelModel( 0, 0, NumFxChannels, this, tr( "FX channel" ) ),
	m_instrument( NULL ),
	m_soundShaping( this ),
	m_arpeggiator( this ),
	m_chordCreator( this ),
	m_piano( this )
{
	connect( baseNoteModel(), SIGNAL( dataChanged() ),
			this, SLOT( updateBaseNote() ) );
	connect( &m_pitchModel, SIGNAL( dataChanged() ),
			this, SLOT( updatePitch() ) );
	connect( &m_pitchRangeModel, SIGNAL( dataChanged() ),
			this, SLOT( updatePitchRange() ) );


	for( int i = 0; i < NumKeys; ++i )
	{
		m_notes[i] = NULL;
		m_runningMidiNotes[i] = 0;
	}


	setName( tr( "Default preset" ) );

}




InstrumentTrack::~InstrumentTrack()
{
	// kill all running notes
	silenceAllNotes();

	engine::getMixer()->removePlayHandles( this );

	// now we're save deleting the instrument
	delete m_instrument;
}




void InstrumentTrack::processAudioBuffer( sampleFrame * _buf,
							const fpp_t _frames,
							notePlayHandle * _n )
{
	// we must not play the sound if this instrumentTrack is muted...
	if( isMuted() || ( _n && _n->bbTrackMuted() ) )
	{
		return;
	}

	// if effects "went to sleep" because there was no input, wake them up
	// now
	m_audioPort.effects()->startRunning();

	float v_scale = (float) getVolume() / DefaultVolume;

	// instruments using instrument-play-handles will call this method
	// without any knowledge about notes, so they pass NULL for _n, which
	// is no problem for us since we just bypass the envelopes+LFOs
	if( _n != NULL )
	{
		m_soundShaping.processAudioBuffer( _buf, _frames, _n );
		v_scale *= ( (float) _n->getVolume() / DefaultVolume );
	}
	else
	{
		if( getVolume() < DefaultVolume &&
					m_instrument->isMidiBased() )
		{
			v_scale = 1;
		}
	}

	m_audioPort.setNextFxChannel( m_effectChannelModel.value() );
	
	int panning = m_panningModel.value();
	if( _n != NULL )
	{
		panning += _n->getPanning();
		panning = tLimit<int>( panning, PanningLeft, PanningRight );
	}
	engine::getMixer()->bufferToPort( _buf, ( _n != NULL ) ?
		qMin<f_cnt_t>(_n->framesLeftForCurrentPeriod(), _frames ) :
								_frames,
			( _n != NULL ) ? _n->offset() : 0,
			panningToVolumeVector( panning,	v_scale ),
								&m_audioPort );
}




midiEvent InstrumentTrack::applyMasterKey( const midiEvent & _me )
{
	midiEvent copy( _me );
	switch( _me.m_type )
	{
		case MidiNoteOn:
		case MidiNoteOff:
		case MidiKeyPressure:
			copy.key() = masterKey( _me.key() );
			break;
		default:
			break;
	}
	return copy;
}




void InstrumentTrack::processInEvent( const midiEvent & _me,
							const midiTime & _time )
{
	engine::getMixer()->lock();
	switch( _me.m_type )
	{
		// we don't send MidiNoteOn, MidiNoteOff and MidiKeyPressure
		// events to instrument as notePlayHandle will send them on its
		// own
		case MidiNoteOn: 
			if( _me.velocity() > 0 )
			{
				if( m_notes[_me.key()] == NULL )
				{
					if( !configManager::inst()->value( "ui",
						"manualchannelpiano" ).toInt() )
					{
						m_piano.setKeyState(
							_me.key(), true );
					}
					// create temporary note
					note n;
					n.setKey( _me.key() );
					n.setVolume( _me.getVolume() );

					// create (timed) note-play-handle
					notePlayHandle * nph = new
						notePlayHandle( this,
							_time.frames(
						engine::framesPerTick() ),
						typeInfo<f_cnt_t>::max() / 2,
									n );
					if( engine::getMixer()->addPlayHandle(
									nph ) )
					{
						m_notes[_me.key()] = nph;
					}

					emit noteOn( n );
				}
				break;
			}

		case MidiNoteOff:
		{
			notePlayHandle * n = m_notes[_me.key()];
			if( n != NULL )
			{
				// create dummy-note which has the same length
				// as the played note for sending it later
				// to all slots connected to signal noteOff()
				// this is for example needed by piano-roll for
				// recording notes into a pattern
				note done_note(
					midiTime( static_cast<f_cnt_t>(
						n->totalFramesPlayed() /
						engine::framesPerTick() ) ),
							0,
							n->key(),
							n->getVolume(),
							n->getPanning() );

				n->noteOff();
				m_notes[_me.key()] = NULL;

				emit noteOff( done_note );
			}
			break;
		}

		case MidiKeyPressure:
			if( m_notes[_me.key()] != NULL )
			{
				m_notes[_me.key()]->setVolume( _me.getVolume() );
			}
			break;

		case MidiPitchBend:
			// updatePitch() is connected to
			// m_pitchModel::dataChanged() which will send out
			// MidiPitchBend events
			m_pitchModel.setValue( m_pitchModel.minValue() +
					_me.m_data.m_param[0] *
						m_pitchModel.range() / 16384 );
			break;

		case MidiControlChange:
		case MidiProgramChange:
			m_instrument->handleMidiEvent( _me, _time );
			break;
		
		case MidiMetaEvent:
			// handle special cases such as note panning
			switch( _me.m_metaEvent )
			{
				case MidiNotePanning:
					if( m_notes[_me.key()] != NULL )
					{
		m_notes[_me.key()]->setPanning( _me.getPanning() );
					}
					break;
				default:
					printf( "instrument-track: unhandled "
						    "MIDI meta event: %i\n",
							_me.m_metaEvent );
					break;
			}
			break;
			
		default:
			if( !m_instrument->handleMidiEvent( _me, _time ) )
			{
				printf( "instrument-track: unhandled "
					"MIDI event %d\n", _me.m_type );
			}
			break;
	}
	engine::getMixer()->unlock();
}




void InstrumentTrack::processOutEvent( const midiEvent & _me,
							const midiTime & _time )
{
	int k;

	switch( _me.m_type )
	{
		case MidiNoteOn:
			if( !configManager::inst()->value( "ui",
						"manualchannelpiano" ).toInt() )
			{
				m_piano.setKeyState( _me.key(), true );
			}
			if( !configManager::inst()->value( "ui",
				"disablechannelactivityindicators" ).toInt() )
			{
				if( m_notes[_me.key()] == NULL )
				{
					emit newNote();
				}
			}
			k = masterKey( _me.key() );
			if( k >= 0 && k < NumKeys )
			{
				if( m_runningMidiNotes[k] > 0 )
				{
					m_instrument->handleMidiEvent(
	midiEvent( MidiNoteOff, midiPort()->realOutputChannel(), k, 0 ),
									_time );
				}
				++m_runningMidiNotes[k];
				m_instrument->handleMidiEvent(
	midiEvent( MidiNoteOn, midiPort()->realOutputChannel(), k,
						_me.velocity() ), _time );
			}
			break;

		case MidiNoteOff:
			if( !configManager::inst()->value( "ui",
						"manualchannelpiano" ).toInt() )
			{
				m_piano.setKeyState( _me.key(), false );
			}
			k = masterKey( _me.key() );
			if( k >= 0 && k < NumKeys &&
						--m_runningMidiNotes[k] <= 0 )
			{
				m_instrument->handleMidiEvent(
	midiEvent( MidiNoteOff, midiPort()->realOutputChannel(), k, 0 ),
									_time );
			}
			break;

		default:
			if( m_instrument != NULL )
			{
				m_instrument->handleMidiEvent(
							applyMasterKey( _me ),
									_time );
			}
			break;
	}

	// if appropriate, midi-port does futher routing
	m_midiPort.processOutEvent( _me, _time );
}




void InstrumentTrack::silenceAllNotes()
{
	engine::getMixer()->lock();
	for( int i = 0; i < NumKeys; ++i )
	{
		m_notes[i] = NULL;
		m_runningMidiNotes[i] = 0;
	}

	// invalidate all NotePlayHandles linked to this track
	m_processHandles.clear();
	engine::getMixer()->removePlayHandles( this,
						playHandle::NotePlayHandle );
	engine::getMixer()->unlock();
}




f_cnt_t InstrumentTrack::beatLen( notePlayHandle * _n ) const
{
	if( m_instrument != NULL )
	{
		const f_cnt_t len = m_instrument->beatLen( _n );
		if( len > 0 )
		{
			return len;
		}
	}
	return m_soundShaping.envFrames();
}




void InstrumentTrack::playNote( notePlayHandle * _n, 
						sampleFrame * _working_buffer )
{
	// arpeggio- and chord-widget has to do its work -> adding sub-notes
	// for chords/arpeggios
	m_chordCreator.processNote( _n );
	m_arpeggiator.processNote( _n );

	if( !_n->isArpeggioBaseNote() && m_instrument != NULL )
	{
		// all is done, so now lets play the note!
		m_instrument->playNote( _n, _working_buffer );
	}
}




QString InstrumentTrack::instrumentName() const
{
	if( m_instrument != NULL )
	{
		return m_instrument->displayName();
	}
	return QString::null;
}




void InstrumentTrack::deleteNotePluginData( notePlayHandle * _n )
{
	if( m_instrument != NULL )
	{
		m_instrument->deleteNotePluginData( _n );
	}

	// Notes deleted when keys still pressed
	if( m_notes[_n->key()] == _n )
	{
		note done_note( midiTime( static_cast<f_cnt_t>(
						_n->totalFramesPlayed() /
						engine::framesPerTick() ) ),
					0, _n->key(),
					_n->getVolume(), _n->getPanning() );
		_n->noteOff();
		m_notes[_n->key()] = NULL;
		emit noteOff( done_note );
	}
}




void InstrumentTrack::setName( const QString & _new_name )
{
	// when changing name of track, also change name of those patterns,
	// which have the same name as the instrument-track
	for( int i = 0; i < numOfTCOs(); ++i )
	{
		pattern * p = dynamic_cast<pattern *>( getTCO( i ) );
		if( ( p != NULL && p->name() == name() ) || p->name() == "" )
		{
			p->setName( _new_name );
		}
	}

	track::setName( _new_name );
	m_midiPort.setName( name() );
	m_audioPort.setName( name() );

	emit nameChanged();
}






void InstrumentTrack::updateBaseNote()
{
	engine::getMixer()->lock();
	for( NotePlayHandleList::Iterator it = m_processHandles.begin();
					it != m_processHandles.end(); ++it )
	{
		( *it )->updateFrequency();
	}
	engine::getMixer()->unlock();
}




void InstrumentTrack::updatePitch()
{
	updateBaseNote();
	processOutEvent( midiEvent( MidiPitchBend,
					midiPort()->realOutputChannel(),
					midiPitch() ), 0 );
}




void InstrumentTrack::updatePitchRange()
{
	const int r = m_pitchRangeModel.value();
	m_pitchModel.setRange( -100 * r, 100 * r );
}




int InstrumentTrack::masterKey( int _midi_key ) const
{
	int key = baseNoteModel()->value() - engine::getSong()->masterPitch();
	return tLimit<int>( _midi_key - ( key - DefaultKey ), 0, NumKeys );
}




void InstrumentTrack::removeMidiPortNode( multimediaProject & _mmp )
{
	QDomNodeList n = _mmp.elementsByTagName( "midiport" );
	n.item( 0 ).parentNode().removeChild( n.item( 0 ) );
}




bool InstrumentTrack::play( const midiTime & _start,
					const fpp_t _frames,
					const f_cnt_t _offset,
							Sint16 _tco_num )
{
	const float frames_per_tick = engine::framesPerTick();

	tcoVector tcos;
	bbTrack * bb_track = NULL;
	if( _tco_num >= 0 )
	{
		trackContentObject * tco = getTCO( _tco_num );
		tcos.push_back( tco );
		bb_track = bbTrack::findBBTrack( _tco_num );
	}
	else
	{
		getTCOsInRange( tcos, _start, _start + static_cast<int>(
					_frames / frames_per_tick ) );
	}

	// Handle automation: detuning
	for( NotePlayHandleList::Iterator it = m_processHandles.begin();
					it != m_processHandles.end(); ++it )
	{
		( *it )->processMidiTime( _start );
	}

	if ( tcos.size() == 0 )
	{
		return false;
	}

	bool played_a_note = false;	// will be return variable

	for( tcoVector::Iterator it = tcos.begin(); it != tcos.end(); ++it )
	{
		pattern * p = dynamic_cast<pattern *>( *it );
		// everything which is not a pattern or muted won't be played
		if( p == NULL || ( *it )->isMuted() )
		{
			continue;
		}
		midiTime cur_start = _start;
        // Make cur_start the offset within the pattern
		if( _tco_num < 0 )
		{
			cur_start -= p->startPosition();
		}
		if( p->frozen() && !engine::getSong()->isExporting() )
		{
			if( cur_start > 0 )
			{
				continue;
			}

			samplePlayHandle * handle = new samplePlayHandle( p );
			handle->setBBTrack( bb_track );
			handle->setOffset( _offset );
			// send it to the mixer
			engine::getMixer()->addPlayHandle( handle );
			played_a_note = true;
			continue;
		}

		// get all notes from the given pattern...
		const NoteVector & notes = p->notes();
		// ...and set our index to zero
		NoteVector::ConstIterator it = notes.begin();
#if LMMS_SINGERBOT_SUPPORT
		int note_idx = 0;
#endif

		// very effective algorithm for playing notes that are
		// positioned within the current sample-frame


		if( cur_start > 0 )
		{
			// skip notes which are posated before start-tact
			while( it != notes.end() && ( *it )->pos() < cur_start )
			{
#if LMMS_SINGERBOT_SUPPORT
				if( ( *it )->length() != 0 )
				{
					++note_idx;
				}
#endif
				++it;
			}
		}

		note * cur_note;
		while( it != notes.end() &&
					( cur_note = *it )->pos() == cur_start )
		{
			if( cur_note->length() != 0 )
			{
				const f_cnt_t note_frames =
					cur_note->length().frames(
							frames_per_tick );

				notePlayHandle * note_play_handle =
					new notePlayHandle( this, _offset,
								note_frames,
								*cur_note );
				note_play_handle->setBBTrack( bb_track );
#if LMMS_SINGERBOT_SUPPORT
				note_play_handle->setPatternIndex( note_idx );
#endif
				engine::getMixer()->addPlayHandle(
							note_play_handle );
				played_a_note = true;
#if LMMS_SINGERBOT_SUPPORT
				++note_idx;
#endif
			}
			++it;
		}
	}
	return played_a_note;
}




trackContentObject * InstrumentTrack::createTCO( const midiTime & )
{
	return new pattern( this );
}




trackView * InstrumentTrack::createView( trackContainerView * _tcv )
{
	return new InstrumentTrackView( this, _tcv );
}




void InstrumentTrack::saveTrackSpecificSettings( QDomDocument & _doc,
							QDomElement & _this )
{
	m_volumeModel.saveSettings( _doc, _this, "vol" );
	m_panningModel.saveSettings( _doc, _this, "pan" );
	m_pitchModel.saveSettings( _doc, _this, "pitch" );
	m_pitchRangeModel.saveSettings( _doc, _this, "pitchrange" );

	m_effectChannelModel.saveSettings( _doc, _this, "fxch" );
	baseNoteModel()->saveSettings( _doc, _this, "basenote" );

	if( m_instrument != NULL )
	{
		QDomElement i = _doc.createElement( "instrument" );
		i.setAttribute( "name", m_instrument->descriptor()->name );
		m_instrument->saveState( _doc, i );
		_this.appendChild( i );
	}
	m_soundShaping.saveState( _doc, _this );
	m_chordCreator.saveState( _doc, _this );
	m_arpeggiator.saveState( _doc, _this );
	m_midiPort.saveState( _doc, _this );
	m_audioPort.effects()->saveState( _doc, _this );
}




void InstrumentTrack::loadTrackSpecificSettings( const QDomElement & _this )
{
	silenceAllNotes();

	engine::getMixer()->lock();

	m_volumeModel.loadSettings( _this, "vol" );

	// compat-hacks - move to mmp::upgrade
	if( _this.hasAttribute( "surpos" ) || _this.hasAttribute( "surpos-x" )
		|| !_this.firstChildElement( "automationpattern" ).
				firstChildElement( "surpos-x" ).isNull() )
	{
		surroundAreaModel m( this, this );
		m.loadSettings( _this, "surpos" );
		m_panningModel.setValue( m.x() * 100 / SURROUND_AREA_SIZE );
	}
	else
	{
		m_panningModel.loadSettings( _this, "pan" );
	}

	m_pitchRangeModel.loadSettings( _this, "pitchrange" );
	m_pitchModel.loadSettings( _this, "pitch" );
	m_effectChannelModel.loadSettings( _this, "fxch" );

	if( _this.hasAttribute( "baseoct" ) )
	{
		// TODO: move this compat code to mmp.cpp -> upgrade()
		baseNoteModel()->setInitValue( _this.
			attribute( "baseoct" ).toInt()
				* KeysPerOctave
				+ _this.attribute( "basetone" ).toInt() );
	}
	else
	{
		baseNoteModel()->loadSettings( _this, "basenote" );
	}

	// clear effect-chain just in case we load an old preset without FX-data
	m_audioPort.effects()->clear();

	QDomNode node = _this.firstChild();
	while( !node.isNull() )
	{
		if( node.isElement() )
		{
			if( m_soundShaping.nodeName() == node.nodeName() )
			{
				m_soundShaping.restoreState( node.toElement() );
			}
			else if( m_chordCreator.nodeName() == node.nodeName() )
			{
				m_chordCreator.restoreState( node.toElement() );
			}
			else if( m_arpeggiator.nodeName() == node.nodeName() )
			{
				m_arpeggiator.restoreState( node.toElement() );
			}
			else if( m_midiPort.nodeName() == node.nodeName() )
			{
				m_midiPort.restoreState( node.toElement() );
			}
			else if( m_audioPort.effects()->nodeName() == node.nodeName() )
			{
				m_audioPort.effects()->restoreState( node.toElement() );
			}
			else if( node.nodeName() == "instrument" )
			{
				delete m_instrument;
				m_instrument = NULL;
				m_instrument = Instrument::instantiate(
					node.toElement().attribute( "name" ),
									this );
				m_instrument->restoreState(
						node.firstChildElement() );
				emit instrumentChanged();
			}
			// compat code - if node-name doesn't match any known
			// one, we assume that it is an instrument-plugin
			// which we'll try to load
			else if( node.nodeName() != "connection" &&
					automationPattern::classNodeName() !=
							node.nodeName() &&
					!node.toElement().hasAttribute( "id" ) )
			{
				delete m_instrument;
				m_instrument = NULL;
				m_instrument = Instrument::instantiate(
							node.nodeName(), this );
				if( m_instrument->nodeName() ==
							node.nodeName() )
				{
					m_instrument->restoreState(
							node.toElement() );
				}
				emit instrumentChanged();
			}
		}
		node = node.nextSibling();
        }
	engine::getMixer()->unlock();
}




Instrument * InstrumentTrack::loadInstrument( const QString & _plugin_name )
{
	silenceAllNotes();

	engine::getMixer()->lock();
	delete m_instrument;
	m_instrument = Instrument::instantiate( _plugin_name, this );
	engine::getMixer()->unlock();

	emit instrumentChanged();

	return m_instrument;
}





// #### ITV:


QQueue<InstrumentTrackWindow *> InstrumentTrackView::s_windows;



InstrumentTrackView::InstrumentTrackView( InstrumentTrack * _it,
						trackContainerView * _tcv ) :
	trackView( _it, _tcv ),
	m_window( NULL ),
	m_lastPos( -1, -1 )
{
	setAcceptDrops( true );
	setFixedHeight( 32 );

	m_tlb = new trackLabelButton( this, getTrackSettingsWidget() );
	m_tlb->setCheckable( true );
	m_tlb->setIcon( embed::getIconPixmap( "instrument_track" ) );
	m_tlb->move( 3, 1 );
	m_tlb->show();

	connect( m_tlb, SIGNAL( toggled( bool ) ),
			this, SLOT( toggleInstrumentWindow( bool ) ) );

	connect( _it, SIGNAL( nameChanged() ),
			m_tlb, SLOT( update() ) );

	// creation of widgets for track-settings-widget
	m_volumeKnob = new knob( knobSmall_17, getTrackSettingsWidget(),
							tr( "Volume" ) );
	m_volumeKnob->setVolumeKnob( true );
	m_volumeKnob->setModel( &_it->m_volumeModel );
	m_volumeKnob->setHintText( tr( "Volume:" ) + " ", "%" );
	m_volumeKnob->move( DEFAULT_SETTINGS_WIDGET_WIDTH-24*2, 4 );
	m_volumeKnob->setLabel( tr( "VOL" ) );
	m_volumeKnob->show();
	m_volumeKnob->setWhatsThis( tr( volume_help ) );

	m_panningKnob = new knob( knobSmall_17, getTrackSettingsWidget(),
							tr( "Panning" ) );
	m_panningKnob->setModel( &_it->m_panningModel );
	m_panningKnob->setHintText( tr( "Panning:" ) + " ", "%" );
	m_panningKnob->move( DEFAULT_SETTINGS_WIDGET_WIDTH-24, 4 );
	m_panningKnob->setLabel( tr( "PAN" ) );
	m_panningKnob->show();


	m_midiMenu = new QMenu( tr( "MIDI" ), this );

	// sequenced MIDI?
	if( !engine::getMixer()->midiClient()->isRaw() )
	{
		_it->m_midiPort.m_readablePortsMenu = new MidiPortMenu(
							MidiPort::Input );
		_it->m_midiPort.m_writablePortsMenu = new MidiPortMenu(
							MidiPort::Output );
		_it->m_midiPort.m_readablePortsMenu->setModel(
							&_it->m_midiPort );
		_it->m_midiPort.m_writablePortsMenu->setModel(
							&_it->m_midiPort );
		m_midiInputAction = m_midiMenu->addMenu(
					_it->m_midiPort.m_readablePortsMenu );
		m_midiOutputAction = m_midiMenu->addMenu(
					_it->m_midiPort.m_writablePortsMenu );
	}
	else
	{
		m_midiInputAction = m_midiMenu->addAction( "" );
		m_midiOutputAction = m_midiMenu->addAction( "" );
		m_midiInputAction->setCheckable( true );
		m_midiOutputAction->setCheckable( true );
		connect( m_midiInputAction, SIGNAL( changed() ), this,
						SLOT( midiInSelected() ) );
		connect( m_midiOutputAction, SIGNAL( changed() ), this,
					SLOT( midiOutSelected() ) );
		connect( &_it->m_midiPort, SIGNAL( modeChanged() ),
				this, SLOT( midiConfigChanged() ) );
	}

	m_midiInputAction->setText( tr( "Input" ) );
	m_midiOutputAction->setText( tr( "Output" ) );

	m_activityIndicator = new fadeButton( QColor( 56, 60, 72 ),
						QColor( 64, 255, 16 ),
						getTrackSettingsWidget() );
	m_activityIndicator->setGeometry(
			DEFAULT_SETTINGS_WIDGET_WIDTH-2*24-11, 2, 8, 28 );
	m_activityIndicator->show();
	connect( m_activityIndicator, SIGNAL( pressed() ),
				this, SLOT( activityIndicatorPressed() ) );
	connect( m_activityIndicator, SIGNAL( released() ),
				this, SLOT( activityIndicatorReleased() ) );
	connect( _it, SIGNAL( newNote() ),
				m_activityIndicator, SLOT( activate() ) );


	setModel( _it );
}




InstrumentTrackView::~InstrumentTrackView()
{
	freeInstrumentTrackWindow();

	delete model()->m_midiPort.m_readablePortsMenu;
	delete model()->m_midiPort.m_writablePortsMenu;
}




// TODO: Add windows to free list on freeInstrumentTrackWindow. 
// But, don't NULL m_window or disconnect signals.  This will allow windows 
// that are being show/hidden frequently to stay connected.
void InstrumentTrackView::freeInstrumentTrackWindow()
{
	if( m_window != NULL )
	{
		m_lastPos = m_window->parentWidget()->pos();
		if( s_windows.count() < INSTRUMENT_WINDOW_CACHE_SIZE )
		{
			model()->setHook( NULL );
			m_window->parentWidget()->hide();
			m_window->setModel(
				engine::dummyTrackContainer()->
						dummyInstrumentTrack() );
			m_window->updateInstrumentView();
			s_windows.enqueue( m_window );
		}
		else
		{
			delete m_window;
		}
		
		m_window = NULL;
	}
}




void InstrumentTrackView::cleanupWindowPool()
{
	while( s_windows.count() )
	{
		delete s_windows.dequeue();
	}
}




InstrumentTrackWindow * InstrumentTrackView::getInstrumentTrackWindow()
{
	if( m_window != NULL )
	{
	}
	else if( !s_windows.isEmpty() )
	{
		m_window = s_windows.dequeue();
		
		m_window->setInstrumentTrackView( this );
		m_window->setModel( model() );
		m_window->updateInstrumentView();
		model()->setHook( m_window );

		if( m_lastPos.x() > 0 || m_lastPos.y() > 0 )
		{
			m_window->parentWidget()->move( m_lastPos );
		}
	}
	else
	{
		m_window = new InstrumentTrackWindow( this );
	}
		
	return m_window;
}




void InstrumentTrackView::dragEnterEvent( QDragEnterEvent * _dee )
{
	InstrumentTrackWindow::dragEnterEventGeneric( _dee );
	if( !_dee->isAccepted() )
	{
		trackView::dragEnterEvent( _dee );
	}
}




void InstrumentTrackView::dropEvent( QDropEvent * _de )
{
	getInstrumentTrackWindow()->dropEvent( _de );
	trackView::dropEvent( _de );
}




void InstrumentTrackView::toggleInstrumentWindow( bool _on )
{
	getInstrumentTrackWindow()->toggleVisibility( _on );
	
	if( !_on )
	{
		freeInstrumentTrackWindow();
	}
}




void InstrumentTrackView::activityIndicatorPressed()
{
	model()->processInEvent(
			midiEvent( MidiNoteOn, 0, DefaultKey, MidiMaxVelocity ),
								midiTime() );
}




void InstrumentTrackView::activityIndicatorReleased()
{
	model()->processInEvent( midiEvent( MidiNoteOff, 0, DefaultKey, 0 ),
								midiTime() );
}





void InstrumentTrackView::midiInSelected()
{
	if( model() )
	{
		model()->m_midiPort.setReadable(
					m_midiInputAction->isChecked() );
	}
}




void InstrumentTrackView::midiOutSelected()
{
	if( model() )
	{
		model()->m_midiPort.setWritable(
					m_midiOutputAction->isChecked() );
	}
}




void InstrumentTrackView::midiConfigChanged()
{
	m_midiInputAction->setChecked( model()->m_midiPort.isReadable() );
	m_midiOutputAction->setChecked( model()->m_midiPort.isWritable() );
}






class fxLineLcdSpinBox : public lcdSpinBox 
{
public:
	fxLineLcdSpinBox( int _num_digits, QWidget * _parent,
				const QString & _name ) :
		lcdSpinBox( _num_digits, _parent, _name )
	{
	}


protected:
	virtual void mouseDoubleClickEvent ( QMouseEvent * _me )
	{
		engine::fxMixerView()->setCurrentFxLine( model()->value() );
		//engine::fxMixerView()->raise();
	}

} ;



// #### ITW:
InstrumentTrackWindow::InstrumentTrackWindow( InstrumentTrackView * _itv ) :
	QWidget(),
	ModelView( NULL, this ),
	m_track( _itv->model() ),
	m_itv( _itv ),
	m_instrumentView( NULL )
{
	setAcceptDrops( true );

	// init own layout + widgets
	setFocusPolicy( Qt::StrongFocus );
	QVBoxLayout * vlayout = new QVBoxLayout( this );
	vlayout->setMargin( 0 );
	vlayout->setSpacing( 0 );

	m_generalSettingsWidget = new tabWidget( tr( "GENERAL SETTINGS" ),
									this );
	m_generalSettingsWidget->setFixedHeight( 90 );

	// setup line-edit for changing channel-name
	m_nameLineEdit = new QLineEdit( m_generalSettingsWidget );
	m_nameLineEdit->setFont( pointSize<8>(
						m_nameLineEdit->font() ) );
	m_nameLineEdit->setGeometry( 10, 16, 196, 20 );
	connect( m_nameLineEdit, SIGNAL( textChanged( const QString & ) ),
				this, SLOT( textChanged( const QString & ) ) );

	m_saveSettingsBtn = new QPushButton( embed::getIconPixmap(
							"project_save" ), "",
						m_generalSettingsWidget );
	m_saveSettingsBtn->setGeometry( 216, 14, 24, 24 );
	connect( m_saveSettingsBtn, SIGNAL( clicked() ), this,
					SLOT( saveSettingsBtnClicked() ) );
	toolTip::add( m_saveSettingsBtn,
		tr( "Save instrument track settings in a preset file" ) );
	m_saveSettingsBtn->setWhatsThis(
		tr( "Click here, if you want to save current channel settings "
			"in a preset-file. Later you can load this preset by "
			"double-clicking it in the preset-browser." ) );


	// setup volume-knob
	m_volumeKnob = new knob( knobBright_26, m_generalSettingsWidget,
						tr( "Instrument volume" ) );
	m_volumeKnob->setVolumeKnob( true );
	m_volumeKnob->move( 8, 46 );
	m_volumeKnob->setHintText( tr( "Volume:" ) + " ", "%" );
	m_volumeKnob->setLabel( tr( "VOL" ) );

	m_volumeKnob->setWhatsThis( tr( volume_help ) );


	// setup panning-knob
	m_panningKnob = new knob( knobBright_26, m_generalSettingsWidget,
							tr( "Panning" ) );
	m_panningKnob->move( m_volumeKnob->x() +
					m_volumeKnob->width() + 12, 46 );
	m_panningKnob->setHintText( tr( "Panning:" ) + " ", "" );
	m_panningKnob->setLabel( tr( "PAN" ) );


	m_pitchKnob = new knob( knobBright_26, m_generalSettingsWidget,
							tr( "Pitch" ) );
	m_pitchKnob->move( m_panningKnob->x() +
					m_panningKnob->width() + 24, 46 );
	m_pitchKnob->setHintText( tr( "Pitch:" ) + " ", " " + tr( "cents" ) );
	m_pitchKnob->setLabel( tr( "PITCH" ) );


	m_pitchRange = new lcdSpinBox( 2, m_generalSettingsWidget,
						tr( "Pitch range (semitones)" ) );
	m_pitchRange->setLabel( tr( "RANGE" ) );
	m_pitchRange->move( m_pitchKnob->x() +
					m_pitchKnob->width() + 8, 46 );

	// setup spinbox for selecting FX-channel
	m_effectChannelNumber = new fxLineLcdSpinBox( 2, m_generalSettingsWidget,
						tr( "FX channel" ) );
	m_effectChannelNumber->setLabel( tr( "FX CHNL" ) );
	m_effectChannelNumber->move( m_pitchRange->x() +
					m_pitchRange->width() + 24, 46 );


	m_tabWidget = new tabWidget( "", this );
	m_tabWidget->setFixedHeight( INSTRUMENT_HEIGHT + 10 );


	// create tab-widgets
	m_ssView = new InstrumentSoundShapingView( m_tabWidget );
	QWidget * instrument_functions = new QWidget( m_tabWidget );
	m_chordView = new ChordCreatorView( &m_track->m_chordCreator,
							instrument_functions );
	m_arpView= new ArpeggiatorView( &m_track->m_arpeggiator,
							instrument_functions );
	m_midiView = new InstrumentMidiIOView( m_tabWidget );
	m_effectView = new EffectRackView( m_track->m_audioPort.effects(),
								m_tabWidget );
	m_tabWidget->addTab( m_ssView, tr( "ENV/LFO" ), 1 );
	m_tabWidget->addTab( instrument_functions, tr( "FUNC" ), 2 );
	m_tabWidget->addTab( m_effectView, tr( "FX" ), 3 );
	m_tabWidget->addTab( m_midiView, tr( "MIDI" ), 4 );

	// setup piano-widget
	m_pianoView= new PianoView( this );

	vlayout->addWidget( m_generalSettingsWidget );
	vlayout->addWidget( m_tabWidget );
	vlayout->addWidget( m_pianoView );


	setModel( _itv->model() );

	updateInstrumentView();

	setFixedWidth( INSTRUMENT_WIDTH );
	resize( sizeHint() );

	QMdiSubWindow * subWin = 
			engine::mainWindow()->workspace()->addSubWindow( this );
	Qt::WindowFlags flags = subWin->windowFlags();
	flags |= Qt::MSWindowsFixedSizeDialogHint;
	flags &= ~Qt::WindowMaximizeButtonHint;
	subWin->setWindowFlags( flags );
	subWin->setWindowIcon( embed::getIconPixmap( "instrument_track" ) );
	subWin->setFixedSize( subWin->size() );
	subWin->hide();
}




InstrumentTrackWindow::~InstrumentTrackWindow()
{
	delete m_instrumentView;
	if( engine::mainWindow()->workspace() )
	{
		parentWidget()->hide();
		parentWidget()->deleteLater();
	}
}




void InstrumentTrackWindow::modelChanged()
{
	m_track = castModel<InstrumentTrack>();

	m_nameLineEdit->setText( m_track->name() );

	disconnect( m_track, SIGNAL( nameChanged() ) );
	disconnect( m_track, SIGNAL( instrumentChanged() ) );

	connect( m_track, SIGNAL( nameChanged() ),
			this, SLOT( updateName() ) );
	connect( m_track, SIGNAL( instrumentChanged() ),
			this, SLOT( updateInstrumentView() ) );
	
	m_volumeKnob->setModel( &m_track->m_volumeModel );
	m_panningKnob->setModel( &m_track->m_panningModel );
	m_effectChannelNumber->setModel( &m_track->m_effectChannelModel );
	m_pianoView->setModel( &m_track->m_piano );

	if( m_track->instrument() && m_track->instrument()->isBendable() )
	{
		m_pitchKnob->setModel( &m_track->m_pitchModel );
		m_pitchRange->setModel( &m_track->m_pitchRangeModel );
		m_pitchKnob->show();
	}
	else
	{
		m_pitchKnob->hide();
		m_pitchKnob->setModel( NULL );
	}

	m_ssView->setModel( &m_track->m_soundShaping );
	m_chordView->setModel( &m_track->m_chordCreator );
	m_arpView->setModel( &m_track->m_arpeggiator );
	m_midiView->setModel( &m_track->m_midiPort );
	m_effectView->setModel( m_track->m_audioPort.effects() );
	updateName();
}




void InstrumentTrackWindow::saveSettingsBtnClicked()
{
	QFileDialog sfd( this, tr( "Save preset" ), "",
				tr( "XML preset file (*.xpf)" ) );

	QString preset_root = configManager::inst()->userPresetsDir();
	if( !QDir( preset_root ).exists() )
	{
		QDir().mkdir( preset_root );
	}
	if( !QDir( preset_root + m_track->instrumentName() ).exists() )
	{
		QDir( preset_root ).mkdir( m_track->instrumentName() );
	}

	sfd.setAcceptMode( QFileDialog::AcceptSave );
	sfd.setDirectory( preset_root + m_track->instrumentName() );
	sfd.setFileMode( QFileDialog::AnyFile );
	if( sfd.exec () == QDialog::Accepted &&
		!sfd.selectedFiles().isEmpty() && sfd.selectedFiles()[0] != ""
	)
	{
		multimediaProject mmp(
				multimediaProject::InstrumentTrackSettings );
		m_track->setSimpleSerializing();
		m_track->saveSettings( mmp, mmp.content() );
		QString f = sfd.selectedFiles()[0];
		mmp.writeFile( f );
	}
}





void InstrumentTrackWindow::updateName()
{
	setWindowTitle( m_track->name() );

	if( m_nameLineEdit->text() != m_track->name() )
	{
		m_nameLineEdit->setText( m_track->name() );
	}
}





void InstrumentTrackWindow::updateInstrumentView()
{
	delete m_instrumentView;
	if( m_track->m_instrument != NULL )
	{
		m_instrumentView = m_track->m_instrument->createView(
								m_tabWidget );
		m_tabWidget->addTab( m_instrumentView, tr( "PLUGIN" ), 0 );
		m_tabWidget->setActiveTab( 0 );

		modelChanged(); 		// Get the instrument window to refresh
		m_track->dataChanged(); // Get the text on the trackButton to change
	}
}




void InstrumentTrackWindow::textChanged( const QString & _new_name )
{
	m_track->setName( _new_name );
	engine::getSong()->setModified();
}




void InstrumentTrackWindow::toggleVisibility( bool _on )
{

	if( _on )
	{
		show();
		parentWidget()->show();
		parentWidget()->raise();
	}
	else
	{
		parentWidget()->hide();
	}
}




void InstrumentTrackWindow::closeEvent( QCloseEvent * _ce )
{
	_ce->ignore();
	if( engine::mainWindow()->workspace() )
	{
		parentWidget()->hide();
	}
	else
	{
		hide();
	}
	m_itv->m_tlb->setFocus();
	m_itv->m_tlb->setChecked( false );
}




void InstrumentTrackWindow::focusInEvent( QFocusEvent * )
{
	m_pianoView->setFocus();
}




void InstrumentTrackWindow::dragEnterEventGeneric( QDragEnterEvent * _dee )
{
	stringPairDrag::processDragEnterEvent( _dee, "instrument,presetfile,"
							"pluginpresetfile" );
}




void InstrumentTrackWindow::dragEnterEvent( QDragEnterEvent * _dee )
{
	dragEnterEventGeneric( _dee );
}




void InstrumentTrackWindow::dropEvent( QDropEvent * _de )
{
	QString type = stringPairDrag::decodeKey( _de );
	QString value = stringPairDrag::decodeValue( _de );
	if( type == "instrument" )
	{
		m_track->loadInstrument( value );
		engine::getSong()->setModified();
		_de->accept();
	}
	else if( type == ResourceItem::mimeKey() )
	{
		const ResourceItem * item =
			engine::mergedResourceDB()->itemByHash( value );
		if( !item )
		{
			return;
		}
		ResourceAction action( item );
		if( action.loadByPlugin( m_track ) )
		{
			_de->accept();
		}
		else if( item->type() == ResourceItem::TypePreset &&
				action.loadPreset( m_track ) )
		{
			engine::getSong()->setModified();
			_de->accept();
		}
	}
}




void InstrumentTrackWindow::saveSettings( QDomDocument & _doc,
							QDomElement & _this )
{
	_this.setAttribute( "tab", m_tabWidget->activeTab() );
	MainWindow::saveWidgetState( this, _this );
}




void InstrumentTrackWindow::loadSettings( const QDomElement & _this )
{
	m_tabWidget->setActiveTab( _this.attribute( "tab" ).toInt() );
	MainWindow::restoreWidgetState( this, _this );
	if( isVisible() )
	{
		m_itv->m_tlb->setChecked( true );
	}
}




#include "moc_InstrumentTrack.cxx"

