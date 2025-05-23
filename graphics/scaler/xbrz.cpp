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

#include <cstring>
#include <future>
#include <vector>

#include "graphics/scaler/xbrz.h"
#include "graphics/scaler/xbrz/xbrz.h"

xBRZScaler::xBRZScaler(const Graphics::PixelFormat &format, int nThreads)
	: Scaler(format), _tpool(nThreads), _nThreads(nThreads), _CLUT16(nullptr) {

	_factor = 1;
	if (format.bytesPerPixel == 2) {
		initLUT(format);
	}
}

xBRZScaler::~xBRZScaler() {
	delete[] _CLUT16;
	_CLUT16 = nullptr;
}

void xBRZScaler::initLUT(const Graphics::PixelFormat &format) {
	uint8 a, r, g, b;

	_CLUT16 = new uint32[65536];

	for (int color = 65535; color >= 0; --color) {
		format.colorToARGB(color, a, r, g, b);
		uint32 argb = (a << 24) | (r << 16) | (g << 8) | b;
		_CLUT16[color] = argb;
	}
}

void xBRZScaler::scaleIntern(const uint8 *srcPtr, uint32 srcPitch,
							uint8 *dstPtr, uint32 dstPitch, int width, int height, int x, int y) {

	xbrz::ScalerCfg cfg;
	int BytesPerPixel = sizeof(uint32_t);
	uint32_t *src = (uint32_t *)srcPtr;
	uint32_t *dst = (uint32_t *)dstPtr;

	// Make sure src is a rect with srcWidth == srcPitch and 32 bit color depth
	int srcLineCount = height;
	uint32 srcWidth = width;
	uint32 srcWidthInByte = BytesPerPixel * srcWidth;
	if (srcPitch > srcWidthInByte || _format.bytesPerPixel == 2) {
		src =  (uint32_t *)malloc(srcWidthInByte * srcLineCount);
		uint32_t *tmp = src;
		for (int i = 0; i < height; ++i) {
			int lineOffset = i * srcPitch;
			const uint8 *copyStart = srcPtr + lineOffset;
			if (_format.bytesPerPixel == 2) {
				for (uint32 j = 0; j < srcWidth; ++j) {
					uint16 color = *(uint16 *)(copyStart + (j << 1));
					*(tmp + j) = _CLUT16[color];
				}
			} else {
				std::memcpy(tmp, copyStart, srcWidthInByte);
			}
			tmp += srcWidth;
		}
	}

	int dstLineCount = _factor * srcLineCount;
	uint32 dstWidth = _factor * srcWidth;
	uint32 dstWidthInByte = BytesPerPixel * dstWidth;
	if (dstPitch > dstWidthInByte || _format.bytesPerPixel == 2) {
		dst = (uint32_t *)malloc(dstWidthInByte * dstLineCount);
	}

	int linesPerThread = srcLineCount / _nThreads;
	if (linesPerThread < 8)
		linesPerThread = 8;

	std::vector<std::future<void>> futures;

	int scaleStart = 0;
	while(scaleStart < srcLineCount) {
		int scaleEnd = scaleStart + linesPerThread;
		auto future = _tpool.push(
				[=](int threadId) {
					xbrz::scale(_factor, src, dst, srcWidth, srcLineCount,
							xbrz::ColorFormat::ARGB, cfg, scaleStart, scaleEnd);
				}
		);
		futures.emplace_back(std::move(future));
		scaleStart = scaleEnd;
	}

	Common::for_each(futures.begin(), futures.end(),
			[](std::future<void>& future) {
				future.get();
			});

	if (dstPitch > dstWidthInByte || _format.bytesPerPixel == 2) {
		uint8 *tmp = dstPtr;
		for (int i = 0; i < dstLineCount; ++i) {
			int lineOffset = i * dstWidth;
			const uint32 *copyStart = dst + lineOffset;
			if (_format.bytesPerPixel == 2) {
				for (uint32 j = 0; j < dstWidth; ++j) {
					uint32 argb = *(copyStart + j);
					uint16 color = _format.ARGBToColor(argb >> 24, argb >> 16, argb >> 8, argb);
					*(uint16 *)(tmp + (j << 1)) = color;
				}
			} else {
				std::memcpy(tmp, copyStart, dstWidthInByte);
			}
			tmp += dstPitch;
		}
		free(dst);
	}

	if (srcPitch > srcWidthInByte || _format.bytesPerPixel == 2) {
		free(src);
	}
}

uint xBRZScaler::increaseFactor() {
	if (_factor < xbrz::SCALE_FACTOR_MAX)
		setFactor(_factor + 1);
	return _factor;
}

uint xBRZScaler::decreaseFactor() {
	if (_factor > 1)
		setFactor(_factor - 1);
	return _factor;
}


class xBRZPlugin : public ScalerPluginObject {
public:
	xBRZPlugin();

	bool canDrawCursor() const override { return true; }
	uint extraPixels() const override { return 1; }
};

xBRZPlugin::xBRZPlugin() {
	for (uint i = 1; i <= xbrz::SCALE_FACTOR_MAX; ++i)
		_factors.push_back(i);
}

class xBRZ2Plugin final : public xBRZPlugin {
	Scaler *createInstance(const Graphics::PixelFormat &format) const override { return new xBRZScaler(format, 2); }
	const char *getName() const override { return "xbrz2"; }
	const char *getPrettyName() const override  { return "xBRZ (2 threads)"; }
};

class xBRZ4Plugin final : public xBRZPlugin {
	Scaler *createInstance(const Graphics::PixelFormat &format) const override { return new xBRZScaler(format, 4); }
	const char *getName() const override { return "xbrz4"; }
	const char *getPrettyName() const override  { return "xBRZ (4 threads)"; }
};

class xBRZ6Plugin final : public xBRZPlugin {
	Scaler *createInstance(const Graphics::PixelFormat &format) const override { return new xBRZScaler(format, 6); }
	const char *getName() const override { return "xbrz6"; }
	const char *getPrettyName() const override  { return "xBRZ (6 threads)"; }
};

class xBRZ8Plugin final : public xBRZPlugin {
	Scaler *createInstance(const Graphics::PixelFormat &format) const override { return new xBRZScaler(format, 8); }
	const char *getName() const override { return "xbrz8"; }
	const char *getPrettyName() const override  { return "xBRZ (8 threads)"; }
};

REGISTER_PLUGIN_STATIC(XBRZ2, PLUGIN_TYPE_SCALER, xBRZ2Plugin);
REGISTER_PLUGIN_STATIC(XBRZ4, PLUGIN_TYPE_SCALER, xBRZ4Plugin);
REGISTER_PLUGIN_STATIC(XBRZ6, PLUGIN_TYPE_SCALER, xBRZ6Plugin);
REGISTER_PLUGIN_STATIC(XBRZ8, PLUGIN_TYPE_SCALER, xBRZ8Plugin);
