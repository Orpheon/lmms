/*
 * engine.cpp - implementation of LMMS' engine-system
 *
 * Copyright (c) 2006-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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
#include <QtXml/QDomDocument>

#include "engine.h"
#include "automation_editor.h"
#include "automation_recorder.h"
#include "bb_editor.h"
#include "bb_track_container.h"
#include "config_mgr.h"
#include "ControllerRackView.h"
#include "FxMixer.h"
#include "FxMixerView.h"
#include "InstrumentTrack.h"
#include "ladspa_2_lmms.h"
#include "MainWindow.h"
#include "mixer.h"
#include "pattern.h"
#include "piano_roll.h"
#include "ProjectJournal.h"
#include "project_notes.h"
#include "Plugin.h"
#include "song_editor.h"
#include "song.h"
#include "Song.h"
#include "Sequencer.h"
#include "MidiControlListener.h"

#include "ResourceDB.h"
#include "LocalResourceProvider.h"
#include "WebResourceProvider.h"
#include "UnifiedResourceProvider.h"


bool engine::s_hasGUI = true;
bool engine::s_suppressMessages = false;
float engine::s_framesPerTick;
mixer * engine::s_mixer = NULL;
FxMixer * engine::s_fxMixer = NULL;
FxMixerView * engine::s_fxMixerView = NULL;
MainWindow * engine::s_mainWindow = NULL;
bbTrackContainer * engine::s_bbTrackContainer = NULL;
Sequencer * engine::s_sequencer = NULL;
song * engine::s_song = NULL;
Song * engine::s_songTng = NULL;
ResourceDB * engine::s_workingDirResourceDB = NULL;
ResourceDB * engine::s_webResourceDB = NULL;
ResourceDB * engine::s_mergedResourceDB = NULL;
songEditor * engine::s_songEditor = NULL;
automationEditor * engine::s_automationEditor = NULL;
AutomationRecorder * engine::s_automationRecorder = NULL;
bbEditor * engine::s_bbEditor = NULL;
pianoRoll * engine::s_pianoRoll = NULL;
projectNotes * engine::s_projectNotes = NULL;
ProjectJournal * engine::s_projectJournal = NULL;
ladspa2LMMS * engine::s_ladspaManager = NULL;
DummyTrackContainer * engine::s_dummyTC = NULL;
ControllerRackView * engine::s_controllerRackView = NULL;
MidiControlListener * engine::s_midiControlListener = NULL;
QMap<QString, QString> engine::s_pluginFileHandling;
LmmsStyle * engine::s_lmmsStyle = NULL;




void engine::init( const bool _has_gui )
{
	s_hasGUI = _has_gui;

	initPluginFileHandling();

	s_projectJournal = new ProjectJournal;
	s_mixer = new mixer;
	s_sequencer = new Sequencer();
	s_song = new ::song();
	s_songTng = new Song();


	// init resource framework
	s_workingDirResourceDB =
		( new LocalResourceProvider( ResourceItem::BaseWorkingDir,
													QString() ) )->database();
	ResourceDB * shippedResourceDB =
		( new LocalResourceProvider( ResourceItem::BaseDataDir,
													QString() ) )->database();
	s_webResourceDB =
		( new WebResourceProvider( "http://lmms.sourceforge.net" ) )
																->database();

	UnifiedResourceProvider * unifiedResource = new UnifiedResourceProvider;
	unifiedResource->addDatabase( s_workingDirResourceDB );
	unifiedResource->addDatabase( shippedResourceDB );
	unifiedResource->addDatabase( s_webResourceDB );

	s_mergedResourceDB = unifiedResource->database();


	s_fxMixer = new FxMixer;
	s_bbTrackContainer = new bbTrackContainer;

	s_ladspaManager = new ladspa2LMMS;

	s_projectJournal->setJournalling( TRUE );

	s_mixer->initDevices();

	s_midiControlListener = new MidiControlListener();

	s_automationRecorder = new AutomationRecorder;

	if( s_hasGUI )
	{
		s_mainWindow = new MainWindow;
		s_songEditor = new songEditor( s_song, s_songEditor );
		s_fxMixerView = new FxMixerView;
		s_controllerRackView = new ControllerRackView;
		s_projectNotes = new projectNotes;
		s_bbEditor = new bbEditor( s_bbTrackContainer );
		s_pianoRoll = new pianoRoll;
		s_automationEditor = new automationEditor;

		s_mainWindow->finalize();
	}

	s_dummyTC = new DummyTrackContainer;

	s_mixer->startProcessing();
}




void engine::destroy()
{
	configManager::inst()->saveConfigFile();

	s_mixer->stopProcessing();

	delete s_projectNotes;
	s_projectNotes = NULL;
	delete s_songEditor;
	s_songEditor = NULL;
	delete s_bbEditor;
	s_bbEditor = NULL;
	delete s_pianoRoll;
	s_pianoRoll = NULL;
	delete s_automationEditor;
	s_automationEditor = NULL;

	delete s_fxMixerView;
	s_fxMixerView = NULL;

	InstrumentTrackView::cleanupWindowPool();

	s_song->clearProject();
	delete s_bbTrackContainer;
	s_bbTrackContainer = NULL;
	delete s_dummyTC;
	s_dummyTC = NULL;

	delete s_mixer;
	s_mixer = NULL;
	delete s_fxMixer;
	s_fxMixer = NULL;

	delete s_ladspaManager;

	//delete configManager::inst();
	delete s_projectJournal;
	s_projectJournal = NULL;
	s_mainWindow = NULL;

	delete s_song;
	s_song = NULL;

	delete s_automationRecorder;
	s_automationRecorder = NULL;

	delete s_mergedResourceDB->provider();
	s_mergedResourceDB = NULL;

	delete configManager::inst();
}




void engine::updateFramesPerTick()
{
	s_framesPerTick = s_mixer->processingSampleRate() * 60.0f * 4 /
				DefaultTicksPerTact / s_song->getTempo();
}




void engine::initPluginFileHandling()
{
	QVector<Plugin::Descriptor> pluginDescriptors;
	Plugin::getDescriptorsOfAvailPlugins( pluginDescriptors );
	for( QVector<Plugin::Descriptor>::Iterator it = pluginDescriptors.begin();
										it != pluginDescriptors.end(); ++it )
	{
		if( it->type == Plugin::Instrument )
		{
			const char * * suppFileTypes = it->supportedFileTypes;
			while( suppFileTypes && *suppFileTypes != NULL )
			{
				s_pluginFileHandling[*suppFileTypes] = it->name;
				++suppFileTypes;
			}
		}
	}
}




void engine::loadConfiguration( QDomDocument & doc )
{
	// must be a call to a static method as the engine
	// is not yet created and initialized and 
	// s_midiControlListener is still NULL.
	MidiControlListener::rememberConfiguration( doc );
}




void engine::saveConfiguration( QDomDocument & doc )
{
	s_midiControlListener->saveConfiguration( doc );
}


