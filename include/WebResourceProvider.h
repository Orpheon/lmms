/*
 * WebResourceProvider.h - header file for WebResourceProvider
 *
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

#ifndef _WEB_RESOURCE_PROVIDER_H
#define _WEB_RESOURCE_PROVIDER_H

#include <QtXml/QDomNode>

#include "ResourceProvider.h"
#include "ResourceItem.h"
#include "ProgressTrackingNetworkAccessManager.h"


class QBuffer;
class QNetworkReply;


/*! \brief The WebResourceProvider class provides a ResourceProvider
 * implementation which serves data from the internet (currently
 * specific to LSP only).
 *
 * Most of this class is network management code (quite highlevel thanks to
 * QtNetwork though). Besides that it translates web resource index files into
 * a ResourceDB and fetches files by given hash.
 */

class WebResourceProvider : public ResourceProvider
{
	Q_OBJECT
public:
	WebResourceProvider( const QString & _url );
	virtual ~WebResourceProvider();

	virtual QString providerName() const
	{
		return "WebResourceProvider";
	}

	virtual void updateDatabase();

	virtual int dataSize( const ResourceItem * _item ) const
	{
		// asume that the size we have set before from the web
		// catalogue is correct
		return _item->size();
	}
	virtual QByteArray fetchData( const ResourceItem * _item,
					int _maxSize = -1 ) const;

	virtual bool isLocal() const
	{
		return false;
	}


private slots:
	void finishDownload( QNetworkReply * _reply );


private:
	ResourceItem::Relation * addRelation( ResourceItem::Relation * _parent,
											ResourceItem * _item );
	void importNodeIntoDB( const QDomNode & n,
							ResourceItem::Relation * _parent );
	void download( const QUrl & _url, QBuffer * _target ) const;
	void downloadAsync( const QUrl & _url, QBuffer * _target ) const;

	typedef QMap<QNetworkReply *, QBuffer *> DownloadMap;
	static DownloadMap m_downloads;

} ;


#endif
