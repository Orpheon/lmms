/*
 * AutomationPattern.h - declaration of class AutomationPattern, which contains
 *                       all information about an automation pattern
 *
 * Copyright (c) 2008-2010 Tobias Doerffel <tobydox/at/users.sourceforge.net>
 * Copyright (c) 2006-2008 Javier Serrano Polo <jasp00/at/users.sourceforge.net>
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

#ifndef _AUTOMATION_PATTERN_H
#define _AUTOMATION_PATTERN_H

#include <QtCore/QPointer>

#include "track.h"


class AutomationTrack;
class midiTime;



class EXPORT AutomationPattern : public trackContentObject
{
	Q_OBJECT
public:
	typedef QMap<int, float> timeMap;
	typedef QVector<QPointer<AutomatableModel> > objectVector;

	AutomationPattern( AutomationTrack * _auto_track );
	AutomationPattern( const AutomationPattern & _pat_to_copy );
	virtual ~AutomationPattern();

	void addObject( AutomatableModel * _obj, bool _search_dup = true );

	const AutomatableModel * firstObject() const;

	virtual midiTime length() const;

	midiTime putValue( const midiTime & _time, const float _value,
						const bool _quant_pos = true );

	void removeValue( const midiTime & _time );

	inline const timeMap & getTimeMap() const
	{
		return m_timeMap;
	}

	inline timeMap & getTimeMap()
	{
		return m_timeMap;
	}

	inline bool hasAutomation() const
	{
		return m_hasAutomation;
	}

	float valueAt( const midiTime & _time ) const;

	const QString name() const;

	// settings-management
	virtual void saveSettings( QDomDocument & _doc, QDomElement & _parent );
	virtual void loadSettings( const QDomElement & _this );

	static inline const QString classNodeName()
	{
		return "automationpattern";
	}

	inline virtual QString nodeName() const
	{
		return classNodeName();
	}

	void processMidiTime( const midiTime & _time );

	virtual trackContentObjectView * createView( trackView * _tv );


	static bool isAutomated( const AutomatableModel * _m );
	static AutomationPattern * globalAutomationPattern( AutomatableModel * _m );
	static void resolveAllIDs();


public slots:
	void clear();
	void openInAutomationEditor();
	void objectDestroyed( jo_id_t );


private:
	AutomationTrack * m_autoTrack;
	QVector<jo_id_t> m_idsToResolve;
	objectVector m_objects;
	timeMap m_timeMap;	// actual values
	bool m_hasAutomation;


	friend class AutomationPatternView;

} ;


#endif
