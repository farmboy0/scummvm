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

#include "cruise/cruise_main.h"
#include "cruise/cruise.h"

namespace Cruise {

static const char *endTexts[] = {
	"The End",				// English
	"Fin",					// French
	"Ende",					// German
	"El Fin",				// Spanish
	"Fine"					// Italian
};

enum EndTextIndex {
	kEnglish = 0,
	kFrench = 1,
	kGerman = 2,
	kSpanish = 3,
	kItalian = 4
};

backgroundIncrustStruct backgroundIncrustHead;

void resetBackgroundIncrustList(backgroundIncrustStruct *pHead) {
	pHead->next = nullptr;
	pHead->prev = nullptr;
}

// blit background to another one
void addBackgroundIncrustSub1(int fileIdx, int X, int Y, char *ptr2, int16 scale, char *destBuffer, char *dataPtr) {
	assert((dataPtr != nullptr) && (*dataPtr != 0));

	buildPolyModel(X, Y, scale, ptr2, destBuffer, dataPtr);
}

void backupBackground(backgroundIncrustStruct *pIncrust, int X, int Y, int width, int height, uint8* pBackground) {
	pIncrust->saveWidth = width;
	pIncrust->saveHeight = height;
	pIncrust->saveSize = width * height;
	pIncrust->savedX = X;
	pIncrust->savedY = Y;

	pIncrust->ptr = (uint8 *)MemAlloc(width * height);
	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			int xp = j + X;
			int yp = i + Y;

			pIncrust->ptr[i * width + j] = ((xp < 0) || (yp < 0) || (xp >= 320) || (yp >= 200)) ?
				0 : pBackground[yp * 320 + xp];
		}
	}
}

void restoreBackground(backgroundIncrustStruct *pIncrust) {
	if (!pIncrust)
		return;
	if (pIncrust->type != 1)
		return;
	if (pIncrust->ptr == nullptr)
		return;

	uint8* pBackground = backgroundScreens[pIncrust->backgroundIdx];
	if (pBackground == nullptr)
		return;

	backgroundChanged[pIncrust->backgroundIdx] = true;

	int X = pIncrust->savedX;
	int Y = pIncrust->savedY;
	int width = pIncrust->saveWidth;
	int height = pIncrust->saveHeight;

	for (int i = 0; i < height; i++) {
		for (int j = 0; j < width; j++) {
			int xp = j + X;
			int yp = i + Y;

			if ((xp >= 0) && (yp >= 0) && (xp < 320) && (yp < 200))
				pBackground[yp * 320 + xp] = pIncrust->ptr[i * width + j];
		}
	}
}

backgroundIncrustStruct *addBackgroundIncrust(int16 overlayIdx,	int16 objectIdx, backgroundIncrustStruct *pHead, int16 scriptNumber, int16 scriptOverlay, int16 backgroundIdx, int16 saveBuffer) {
	objectParamsQuery params;
	getMultipleObjectParam(overlayIdx, objectIdx, &params);

	uint8 *ptr = filesDatabase[params.fileIdx].subData.ptr;

	// Don't process any further if not a sprite or polygon
	if (!ptr)
		return nullptr;

	if ((filesDatabase[params.fileIdx].subData.resourceType != OBJ_TYPE_SPRITE) &&
		(filesDatabase[params.fileIdx].subData.resourceType != OBJ_TYPE_POLY))
		return nullptr;

	uint8 *backgroundPtr = backgroundScreens[backgroundIdx];
	assert(backgroundPtr != nullptr);

	backgroundChanged[backgroundIdx] = true;
	backgroundIncrustStruct *currentHead = pHead;
	backgroundIncrustStruct *currentHead2 = currentHead->next;

	while (currentHead2) {
		currentHead = currentHead2;
		currentHead2 = currentHead->next;
	}

	backgroundIncrustStruct *newElement = (backgroundIncrustStruct *)mallocAndZero(sizeof(backgroundIncrustStruct));

	if (!newElement)
		return nullptr;

	newElement->next = currentHead->next;
	currentHead->next = newElement;

	if (!currentHead2)
		currentHead2 = pHead;

	newElement->prev = currentHead2->prev;
	currentHead2->prev = newElement;
	newElement->objectIdx = objectIdx;
	newElement->type = saveBuffer;
	newElement->backgroundIdx = backgroundIdx;
	newElement->overlayIdx = overlayIdx;
	newElement->scriptNumber = scriptNumber;
	newElement->scriptOverlayIdx = scriptOverlay;
	newElement->X = params.X;
	newElement->Y = params.Y;
	newElement->scale = params.scale;
	newElement->frame = params.fileIdx;
	newElement->spriteId = filesDatabase[params.fileIdx].subData.index;
	newElement->ptr = nullptr;
	Common::strcpy_s(newElement->name, filesDatabase[params.fileIdx].subData.name);

	if (_vm->getLanguage() != Common::RU_RUS && !strcmp(newElement->name, "FIN14.SET")) {	// "The End" (nothing for Russian version)
		const char *text;

		switch (_vm->getLanguage()) {
		case Common::EN_GRB:
		case Common::EN_ANY:
			text = endTexts[kEnglish];
			break;
		case Common::FR_FRA:
			text = endTexts[kFrench];
			break;
		case Common::DE_DEU:
			text = endTexts[kGerman];
			break;
		case Common::ES_ESP:
			text = endTexts[kSpanish];
			break;
		case Common::IT_ITA:
			text = endTexts[kItalian];
			break;
		default:
			text = endTexts[kEnglish];
		}

		_vm->sayText(text, Common::TextToSpeechManager::QUEUE);
	} else if (!strcmp(newElement->name, "CFACSP02.SET")) {	// Title
		if (_vm->getLanguage() == Common::FR_FRA) {
			_vm->sayText("Croisi\212re pour un Cadavre", Common::TextToSpeechManager::QUEUE);
		} else if (_vm->getLanguage() == Common::RU_RUS) {
			// "Круиз для мертвеца"
			_vm->sayText("\x8a\xe0\x93\xa8\xa7 \xa4\xab\xef \xac\xa5\xe0\xe2\xa2\xa5\xe6\xa0", Common::TextToSpeechManager::QUEUE);
		} else {
			_vm->sayText("Cruise for a Corpse", Common::TextToSpeechManager::QUEUE);
		}
	}

	if (filesDatabase[params.fileIdx].subData.resourceType == OBJ_TYPE_SPRITE) {
		// sprite
		int width = filesDatabase[params.fileIdx].width;
		int height = filesDatabase[params.fileIdx].height;
		if (saveBuffer == 1)
			backupBackground(newElement, newElement->X, newElement->Y, width, height, backgroundPtr);

		drawSprite(width, height, nullptr, filesDatabase[params.fileIdx].subData.ptr, newElement->Y,
			newElement->X, backgroundPtr, filesDatabase[params.fileIdx].subData.ptrMask);
	} else {
		// poly
		if (saveBuffer == 1) {
			int newX;
			int newY;
			int newScale;
			char *newFrame;

			int sizeTable[4];	// 0 = left, 1 = right, 2 = bottom, 3 = top

			// this function checks if the dataPtr is not 0, else it retrieves the data for X, Y, scale and DataPtr again (OLD: mainDrawSub1Sub1)
			flipPoly(params.fileIdx, (int16 *)filesDatabase[params.fileIdx].subData.ptr, params.scale, &newFrame, newElement->X, newElement->Y, &newX, &newY, &newScale);

			// this function fills the sizeTable for the poly (OLD: mainDrawSub1Sub2)
			getPolySize(newX, newY, newScale, sizeTable, (unsigned char*)newFrame);

			int width = (sizeTable[1] + 2) - (sizeTable[0] - 2) + 1;
			int height = sizeTable[3] - sizeTable[2] + 1;

			backupBackground(newElement, sizeTable[0] - 2, sizeTable[2], width, height, backgroundPtr);
		}

		addBackgroundIncrustSub1(params.fileIdx, newElement->X, newElement->Y, nullptr, params.scale, (char *)backgroundPtr, (char *)filesDatabase[params.fileIdx].subData.ptr);
	}

	return newElement;
}

void regenerateBackgroundIncrust(backgroundIncrustStruct *pHead) {
	lastAni[0] = 0;
	backgroundIncrustStruct *pl = pHead->next;

	while (pl) {
		backgroundIncrustStruct* pl2 = pl->next;

		int frame = pl->frame;
		if (frame < 0)
			error("regenerateBackgroundIncrust() : Unexpected use of negative frame index");

		if ((filesDatabase[frame].subData.ptr == nullptr) || (strcmp(pl->name, filesDatabase[frame].subData.name))) {
			frame = NUM_FILE_ENTRIES - 1;
			if (loadFile(pl->name, frame, pl->spriteId) < 0)
				frame = -1;
		}

		if (frame >= 0) {
			if (filesDatabase[frame].subData.resourceType == OBJ_TYPE_SPRITE) {
				// Sprite
				int width = filesDatabase[frame].width;
				int height = filesDatabase[frame].height;

				drawSprite(width, height, nullptr, filesDatabase[frame].subData.ptr, pl->Y, pl->X, backgroundScreens[pl->backgroundIdx], filesDatabase[frame].subData.ptrMask);
			} else {
				// Poly
				addBackgroundIncrustSub1(frame, pl->X, pl->Y, nullptr, pl->scale, (char *)backgroundScreens[pl->backgroundIdx], (char *)filesDatabase[frame].subData.ptr);
			}

			backgroundChanged[pl->backgroundIdx] = true;
		}

		pl = pl2;
	}

	lastAni[0] = 0;
}

void freeBackgroundIncrustList(backgroundIncrustStruct *pHead) {
	backgroundIncrustStruct *pCurrent = pHead->next;

	while (pCurrent) {
		backgroundIncrustStruct *pNext = pCurrent->next;

		if (pCurrent->ptr)
			MemFree(pCurrent->ptr);

		MemFree(pCurrent);
		pCurrent = pNext;
	}

	resetBackgroundIncrustList(pHead);
}

void removeBackgroundIncrust(int overlay, int idx, backgroundIncrustStruct * pHead) {
	objectParamsQuery params;

	getMultipleObjectParam(overlay, idx, &params);

	int x = params.X;
	int y = params.Y;

	backgroundIncrustStruct *pCurrent = pHead->next;
	while (pCurrent) {
		if ((pCurrent->overlayIdx == overlay || overlay == -1) && (pCurrent->objectIdx == idx || idx == -1) && (pCurrent->X == x) && (pCurrent->Y == y))
			pCurrent->type = -1;

		pCurrent = pCurrent->next;
	}

	backgroundIncrustStruct *pCurrentHead = pHead;
	pCurrent = pHead->next;

	while (pCurrent) {
		if (pCurrent->type == -1) {
			backgroundIncrustStruct *pNext = pCurrent->next;
			backgroundIncrustStruct *bx = pCurrentHead;

			bx->next = pNext;
			backgroundIncrustStruct *cx = pNext;

			if (!pNext)
				cx = pHead;

			bx = cx;
			bx->prev = pCurrent->next;

			if (pCurrent->ptr)
				MemFree(pCurrent->ptr);

			MemFree(pCurrent);
			pCurrent = pNext;
		} else {
			pCurrentHead = pCurrent;
			pCurrent = pCurrent->next;
		}
	}
}

void unmergeBackgroundIncrust(backgroundIncrustStruct * pHead, int ovl, int idx) {
	objectParamsQuery params;
	getMultipleObjectParam(ovl, idx, &params);

	int x = params.X;
	int y = params.Y;

	backgroundIncrustStruct *pl = pHead;
	backgroundIncrustStruct *pl2 = pl;
	pl = pl2->next;

	while (pl) {
		pl2 = pl;
		if ((pl->overlayIdx == ovl) || (ovl == -1)) {
			if ((pl->objectIdx == idx) || (idx == -1)) {
				if ((pl->X == x) && (pl->Y == y))
					restoreBackground(pl);
			}
		}

		pl = pl2->next;
	}
}

} // End of namespace Cruise
