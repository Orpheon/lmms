/*
 * AutomationControlPoint.h - declaration of class AutomationControlPoint, which contains
 *                            the position of the automation at a certain point
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

#ifndef _AUTOMATION_CONTROL_POINT_H
#define _AUTOMATION_CONTROL_POINT_H

#include "midi_time.h"

class AutomationControlPoint
{
    midiTime m_time;
    float m_value;

    public:
    inline float getTime()
    {
        return m_time;
    }
    inline float getValue()
    {
        return m_value;
    }
    inline void setTime(midiTime _new_time)
    {
        m_time = _new_time;
    }
    inline void setValue(float _new_value)
    {
        m_value = _new_value;
    }
};

#endif
