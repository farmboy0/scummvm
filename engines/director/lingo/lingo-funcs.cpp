/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "audio/audiostream.h"
#include "common/file.h"
#include "common/macresman.h"
#include "common/system.h"

#include "graphics/macgui/macwindowmanager.h"

#include "director/director.h"
#include "director/archive.h"
#include "director/cursor.h"
#include "director/movie.h"
#include "director/score.h"
#include "director/sound.h"
#include "director/window.h"

#include "director/lingo/lingo-builtins.h"


namespace Director {

void Lingo::func_goto(Datum &frame, Datum &movie, bool calledfromgo) {
	_vm->_playbackPaused = false;

	if (!_vm->getCurrentMovie())
		return;

	if (movie.type == VOID && frame.type == VOID)
		return;

	Window *stage = _vm->getCurrentWindow();
	Score *score = stage->getCurrentMovie()->getScore();

	stage->_skipFrameAdvance = true;

	// If there isn't already frozen Lingo (e.g. from a previous func_goto we haven't yet unfrozen),
	// freeze this script context. We'll return to it after entering the next frame.

	// Returning from a script with "play done" does not freeze the state. Instead it obliterates it.
	if (!g_lingo->_playDone)
		g_lingo->_freezeState = true;

	if (movie.type != VOID) {
		Common::String movieFilenameRaw = movie.asString();

		if (!stage->setNextMovie(movieFilenameRaw))
			return;

		// If we reached here from b_go, and the movie is getting swapped out,
		// reset all of the custom event handlers.
		if (calledfromgo)
			g_lingo->resetLingoGo();

		if (g_lingo->_updateMovieEnabled) {
			// Save the movie when branching to another movie.
			LB::b_saveMovie(0);
		}

		score->_playState = kPlayStopped;

		stage->_nextMovie.frameS.clear();
		stage->_nextMovie.frameI = -1;

		if (frame.type == STRING) {
			debugC(3, kDebugLingoExec, "Lingo::func_goto(): going to movie \"%s\", frame \"%s\"", movieFilenameRaw.c_str(), frame.u.s->c_str());
			stage->_nextMovie.frameS = *frame.u.s;
		} else if (frame.type != VOID) {
			debugC(3, kDebugLingoExec, "Lingo::func_goto(): going to movie \"%s\", frame %d", movieFilenameRaw.c_str(), frame.asInt());
			stage->_nextMovie.frameI = frame.asInt();
		} else {
			debugC(3, kDebugLingoExec, "Lingo::func_goto(): going to start of movie \"%s\"", movieFilenameRaw.c_str());
		}

		// Set cursor to watch.
		score->_defaultCursor.readFromResource(4);
		score->renderCursor(stage->getMousePos());

		return;
	}

	if (frame.type == STRING) {
		debugC(3, kDebugLingoExec, "Lingo::func_goto(): going to frame \"%s\"", frame.u.s->c_str());
		score->setStartToLabel(*frame.u.s);
	} else {
		debugC(3, kDebugLingoExec, "Lingo::func_goto(): going to frame %d", frame.asInt());
		score->setCurrentFrame(frame.asInt());
	}
}

void Lingo::func_gotoloop() {
	if (!_vm->getCurrentMovie())
		return;
	Window *stage = _vm->getCurrentWindow();
	Score *score = stage->getCurrentMovie()->getScore();
	debugC(3, kDebugLingoExec, "Lingo::func_gotoloop(): looping frame %d", score->getCurrentFrameNum());

	score->gotoLoop();

	stage->_skipFrameAdvance = true;
}

void Lingo::func_gotonext() {
	if (!_vm->getCurrentMovie())
		return;

	Window *stage = _vm->getCurrentWindow();
	Score *score = stage->getCurrentMovie()->getScore();
	score->gotoNext();
	debugC(3, kDebugLingoExec, "Lingo::func_gotonext(): going to next frame %d", score->getNextFrame());

	stage->_skipFrameAdvance = true;
}

void Lingo::func_gotoprevious() {
	if (!_vm->getCurrentMovie())
		return;

	Window *stage = _vm->getCurrentWindow();
	Score *score = stage->getCurrentMovie()->getScore();
	score->gotoPrevious();
	debugC(3, kDebugLingoExec, "Lingo::func_gotoprevious(): going to previous frame %d", score->getNextFrame());

	stage->_skipFrameAdvance = true;
}

void Lingo::func_play(Datum &frame, Datum &movie) {
	MovieReference ref;
	Window *stage = _vm->getCurrentWindow();


	// play #done
	if (frame.type == SYMBOL) {
		if (!frame.u.s->equals("done")) {
			warning("Lingo::func_play: unknown symbol: #%s", frame.u.s->c_str());
			return;
		}
		_playDone = true;
		if (stage->_movieStack.empty()) {	// No op if no nested movies

			return;
		}
		ref = stage->_movieStack.back();

		stage->_movieStack.pop_back();

		Datum m, f;

		if (ref.movie.empty()) {
			m.type = VOID;
		} else {
			m.type = STRING;
			m.u.s = new Common::String(ref.movie);
		}

		f.type = INT;
		f.u.i = ref.frameI;

		func_goto(f, m);

		return;
	}

	if (!_vm->getCurrentMovie()) {
		warning("Lingo::func_play(): no movie");
		return;
	}

	if (movie.type != VOID) {
		ref.movie = _vm->getCurrentMovie()->_movieArchive->getPathName().toString(g_director->_dirSeparator);
	}
	ref.frameI = _vm->getCurrentMovie()->getScore()->getCurrentFrameNum();

	// if we are issuing play command from script channel script. then play done should return to next frame
	if (g_lingo->_currentChannelId == 0)
		ref.frameI++;

	stage->_movieStack.push_back(ref);

	func_goto(frame, movie);
	_freezePlay = true;
}

void Lingo::func_cursor(Datum cursorDatum) {
	Window *stage = _vm->getCurrentWindow();
	Score *score = stage->getCurrentMovie()->getScore();
	if (cursorDatum.type == ARRAY){
		score->_defaultCursor.readFromCast(cursorDatum);
	} else {
		score->_defaultCursor.readFromResource(cursorDatum);
	}
	score->_cursorDirty = true;
}

void Lingo::func_beep(int repeats) {
	for (int r = 1; r <= repeats; r++) {
		_vm->getCurrentWindow()->getSoundManager()->systemBeep();
		if (r < repeats)
			g_director->delayMillis(400);
	}
}

int Lingo::func_marker(int m) 	{
	if (!_vm->getCurrentMovie())
		return 0;

	Window *stage = _vm->getCurrentWindow();
	Score *score = stage->getCurrentMovie()->getScore();
	int labelNumber = score->getCurrentLabelNumber();
	if (m != 0) {
		if (m < 0) {
			for (int marker = 0; marker > m; marker--)
				labelNumber = score->getPreviousLabelNumber(labelNumber);
		} else {
			for (int marker = 0; marker < m; marker++)
				labelNumber = score->getNextLabelNumber(labelNumber);
		}
	}

	return labelNumber;
}

uint16 Lingo::func_label(Datum &label) {
	Window *stage = _vm->getCurrentWindow();
	Score *score = stage->getCurrentMovie()->getScore();

	if (!score->_labels)
		return 0;

	if (label.type == STRING)
		return score->getLabel(*label.u.s);

	int num = CLIP<int>(label.asInt() - 1, 0, score->_labels->size() - 1);

	uint16 res = score->getNextLabelNumber(0);

	while (--num > 0)
		res = score->getNextLabelNumber(res);

	return res;
}

} // End of namespace Director
