/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "qmgc/encode.h"
#include "qmgc/image.h"

#include "util.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace qmgc;

int main(int argc, char** argv) {
    std::vector<std::string> inputs;
    std::string output;
    int delay = 0;
    bool force_animation = false;
    int scale_w = 0, scale_h = 0;
    int qversion = 0;
    std::vector<std::string> args(argv + 1, argv + argc);
    std::vector<std::string> positional;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--delay") {
            if (i + 1 >= args.size()) return 1;
            delay = std::stoi(args[++i]);
        } else if (args[i] == "--qversion") {
            if (i + 1 >= args.size()) return 1;
            qversion = std::stoi(args[++i]);
            if (qversion == 11) qversion = 0x0b;
            else if (qversion == 15) qversion = 0x0f;
            else {
                std::fprintf(stderr, "error: --qversion must be 11 or 15\n");
                return 1;
            }
        } else if (args[i] == "--force-animation") {
            force_animation = true;
        } else if (args[i] == "--scale") {
            if (i + 1 >= args.size()) return 1;
            const std::string& wh = args[++i];
            size_t x = wh.find('x');
            if (x == std::string::npos) x = wh.find('X');
            if (x == std::string::npos) return 1;
            scale_w = std::stoi(wh.substr(0, x));
            scale_h = std::stoi(wh.substr(x + 1));
        } else if (args[i] == "-h" || args[i] == "--help") {
            std::printf("usage: qmgc [--delay N] [--qversion 11|15] [--force-animation] [--scale WxH] input... output.qmg\n");
            return 0;
        } else {
            positional.push_back(args[i]);
        }
    }
    if (positional.size() < 2) return 1;
    output = positional.back();
    positional.pop_back();
    inputs = positional;
    try {
        std::vector<Image> loaded(inputs.size());
        detail::parallel_for(static_cast<int>(inputs.size()), [&](int i) { loaded[i] = load_image(inputs[i]); });
        if (scale_w > 0 && scale_h > 0) {
            for (auto& im : loaded) im = scale_image(im, scale_w, scale_h);
        }
        std::vector<uint8_t> data;
        if (force_animation || loaded.size() > 1) {
            data = encode_animation(loaded, delay, qversion);
        } else {
            data = encode_still(loaded[0]);
        }
        if (!detail::write_file(output, data)) return 1;
    } catch (const std::exception& e) { std::fprintf(stderr, "error: %s\n", e.what());
        return 1;
    }
    return 0;
}
