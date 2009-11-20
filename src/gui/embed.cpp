/*
 * embed.cpp - misc stuff for using embedded resources (linked into binary)
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


#include <QtGui/QImage>
#include <QHash>

#include "embed.h"
#include "PathConfig.h"



#ifndef PLUGIN_NAME
namespace embed
#else
namespace PLUGIN_NAME
#endif
{

namespace
{
	static QHash<QString, QPixmap> s_pixmapCache;
}

#include "embedded_resources.h"


QPixmap getIconPixmap( const char * _name, int _w, int _h )
{
	if( _w == -1 || _h == -1 )
	{
		// Return cached pixmap
		QPixmap cached = s_pixmapCache.value( _name );
		if( !cached.isNull() )
		{
			return cached;
		}

		// Or try to load it
		QString name = QString( _name ) + ".png";

#ifdef PLUGIN_NAME
		QPixmap p( Global::paths().activeThemePath() + "plugins/" +
		           STRINGIFY( PLUGIN_NAME ) + "_" + name );
		if( p.isNull() )
		{
			p = QPixmap( Global::paths().activeThemePath() + name );
		}
#else
		// look whether icon is in artwork-dir
		QPixmap p( Global::paths().activeThemePath() + name );
#endif
		if( p.isNull() )
		{
			// nothing found, so look in default-artwork-dir
			p = QPixmap( Global::paths().defaultThemePath() + name );
		}
        if( p.isNull() )
        {
			const embed::descriptor & e = findEmbeddedData(
									name.toUtf8().constData() );
			// found?
			if( QString( e.name ) == name )
			{
				p.loadFromData( e.data, e.size );
			}
			else
			{
				p = QPixmap( 1, 1 );
			}
		}

		// Save to cache and return
		s_pixmapCache.insert( _name, p );
		return p;
	}

	return getIconPixmap( _name ).
			scaled( _w, _h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
}


QString getText( const char * _name )
{
	const embed::descriptor & e = findEmbeddedData( _name );
	return QString::fromUtf8( (const char *) e.data, e.size );
}


}


