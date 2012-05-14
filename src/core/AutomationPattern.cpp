/*
 * AutomationPattern.cpp - implementation of class AutomationPattern which
 *                         holds dynamic values
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

#include <QtXml/QDomElement>
#include <QtGui/QMouseEvent>
#include <QtGui/QPainter>

#include "AutomationPattern.h"
#include "AutomationPatternView.h"
#include "AutomationEditor.h"
#include "AutomationTrack.h"
#include "AutomationControlPoint.h"
#include "ProjectJournal.h"
#include "bb_track_container.h"
#include "song.h"



AutomationPattern::AutomationPattern( AutomationTrack * _auto_track ) :
	trackContentObject( _auto_track ),
	m_autoTrack( _auto_track ),
	m_objects(),
	m_hasAutomation( false )
{
	changeLength( midiTime( 1, 0 ) );
}




AutomationPattern::AutomationPattern( const AutomationPattern & _pat_to_copy ) :
	trackContentObject( _pat_to_copy.m_autoTrack ),
	m_autoTrack( _pat_to_copy.m_autoTrack ),
	m_objects( _pat_to_copy.m_objects ),
	m_hasAutomation( _pat_to_copy.m_hasAutomation )
{
    m_controlPoints = _pat_to_copy.getControlPoints();
}




AutomationPattern::~AutomationPattern()
{
	if( engine::automationEditor() &&
		engine::automationEditor()->currentPattern() == this )
	{
		engine::automationEditor()->setCurrentPattern( NULL );
	}
}




void AutomationPattern::addObject( AutomatableModel * _obj, bool _search_dup )
{
	bool addIt = true;

	if( _search_dup )
	{
		for( objectVector::iterator it = m_objects.begin();
					it != m_objects.end(); ++it )
		{
			if( *it == _obj )
			{
				// Already exists
				// TODO: Maybe let the user know in some non-annoying way
				addIt = false;
				break;
			}
		}
	}

	if( addIt )
	{
		m_objects += _obj;
		// been empty before?
		if( m_objects.size() == 1 && !hasAutomation() )
		{
			// then initialize default-value
			addControlPoint( 0, _obj->value<float>());
		}
		connect( _obj, SIGNAL( destroyed( jo_id_t ) ),
				this, SLOT( objectDestroyed( jo_id_t ) ),
							Qt::DirectConnection );
	}
}




const AutomatableModel * AutomationPattern::firstObject() const
{
	AutomatableModel * m;
	if( !m_objects.isEmpty() && ( m = m_objects.first() ) != NULL )
	{
		return m;
	}

	static FloatModel _fm( 0, 0, 1, 0.001 );
	return &_fm;
}




midiTime AutomationPattern::length() const
{
    return m_controlPoints.last().getTime();
}




void AutomationPattern::addControlPoint( midiTime _t, float _val )
{
    AutomationControlPoint newCP;
    newCP.setTime( _t );
    newCP.setValue( _val );

    if( m_controlPoints.isEmpty() )
    {
        m_controlPoints.append( newCP );
    }
    else
    {
        AutomationControlPoint tempCP;

        if( m_controlPoints.last().getTime() < tempCP.getTime() )
        {
            // The new control point is the last of the list. No need to iterate
            m_controlPoints.append( tempCP );
        }
        else
        {
            QMutableListIterator<AutomationControlPoint> iterator( m_controlPoints );
            while( iterator.hasNext() )
            {
                if ( iterator.next().getTime() < tempCP.getTime )
                {
                    m_controlPoints.insert( iterator, tempCP );
                    break;
                }
            }
        }
    }

    emit dataChanged();
}




void AutomationPattern::removeControlPoint( midiTime _t, float _val)
{
    QMutableListIterator<AutomationControlPoint> iterator( m_controlPoints );
    while( iterator.hasNext() )
    {
        if( iterator.prev().getTime() == _t )
        {
            iterator.remove();
            break;
        }
    }

    emit dataChanged();
}




float AutomationPattern::valueAt( const midiTime & _time ) const
{
    if( m_controlPoints.isEmpty() )
    {
        // Should never happen
        return 0;
    }

    if( m_controlPoints.last().getTime() < _time )
    {
        // The value wanted is outside of the range of the current automation
        return m_controlPoints.last().getValue();
    }

    QMutableListIterator<AutomationControlPoint> iterator( m_controlPoints );
    while( iterator.hasNext() )
    {
        if( iterator.next().getTime() > _time )
        {
            // The wanted midiTime is between .prev() and .next()

            AutomationControlPoint prev_point=iterator.prev(), next_point=iterator.next();
            float d, t;// d = x distance between the two points, t is the interpolation factor

            d = next_point.getTime() - prev_point.getTime();
            t = ( _time - prev_point.getTime() ) / d;
            return (prev_point.getValue() + t * ( next_point.getValue() - prev_point.getValue() ) )// Standard linear interpolation: a + t(b-a)
        }
    }
}




void AutomationPattern::saveSettings( QDomDocument & _doc, QDomElement & _this )
{
	_this.setAttribute( "pos", startPosition() );
	_this.setAttribute( "len", trackContentObject::length() );
	_this.setAttribute( "name", name() );
	_this.setAttribute( "num_cp", m_controlPoints.size() );

    QMutableListIterator<AutomationControlPoint> iterator( m_controlPoints );
    while( iterator.hasNext() )
    {
        QDomElement element = _doc.createElement( "CP" );
        element.setAttribute( "time", iterator.next().getTime() );
        element.setAttribute( "value", iterator.next().getValue() );
        _this.appendChild( element );
    }

	for( objectVector::const_iterator it = m_objects.begin();
						it != m_objects.end(); ++it )
	{
		if( *it )
		{
			QDomElement element = _doc.createElement( "object" );
			element.setAttribute( "id", ( *it )->id() );
			_this.appendChild( element );
		}
	}
}




void AutomationPattern::loadSettings( const QDomElement & _this )
{
	clear();

	movePosition( _this.attribute( "pos" ).toInt() );
	setName( _this.attribute( "name" ) );
	float numControlPoints = _this.attribute( "num_cp" );

	for( QDomNode node = _this.firstChild(); !node.isNull();
						node = node.nextSibling() )
	{
		QDomElement element = node.toElement();
		if( element.isNull()  )
		{
			continue;
		}
		if( element.tagName() == "CP" )// IMPORTANT: THIS ASSUMES THAT ALL CONTROL POINT VALUES ARE SAVED IN ORDER
		{
		    AutomationControlPoint newControlPoint;
		    newControlPoint.setTime( midiTime( element.attribute( "time" ).toInt() ) );
		    newControlPoint.setValue( element.attribute( "value" ).toFloat() );
			m_controlPoints.append(newControlPoint);
		}
		else if( element.tagName() == "object" )
		{
			m_idsToResolve << element.attribute( "id" ).toInt();
		}
	}

	m_hasAutomation = not m_controlPoints.isEmpty();
	if( m_hasAutomation == false )
	{
		for( objectVector::iterator it = m_objects.begin();
						it != m_objects.end(); ++it )
		{
			if( *it )
			{
				( *it )->setValue( m_controlPoints.first().getValue() );
			}
		}
	}
	int len = _this.attribute( "len" ).toInt();
	if( len <= 0 )
	{
		len = length();
	}
	changeLength( len );
}




const QString AutomationPattern::name() const
{
	if( !trackContentObject::name().isEmpty() )
	{
		return trackContentObject::name();
	}
	if( !m_objects.isEmpty() && m_objects.first() != NULL )
	{
		return m_objects.first()->fullDisplayName();
	}
	return tr( "Drag a control while pressing <Ctrl>" );
}




void AutomationPattern::processMidiTime( const midiTime & _time )
{
	if( _time >= 0 && m_hasAutomation )
	{
		const float val = valueAt( _time );
		for( objectVector::iterator it = m_objects.begin();
						it != m_objects.end(); ++it )
		{
			if( *it )
			{
				( *it )->setAutomatedValue( val );
			}

		}
	}
}





trackContentObjectView * AutomationPattern::createView( trackView * _tv )
{
	return new AutomationPatternView( this, _tv );
}





bool AutomationPattern::isAutomated( const AutomatableModel * _m )
{
	trackContainer::trackList l = engine::getSong()->tracks() +
				engine::getBBTrackContainer()->tracks();
	l += engine::getSong()->globalAutomationTrack();
	for( trackContainer::trackList::const_iterator it = l.begin();
							it != l.end(); ++it )
	{
		if( ( *it )->type() == track::AutomationTrack ||
			( *it )->type() == track::HiddenAutomationTrack )
		{
			const track::tcoVector & v = ( *it )->getTCOs();
			for( track::tcoVector::const_iterator j = v.begin();
							j != v.end(); ++j )
			{
				const AutomationPattern * a =
								dynamic_cast<const AutomationPattern *>( *j );
				if( a && a->m_hasAutomation )
				{
	for( objectVector::const_iterator k = a->m_objects.begin();
					k != a->m_objects.end(); ++k )
	{
		if( *k == _m )
		{
			return true;
		}
	}
				}
			}
		}
	}
	return false;
}





AutomationPattern * AutomationPattern::globalAutomationPattern(
							AutomatableModel * _m )
{
	AutomationTrack * t = engine::getSong()->globalAutomationTrack();
	track::tcoVector v = t->getTCOs();
	for( track::tcoVector::const_iterator j = v.begin(); j != v.end(); ++j )
	{
		AutomationPattern * a = dynamic_cast<AutomationPattern *>( *j );
		if( a )
		{
			for( objectVector::const_iterator k = a->m_objects.begin();
												k != a->m_objects.end(); ++k )
			{
				if( *k == _m )
				{
					return a;
				}
			}
		}
	}

	AutomationPattern * a = new AutomationPattern( t );
	a->addObject( _m, false );
	return a;
}




void AutomationPattern::resolveAllIDs()
{
	trackContainer::trackList l = engine::getSong()->tracks() +
				engine::getBBTrackContainer()->tracks();
	l += engine::getSong()->globalAutomationTrack();
	for( trackContainer::trackList::iterator it = l.begin();
							it != l.end(); ++it )
	{
		if( ( *it )->type() == track::AutomationTrack ||
			 ( *it )->type() == track::HiddenAutomationTrack )
		{
			track::tcoVector v = ( *it )->getTCOs();
			for( track::tcoVector::iterator j = v.begin();
							j != v.end(); ++j )
			{
				AutomationPattern * a = dynamic_cast<AutomationPattern *>( *j );
				if( a )
				{
	for( QVector<jo_id_t>::Iterator k = a->m_idsToResolve.begin();
					k != a->m_idsToResolve.end(); ++k )
	{
		JournallingObject * o = engine::projectJournal()->
										journallingObject( *k );
		if( o && dynamic_cast<AutomatableModel *>( o ) )
		{
			a->addObject( dynamic_cast<AutomatableModel *>( o ), false );
		}
	}
	a->m_idsToResolve.clear();
	a->dataChanged();
				}
			}
		}
	}
}




void AutomationPattern::clear()
{
	const float val = firstObject()->value<float>();
    m_controlPoints.clear();
    addControlPoint( 0, val );

	if( engine::automationEditor() &&
		engine::automationEditor()->currentPattern() == this )
	{
		engine::automationEditor()->update();
	}
}




void AutomationPattern::openInAutomationEditor()
{
	engine::automationEditor()->setCurrentPattern( this );
	engine::automationEditor()->parentWidget()->show();
	engine::automationEditor()->setFocus();
}




void AutomationPattern::objectDestroyed( jo_id_t _id )
{
	// TODO: distict between temporary removal (e.g. LADSPA controls
	// when switching samplerate) and real deletions because in the latter
	// case we had to remove ourselves if we're the global automation
	// pattern of the destroyed object
	m_idsToResolve += _id;
}




#include "moc_AutomationPattern.cxx"

/* vim: set tw=0 noexpandtab: */
