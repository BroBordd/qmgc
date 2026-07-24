use rayon::prelude::*;
use std::collections::HashMap;
use std::env;
use std::fs::File;
use std::io::Write;
use std::sync::OnceLock;
static QMAGE_ORI_DELTA: [u16; 256] = [
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
];
static RESID_LUT: OnceLock<Vec<Option<(u8, u16)>>> = OnceLock::new();
fn get_lut() -> &'static Vec<Option<(u8, u16)>> {
    RESID_LUT.get_or_init(|| {
        let mut lut = vec![None; 65536];
        for nb_bits in 0..7 {
            let base = (2 << nb_bits) - 2;
            let count = 1 << (nb_bits + 1);
            for idx in 0..count {
                let ti = idx + base;
                if ti < 256 {
                    let v = QMAGE_ORI_DELTA[ti] as usize;
                    if lut[v].is_none() {
                        lut[v] = Some((nb_bits as u8, idx as u16));
                    }
                }
            }
        }
        lut
    })
}
const DIRS: [(isize, isize); 3] = [(-1, 0), (0, -1), (-1, -1)];
struct BitWriter { buf: Vec<u8>, bitpos: u8 }
impl BitWriter {
    fn new() -> Self { Self { buf: Vec::new(), bitpos: 0 } }
    fn write(&mut self, val: u32, n: u8) {
        for i in (0..n).rev() {
            let bit = (val >> i) & 1;
            if self.bitpos == 0 { self.buf.push(0); }
            if bit != 0 {
                let len = self.buf.len();
                self.buf[len - 1] |= 1 << (7 - self.bitpos);
            }
            self.bitpos = (self.bitpos + 1) & 7;
        }
    }
}
fn encode_value(mut n: u32, out: &mut Vec<u8>) {
    while n >= 0xFF {
        out.push(0xFF);
        n -= 0xFF;
    }
    out.push(n as u8);
}
fn pad4(data: &mut Vec<u8>) {
    let rem = (4 - (data.len() % 4)) % 4;
    data.extend(std::iter::repeat(0).take(rem));
}
fn put_u16le(out: &mut Vec<u8>, v: u16) { out.extend_from_slice(&v.to_le_bytes()); }
fn put_u32le(out: &mut Vec<u8>, v: u32) { out.extend_from_slice(&v.to_le_bytes()); }
fn gp(dst: &[u16], w: usize, h: usize, x: isize, y: isize) -> u16 {
    if x >= 0 && (x as usize) < w && y >= 0 && (y as usize) < h {
        dst[(y as usize) * w + (x as usize)]
    } else { 0 }
}
fn choose_mode(dst: &[u16], pixels: &[u16], w: usize, h: usize, x: usize, y: usize) -> u8 {
    let bw = 4.min(w - x);
    let bh = 4.min(h - y);
    if x > 0 {
        let mut can_edge = true;
        for j in 0..bh {
            let left = pixels[(y + j) * w + (x - 1)];
            for i in 0..bw {
                if pixels[(y + j) * w + (x + i)] != left { can_edge = false; break; }
            }
            if !can_edge { break; }
        }
        if can_edge { return 3; }
    }
    let mut best_mode = 0;
    let mut best_copies = -1_isize;
    for mode in 0..3 {
        let mut block = [[0u16; 4]; 4];
        let mut set = [[false; 4]; 4];
        let (dx, dy) = DIRS[mode];
        let mut copies = 0;
        for j in 0..bh {
            for i in 0..bw {
                let actual = pixels[(y + j) * w + (x + i)];
                let nx = (x + i) as isize + dx;
                let ny = (y + j) as isize + dy;
                let pred = if nx >= x as isize && nx < (x + 4) as isize && ny >= y as isize && ny < (y + 4) as isize && set[(ny - y as isize) as usize][(nx - x as isize) as usize] {
                    block[(ny - y as isize) as usize][(nx - x as isize) as usize]
                } else {
                    gp(dst, w, h, nx, ny)
                };
                if pred == actual { copies += 1; }
                block[j][i] = actual;
                set[j][i] = true;
            }
        }
        if copies > best_copies { best_copies = copies; best_mode = mode; }
    }
    best_mode as u8
}
fn encode_w2_depth1(pairs: &[u32]) -> Vec<u8> {
    let mut counts = HashMap::new();
    let mut fso = Vec::new();
    for &p in pairs {
        if !counts.contains_key(&p) { fso.push(p); }
        *counts.entry(p).or_insert(0) += 1;
    }
    let mut tvals = Vec::new();
    if counts.len() == 1 {
        tvals.push(pairs[0]);
    } else {
        fso.sort_by(|a, b| counts[b].cmp(&counts[a]));
        for &v in &fso {
            if counts[&v] >= 2 {
                tvals.push(v);
                if tvals.len() >= 256 { break; }
            }
        }
        if tvals.is_empty() { tvals.push(fso[0]); }
    }
    let index: HashMap<u32, u32> = tvals.iter().enumerate().map(|(i, &v)| (v, i as u32)).collect();
    let mut idx_s = Vec::new();
    let mut run_s = Vec::new();
    let mut lit_s = Vec::new();
    let mut lit_c = 0;
    let mut i = 0;
    while i < pairs.len() {
        let p = pairs[i];
        if let Some(&idx) = index.get(&p) {
            let mut j = i + 1;
            while j < pairs.len() && pairs[j] == p { j += 1; }
            encode_value(idx + 1, &mut idx_s);
            encode_value((j - i - 1) as u32, &mut run_s);
            i = j;
        } else {
            encode_value(0, &mut idx_s);
            lit_s.extend_from_slice(&p.to_le_bytes());
            lit_c += 1;
            i += 1;
        }
    }
    pad4(&mut idx_s);
    pad4(&mut run_s);
    let mut out = Vec::new();
    put_u32le(&mut out, tvals.len() as u32);
    put_u32le(&mut out, idx_s.len() as u32);
    put_u32le(&mut out, run_s.len() as u32);
    put_u32le(&mut out, lit_c);
    for v in tvals { put_u32le(&mut out, v); }
    out.extend(idx_s);
    out.extend(run_s);
    out.extend(lit_s);
    out
}
fn encode_a9ll_kf(w: usize, h: usize, pixels: &[u16], total: u16, ordinal: u16, delay: u16) -> Vec<u8> {
    let mut bw1 = BitWriter::new();
    let mut bw2 = BitWriter::new();
    let mut gb3 = Vec::new();
    let mut dst = vec![0; w * h];
    for y in (0..h).step_by(4) {
        for x in (0..w).step_by(4) {
            let mode = choose_mode(&dst, pixels, w, h, x, y);
            bw1.write(mode as u32, 2);
            if mode == 3 {
                let bh = 4.min(h - y);
                let bw_ = 4.min(w - x);
                for j in 0..bh {
                    for i in 0..bw_ { dst[(y + j) * w + x + i] = dst[(y + j) * w + x - 1]; }
                }
                continue;
            }
            let (dx, dy) = DIRS[mode as usize];
            let mut cbp = 0u16;
            let mut ops = Vec::new();
            let mut k = 0;
            for j in 0..4 {
                for i in 0..4 {
                    if x + i >= w || y + j >= h { continue; }
                    let actual = pixels[(y + j) * w + x + i];
                    let pred = gp(&dst, w, h, (x + i) as isize + dx, (y + j) as isize + dy);
                    if pred == actual {
                        cbp |= 1 << k;
                        ops.push((false, actual, pred));
                    } else {
                        ops.push((true, actual, pred));
                    }
                    dst[(y + j) * w + x + i] = actual;
                    k += 1;
                }
            }
            put_u16le(&mut gb3, cbp);
            let lut = get_lut();
            for (is_edge, actual, pred) in ops {
                if is_edge {
                    let diff = actual.wrapping_sub(pred);
                    if let Some((nb, idx)) = lut[diff as usize] {
                        bw2.write(nb as u32, 3);
                        bw1.write(idx as u32, nb + 1);
                    } else {
                        bw2.write(7, 3);
                        put_u16le(&mut gb3, actual);
                    }
                }
            }
        }
    }
    let gb1_s = (24 + 8 + bw1.buf.len()) as u32;
    let gb3_s = gb1_s + bw2.buf.len() as u32;
    let mut out = vec![b'Q', b'M', 0x0b, 0x00, 0x80, 0x00];
    put_u16le(&mut out, w as u16);
    put_u16le(&mut out, h as u16);
    out.extend_from_slice(&[0, 0]);
    put_u32le(&mut out, 0);
    put_u16le(&mut out, total);
    put_u16le(&mut out, ordinal);
    put_u16le(&mut out, delay);
    out.extend_from_slice(&[0, 0]);
    put_u32le(&mut out, gb1_s);
    put_u32le(&mut out, gb3_s);
    out.extend(&bw1.buf);
    out.extend(&bw2.buf);
    out.extend(gb3);
    let len = out.len() as u32;
    out[12..16].copy_from_slice(&len.to_le_bytes());
    out
}
fn mb_equal(a: &[u16], b: &[u16], w: usize, h: usize, x: usize, y: usize) -> bool {
    let mj = 16.min(h - y);
    let mi = 16.min(w - x);
    for j in 0..mj {
        let idx = (y + j) * w + x;
        if a[idx..idx + mi] != b[idx..idx + mi] { return false; }
    }
    true
}
fn encode_a9ll_ani(w: usize, h: usize, pixels: &[u16], ref_px: &[u16], total: u16, ordinal: u16, delay: u16) -> Vec<u8> {
    let mut bw1 = BitWriter::new();
    let mut lit = Vec::new();
    let mut dst = vec![0; w * h];
    for y in (0..h).step_by(16) {
        for x in (0..w).step_by(16) {
            if w - x >= 16 && h - y >= 16 {
                if mb_equal(pixels, ref_px, w, h, x, y) {
                    bw1.write(1, 1);
                    bw1.write(1, 1);
                    for j in 0..16 {
                        for i in 0..16 { dst[(y + j) * w + x + i] = ref_px[(y + j) * w + x + i]; }
                    }
                } else {
                    bw1.write(0, 1);
                    for j in (0..16).step_by(4) {
                        for i in (0..16).step_by(4) {
                            let mode = choose_mode(&dst, pixels, w, h, x + i, y + j);
                            bw1.write(mode as u32, 2);
                            if mode == 3 {
                                for jj in 0..4 {
                                    for ii in 0..4 { dst[(y + j + jj) * w + x + i + ii] = dst[(y + j + jj) * w + x + i - 1]; }
                                }
                                continue;
                            }
                            let (dx, dy) = DIRS[mode as usize];
                            for jj in 0..4 {
                                for ii in 0..4 {
                                    let actual = pixels[(y + j + jj) * w + x + i + ii];
                                    let pred = gp(&dst, w, h, (x + i + ii) as isize + dx, (y + j + jj) as isize + dy);
                                    if pred == actual {
                                        bw1.write(1, 1);
                                        dst[(y + j + jj) * w + x + i + ii] = pred;
                                    } else {
                                        bw1.write(0, 1);
                                        let diff = actual.wrapping_sub(pred);
                                        let lut = get_lut();
                                        if let Some((nb, idx)) = lut[diff as usize] {
                                            bw1.write(nb as u32, 3);
                                            bw1.write(idx as u32, nb + 1);
                                        } else {
                                            bw1.write(7, 3);
                                            put_u16le(&mut lit, actual);
                                        }
                                        dst[(y + j + jj) * w + x + i + ii] = actual;
                                    }
                                }
                            }
                        }
                    }
                }
            } else {
                bw1.write(0, 1);
                for yy in (y..(y + 16).min(h)).step_by(4) {
                    for xx in (x..(x + 16).min(w)).step_by(4) {
                        if xx + 4 <= w && yy + 4 <= h {
                            let mode = choose_mode(&dst, pixels, w, h, xx, yy);
                            bw1.write(mode as u32, 2);
                            if mode == 3 {
                                for jj in 0..4 {
                                    for ii in 0..4 { dst[(yy + jj) * w + xx + ii] = dst[(yy + jj) * w + xx - 1]; }
                                }
                                continue;
                            }
                            let (dx, dy) = DIRS[mode as usize];
                            for jj in 0..4 {
                                for ii in 0..4 {
                                    let actual = pixels[(yy + jj) * w + xx + ii];
                                    let pred = gp(&dst, w, h, (xx + ii) as isize + dx, (yy + jj) as isize + dy);
                                    if pred == actual {
                                        bw1.write(1, 1);
                                        dst[(yy + jj) * w + xx + ii] = pred;
                                    } else {
                                        bw1.write(0, 1);
                                        let diff = actual.wrapping_sub(pred);
                                        let lut = get_lut();
                                        if let Some((nb, idx)) = lut[diff as usize] {
                                            bw1.write(nb as u32, 3);
                                            bw1.write(idx as u32, nb + 1);
                                        } else {
                                            bw1.write(7, 3);
                                            put_u16le(&mut lit, actual);
                                        }
                                        dst[(yy + jj) * w + xx + ii] = actual;
                                    }
                                }
                            }
                        } else {
                            for jj in 0..4 {
                                for ii in 0..4 {
                                    if xx + ii < w && yy + jj < h {
                                        let val = pixels[(yy + jj) * w + xx + ii];
                                        put_u16le(&mut lit, val);
                                        dst[(yy + jj) * w + xx + ii] = val;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    pad4(&mut bw1.buf);
    let gb1_s = (24 + 8 + bw1.buf.len()) as u32;
    let mut out = vec![b'Q', b'M', 0x0b, 0x00, 0x80, 0x00];
    put_u16le(&mut out, w as u16);
    put_u16le(&mut out, h as u16);
    out.extend_from_slice(&[0, 0]);
    put_u32le(&mut out, 0);
    put_u16le(&mut out, total);
    put_u16le(&mut out, ordinal);
    put_u16le(&mut out, delay);
    out.extend_from_slice(&[0, 0]);
    put_u32le(&mut out, gb1_s);
    put_u32le(&mut out, 0);
    out.extend(bw1.buf);
    out.extend(lit);
    let len = out.len() as u32;
    out[12..16].copy_from_slice(&len.to_le_bytes());
    out
}
fn main() {
    let mut args: Vec<String> = env::args().collect();
    let mut delay = 0;
    let mut force_ani = false;
    let mut scale = None;
    let mut inputs = Vec::new();
    let mut i = 1;
    while i < args.len() {
        if args[i] == "--delay" {
            i += 1;
            delay = args[i].parse().unwrap();
        } else if args[i] == "--force-animation" {
            force_ani = true;
        } else if args[i] == "--scale" {
            i += 1;
            let parts: Vec<&str> = args[i].split(|c| c == 'x' || c == 'X').collect();
            scale = Some((parts[0].parse().unwrap(), parts[1].parse().unwrap()));
        } else if args[i] == "-h" || args[i] == "--help" {
            std::process::exit(0);
        } else {
            inputs.push(args[i].clone());
        }
        i += 1;
    }
    if inputs.len() < 2 { std::process::exit(1); }
    let out_path = inputs.pop().unwrap();
    let mut frames = Vec::new();
    for p in inputs {
        let mut img = image::open(&p).unwrap().into_rgb8();
        let (mut w, mut h) = img.dimensions();
        if let Some((sw, sh)) = scale {
            img = image::imageops::resize(&img, sw, sh, image::imageops::FilterType::Nearest);
            w = sw;
            h = sh;
        }
        let px: Vec<u16> = img.pixels().map(|p| {
            let r = p[0] as u16; let g = p[1] as u16; let b = p[2] as u16;
            ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        }).collect();
        frames.push((w as usize, h as usize, px));
    }
    let (w0, h0) = (frames[0].0, frames[0].1);
    let mut out_data = Vec::new();
    if force_ani || frames.len() > 1 {
        let mut outputs: Vec<Vec<u8>> = vec![Vec::new(); frames.len()];
        outputs[0] = encode_a9ll_kf(w0, h0, &frames[0].2, frames.len() as u16, 1, delay);
        let refs: Vec<&[u16]> = frames.iter().map(|f| f.2.as_slice()).collect();
        outputs[1..].par_iter_mut().enumerate().for_each(|(i, out)| {
            *out = encode_a9ll_ani(w0, h0, refs[i + 1], refs[i], frames.len() as u16, (i + 2) as u16, delay);
        });
        for mut b in outputs { out_data.append(&mut b); }
    } else {
        let px = &frames[0].2;
        let mut pairs = Vec::with_capacity((px.len() + 1) / 2);
        let mut i = 0;
        while i < px.len() {
            let a = px[i] as u32;
            let b = if i + 1 < px.len() { px[i + 1] as u32 } else { a };
            pairs.push(a | (b << 16));
            i += 2;
        }
        let mut body = encode_w2_depth1(&pairs);
        out_data.extend_from_slice(&[b'Q', b'M', 0x0b, 0x00, 0x00, 0x81]);
        put_u16le(&mut out_data, w0 as u16);
        put_u16le(&mut out_data, h0 as u16);
        out_data.extend_from_slice(&[0, 0]);
        out_data.append(&mut body);
    }
    let mut f = File::create(out_path).unwrap();
    f.write_all(&out_data).unwrap();
}
