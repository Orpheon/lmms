/*
 * InlineAutomation.h - class for automating something "inline"
 *
 * Copyright (c) 2008-2010 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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

#ifndef _INLINE_AUTOMATION_H
#define _INLINE_AUTOMATION_H

#include "AutomationPattern.h"
#include "shared_object.h"


class InlineAutomation : public FloatModel, public sharedObject
{
public:
	InlineAutomation() :
		FloatModel(),
		sharedObject(),
		m_autoPattern( NULL )
	{
	}

	virtual ~InlineAutomation()
	{
		if( m_autoPattern )
		{
			delete m_autoPattern;
		}
	}

	virtual float defaultValue() const = 0;

	inline bool hasAutomation() const
	{
		return m_autoPattern != NULL &&
			!typeInfo<float>::isEqual(
					m_autoPattern->getTimeMap()[0],
							defaultValue() );
	}

	AutomationPattern * automationPattern()
	{
		if( m_autoPattern == NULL )
		{
			m_autoPattern = new AutomationPattern( NULL );
			m_autoPattern->addObject( this );
		}
		return m_autoPattern;
	}

	virtual void saveSettings( QDomDocument & _doc, QDomElement & _parent );
	virtual void loadSettings( const QDomElement & _this );


private:
	AutomationPattern * m_autoPattern;

} ;


#endif
