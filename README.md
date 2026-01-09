Targets snapshot 26.1-snapshot-1.

Mod usage:
- Go to overworld location with untouched bedrock, deepslate-stone transition and ore veins. Set sufficiently high render distance to load 33x33 such chunks
- Execute `/savecrackdata`
  
Program usage:
- Download and build kissat (https://github.com/arminbiere/kissat)
- `clang++ blocks.cpp xsinv.cpp -std=c++2c -O3 -march=native -fopenmp -I[path to kissat/src] [path to libkissat.a]`
- `./a.out [path to seed_crack_data.dat] 31`
- pray for seed output, which should take several minutes

Pre-built x86-64 binaries of `./a.out` in `bins/avx512.out`, `bins/avx2.out` and `bins/fallback.out`. Requires OpenMP; may not be compatible with some Linux distributions. `avx512.out` is the fastest, but requires AVX-512 support, which is not available on many CPUs. Most devices should be able to run `avx2.out`. Some may require `fallback.out`.

Crashes on `assert(res == 10)` can usually be bypassed by trying a different region.

Algorithm for inverting `forkPositional()` (in `xsinv.cpp`) stolen from https://github.com/pseudogravity/xoroshiro/blob/main/from2nextlongs.cu (reimplemented for CPUs with bit packing and vectorization).
