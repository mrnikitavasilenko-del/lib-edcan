/*
 * edcan_defines.h
 *
 *  Created on: Jul 17, 2024
 *      Author: colorbass
 */

#ifndef INC_EDCAN_DEFINES_H_
#define INC_EDCAN_DEFINES_H_

/*
 * Compatibility entrypoint.
 *
 * Historical behavior: this header included implementation (.c) files directly.
 * This is convenient, but it MUST be included from exactly one translation unit,
 * otherwise you'll get multiple-definition linker errors.
 *
 * Recommended usage:
 *   - In exactly ONE .c file:
 *       #define EDCAN_IMPLEMENTATION
 *       #include "edcan_defines.h"
 *   - Everywhere else:
 *       #include "edcan.h"
 *
 * Backward compatible behavior:
 *   - If you don't define anything, we enable implementation by default.
 *   - Define EDCAN_NO_IMPLEMENTATION to include only headers (no .c includes).
 */
#if !defined(EDCAN_IMPLEMENTATION) && !defined(EDCAN_NO_IMPLEMENTATION)
#define EDCAN_IMPLEMENTATION
#endif

#ifdef EDCAN_IMPLEMENTATION
#include "edcan.c"
#include "edcan_buffer.c"
#include "edcan_handler.c"
#include "edcan_log.c"
#endif

#endif /* INC_EDCAN_DEFINES_H_ */
