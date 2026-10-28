/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef QMGC_ENCODE_H
#define QMGC_ENCODE_H

#include "qmgc/types.h"
#include <cstdint>
#include <vector>

namespace qmgc {

std::vector<uint8_t> encode_still(const Image& img);
// qversion: 0x0f, 0x0b, or 0 (default) for automatic. Automatic tries 0x0f first
// and falls back to 0x0b when a frame exceeds the 16-bit length field that
// 0x0f headers use (0x0b headers carry a 32-bit length, so no such limit).
std::vector<uint8_t> encode_animation(const std::vector<Image>& frames, int delay, int qversion = 0);

} // namespace qmgc

#endif
