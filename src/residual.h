/* SPDX-License-Identifier: GPL-3.0-or-later */
#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <utility>

namespace qmgc {
namespace detail {

struct ResidualChoice {
    bool found = false;
    uint8_t nb_bits = 0;
    uint16_t idx = 0;
};

void init_resid_lut();
std::optional<std::pair<uint8_t, uint16_t>> best_residual(uint16_t pred, uint16_t actual);

} // namespace detail
} // namespace qmgc
