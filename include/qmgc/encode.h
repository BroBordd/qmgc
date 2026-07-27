/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef QMGC_ENCODE_H
#define QMGC_ENCODE_H

#include "qmgc/types.h"
#include <cstdint>
#include <vector>

namespace qmgc {

std::vector<uint8_t> encode_still(const Image& img);
std::vector<uint8_t> encode_animation(const std::vector<Image>& frames, int delay);

} // namespace qmgc

#endif
