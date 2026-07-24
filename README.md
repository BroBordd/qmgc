# qmgc

qmgc (Qmage Compiler) is an experimental suite for compiling standard image assets into Samsung's proprietary Qmage (QMG) binary formats. Use at your own risk.

## Implementation Details

The compiler outputs W2 structures for static images and A9LL structures for animations. 

W2 operates on an RGB565 plane. It generates a dynamic symbol palette derived from localized frequency grouping, which is encoded over interleaved index and run-length bitstreams. Lookup misses bypass the LUT natively as 32-bit interleaved raw literals. 

A9LL employs block-based spatial prediction. Sequences are segmented into 16x16 macroblocks, subdivided into 4x4 coordinate grids. Spatiotemporal differentials between blocks are quantized against a deterministic 256-entry lookup table. Bit-depths vary adaptively per block based on correlation. Edge propagations bypass residual encoding.

## Layout

- `c/` - Reference C++ implementation (requires `libpng`, `libjpeg`)
- `go/` - Native Go port 
- `rust/` - Native Rust port
- `python/` - Python port (requires `Pillow`)
