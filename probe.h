/** ZCT/probe.h--Created 012608 **/
#include "zct.h"

#ifndef PROBE_H
#define PROBE_H

#ifdef EGBB

#	define EGBB_CACHE_SIZE (1 << 22) /* 4 MB */
#	define _NOTFOUND 32767
#	define WIN_SCORE 5000
#	define WIN_PLY   40

extern BOOL egbb_is_loaded;

#endif /* EGBB */

#endif /* PROBE_H */
