/*
 * OpenCoreFromClover.h
 *
 *  Created on: Sep 17, 2020
 *      Author: jief
 */

#ifndef OPENCOREFROMCLOVER_H_
#define OPENCOREFROMCLOVER_H_

#include <Library/DebugLib.h>

#undef _DEBUG
#undef DEBUG

#include "../rEFIt_UEFI/Platform/BootLog.h"

#define DEBUG(Expression) DebugLogForOC Expression

#undef MDEPKG_NDEBUG

#endif /* OPENCOREFROMCLOVER_H_ */
