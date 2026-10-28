/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "qmgc/encode.h"

#include "encode_a9ll.h"
#include "encode_w2.h"
#include "qmgc/image.h"
#include "residual.h"
#include "util.h"

#include <vector>

namespace qmgc {

std::vector<uint8_t> encode_still(const Image& img) {
    detail::init_resid_lut();
    return detail::encode_still_w2(img);
}

static std::vector<uint8_t> encode_animation_ver(const std::vector<Image>& frames, int delay, int qversion);

std::vector<uint8_t> encode_animation(const std::vector<Image>& frames, int delay, int qversion) {
    if (qversion == 0) {
        try {
            return encode_animation_ver(frames, delay, 0x0f);
        } catch (const QmgError&) {
            // Most likely a delta/keyframe too large for the 16-bit length field.
            return encode_animation_ver(frames, delay, 0x0b);
        }
    }
    if (qversion != 0x0b && qversion != 0x0f) throw QmgError("unsupported qversion (use 11 or 15)");
    return encode_animation_ver(frames, delay, qversion);
}

static std::vector<uint8_t> encode_animation_ver(const std::vector<Image>& frames, int delay, int qversion) {
    if (frames.empty()) throw QmgError("no frames");
    int width0 = frames[0].width, height0 = frames[0].height;
    for (size_t i = 0; i < frames.size(); ++i) {
        if (frames[i].width != width0 || frames[i].height != height0) {
            throw QmgError("frame " + std::to_string(i + 1) + " is " + std::to_string(frames[i].width) + "x" +
                           std::to_string(frames[i].height) + ", expected " + std::to_string(width0) + "x" +
                           std::to_string(height0));
        }
    }
    detail::init_resid_lut();
    int total = static_cast<int>(frames.size());
    std::vector<std::vector<uint16_t>> frames565(total);
    detail::parallel_for(total, [&](int i) { frames565[i] = pixels_to_565(frames[i]); });
    std::vector<std::vector<uint8_t>> outputs(total);
    outputs[0] = detail::encode_a9ll_keyframe(width0, height0, frames565[0], total, 1, delay, qversion);
    if (total > 1) {
        detail::parallel_for(total - 1, [&](int k) {
            int i = k + 1;
            int ordinal = i + 1;
            outputs[i] = detail::encode_a9ll_ani(width0, height0, frames565[i], frames565[i - 1], total, ordinal, delay, qversion);
        });
    }
    std::vector<uint8_t> out;
    size_t total_size = 0;
    for (auto& o : outputs) total_size += o.size();
    out.reserve(total_size);
    for (auto& o : outputs) out.insert(out.end(), o.begin(), o.end());
    return out;
}

} // namespace qmgc
