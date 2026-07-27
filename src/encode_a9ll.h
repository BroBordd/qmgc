/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <cstdint>
#include <vector>

namespace qmgc {
namespace detail {

std::vector<uint8_t> encode_a9ll_keyframe(int width, int height, const std::vector<uint16_t>& pixels,
                                         int total, int ordinal, int delay);
std::vector<uint8_t> encode_a9ll_ani(int width, int height, const std::vector<uint16_t>& pixels,
                                     const std::vector<uint16_t>& ref, int total, int ordinal, int delay);
void finalize_anim_block(std::vector<uint8_t>& block, int ordinal);

} // namespace detail
} // namespace qmgc
