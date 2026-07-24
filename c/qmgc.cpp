#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>
#include <png.h>
extern "C" {
#include <jpeglib.h>
#include <jerror.h>
}
#include <csetjmp>

static const uint16_t QMAGE_ORI_DELTA[256] = {
    0xFFE0, 0x0020, 0xFFFF, 0xF7E0, 0xF800, 0xFFDF, 0x0001, 0x0800,
    0x0021, 0x0820, 0x0841, 0xF7BF, 0xF7DF, 0x0821, 0xF7C0, 0xFFBF,
    0x0041, 0xFFC0, 0x0861, 0x0840, 0x001F, 0x0862, 0xFFE1, 0xF79F,
    0x0040, 0xF7FF, 0xF820, 0x0842, 0x1082, 0x07E0, 0xF79E, 0x0801,
    0x1061, 0x1062, 0x07FF, 0xEF7E, 0xEF9F, 0xF801, 0xF7BE, 0xEFBF,
    0x0022, 0x0882, 0xEFE0, 0xEFC0, 0x07DF, 0x1041, 0x081F, 0x10A3,
    0x0042, 0x1081, 0xEF9E, 0xF7E1, 0xF821, 0xF77E, 0xFF9F, 0x10A2,
    0x1040, 0xFFBE, 0x1083, 0x1020, 0xEF5D, 0xEF7F, 0xFFDE, 0xEF5E,
    0x0002, 0x0061, 0xF7A0, 0x18C3, 0xEF7D, 0x0822, 0xFF9E, 0xF000,
    0x0860, 0xE73D, 0xEFA0, 0x0881, 0xEFDF, 0x07BF, 0xF77F, 0x0062,
    0xFFFE, 0x18A3, 0x10C3, 0x0883, 0x18E4, 0x1060, 0x0863, 0x1000,
    0x18A2, 0xE71C, 0xF7DE, 0x1021, 0x1042, 0x18C4, 0xEF3D, 0x18E3,
    0xF841, 0xE75D, 0x1881, 0xE73C, 0xE7C0, 0x18C2, 0xE75E, 0xF77D,
    0x10A4, 0x07C0, 0xF81F, 0x10C4, 0xFF7E, 0x003F, 0xF840, 0xE79F,
    0x2104, 0xFFA0, 0xFFC1, 0x18A1, 0xE77F, 0xEFBE, 0xF79D, 0x1882,
    0x10A1, 0xE71D, 0x1063, 0xE7A0, 0xE7E0, 0xF7C1, 0x1861, 0x08A3,
    0x1840, 0xE73E, 0x083F, 0xF001, 0xDEFC, 0xF75E, 0x0060, 0x2125,
    0x07E1, 0x0FFF, 0xEF5F, 0xE77E, 0xEFE1, 0xE6FC, 0x10C2, 0x08A2,
    0x0023, 0x1860, 0xEF3C, 0xEF3E, 0x0043, 0xF020, 0xF75D, 0x079F,
    0x101F, 0xDEDB, 0xEF5C, 0xFFE2, 0xF822, 0x20E4, 0x001E, 0x0843,
    0xE7BF, 0x20E3, 0x0082, 0xDEFB, 0xEF9D, 0x1904, 0xFF7F, 0x1820,
    0xE75F, 0x0063, 0xEF80, 0x2105, 0x1905, 0xDEFD, 0x2124, 0xDF1C,
    0x2103, 0x1080, 0xDF1D, 0xE6FB, 0x07BE, 0x2945, 0xDEDC, 0xF021,
    0x0FE0, 0xE71B, 0xEFFF, 0xF842, 0x20C2, 0x079E, 0xF802, 0x0FDF,
    0x20E2, 0x18E2, 0xE780, 0x07DE, 0x1880, 0xDFA0, 0x20C1, 0xFFBD,
    0x08A4, 0x10E4, 0x18C1, 0xDFC0, 0x103F, 0x0003, 0xDEBA, 0xFF7D,
    0x1841, 0xE800, 0x20A1, 0xEFC1, 0xFF9D, 0xF780, 0x18E5, 0x10E3,
    0x1084, 0x0081, 0x2966, 0xFFDD, 0x08C3, 0x18A4, 0xDEDA, 0xFF5E,
    0xDF7F, 0x1883, 0xDEBB, 0x2145, 0xD6BB, 0xF7BD, 0x20C3, 0x1903,
    0x077E, 0x0884, 0xF861, 0xE71E, 0x1001, 0xE75C, 0x0083, 0xD6BA,
    0x07FE, 0x2102, 0xDF80, 0x2081, 0x0880, 0xDF3E, 0x2946, 0xE6FD,
    0x2146, 0xEF1C, 0xDF9F, 0xDF1E, 0xE73F, 0xE79E, 0x20A2, 0xD69A,
};
struct DirVec { int dx, dy; };
static const DirVec DIRS[3] = {{-1, 0}, {0, -1}, {-1, -1}};
struct QmgError : std::runtime_error {
    explicit QmgError(const std::string& msg) : std::runtime_error(msg) {}
};
struct ResidualChoice {
    bool found = false;
    uint8_t nb_bits = 0;
    uint16_t idx = 0;
};
static std::array<ResidualChoice, 65536> RESID_LUT;
static void init_resid_lut() {
    for (int nb_bits = 0; nb_bits < 7; ++nb_bits) {
        int base = (2 << nb_bits) - 2;
        int count = 1 << (nb_bits + 1);
        for (int idx = 0; idx < count; ++idx) {
            int ti = idx + base;
            if (ti >= 256) continue;
            uint16_t v = QMAGE_ORI_DELTA[ti];
            if (!RESID_LUT[v].found) {
                RESID_LUT[v] = {true, static_cast<uint8_t>(nb_bits), static_cast<uint16_t>(idx)};
            }
        }
    }
}
static inline std::optional<std::pair<uint8_t, uint16_t>> best_residual(uint16_t pred, uint16_t actual) {
    uint16_t diff = static_cast<uint16_t>(actual - pred);
    const ResidualChoice& c = RESID_LUT[diff];
    if (!c.found) return std::nullopt;
    return std::make_pair(c.nb_bits, c.idx);
}
class BitWriter {
public:
    void write(uint32_t val, int n) {
        for (int i = n - 1; i >= 0; --i) {
            int bit = (val >> i) & 1;
            if (bitpos_ == 0) buf_.push_back(0);
            if (bit) buf_.back() |= static_cast<uint8_t>(1u << (7 - bitpos_));
            bitpos_ = (bitpos_ + 1) & 7;
        }
    }
    const std::vector<uint8_t>& bytes() const { return buf_; }
private:
    std::vector<uint8_t> buf_;
    int bitpos_ = 0;
};
static inline uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}
static void encode_value(uint32_t n, std::vector<uint8_t>& out) {
    while (n >= 0xFF) {
        out.push_back(0xFF);
        n -= 0xFF;
    }
    out.push_back(static_cast<uint8_t>(n & 0xFF));
}
static void pad4(std::vector<uint8_t>& data) {
    size_t rem = (4 - (data.size() % 4)) % 4;
    data.resize(data.size() + rem, 0);
}
static void put_u16le(std::vector<uint8_t>& out, uint16_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
}
static void put_u32le(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back(static_cast<uint8_t>(v & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
}
static void patch_u32le(std::vector<uint8_t>& out, size_t off, uint32_t v) {
    out[off] = static_cast<uint8_t>(v & 0xFF);
    out[off + 1] = static_cast<uint8_t>((v >> 8) & 0xFF);
    out[off + 2] = static_cast<uint8_t>((v >> 16) & 0xFF);
    out[off + 3] = static_cast<uint8_t>((v >> 24) & 0xFF);
}
template <typename Fn>
static void parallel_for(int n, Fn&& fn) {
    if (n <= 0) return;
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 4;
    int workers = static_cast<int>(std::min<unsigned>(hw, static_cast<unsigned>(n)));
    if (workers <= 1) {
        for (int i = 0; i < n; ++i) fn(i);
        return;
    }
    std::atomic<int> next{0};
    std::vector<std::thread> pool;
    for (int t = 0; t < workers; ++t) {
        pool.emplace_back([&]() {
            for (;;) {
                int i = next.fetch_add(1);
                if (i >= n) break;
                fn(i);
            }
        });
    }
    for (auto& th : pool) th.join();
}
struct Image {
    int width = 0, height = 0;
    std::vector<uint8_t> rgb;
};
static Image scale_image(const Image& src, int tw, int th) {
    if (tw < 1 || th < 1 || tw > 0xFFFF || th > 0xFFFF) {
        throw QmgError("invalid --scale dimensions " + std::to_string(tw) + "x" + std::to_string(th));
    }
    if (tw == src.width && th == src.height) return src;
    Image out;
    out.width = tw;
    out.height = th;
    out.rgb.resize(static_cast<size_t>(tw) * th * 3);
    for (int y = 0; y < th; ++y) {
        int sy = y * src.height / th;
        if (sy >= src.height) sy = src.height - 1;
        const uint8_t* srow = src.rgb.data() + static_cast<size_t>(sy) * src.width * 3;
        uint8_t* drow = out.rgb.data() + static_cast<size_t>(y) * tw * 3;
        for (int x = 0; x < tw; ++x) {
            int sx = x * src.width / tw;
            if (sx >= src.width) sx = src.width - 1;
            drow[x * 3 + 0] = srow[sx * 3 + 0];
            drow[x * 3 + 1] = srow[sx * 3 + 1];
            drow[x * 3 + 2] = srow[sx * 3 + 2];
        }
    }
    return out;
}
static Image load_png(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) throw QmgError(path + ": cannot open file");
    uint8_t sig[8];
    if (std::fread(sig, 1, 8, fp) != 8 || png_sig_cmp(sig, 0, 8) != 0) {
        std::fclose(fp);
        throw QmgError(path + ": not a valid PNG file");
    }
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) { std::fclose(fp); throw QmgError(path + ": png_create_read_struct failed"); }
    png_infop info = png_create_info_struct(png);
    if (!info) { png_destroy_read_struct(&png, nullptr, nullptr); std::fclose(fp); throw QmgError(path + ": png_create_info_struct failed"); }
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(fp);
        throw QmgError(path + ": PNG decode error");
    }
    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);
    png_uint_32 w, h;
    int bit_depth, color_type;
    png_get_IHDR(png, info, &w, &h, &bit_depth, &color_type, nullptr, nullptr, nullptr);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
    png_set_strip_alpha(png);
    png_set_interlace_handling(png);
    png_read_update_info(png, info);
    if (w < 1 || h < 1 || w > 0xFFFF || h > 0xFFFF) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(fp);
        throw QmgError(path + ": unsupported dimensions " + std::to_string(w) + "x" + std::to_string(h));
    }
    size_t rowbytes = png_get_rowbytes(png, info);
    Image img;
    img.width = static_cast<int>(w);
    img.height = static_cast<int>(h);
    img.rgb.resize(static_cast<size_t>(w) * h * 3);
    std::vector<png_bytep> rows(h);
    std::vector<uint8_t> buf(rowbytes * h);
    for (png_uint_32 y = 0; y < h; ++y) rows[y] = buf.data() + y * rowbytes;
    png_read_image(png, rows.data());
    png_read_end(png, nullptr);
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(fp);
    for (png_uint_32 y = 0; y < h; ++y) {
        std::memcpy(img.rgb.data() + static_cast<size_t>(y) * w * 3, rows[y], static_cast<size_t>(w) * 3);
    }
    return img;
}
struct JpegErrorMgr {
    struct jpeg_error_mgr pub;
    jmp_buf setjmp_buffer;
};
static void jpeg_error_exit(j_common_ptr cinfo) {
    JpegErrorMgr* err = reinterpret_cast<JpegErrorMgr*>(cinfo->err);
    std::longjmp(err->setjmp_buffer, 1);
}
static Image load_jpeg(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) throw QmgError(path + ": cannot open file");
    struct jpeg_decompress_struct cinfo;
    JpegErrorMgr jerr;
    cinfo.err = jpeg_std_error(&jerr.pub);
    jerr.pub.error_exit = jpeg_error_exit;
    if (setjmp(jerr.setjmp_buffer)) {
        char msg[JMSG_LENGTH_MAX];
        (*cinfo.err->format_message)(reinterpret_cast<j_common_ptr>(&cinfo), msg);
        jpeg_destroy_decompress(&cinfo);
        std::fclose(fp);
        throw QmgError(path + ": JPEG decode error (" + msg + ")");
    }
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, fp);
    jpeg_read_header(&cinfo, TRUE);
    cinfo.out_color_space = JCS_RGB;
    jpeg_start_decompress(&cinfo);
    int width = static_cast<int>(cinfo.output_width);
    int height = static_cast<int>(cinfo.output_height);
    if (width < 1 || height < 1 || width > 0xFFFF || height > 0xFFFF) {
        jpeg_destroy_decompress(&cinfo);
        std::fclose(fp);
        throw QmgError(path + ": unsupported dimensions " + std::to_string(width) + "x" + std::to_string(height));
    }
    int row_stride = width * cinfo.output_components;
    Image img;
    img.width = width;
    img.height = height;
    img.rgb.resize(static_cast<size_t>(width) * height * 3);
    std::vector<uint8_t> rowbuf(row_stride);
    uint8_t* rowptr[1];
    while (cinfo.output_scanline < cinfo.output_height) {
        rowptr[0] = rowbuf.data();
        jpeg_read_scanlines(&cinfo, rowptr, 1);
        std::memcpy(img.rgb.data() + static_cast<size_t>(cinfo.output_scanline - 1) * width * 3, rowbuf.data(),
                    static_cast<size_t>(row_stride));
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    std::fclose(fp);
    return img;
}
static Image load_image(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "rb");
    if (!fp) throw QmgError(path + ": cannot open file");
    uint8_t magic[8] = {0};
    size_t n = std::fread(magic, 1, 8, fp);
    std::fclose(fp);
    (void)n;
    static const uint8_t PNG_SIG[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    if (std::memcmp(magic, PNG_SIG, 8) == 0) return load_png(path);
    if (magic[0] == 0xFF && magic[1] == 0xD8) return load_jpeg(path);
    throw QmgError(path + ": not a recognized PNG or JPEG file");
}
static std::vector<uint16_t> pixels_to_565(const Image& img) {
    std::vector<uint16_t> out(static_cast<size_t>(img.width) * img.height);
    parallel_for(img.height, [&](int y) {
        const uint8_t* row = img.rgb.data() + static_cast<size_t>(y) * img.width * 3;
        uint16_t* orow = out.data() + static_cast<size_t>(y) * img.width;
        for (int x = 0; x < img.width; ++x) {
            orow[x] = rgb565(row[x * 3 + 0], row[x * 3 + 1], row[x * 3 + 2]);
        }
    });
    return out;
}
static std::vector<uint32_t> to_pairs(const std::vector<uint16_t>& px) {
    std::vector<uint16_t> vals = px;
    if (vals.size() % 2) vals.push_back(vals.back());
    std::vector<uint32_t> out(vals.size() / 2);
    parallel_for(static_cast<int>(out.size()), [&](int i) {
        out[i] = static_cast<uint32_t>(vals[2 * i]) | (static_cast<uint32_t>(vals[2 * i + 1]) << 16);
    });
    return out;
}
static std::vector<uint8_t> encode_w2_depth1(const std::vector<uint32_t>& pairs, size_t max_table = 256) {
    std::unordered_map<uint32_t, uint32_t> counts;
    std::vector<uint32_t> first_seen_order;
    counts.reserve(pairs.size());
    for (uint32_t p : pairs) {
        auto it = counts.find(p);
        if (it == counts.end()) {
            counts.emplace(p, 1);
            first_seen_order.push_back(p);
        } else {
            ++it->second;
        }
    }
    std::vector<uint32_t> table_vals;
    if (counts.size() == 1) {
        table_vals.push_back(pairs[0]);
    } else {
        std::vector<uint32_t> ordered = first_seen_order;
        std::stable_sort(ordered.begin(), ordered.end(), [&](uint32_t a, uint32_t b) {
            return counts[a] > counts[b];
        });
        for (uint32_t v : ordered) {
            if (counts[v] >= 2) {
                table_vals.push_back(v);
                if (table_vals.size() >= max_table) break;
            }
        }
        if (table_vals.empty()) {
            table_vals.push_back(ordered.front());
        }
    }
    std::unordered_map<uint32_t, uint32_t> index;
    index.reserve(table_vals.size());
    for (size_t i = 0; i < table_vals.size(); ++i) index.emplace(table_vals[i], static_cast<uint32_t>(i));
    std::vector<uint8_t> idx_stream, run_stream, lit_stream;
    uint32_t literal_count = 0;
    size_t i = 0, n = pairs.size();
    while (i < n) {
        uint32_t pair = pairs[i];
        auto it = index.find(pair);
        if (it != index.end()) {
            size_t j = i + 1;
            while (j < n && pairs[j] == pair) ++j;
            encode_value(it->second + 1, idx_stream);
            encode_value(static_cast<uint32_t>(j - i - 1), run_stream);
            i = j;
        } else {
            encode_value(0, idx_stream);
            lit_stream.push_back(static_cast<uint8_t>(pair & 0xFF));
            lit_stream.push_back(static_cast<uint8_t>((pair >> 8) & 0xFF));
            lit_stream.push_back(static_cast<uint8_t>((pair >> 16) & 0xFF));
            lit_stream.push_back(static_cast<uint8_t>((pair >> 24) & 0xFF));
            ++literal_count;
            ++i;
        }
    }
    pad4(idx_stream);
    pad4(run_stream);
    std::vector<uint8_t> table;
    for (uint32_t v : table_vals) put_u32le(table, v);
    std::vector<uint8_t> out;
    put_u32le(out, static_cast<uint32_t>(table_vals.size()));
    put_u32le(out, static_cast<uint32_t>(idx_stream.size()));
    put_u32le(out, static_cast<uint32_t>(run_stream.size()));
    put_u32le(out, literal_count);
    out.insert(out.end(), table.begin(), table.end());
    out.insert(out.end(), idx_stream.begin(), idx_stream.end());
    out.insert(out.end(), run_stream.begin(), run_stream.end());
    out.insert(out.end(), lit_stream.begin(), lit_stream.end());
    return out;
}
static std::vector<uint8_t> encode_still(const Image& img) {
    std::vector<uint32_t> pairs = to_pairs(pixels_to_565(img));
    std::vector<uint8_t> body = encode_w2_depth1(pairs);
    std::vector<uint8_t> header = {'Q', 'M', 0x0b, 0x00, 0x00, 0x81};
    put_u16le(header, static_cast<uint16_t>(img.width));
    put_u16le(header, static_cast<uint16_t>(img.height));
    header.push_back(0);
    header.push_back(0);
    header.insert(header.end(), body.begin(), body.end());
    return header;
}
static inline uint16_t gp(const std::vector<uint16_t>& dst, int w, int h, int x, int y) {
    if (x >= 0 && x < w && y >= 0 && y < h) return dst[static_cast<size_t>(y) * w + x];
    return 0;
}
static int choose_mode(const std::vector<uint16_t>& dst, const std::vector<uint16_t>& pixels,
                        int w, int h, int x, int y) {
    int bw = std::min(4, w - x);
    int bh = std::min(4, h - y);
    if (x > 0) {
        bool can_edge = true;
        for (int j = 0; j < bh && can_edge; ++j) {
            uint16_t left = pixels[static_cast<size_t>(y + j) * w + (x - 1)];
            for (int i = 0; i < bw; ++i) {
                if (pixels[static_cast<size_t>(y + j) * w + (x + i)] != left) { can_edge = false; break; }
            }
        }
        if (can_edge) return 3;
    }
    int best_mode = 0, best_copies = -1;
    for (int mode = 0; mode < 3; ++mode) {
        uint16_t block[4][4];
        bool set[4][4] = {{false}};
        int dx = DIRS[mode].dx, dy = DIRS[mode].dy;
        int copies = 0;
        for (int j = 0; j < bh; ++j) {
            for (int i = 0; i < bw; ++i) {
                uint16_t actual = pixels[static_cast<size_t>(y + j) * w + (x + i)];
                int nx = x + i + dx, ny = y + j + dy;
                uint16_t pred;
                if (nx >= x && nx < x + 4 && ny >= y && ny < y + 4 && set[ny - y][nx - x]) {
                    pred = block[ny - y][nx - x];
                } else {
                    pred = gp(dst, w, h, nx, ny);
                }
                if (pred == actual) ++copies;
                block[j][i] = actual;
                set[j][i] = true;
            }
        }
        if (copies > best_copies) { best_copies = copies; best_mode = mode; }
    }
    return best_mode;
}
static void emit_residual_kf(BitWriter& bw1, BitWriter& bw2, std::vector<uint8_t>& gb3,
                              uint16_t pred, uint16_t actual) {
    auto choice = best_residual(pred, actual);
    if (!choice) {
        bw2.write(7, 3);
        put_u16le(gb3, actual);
    } else {
        bw2.write(choice->first, 3);
        bw1.write(choice->second, choice->first + 1);
    }
}
static std::vector<uint8_t> encode_a9ll_keyframe(int width, int height, const std::vector<uint16_t>& pixels,
                                                  int total, int ordinal, int delay) {
    BitWriter bw1, bw2;
    std::vector<uint8_t> gb3;
    std::vector<uint16_t> dst(static_cast<size_t>(width) * height, 0);
    struct Op { bool is_edge; uint16_t actual, pred; };
    std::vector<Op> ops;
    ops.reserve(16);
    for (int y = 0; y < height; y += 4) {
        for (int x = 0; x < width; x += 4) {
            int mode = choose_mode(dst, pixels, width, height, x, y);
            bw1.write(static_cast<uint32_t>(mode), 2);
            if (mode == 3) {
                int bh = std::min(4, height - y), bw = std::min(4, width - x);
                for (int j = 0; j < bh; ++j)
                    for (int i = 0; i < bw; ++i)
                        dst[static_cast<size_t>(y + j) * width + x + i] = dst[static_cast<size_t>(y + j) * width + x - 1];
                continue;
            }
            int dx = DIRS[mode].dx, dy = DIRS[mode].dy;
            uint16_t cbp = 0;
            ops.clear();
            int k = 0;
            for (int j = 0; j < 4; ++j) {
                for (int i = 0; i < 4; ++i) {
                    if (x + i >= width || y + j >= height) continue;
                    uint16_t actual = pixels[static_cast<size_t>(y + j) * width + x + i];
                    uint16_t pred = gp(dst, width, height, x + i + dx, y + j + dy);
                    if (pred == actual) {
                        cbp |= static_cast<uint16_t>(1u << k);
                        ops.push_back({false, actual, pred});
                    } else {
                        ops.push_back({true, actual, pred});
                    }
                    dst[static_cast<size_t>(y + j) * width + x + i] = actual;
                    ++k;
                }
            }
            put_u16le(gb3, cbp);
            for (auto& op : ops) {
                if (op.is_edge) emit_residual_kf(bw1, bw2, gb3, op.pred, op.actual);
            }
        }
    }
    const auto& bits1 = bw1.bytes();
    const auto& bits2 = bw2.bytes();
    const size_t header_size = 24;
    uint32_t gb1_start = static_cast<uint32_t>(header_size + 8 + bits1.size());
    uint32_t gb3_start = static_cast<uint32_t>(gb1_start + bits2.size());
    std::vector<uint8_t> block;
    block.insert(block.end(), {'Q', 'M', 0x0b, 0x00, 0x80, 0x00});
    put_u16le(block, static_cast<uint16_t>(width));
    put_u16le(block, static_cast<uint16_t>(height));
    block.push_back(0); block.push_back(0);
    put_u32le(block, 0);
    put_u16le(block, static_cast<uint16_t>(total));
    put_u16le(block, static_cast<uint16_t>(ordinal));
    put_u16le(block, static_cast<uint16_t>(delay & 0xFFFF));
    block.push_back(0); block.push_back(0);
    put_u32le(block, gb1_start);
    put_u32le(block, gb3_start);
    block.insert(block.end(), bits1.begin(), bits1.end());
    block.insert(block.end(), bits2.begin(), bits2.end());
    block.insert(block.end(), gb3.begin(), gb3.end());
    patch_u32le(block, 12, static_cast<uint32_t>(block.size()));
    return block;
}
static void encode_residual_ani(BitWriter& bw1, std::vector<uint8_t>& lit, uint16_t pred, uint16_t actual) {
    auto choice = best_residual(pred, actual);
    if (!choice) {
        bw1.write(7, 3);
        put_u16le(lit, actual);
    } else {
        bw1.write(choice->first, 3);
        bw1.write(choice->second, choice->first + 1);
    }
}
static void encode_pixel_ani(BitWriter& bw1, std::vector<uint8_t>& lit, std::vector<uint16_t>& dst,
                              int w, int h, int x, int y, uint16_t actual, int dx, int dy) {
    uint16_t pred = gp(dst, w, h, x + dx, y + dy);
    if (pred == actual) {
        bw1.write(1, 1);
        dst[static_cast<size_t>(y) * w + x] = pred;
    } else {
        bw1.write(0, 1);
        encode_residual_ani(bw1, lit, pred, actual);
        dst[static_cast<size_t>(y) * w + x] = actual;
    }
}
static void encode_block2_ani(BitWriter& bw1, std::vector<uint8_t>& lit, std::vector<uint16_t>& dst,
                               const std::vector<uint16_t>& pixels, int w, int h, int x, int y) {
    int mode = choose_mode(dst, pixels, w, h, x, y);
    bw1.write(static_cast<uint32_t>(mode), 2);
    if (mode == 3) {
        int bh = std::min(4, h - y), bw = std::min(4, w - x);
        for (int j = 0; j < bh; ++j)
            for (int i = 0; i < bw; ++i)
                dst[static_cast<size_t>(y + j) * w + x + i] = dst[static_cast<size_t>(y + j) * w + x - 1];
        return;
    }
    int dx = DIRS[mode].dx, dy = DIRS[mode].dy;
    for (int j = 0; j < 4; ++j) {
        for (int i = 0; i < 4; ++i) {
            if (x + i < w && y + j < h) {
                encode_pixel_ani(bw1, lit, dst, w, h, x + i, y + j,
                                  pixels[static_cast<size_t>(y + j) * w + x + i], dx, dy);
            }
        }
    }
}
static bool mb_equal(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b, int w, int h,
                      int x, int y, int bw = 16, int bh = 16) {
    int mj = std::min(bh, h - y), mi = std::min(bw, w - x);
    for (int j = 0; j < mj; ++j) {
        const uint16_t* ra = a.data() + static_cast<size_t>(y + j) * w + x;
        const uint16_t* rb = b.data() + static_cast<size_t>(y + j) * w + x;
        if (std::memcmp(ra, rb, static_cast<size_t>(mi) * sizeof(uint16_t)) != 0) return false;
    }
    return true;
}
static std::vector<uint8_t> encode_a9ll_ani(int width, int height, const std::vector<uint16_t>& pixels,
                                             const std::vector<uint16_t>& ref, int total, int ordinal, int delay) {
    BitWriter bw1;
    std::vector<uint8_t> lit;
    std::vector<uint16_t> dst(static_cast<size_t>(width) * height, 0);
    for (int y = 0; y < height; y += 16) {
        for (int x = 0; x < width; x += 16) {
            bool full = (width - x >= 16) && (height - y >= 16);
            if (full) {
                if (mb_equal(pixels, ref, width, height, x, y)) {
                    bw1.write(1, 1);
                    bw1.write(1, 1);
                    for (int j = 0; j < 16; ++j)
                        for (int i = 0; i < 16; ++i)
                            dst[static_cast<size_t>(y + j) * width + x + i] = ref[static_cast<size_t>(y + j) * width + x + i];
                } else {
                    bw1.write(0, 1);
                    for (int j = 0; j < 16; j += 4)
                        for (int i = 0; i < 16; i += 4)
                            encode_block2_ani(bw1, lit, dst, pixels, width, height, x + i, y + j);
                }
            } else {
                bw1.write(0, 1);
                for (int yy = y; yy < std::min(y + 16, height); yy += 4) {
                    for (int xx = x; xx < std::min(x + 16, width); xx += 4) {
                        if (xx + 4 <= width && yy + 4 <= height) {
                            encode_block2_ani(bw1, lit, dst, pixels, width, height, xx, yy);
                        } else {
                            for (int j = 0; j < 4; ++j) {
                                for (int i = 0; i < 4; ++i) {
                                    if (xx + i < width && yy + j < height) {
                                        uint16_t val = pixels[static_cast<size_t>(yy + j) * width + xx + i];
                                        put_u16le(lit, val);
                                        dst[static_cast<size_t>(yy + j) * width + xx + i] = val;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    std::vector<uint8_t> bits = bw1.bytes();
    pad4(bits);
    const size_t header_size = 24;
    uint32_t gb1_start = static_cast<uint32_t>(header_size + 8 + bits.size());
    std::vector<uint8_t> block;
    block.insert(block.end(), {'Q', 'M', 0x0b, 0x00, 0x80, 0x00});
    put_u16le(block, static_cast<uint16_t>(width));
    put_u16le(block, static_cast<uint16_t>(height));
    block.push_back(0); block.push_back(0);
    put_u32le(block, 0);
    put_u16le(block, static_cast<uint16_t>(total));
    put_u16le(block, static_cast<uint16_t>(ordinal));
    put_u16le(block, static_cast<uint16_t>(delay & 0xFFFF));
    block.push_back(0); block.push_back(0);
    put_u32le(block, gb1_start);
    put_u32le(block, 0);
    block.insert(block.end(), bits.begin(), bits.end());
    block.insert(block.end(), lit.begin(), lit.end());
    patch_u32le(block, 12, static_cast<uint32_t>(block.size()));
    return block;
}
static std::vector<uint8_t> encode_animation(const std::vector<Image>& frames, int delay) {
    if (frames.empty()) throw QmgError("no frames");
    int width0 = frames[0].width, height0 = frames[0].height;
    for (size_t i = 0; i < frames.size(); ++i) {
        if (frames[i].width != width0 || frames[i].height != height0) {
            throw QmgError("frame " + std::to_string(i + 1) + " is " + std::to_string(frames[i].width) + "x" +
                            std::to_string(frames[i].height) + ", expected " + std::to_string(width0) + "x" + std::to_string(height0));
        }
    }
    int total = static_cast<int>(frames.size());
    std::vector<std::vector<uint16_t>> frames565(total);
    parallel_for(total, [&](int i) { frames565[i] = pixels_to_565(frames[i]); });
    std::vector<std::vector<uint8_t>> outputs(total);
    outputs[0] = encode_a9ll_keyframe(width0, height0, frames565[0], total, 1, delay);
    if (total > 1) {
        parallel_for(total - 1, [&](int k) {
            int i = k + 1;
            int ordinal = i + 1;
            outputs[i] = encode_a9ll_ani(width0, height0, frames565[i], frames565[i - 1], total, ordinal, delay);
        });
    }
    std::vector<uint8_t> out;
    size_t total_size = 0;
    for (auto& o : outputs) total_size += o.size();
    out.reserve(total_size);
    for (auto& o : outputs) out.insert(out.end(), o.begin(), o.end());
    return out;
}
static bool write_file(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t written = std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return written == data.size();
}
int main(int argc, char** argv) {
    init_resid_lut();
    std::vector<std::string> inputs;
    std::string output;
    int delay = 0;
    bool force_animation = false;
    int scale_w = 0, scale_h = 0;
    std::vector<std::string> args(argv + 1, argv + argc);
    std::vector<std::string> positional;
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == "--delay") {
            if (i + 1 >= args.size()) return 1;
            delay = std::stoi(args[++i]);
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
            std::printf("usage: qmg_encode [--delay N] [--force-animation] [--scale WxH] input... output.qmg\n");
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
        parallel_for(static_cast<int>(inputs.size()), [&](int i) { loaded[i] = load_image(inputs[i]); });
        if (scale_w > 0 && scale_h > 0) {
            for (auto& im : loaded) im = scale_image(im, scale_w, scale_h);
        }
        std::vector<uint8_t> data;
        if (force_animation || loaded.size() > 1) {
            data = encode_animation(loaded, delay);
        } else {
            data = encode_still(loaded[0]);
        }
        if (!write_file(output, data)) return 1;
    } catch (const std::exception& e) {
        return 1;
    }
    return 0;
}
