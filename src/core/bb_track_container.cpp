/*
 * bb_track_container.cpp - model-component of BB-Editor
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


#include "bb_track_container.h"
#include "BbTrack.h"
#include "combobox.h"
#include "embed.h"
#include "engine.h"
#include "Song.h"



bbTrackContainer::bbTrackContainer() :
	TrackContainer(),
	m_bbComboBoxModel( this )
{
	connect( &m_bbComboBoxModel, SIGNAL( dataChanged() ),
			this, SLOT( currentBBChanged() ) );
	// we *always* want to receive updates even in case BB actually did
	// not change upon setCurrentBB()-call
	connect( &m_bbComboBoxModel, SIGNAL( dataUnchanged() ),
			this, SLOT( currentBBChanged() ) );
}




bbTrackContainer::~bbTrackContainer()
{
}




bool bbTrackContainer::play( midiTime _start, fpp_t _frames,
							f_cnt_t _offset,
							Sint16 _tco_num )
{
	bool played_a_note = false;
	if( lengthOfBB( _tco_num ) <= 0 )
	{
		return( false );
	}

	_start = _start % ( lengthOfBB( _tco_num ) * midiTime::ticksPerTact() );

	TrackList tl = tracks();
	for( TrackList::iterator it = tl.begin(); it != tl.end(); ++it )
	{
		if( ( *it )->play( _start, _frames, _offset, _tco_num ) )
		{
			played_a_note = true;
		}
	}

	return( played_a_note );
}




void bbTrackContainer::updateAfterTrackAdd()
{
	// make sure, new track(s) have TCOs for every beat/bassline
	for( int i = 0; i < qMax<int>( 1, numOfBBs() ); ++i )
	{
		createTCOsForBB( i );
	}
}




tact_t bbTrackContainer::lengthOfBB( int _bb )
{
	midiTime max_length = midiTime::ticksPerTact();
	/* TODO{TNG} Better BB length support
	const TrackList & tl = tracks();
	for( TrackList::const_iterator it = tl.begin(); it != tl.end(); ++it )
	{
		max_length = qMax( max_length,
					( *it )->getTCO( _bb )->length() );
	}
	*/
	return( max_length.nextFullTact() );
}




int bbTrackContainer::numOfBBs() const
{
	return( engine::song()->countTracks( Track::BBTrack ) );
}




void bbTrackContainer::removeBB( int _bb )
{
	/* TODO{TNG} Support removeBB
	TrackList tl = tracks();
	for( TrackList::iterator it = tl.begin(); it != tl.end(); ++it )
	{
		delete ( *it )->getSegment( _bb );
		( *it )->removeTact( _bb * DefaultTicksPerTact );
	}
	if( _bb <= currentBB() )
	{
		setCurrentBB( qMax( currentBB() - 1, 0 ) );
	}
	*/
}




void bbTrackContainer::swapBB( int _bb1, int _bb2 )
{
	/* TODO{TNG} Support/Remove removeBB
	trackList tl = tracks();
	for( trackList::iterator it = tl.begin(); it != tl.end(); ++it )
	{
		( *it )->swapPositionOfTCOs( _bb1, _bb2 );
	}
	*/
	updateComboBox();
}




void bbTrackContainer::updateBBTrack( TrackSegment * _tco )
{
	BbTrack * t = BbTrack::findBBTrack( _tco->startPosition() /
							DefaultTicksPerTact );
	if( t != NULL )
	{
		t->dataChanged();
	}
}




void bbTrackContainer::fixIncorrectPositions()
{
	/* TODO{TNG} Better BB support
	TrackList tl = tracks();
	for( TrackList::iterator it = tl.begin(); it != tl.end(); ++it )
	{
		for( int i = 0; i < numOfBBs(); ++i )
		{
			( *it )->getTCO( i )->movePosition( midiTime( i, 0 ) );
		}
	}
	*/
}




void bbTrackContainer::play()
{
	/* TODO{TNG} Implement Play or move to sequencer
	if( engine::song()->isPlaying() )
	{
		if( engine::song()->playMode() != song::Mode_PlayBB )
		{
			engine::song()->stop();
			engine::song()->playBB();
		}
		else
		{
			engine::song()->pause();
		}
	}
	else if( engine::song()->isPaused() )
	{
		engine::song()->resumeFromPause();
	}
	else
	{
		engine::song()->playBB();
	}
	*/
}




void bbTrackContainer::stop()
{
	/* TODO{TNG} Implement Play or move to sequencer
	engine::song()->stop();
	*/
}




void bbTrackContainer::updateComboBox()
{
	const int cur_bb = currentBB();

	m_bbComboBoxModel.clear();

	for( int i = 0; i < numOfBBs(); ++i )
	{
		BbTrack * bbt = BbTrack::findBBTrack( i );
		m_bbComboBoxModel.addItem( bbt->name() );
	}
	setCurrentBB( cur_bb );
}




void bbTrackContainer::currentBBChanged()
{
	// first make sure, all channels have a TCO at current BB
	createTCOsForBB( currentBB() );

	// now update all track-labels (the current one has to become white,
	// the others gray)
	TrackList tl = engine::song()->tracks();
	for( TrackList::iterator it = tl.begin(); it != tl.end(); ++it )
	{
		if( ( *it )->type() == Track::BBTrack )
		{
			( *it )->dataChanged();
		}
	}
}




void bbTrackContainer::createTCOsForBB( int _bb )
{
	/* TODO{TNG} WHat is this even for???
	if( numOfBBs() == 0 || engine::song()->isLoadingProject() )
	{
		return;
	}

	TrackList tl = tracks();
	for( int i = 0; i < tl.size(); ++i )
	{
		while( tl[i]->numOfTCOs() < _bb + 1 )
		{
			midiTime position = midiTime( tl[i]->numOfTCOs(), 0 );
			trackContentObject * tco = tl[i]->createTCO( position );
			tco->movePosition( position );
			tco->changeLength( midiTime( 1, 0 ) );
		}
	}
	*/
}



#include "moc_bb_track_container.cxx"

