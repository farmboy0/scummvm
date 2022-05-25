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
 */

#ifndef GRAPHICS_SCALER_XBRZ_H
#define GRAPHICS_SCALER_XBRZ_H

#define FORBIDDEN_SYMBOL_EXCEPTION_asctime
#define FORBIDDEN_SYMBOL_EXCEPTION_clock
#define FORBIDDEN_SYMBOL_EXCEPTION_ctime
#define FORBIDDEN_SYMBOL_EXCEPTION_difftime
#define FORBIDDEN_SYMBOL_EXCEPTION_getdate
#define FORBIDDEN_SYMBOL_EXCEPTION_gmtime
#define FORBIDDEN_SYMBOL_EXCEPTION_localtime
#define FORBIDDEN_SYMBOL_EXCEPTION_mktime
#define FORBIDDEN_SYMBOL_EXCEPTION_time
#define FORBIDDEN_SYMBOL_EXCEPTION_FILE

#include "graphics/scalerplugin.h"
#include "graphics/scaler/ctpl/ctpl_stl.h"

class xBRZScaler : public Scaler {
public:
	xBRZScaler(const Graphics::PixelFormat &format, int nThreads) : Scaler(format), _tpool(nThreads), _nThreads(nThreads)
	{
		_factor = 1;
	}
	uint increaseFactor() override;
	uint decreaseFactor() override;
protected:
	virtual void scaleIntern(const uint8 *srcPtr, uint32 srcPitch,
							uint8 *dstPtr, uint32 dstPitch, int width, int height, int x, int y) override;
private:
	ctpl::thread_pool _tpool;
	int _nThreads;
};


#endif
