package main
import (
	"encoding/binary"
	"flag"
	"image"
	"image/color"
	_ "image/jpeg"
	_ "image/png"
	"os"
	"sort"
	"strconv"
	"strings"
	"sync"
)
var qmageOriDelta = [256]uint16{
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
}
type ResidChoice struct { found bool; nbBits byte; idx uint16 }
var residLUT [65536]ResidChoice
type DirVec struct { dx, dy int }
var dirs = [3]DirVec{{-1, 0}, {0, -1}, {-1, -1}}
func init() {
	for nbBits := 0; nbBits < 7; nbBits++ {
		base := (2 << nbBits) - 2
		count := 1 << (nbBits + 1)
		for idx := 0; idx < count; idx++ {
			ti := idx + base
			if ti < 256 {
				v := qmageOriDelta[ti]
				if !residLUT[v].found {
					residLUT[v] = ResidChoice{true, byte(nbBits), uint16(idx)}
				}
			}
		}
	}
}
type BitWriter struct {
	buf    []byte
	bitpos int
}
func (bw *BitWriter) WriteBits(val uint32, n int) {
	for i := n - 1; i >= 0; i-- {
		bit := (val >> i) & 1
		if bw.bitpos == 0 { bw.buf = append(bw.buf, 0) }
		if bit != 0 { bw.buf[len(bw.buf)-1] |= 1 << (7 - bw.bitpos) }
		bw.bitpos = (bw.bitpos + 1) & 7
	}
}
func rgb565(c color.Color) uint16 {
	r, g, b, _ := c.RGBA()
	return uint16((((r >> 8) >> 3) << 11) | (((g >> 8) >> 2) << 5) | ((b >> 8) >> 3))
}
func encodeValue(n uint32, out *[]byte) {
	for n >= 0xFF {
		*out = append(*out, 0xFF)
		n -= 0xFF
	}
	*out = append(*out, byte(n))
}
func pad4(data *[]byte) {
	rem := (4 - (len(*data) % 4)) % 4
	for i := 0; i < rem; i++ { *data = append(*data, 0) }
}
func putU16LE(out *[]byte, v uint16) {
	var b [2]byte
	binary.LittleEndian.PutUint16(b[:], v)
	*out = append(*out, b[:]...)
}
func putU32LE(out *[]byte, v uint32) {
	var b [4]byte
	binary.LittleEndian.PutUint32(b[:], v)
	*out = append(*out, b[:]...)
}
func gp(dst []uint16, w, h, x, y int) uint16 {
	if x >= 0 && x < w && y >= 0 && y < h { return dst[y*w+x] }
	return 0
}
func chooseMode(dst, pixels []uint16, w, h, x, y int) int {
	bw := 4
	if w-x < 4 { bw = w - x }
	bh := 4
	if h-y < 4 { bh = h - y }
	if x > 0 {
		canEdge := true
		for j := 0; j < bh && canEdge; j++ {
			left := pixels[(y+j)*w+(x-1)]
			for i := 0; i < bw; i++ {
				if pixels[(y+j)*w+(x+i)] != left { canEdge = false; break }
			}
		}
		if canEdge { return 3 }
	}
	bestMode, bestCopies := 0, -1
	for mode := 0; mode < 3; mode++ {
		var block [4][4]uint16
		var set [4][4]bool
		dx, dy := dirs[mode].dx, dirs[mode].dy
		copies := 0
		for j := 0; j < bh; j++ {
			for i := 0; i < bw; i++ {
				actual := pixels[(y+j)*w+(x+i)]
				nx, ny := x+i+dx, y+j+dy
				var pred uint16
				if nx >= x && nx < x+4 && ny >= y && ny < y+4 && set[ny-y][nx-x] {
					pred = block[ny-y][nx-x]
				} else {
					pred = gp(dst, w, h, nx, ny)
				}
				if pred == actual { copies++ }
				block[j][i] = actual
				set[j][i] = true
			}
		}
		if copies > bestCopies { bestCopies, bestMode = copies, mode }
	}
	return bestMode
}
func encodeW2Depth1(pairs []uint32) []byte {
	counts := make(map[uint32]uint32)
	var fso []uint32
	for _, p := range pairs {
		if _, ok := counts[p]; !ok { fso = append(fso, p) }
		counts[p]++
	}
	var tvals []uint32
	if len(counts) == 1 {
		tvals = append(tvals, pairs[0])
	} else {
		sort.SliceStable(fso, func(i, j int) bool { return counts[fso[i]] > counts[fso[j]] })
		for _, v := range fso {
			if counts[v] >= 2 {
				tvals = append(tvals, v)
				if len(tvals) >= 256 { break }
			}
		}
		if len(tvals) == 0 { tvals = append(tvals, fso[0]) }
	}
	idxMap := make(map[uint32]uint32)
	for i, v := range tvals { idxMap[v] = uint32(i) }
	var idxStream, runStream, litStream []byte
	litCount := uint32(0)
	for i := 0; i < len(pairs); {
		p := pairs[i]
		if idx, ok := idxMap[p]; ok {
			j := i + 1
			for j < len(pairs) && pairs[j] == p { j++ }
			encodeValue(idx+1, &idxStream)
			encodeValue(uint32(j-i-1), &runStream)
			i = j
		} else {
			encodeValue(0, &idxStream)
			var b [4]byte
			binary.LittleEndian.PutUint32(b[:], p)
			litStream = append(litStream, b[:]...)
			litCount++
			i++
		}
	}
	pad4(&idxStream)
	pad4(&runStream)
	var out []byte
	putU32LE(&out, uint32(len(tvals)))
	putU32LE(&out, uint32(len(idxStream)))
	putU32LE(&out, uint32(len(runStream)))
	putU32LE(&out, litCount)
	for _, v := range tvals { putU32LE(&out, v) }
	out = append(out, idxStream...)
	out = append(out, runStream...)
	out = append(out, litStream...)
	return out
}
func toPairs(px []uint16) []uint32 {
	if len(px)%2 != 0 { px = append(px, px[len(px)-1]) }
	out := make([]uint32, len(px)/2)
	for i := range out { out[i] = uint32(px[2*i]) | (uint32(px[2*i+1]) << 16) }
	return out
}
func encodeA9LLKeyframe(w, h int, pixels []uint16, total, ordinal, delay int) []byte {
	var bw1, bw2 BitWriter
	var gb3 []byte
	dst := make([]uint16, w*h)
	type op struct { isEdge bool; actual, pred uint16 }
	var ops []op
	for y := 0; y < h; y += 4 {
		for x := 0; x < w; x += 4 {
			mode := chooseMode(dst, pixels, w, h, x, y)
			bw1.WriteBits(uint32(mode), 2)
			if mode == 3 {
				bh, bw_ := 4, 4
				if h-y < 4 { bh = h - y }
				if w-x < 4 { bw_ = w - x }
				for j := 0; j < bh; j++ {
					for i := 0; i < bw_; i++ {
						dst[(y+j)*w+x+i] = dst[(y+j)*w+x-1]
					}
				}
				continue
			}
			dx, dy := dirs[mode].dx, dirs[mode].dy
			cbp := uint16(0)
			ops = ops[:0]
			k := 0
			for j := 0; j < 4; j++ {
				for i := 0; i < 4; i++ {
					if x+i >= w || y+j >= h { continue }
					actual := pixels[(y+j)*w+x+i]
					pred := gp(dst, w, h, x+i+dx, y+j+dy)
					if pred == actual {
						cbp |= 1 << k
						ops = append(ops, op{false, actual, pred})
					} else {
						ops = append(ops, op{true, actual, pred})
					}
					dst[(y+j)*w+x+i] = actual
					k++
				}
			}
			putU16LE(&gb3, cbp)
			for _, o := range ops {
				if o.isEdge {
					diff := o.actual - o.pred
					if choice := residLUT[diff]; choice.found {
						bw2.WriteBits(uint32(choice.nbBits), 3)
						bw1.WriteBits(uint32(choice.idx), int(choice.nbBits)+1)
					} else {
						bw2.WriteBits(7, 3)
						putU16LE(&gb3, o.actual)
					}
				}
			}
		}
	}
	gb1Start := uint32(24 + 8 + len(bw1.buf))
	gb3Start := gb1Start + uint32(len(bw2.buf))
	out := []byte{'Q', 'M', 0x0b, 0x00, 0x80, 0x00}
	putU16LE(&out, uint16(w))
	putU16LE(&out, uint16(h))
	out = append(out, 0, 0)
	putU32LE(&out, 0)
	putU16LE(&out, uint16(total))
	putU16LE(&out, uint16(ordinal))
	putU16LE(&out, uint16(delay))
	out = append(out, 0, 0)
	putU32LE(&out, gb1Start)
	putU32LE(&out, gb3Start)
	out = append(out, bw1.buf...)
	out = append(out, bw2.buf...)
	out = append(out, gb3...)
	binary.LittleEndian.PutUint32(out[12:16], uint32(len(out)))
	return out
}
func mbEqual(a, b []uint16, w, h, x, y int) bool {
	mj, mi := 16, 16
	if h-y < 16 { mj = h - y }
	if w-x < 16 { mi = w - x }
	for j := 0; j < mj; j++ {
		idx := (y+j)*w + x
		for i := 0; i < mi; i++ {
			if a[idx+i] != b[idx+i] { return false }
		}
	}
	return true
}
func encodeA9LLAni(w, h int, pixels, ref []uint16, total, ordinal, delay int) []byte {
	var bw1 BitWriter
	var lit []byte
	dst := make([]uint16, w*h)
	for y := 0; y < h; y += 16 {
		for x := 0; x < w; x += 16 {
			if w-x >= 16 && h-y >= 16 {
				if mbEqual(pixels, ref, w, h, x, y) {
					bw1.WriteBits(1, 1)
					bw1.WriteBits(1, 1)
					for j := 0; j < 16; j++ {
						for i := 0; i < 16; i++ {
							dst[(y+j)*w+x+i] = ref[(y+j)*w+x+i]
						}
					}
				} else {
					bw1.WriteBits(0, 1)
					for j := 0; j < 16; j += 4 {
						for i := 0; i < 16; i += 4 {
							mode := chooseMode(dst, pixels, w, h, x+i, y+j)
							bw1.WriteBits(uint32(mode), 2)
							if mode == 3 {
								for jj := 0; jj < 4; jj++ {
									for ii := 0; ii < 4; ii++ {
										dst[(y+j+jj)*w+x+i+ii] = dst[(y+j+jj)*w+x+i-1]
									}
								}
								continue
							}
							dx, dy := dirs[mode].dx, dirs[mode].dy
							for jj := 0; jj < 4; jj++ {
								for ii := 0; ii < 4; ii++ {
									pxx, pyy := x+i+ii, y+j+jj
									actual := pixels[pyy*w+pxx]
									pred := gp(dst, w, h, pxx+dx, pyy+dy)
									if pred == actual {
										bw1.WriteBits(1, 1)
										dst[pyy*w+pxx] = pred
									} else {
										bw1.WriteBits(0, 1)
										diff := actual - pred
										if ch := residLUT[diff]; ch.found {
											bw1.WriteBits(uint32(ch.nbBits), 3)
											bw1.WriteBits(uint32(ch.idx), int(ch.nbBits)+1)
										} else {
											bw1.WriteBits(7, 3)
											putU16LE(&lit, actual)
										}
										dst[pyy*w+pxx] = actual
									}
								}
							}
						}
					}
				}
			} else {
				bw1.WriteBits(0, 1)
				my, mx := y+16, x+16
				if my > h { my = h }
				if mx > w { mx = w }
				for yy := y; yy < my; yy += 4 {
					for xx := x; xx < mx; xx += 4 {
						if xx+4 <= w && yy+4 <= h {
							mode := chooseMode(dst, pixels, w, h, xx, yy)
							bw1.WriteBits(uint32(mode), 2)
							if mode == 3 {
								for jj := 0; jj < 4; jj++ {
									for ii := 0; ii < 4; ii++ {
										dst[(yy+jj)*w+xx+ii] = dst[(yy+jj)*w+xx-1]
									}
								}
								continue
							}
							dx, dy := dirs[mode].dx, dirs[mode].dy
							for jj := 0; jj < 4; jj++ {
								for ii := 0; ii < 4; ii++ {
									actual := pixels[(yy+jj)*w+xx+ii]
									pred := gp(dst, w, h, xx+ii+dx, yy+jj+dy)
									if pred == actual {
										bw1.WriteBits(1, 1)
										dst[(yy+jj)*w+xx+ii] = pred
									} else {
										bw1.WriteBits(0, 1)
										diff := actual - pred
										if ch := residLUT[diff]; ch.found {
											bw1.WriteBits(uint32(ch.nbBits), 3)
											bw1.WriteBits(uint32(ch.idx), int(ch.nbBits)+1)
										} else {
											bw1.WriteBits(7, 3)
											putU16LE(&lit, actual)
										}
										dst[(yy+jj)*w+xx+ii] = actual
									}
								}
							}
						} else {
							for jj := 0; jj < 4; jj++ {
								for ii := 0; ii < 4; ii++ {
									if xx+ii < w && yy+jj < h {
										val := pixels[(yy+jj)*w+xx+ii]
										putU16LE(&lit, val)
										dst[(yy+jj)*w+xx+ii] = val
									}
								}
							}
						}
					}
				}
			}
		}
	}
	pad4(&bw1.buf)
	gb1Start := uint32(24 + 8 + len(bw1.buf))
	out := []byte{'Q', 'M', 0x0b, 0x00, 0x80, 0x00}
	putU16LE(&out, uint16(w))
	putU16LE(&out, uint16(h))
	out = append(out, 0, 0)
	putU32LE(&out, 0)
	putU16LE(&out, uint16(total))
	putU16LE(&out, uint16(ordinal))
	putU16LE(&out, uint16(delay))
	out = append(out, 0, 0)
	putU32LE(&out, gb1Start)
	putU32LE(&out, 0)
	out = append(out, bw1.buf...)
	out = append(out, lit...)
	binary.LittleEndian.PutUint32(out[12:16], uint32(len(out)))
	return out
}
func main() {
	delay := flag.Int("delay", 0, "")
	forceAni := flag.Bool("force-animation", false, "")
	scale := flag.String("scale", "", "")
	flag.Parse()
	args := flag.Args()
	if len(args) < 2 { os.Exit(1) }
	outPath := args[len(args)-1]
	inPaths := args[:len(args)-1]
	sw, sh := 0, 0
	if *scale != "" {
		parts := strings.Split(strings.ToLower(*scale), "x")
		if len(parts) == 2 {
			sw, _ = strconv.Atoi(parts[0])
			sh, _ = strconv.Atoi(parts[1])
		}
	}
	var frames [][]uint16
	var w0, h0 int
	for _, p := range inPaths {
		f, _ := os.Open(p)
		img, _, _ := image.Decode(f)
		f.Close()
		b := img.Bounds()
		w, h := b.Dx(), b.Dy()
		if sw > 0 && sh > 0 { w, h = sw, sh }
		px := make([]uint16, w*h)
		for y := 0; y < h; y++ {
			sy := y * b.Dy() / h
			if sy >= b.Dy() { sy = b.Dy() - 1 }
			for x := 0; x < w; x++ {
				sx := x * b.Dx() / w
				if sx >= b.Dx() { sx = b.Dx() - 1 }
				px[y*w+x] = rgb565(img.At(b.Min.X+sx, b.Min.Y+sy))
			}
		}
		if len(frames) == 0 {
			w0, h0 = w, h
		}
		frames = append(frames, px)
	}
	var out []byte
	if *forceAni || len(frames) > 1 {
		var wg sync.WaitGroup
		outBlocks := make([][]byte, len(frames))
		outBlocks[0] = encodeA9LLKeyframe(w0, h0, frames[0], len(frames), 1, *delay)
		for i := 1; i < len(frames); i++ {
			wg.Add(1)
			go func(i int) {
				defer wg.Done()
				outBlocks[i] = encodeA9LLAni(w0, h0, frames[i], frames[i-1], len(frames), i+1, *delay)
			}(i)
		}
		wg.Wait()
		for _, b := range outBlocks { out = append(out, b...) }
	} else {
		body := encodeW2Depth1(toPairs(frames[0]))
		out = []byte{'Q', 'M', 0x0b, 0x00, 0x00, 0x81}
		putU16LE(&out, uint16(w0))
		putU16LE(&out, uint16(h0))
		out = append(out, 0, 0)
		out = append(out, body...)
	}
	os.WriteFile(outPath, out, 0644)
}
