/*
 * bb_editor.h - view-component of BB-Editor
 *
 * Copyright (c) 2004-2008 Tobias Doerffel <tobydox/at/users.sourceforge.net>
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


#ifndef _BB_EDITOR_H
#define _BB_EDITOR_H

#include "track_container_view.h"


class bbTrackContainer;
class comboBox;
class toolButton;


class bbEditor : public trackContainerView
{
	Q_OBJECT
public:
	bbEditor( bbTrackContainer * _tc );
	virtual ~bbEditor();

	virtual inline bool fixedTCOs() const
	{
		return( true );
	}

	void removeBBView( int _bb );


public slots:
	void play();
	void stop();
	void updatePosition();
	void addAutomationTrack();

private:
	virtual void keyPressEvent( QKeyEvent * _ke );

	bbTrackContainer * m_bbtc;
	QWidget * m_toolBar;

	toolButton * m_playButton;
	toolButton * m_stopButton;

	comboBox * m_bbComboBox;

} ;


#endif
