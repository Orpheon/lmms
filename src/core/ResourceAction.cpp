/*
 * ResourceBrowser.cpp - implementation of ResourceBrowser
 *
 * Copyright (c) 2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#include "ResourceAction.h"
#include "ResourceItem.h"
#include "ResourceFileMapper.h"
#include "ImportFilter.h"
#include "Instrument.h"
#include "InstrumentTrack.h"
#include "MainWindow.h"
#include "mmp.h"
#include "Song.h"



bool ResourceAction::loadProject()
{
	if( engine::mainWindow()->mayChangeProject() )
	{
		ResourceFileMapper mapper( m_item );
		if( m_item->isLocalResource() )
		{
			engine::song()->loadProject( mapper.fileName() );
		}
		else
		{
			engine::song()->createNewProjectFromTemplate(
						mapper.fileName() );
		}
		return true;
	}
	return false;
}




bool ResourceAction::loadByPlugin( InstrumentTrack * _target )
{
	Instrument * i = _target->instrument();
	if( i == NULL || !i->descriptor()->supportsFileType(
										m_item->nameExtension() ) )
	{
		i = _target->loadInstrument(
			engine::pluginFileHandling()[m_item->nameExtension()] );
	}
	if( i )
	{
		i->loadResource( m_item );
		return true;
	}
	return false;
}




bool ResourceAction::loadPreset( InstrumentTrack * _target )
{
	multimediaProject mmp( m_item->fetchData() );
	InstrumentTrack::removeMidiPortNode( mmp );
	_target->setSimpleSerializing();
	_target->loadSettings( mmp.content().toElement() );

	return true;
}




bool ResourceAction::importProject( TrackContainer * _target )
{
	ResourceFileMapper mapper( m_item );
	/*return*/ ImportFilter::import( mapper.fileName(), _target );
	return true;
}



bool ResourceAction::defaultTrigger()
{
	switch( m_action )
	{
		case LoadProject:
			return loadProject();
		case ImportFile:
			return importProject( engine::song() );
		default:
			break;
	}
	return false;
}



#include "moc_ResourceAction.cxx"

