import sys
import struct
from PIL import Image
QMAGE_ORI_DELTA = (
    0xFFE0,0x0020,0xFFFF,0xF7E0,0xF800,0xFFDF,0x0001,0x0800,0x0021,0x0820,0x0841,0xF7BF,0xF7DF,0x0821,0xF7C0,0xFFBF,
    0x0041,0xFFC0,0x0861,0x0840,0x001F,0x0862,0xFFE1,0xF79F,0x0040,0xF7FF,0xF820,0x0842,0x1082,0x07E0,0xF79E,0x0801,
    0x1061,0x1062,0x07FF,0xEF7E,0xEF9F,0xF801,0xF7BE,0xEFBF,0x0022,0x0882,0xEFE0,0xEFC0,0x07DF,0x1041,0x081F,0x10A3,
    0x0042,0x1081,0xEF9E,0xF7E1,0xF821,0xF77E,0xFF9F,0x10A2,0x1040,0xFFBE,0x1083,0x1020,0xEF5D,0xEF7F,0xFFDE,0xEF5E,
    0x0002,0x0061,0xF7A0,0x18C3,0xEF7D,0x0822,0xFF9E,0xF000,0x0860,0xE73D,0xEFA0,0x0881,0xEFDF,0x07BF,0xF77F,0x0062,
    0xFFFE,0x18A3,0x10C3,0x0883,0x18E4,0x1060,0x0863,0x1000,0x18A2,0xE71C,0xF7DE,0x1021,0x1042,0x18C4,0xEF3D,0x18E3,
    0xF841,0xE75D,0x1881,0xE73C,0xE7C0,0x18C2,0xE75E,0xF77D,0x10A4,0x07C0,0xF81F,0x10C4,0xFF7E,0x003F,0xF840,0xE79F,
    0x2104,0xFFA0,0xFFC1,0x18A1,0xE77F,0xEFBE,0xF79D,0x1882,0x10A1,0xE71D,0x1063,0xE7A0,0xE7E0,0xF7C1,0x1861,0x08A3,
    0x1840,0xE73E,0x083F,0xF001,0xDEFC,0xF75E,0x0060,0x2125,0x07E1,0x0FFF,0xEF5F,0xE77E,0xEFE1,0xE6FC,0x10C2,0x08A2,
    0x0023,0x1860,0xEF3C,0xEF3E,0x0043,0xF020,0xF75D,0x079F,0x101F,0xDEDB,0xEF5C,0xFFE2,0xF822,0x20E4,0x001E,0x0843,
    0xE7BF,0x20E3,0x0082,0xDEFB,0xEF9D,0x1904,0xFF7F,0x1820,0xE75F,0x0063,0xEF80,0x2105,0x1905,0xDEFD,0x2124,0xDF1C,
    0x2103,0x1080,0xDF1D,0xE6FB,0x07BE,0x2945,0xDEDC,0xF021,0x0FE0,0xE71B,0xEFFF,0xF842,0x20C2,0x079E,0xF802,0x0FDF,
    0x20E2,0x18E2,0xE780,0x07DE,0x1880,0xDFA0,0x20C1,0xFFBD,0x08A4,0x10E4,0x18C1,0xDFC0,0x103F,0x0003,0xDEBA,0xFF7D,
    0x1841,0xE800,0x20A1,0xEFC1,0xFF9D,0xF780,0x18E5,0x10E3,0x1084,0x0081,0x2966,0xFFDD,0x08C3,0x18A4,0xDEDA,0xFF5E,
    0xDF7F,0x1883,0xDEBB,0x2145,0xD6BB,0xF7BD,0x20C3,0x1903,0x077E,0x0884,0xF861,0xE71E,0x1001,0xE75C,0x0083,0xD6BA,
    0x07FE,0x2102,0xDF80,0x2081,0x0880,0xDF3E,0x2946,0xE6FD,0x2146,0xEF1C,0xDF9F,0xDF1E,0xE73F,0xE79E,0x20A2,0xD69A,
)
DIRS = ((-1,0), (0,-1), (-1,-1))
RESID_LUT = {}
for nb_bits in range(7):
    base = (2 << nb_bits) - 2
    for idx in range(1 << (nb_bits + 1)):
        ti = idx + base
        if ti < 256:
            v = QMAGE_ORI_DELTA[ti]
            if v not in RESID_LUT:
                RESID_LUT[v] = (nb_bits, idx)
class BitWriter:
    def __init__(self):
        self.buf = bytearray()
        self.bitpos = 0
    def write(self, val, n):
        for i in range(n - 1, -1, -1):
            bit = (val >> i) & 1
            if self.bitpos == 0:
                self.buf.append(0)
            if bit:
                self.buf[-1] |= (1 << (7 - self.bitpos))
            self.bitpos = (self.bitpos + 1) & 7
def rgb565(r, g, b):
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
def encode_value(n, out):
    while n >= 0xFF:
        out.append(0xFF)
        n -= 0xFF
    out.append(n)
def pad4(data):
    rem = (4 - (len(data) % 4)) % 4
    data.extend([0] * rem)
def to_pairs(px):
    if len(px) % 2:
        px.append(px[-1])
    return [px[2*i] | (px[2*i+1] << 16) for i in range(len(px) // 2)]
def encode_w2_depth1(pairs, max_table=256):
    counts = {}
    fso = []
    for p in pairs:
        if p not in counts:
            counts[p] = 1
            fso.append(p)
        else:
            counts[p] += 1
    if len(counts) == 1:
        tvals = [pairs[0]]
    else:
        ordered = sorted(fso, key=lambda x: -counts[x])
        tvals = [v for v in ordered if counts[v] >= 2][:max_table]
        if not tvals:
            tvals = [ordered[0]]
    index_map = {v: i for i, v in enumerate(tvals)}
    idx_stream = bytearray()
    run_stream = bytearray()
    lit_stream = bytearray()
    lit_count = 0
    i = 0
    n = len(pairs)
    while i < n:
        pair = pairs[i]
        if pair in index_map:
            j = i + 1
            while j < n and pairs[j] == pair:
                j += 1
            encode_value(index_map[pair] + 1, idx_stream)
            encode_value(j - i - 1, run_stream)
            i = j
        else:
            encode_value(0, idx_stream)
            lit_stream.extend(struct.pack('<I', pair))
            lit_count += 1
            i += 1
    pad4(idx_stream)
    pad4(run_stream)
    table = bytearray()
    for v in tvals:
        table.extend(struct.pack('<I', v))
    out = bytearray()
    out.extend(struct.pack('<IIII', len(tvals), len(idx_stream), len(run_stream), lit_count))
    out.extend(table)
    out.extend(idx_stream)
    out.extend(run_stream)
    out.extend(lit_stream)
    return out
def encode_still(w, h, rgb):
    px = [rgb565(rgb[i*3], rgb[i*3+1], rgb[i*3+2]) for i in range(w*h)]
    body = encode_w2_depth1(to_pairs(px))
    head = bytearray(b'QM\x0b\x00\x00\x81')
    head.extend(struct.pack('<HHxx', w, h))
    head.extend(body)
    return head
def gp(dst, w, h, x, y):
    if 0 <= x < w and 0 <= y < h:
        return dst[y * w + x]
    return 0
def choose_mode(dst, pixels, w, h, x, y):
    bw = min(4, w - x)
    bh = min(4, h - y)
    if x > 0:
        can_edge = True
        for j in range(bh):
            left = pixels[(y + j) * w + (x - 1)]
            for i in range(bw):
                if pixels[(y + j) * w + (x + i)] != left:
                    can_edge = False
                    break
            if not can_edge:
                break
        if can_edge:
            return 3
    bmode = 0
    bcopies = -1
    for mode in range(3):
        dx, dy = DIRS[mode]
        copies = 0
        blk = [[0]*4 for _ in range(4)]
        set_blk = [[False]*4 for _ in range(4)]
        for j in range(bh):
            for i in range(bw):
                actual = pixels[(y + j) * w + (x + i)]
                nx = x + i + dx
                ny = y + j + dy
                if x <= nx < x + 4 and y <= ny < y + 4 and set_blk[ny - y][nx - x]:
                    pred = blk[ny - y][nx - x]
                else:
                    pred = gp(dst, w, h, nx, ny)
                if pred == actual:
                    copies += 1
                blk[j][i] = actual
                set_blk[j][i] = True
        if copies > bcopies:
            bcopies = copies
            bmode = mode
    return bmode
def encode_a9ll_keyframe(w, h, pixels, total, ordinal, delay):
    bw1 = BitWriter()
    bw2 = BitWriter()
    gb3 = bytearray()
    dst = [0] * (w * h)
    for y in range(0, h, 4):
        for x in range(0, w, 4):
            mode = choose_mode(dst, pixels, w, h, x, y)
            bw1.write(mode, 2)
            if mode == 3:
                bh = min(4, h - y)
                bw_ = min(4, w - x)
                for j in range(bh):
                    for i in range(bw_):
                        dst[(y + j) * w + x + i] = dst[(y + j) * w + x - 1]
                continue
            dx, dy = DIRS[mode]
            cbp = 0
            ops = []
            k = 0
            for j in range(4):
                for i in range(4):
                    if x + i >= w or y + j >= h:
                        continue
                    actual = pixels[(y + j) * w + x + i]
                    pred = gp(dst, w, h, x + i + dx, y + j + dy)
                    if pred == actual:
                        cbp |= (1 << k)
                        ops.append((False, actual, pred))
                    else:
                        ops.append((True, actual, pred))
                    dst[(y + j) * w + x + i] = actual
                    k += 1
            gb3.extend(struct.pack('<H', cbp))
            for is_edge, actual, pred in ops:
                if is_edge:
                    diff = (actual - pred) & 0xFFFF
                    if diff in RESID_LUT:
                        nb, idx = RESID_LUT[diff]
                        bw2.write(nb, 3)
                        bw1.write(idx, nb + 1)
                    else:
                        bw2.write(7, 3)
                        gb3.extend(struct.pack('<H', actual))
    b1 = bw1.buf
    b2 = bw2.buf
    hsize = 24
    gb1_s = hsize + 8 + len(b1)
    gb3_s = gb1_s + len(b2)
    blk = bytearray(b'QM\x0b\x00\x80\x00')
    blk.extend(struct.pack('<HHxx', w, h))
    blk.extend(struct.pack('<IHHHxxII', 0, total, ordinal, delay & 0xFFFF, gb1_s, gb3_s))
    blk.extend(b1)
    blk.extend(b2)
    blk.extend(gb3)
    blk[12:16] = struct.pack('<I', len(blk))
    return blk
def mb_equal(a, b, w, h, x, y):
    mj = min(16, h - y)
    mi = min(16, w - x)
    for j in range(mj):
        idx = (y + j) * w + x
        if a[idx:idx+mi] != b[idx:idx+mi]:
            return False
    return True
def encode_a9ll_ani(w, h, pixels, ref, total, ordinal, delay):
    bw1 = BitWriter()
    lit = bytearray()
    dst = [0] * (w * h)
    for y in range(0, h, 16):
        for x in range(0, w, 16):
            if w - x >= 16 and h - y >= 16:
                if mb_equal(pixels, ref, w, h, x, y):
                    bw1.write(1, 1)
                    bw1.write(1, 1)
                    for j in range(16):
                        for i in range(16):
                            dst[(y + j) * w + x + i] = ref[(y + j) * w + x + i]
                else:
                    bw1.write(0, 1)
                    for j in range(0, 16, 4):
                        for i in range(0, 16, 4):
                            mode = choose_mode(dst, pixels, w, h, x + i, y + j)
                            bw1.write(mode, 2)
                            if mode == 3:
                                for jj in range(4):
                                    for ii in range(4):
                                        dst[(y + j + jj) * w + x + i + ii] = dst[(y + j + jj) * w + x + i - 1]
                                continue
                            dx, dy = DIRS[mode]
                            for jj in range(4):
                                for ii in range(4):
                                    pxx = x + i + ii
                                    pyy = y + j + jj
                                    actual = pixels[pyy * w + pxx]
                                    pred = gp(dst, w, h, pxx + dx, pyy + dy)
                                    if pred == actual:
                                        bw1.write(1, 1)
                                        dst[pyy * w + pxx] = pred
                                    else:
                                        bw1.write(0, 1)
                                        diff = (actual - pred) & 0xFFFF
                                        if diff in RESID_LUT:
                                            nb, idx = RESID_LUT[diff]
                                            bw1.write(nb, 3)
                                            bw1.write(idx, nb + 1)
                                        else:
                                            bw1.write(7, 3)
                                            lit.extend(struct.pack('<H', actual))
                                        dst[pyy * w + pxx] = actual
            else:
                bw1.write(0, 1)
                for yy in range(y, min(y + 16, h), 4):
                    for xx in range(x, min(x + 16, w), 4):
                        if xx + 4 <= w and yy + 4 <= h:
                            mode = choose_mode(dst, pixels, w, h, xx, yy)
                            bw1.write(mode, 2)
                            if mode == 3:
                                for jj in range(4):
                                    for ii in range(4):
                                        dst[(yy + jj) * w + xx + ii] = dst[(yy + jj) * w + xx - 1]
                                continue
                            dx, dy = DIRS[mode]
                            for jj in range(4):
                                for ii in range(4):
                                    actual = pixels[(yy + jj) * w + xx + ii]
                                    pred = gp(dst, w, h, xx + ii + dx, yy + jj + dy)
                                    if pred == actual:
                                        bw1.write(1, 1)
                                        dst[(yy + jj) * w + xx + ii] = pred
                                    else:
                                        bw1.write(0, 1)
                                        diff = (actual - pred) & 0xFFFF
                                        if diff in RESID_LUT:
                                            nb, idx = RESID_LUT[diff]
                                            bw1.write(nb, 3)
                                            bw1.write(idx, nb + 1)
                                        else:
                                            bw1.write(7, 3)
                                            lit.extend(struct.pack('<H', actual))
                                        dst[(yy + jj) * w + xx + ii] = actual
                        else:
                            for jj in range(4):
                                for ii in range(4):
                                    if xx + ii < w and yy + jj < h:
                                        val = pixels[(yy + jj) * w + xx + ii]
                                        lit.extend(struct.pack('<H', val))
                                        dst[(yy + jj) * w + xx + ii] = val
    pad4(bw1.buf)
    hsize = 24
    gb1_s = hsize + 8 + len(bw1.buf)
    blk = bytearray(b'QM\x0b\x00\x80\x00')
    blk.extend(struct.pack('<HHxx', w, h))
    blk.extend(struct.pack('<IHHHxxII', 0, total, ordinal, delay & 0xFFFF, gb1_s, 0))
    blk.extend(bw1.buf)
    blk.extend(lit)
    blk[12:16] = struct.pack('<I', len(blk))
    return blk
def main():
    args = sys.argv[1:]
    delay = 0
    force_ani = False
    scale = None
    i = 0
    inputs = []
    while i < len(args):
        if args[i] == '--delay':
            i += 1
            delay = int(args[i])
        elif args[i] == '--force-animation':
            force_ani = True
        elif args[i] == '--scale':
            i += 1
            x = args[i].lower().split('x')
            scale = (int(x[0]), int(x[1]))
        elif args[i] in ('-h', '--help'):
            sys.exit(0)
        else:
            inputs.append(args[i])
        i += 1
    if len(inputs) < 2:
        sys.exit(1)
    outpath = inputs.pop()
    frames = []
    for p in inputs:
        img = Image.open(p).convert('RGB')
        if scale:
            img = img.resize(scale, Image.NEAREST)
        frames.append((img.width, img.height, img.tobytes()))
    w0, h0, _ = frames[0]
    out_data = bytearray()
    if force_ani or len(frames) > 1:
        px_frames = []
        for w, h, rgb in frames:
            px = [rgb565(rgb[i*3], rgb[i*3+1], rgb[i*3+2]) for i in range(w*h)]
            px_frames.append(px)
        out_data.extend(encode_a9ll_keyframe(w0, h0, px_frames[0], len(frames), 1, delay))
        for i in range(1, len(frames)):
            out_data.extend(encode_a9ll_ani(w0, h0, px_frames[i], px_frames[i-1], len(frames), i + 1, delay))
    else:
        out_data.extend(encode_still(w0, h0, frames[0][2]))
    with open(outpath, 'wb') as f:
        f.write(out_data)
if __name__ == '__main__':
    main()
