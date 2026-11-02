#pragma once
#include "mesh_scan.h"

#define MESH_REPORT_MAX_BYTES 98304U

/* Immutable released/frozen capture only. No decode or map-import authority. */
bool mesh_report_valid(const mesh_scan_t *scan);
/* NULL/0 measures exact CSV bytes, excluding terminal NUL. Output capacity must
 * include that NUL. Invalid input or insufficient capacity changes no output.
 * Aliases with the source/length are rejected before changing length. Other
 * failures set length to zero. No heap, global work buffer, filesystem or radio. */
bool mesh_report_encode(const mesh_scan_t *scan, char *output, size_t capacity,
                         size_t *length);
