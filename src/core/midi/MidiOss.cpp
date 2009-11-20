/*
 * MidiOss.cpp - OSS raw MIDI client
 *
 * Copyright (c) 2005-2009 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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


#include "MidiOss.h"

#ifdef LMMS_HAVE_OSS


#include <QtGui/QCompleter>
#include <QtGui/QDirModel>
#include <QtGui/QLabel>
#include <QtGui/QLineEdit>


#ifdef LMMS_HAVE_STDLIB_H
#include <stdlib.h>
#endif


#include "gui_templates.h"



MidiOss::MidiOss() :
	MidiClientRaw(),
	m_midiDev( probeDevice() ),
	m_quit( false )
{
	// only start thread, if opening of MIDI-device is successful,
	// otherwise isRunning()==false indicates error
	if( m_midiDev.open( QIODevice::ReadWrite ) ||
					m_midiDev.open( QIODevice::ReadOnly ) )
	{
		start( QThread::LowPriority );
	}
}




MidiOss::~MidiOss()
{
	if( isRunning() )
	{
		m_quit = true;
		wait( 1000 );
		terminate();
	}
}




QString MidiOss::probeDevice()
{
	QString dev = cfg().value( "Device" );
	if( dev.isEmpty() )
	{
		if( getenv( "MIDIDEV" ) != NULL )
		{
			return getenv( "MIDIDEV" );
		}
		return "/dev/midi";
	}
	return dev;
}




void MidiOss::sendByte( const Uint8 _c )
{
	m_midiDev.putChar( _c );
}




void MidiOss::run()
{
	while( m_quit == false && m_midiDev.isOpen() )
	{
		char c;
		if( !m_midiDev.getChar( &c ) )
		{
			continue;
		}
		parseData( c );
	}
}





MidiOss::setupWidget::setupWidget( QWidget * _parent ) :
	MidiClientRaw::setupWidget( MidiOss::name(), _parent )
{
	m_device = new QLineEdit( MidiOss::probeDevice(), this );
	m_device->setGeometry( 10, 20, 180, 20 );
	QDirModel * model = new QDirModel( QStringList(), 
			QDir::AllDirs | QDir::System,
			QDir::Name | QDir::DirsFirst,
			this );
	m_device->setCompleter(	new QCompleter( model, this ) );


	QLabel * dev_lbl = new QLabel( tr( "DEVICE" ), this );
	dev_lbl->setFont( pointSize<6>( dev_lbl->font() ) );
	dev_lbl->setGeometry( 10, 40, 180, 10 );
}




MidiOss::setupWidget::~setupWidget()
{
}




void MidiOss::setupWidget::saveSettings()
{
	cfg().setValue( "Device", m_device->text() );
}



#endif


