/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _ACE_UTILS_FETCHMODE_H_
#define _ACE_UTILS_FETCHMODE_H_

#include <ace/types.h>
#include <ace/utils/extview.h>

static inline UBYTE fetchModeGetBitplaneFmode(const tVPort *pVPort) {
#ifdef ACE_USE_AGA_FEATURES
	return pVPort->ubFmode & 0x03;
#else
	(void)pVPort;
	return 0;
#endif
}

static inline UWORD fetchModeGetFetchBlockPixels(const tVPort *pVPort) {
	UBYTE ubBitplaneFmode = fetchModeGetBitplaneFmode(pVPort);

	if(ubBitplaneFmode == 3) {
		return 64;
	}

	if(ubBitplaneFmode == 1 || ubBitplaneFmode == 2) {
		return 32;
	}

	return 16;
}

static inline UWORD fetchModeGetDDfStep(const tVPort *pVPort) {
	UWORD uwWidth = pVPort->pView->uwWidth;
	UBYTE ubBitplaneFmode = fetchModeGetBitplaneFmode(pVPort);

	if(ubBitplaneFmode == 1 || ubBitplaneFmode == 2) {
		return ((uwWidth / 32) - 1) * 16;
	}

	if(ubBitplaneFmode == 3) {
		return ((uwWidth / 64) - 1) * 32;
	}

	return ((uwWidth / 16) - 1) * 8;
}

static inline UWORD fetchModeGetScrollPrefetchBytes(const tVPort *pVPort) {
	UBYTE ubBitplaneFmode = fetchModeGetBitplaneFmode(pVPort);

	if(ubBitplaneFmode == 1 || ubBitplaneFmode == 2) {
		return 4;
	}

	if(ubBitplaneFmode == 3) {
		return 8;
	}

	return 2;
}

static inline UWORD fetchModeGetScrollDDfStartAdjust(const tVPort *pVPort) {
	UBYTE ubBitplaneFmode = fetchModeGetBitplaneFmode(pVPort);

	if(ubBitplaneFmode == 1 || ubBitplaneFmode == 2) {
		return 16;
	}

	if(ubBitplaneFmode == 3) {
		return 32;
	}

	return 8;
}

static inline void fetchModeApplyXScrollCopper(
	const tVPort *pVPort, UWORD *pDDfStrt, UWORD *pModulo
) {
	if(pVPort->eFlags & VP_FLAG_HIRES) {
		*pDDfStrt -= 8; // two more hires 4-part bitplane fetch patterns
		*pModulo -= 4;
		return;
	}

	*pDDfStrt -= fetchModeGetScrollDDfStartAdjust(pVPort);
	*pModulo -= fetchModeGetScrollPrefetchBytes(pVPort);
}

static inline void fetchModeApplyScrollBufferXScrollCopper(
	const tVPort *pVPort, UWORD *pDDfStrt, UWORD *pDDfStop, UWORD *pModulo
) {
	if(pVPort->eFlags & VP_FLAG_HIRES) {
		*pDDfStrt -= 8; // for scroll reasons
		// Start/stop one 4-step bitplane fetch pattern later: 3120
		*pDDfStrt += 4;
		*pDDfStop += 4;

		// One word more for fetch
		*pModulo -= 2;
	}
	else {
		*pDDfStrt -= fetchModeGetScrollDDfStartAdjust(pVPort);
		*pModulo -= fetchModeGetScrollPrefetchBytes(pVPort);
	}
}

static inline LONG fetchModeGetInitialBplOffset(const tVPort *pVPort) {
	if(pVPort->eFlags & VP_FLAG_HIRES) {
		return -4;
	}

	return -(LONG)fetchModeGetScrollPrefetchBytes(pVPort);
}

static inline UWORD fetchModeCalcBplShift(const tVPort *pVPort, UWORD uwScrollX) {
	UBYTE ubBitplaneFmode = fetchModeGetBitplaneFmode(pVPort);
	UWORD uwShift;

	if(ubBitplaneFmode == 3) {
		uwShift = (64 - (uwScrollX & 0x3F)) & 0x3F;
	}
	else if(ubBitplaneFmode == 1 || ubBitplaneFmode == 2) {
		uwShift = (32 - (uwScrollX & 0x1F)) & 0x1F;
	}
	else {
		uwShift = (16 - (uwScrollX & 0xF)) & 0xF;
	}

	if(pVPort->eFlags & VP_FLAG_HIRES) {
		uwShift >>= 1;
	}

	// AGA BPLCON1 (PF1H/PF2H delay):
	// bits 0-3 = PF1H[0:3] (delay 0-15 for odd planes)
	// bits 4-7 = PF2H[0:3] (delay 0-15 for even planes)
	// bits 10-11 = PF1H[6:7] (delay 16, 32 for odd planes)
	// bits 14-15 = PF2H[6:7] (delay 16, 32 for even planes)
	UWORD uwLow = uwShift & 0xF;
	UWORD uwHigh = (uwShift >> 4) & 0x3;
	return uwLow | (uwLow << 4) | (uwHigh << 10) | (uwHigh << 14);
}

static inline UBYTE fetchModeGetCopWaitX(const tVPort *pVPort, UWORD uwDDfStop) {
	UBYTE ubBpp = pVPort->ubBpp;
	
	// In all FMODEs (0, 1, 2, 3), the fetch sequence takes exactly `ubBpp` color clocks 
	// because wider fetches use wider bus and fast page mode to fetch more bits in the same time.
	UWORD uwFetchClocks = ubBpp;
	
	UWORD uwWaitX = uwDDfStop + uwFetchClocks + 2;
	if(uwWaitX > 0xDF) {
		uwWaitX = 0xDF;
	}
	return (UBYTE)(uwWaitX & 0xFE);
}

static inline LONG fetchModeCalcBplOffsetX(const tVPort *pVPort, UWORD uwScrollX) {
	UBYTE ubBitplaneFmode = fetchModeGetBitplaneFmode(pVPort);
	LONG lBplAddX = (((LONG)uwScrollX - 1) >> 4) << 1;

	if(pVPort->eFlags & VP_FLAG_HIRES) {
		return lBplAddX - 2;
	}

	if(ubBitplaneFmode == 3) {
		return (((LONG)uwScrollX - 1) >> 6) << 3;
	}

	if(ubBitplaneFmode == 1 || ubBitplaneFmode == 2) {
		return (((LONG)uwScrollX - 1) >> 5) << 2;
	}

	return lBplAddX;
}

#endif // _ACE_UTILS_FETCHMODE_H_
