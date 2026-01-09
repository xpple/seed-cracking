#include <array>
#include <algorithm>
#include <bitset>
#include <cassert>
#include <execution>
#include <vector>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <bit>
#include <map>
#include <set>
#include <span>
#include <utility>
#include "xsinv.h"
extern "C" {
#include "kissat.h"
}

uint64_t getSeed(int x, int y, int z)
{
	// obviously the first multiplication is 32-bit while the second is 64-bit
        uint64_t seed = int64_t(int32_t(uint32_t(x) * 3129871u) ^ int64_t(uint64_t(z) * 116129781u) ^ int32_t(y));
        return int64_t(((seed * seed) * 42317861) + (seed * 11)) >> 16; // signed right shift
}

template<int ITER>
bool get(uint64_t seedlo, uint64_t seedhi, uint64_t xv, double p)
{
	uint64_t s0 = xv, s1 = 0;
	for(size_t i = 0; i < ITER; i++) {
		uint64_t s12 = s0 ^ s1;
		s0 = std::rotl(s0, 49) ^ s12 ^ s12 << 21;
		s1 = std::rotl(s12, 28);
	}
	seedlo ^= s0;
	seedhi ^= s1;
	uint64_t out = std::rotl(seedlo + seedhi, 17) + seedlo;
	float r = (out >> 40) * 0x1.0p-24;
	return double(r) < p;
}

struct beam_entry {
	size_t nbits;
	uint64_t w1;
	uint64_t w2;
	uint64_t w3;
	int64_t score;
	std::vector<beam_entry> next() {
		std::vector<beam_entry> ret;
		if(nbits < 17)
			for(size_t i = 0; i < 8; i++)
				ret.push_back({nbits + 1, 2 * w1 + i / 4, 2 * w2 + i / 2 % 2, 2 * w3 + i % 2, 0});
		else
			for(size_t i = 0; i < 4; i++)
				ret.push_back({nbits + 1, 2 * w1, 2 * w2 + i / 2 % 2, 2 * w3 + i % 2, 0});
		return ret;
	}
};

std::vector<std::bitset<128>> solve(std::vector<std::tuple<uint64_t, double, bool>> xvs, int mode)
{
	if(xvs.size() == 0)
		return {};
	std::vector<beam_entry> cur { {0, 0, 0, 0, 0} };
	std::vector<beam_entry> next;
	size_t iters = 0;
	for(; iters < 21; iters++)
	{
		for(beam_entry e : cur) {
			printf("At: %zu %zu %zu %zu %zd\n", e.nbits, e.w1, e.w2, e.w3, e.score);
			for(auto to : e.next()) {
				next.push_back(to);
			}
		}
#pragma omp parallel for
		for(beam_entry& to : next) {
			int64_t score = 0;
			uint64_t seedlo = to.w1 << (64-to.nbits) | to.w2 << (64-17-to.nbits);
			uint64_t seedhi = to.w3 << (64-17-to.nbits);
			if(mode <= 3)
				for(auto[xv,p,ret] : xvs)
					score += get<0>(seedlo, seedhi, xv, p) != ret;
			else if(mode == 4)
				for(auto[xv,p,ret] : xvs)
					score += get<1>(seedlo, seedhi, xv, p) != ret;
			else if(mode == 5)
				for(auto[xv,p,ret] : xvs)
					score += (get<2>(seedlo, seedhi, xv, p) != ret) * (ret ? 50 : 1); // increase weight of rare class
			to.score = score;
			//printf("To: %zu %zu %zu %zu %zd\n", to.nbits, to.w1, to.w2, to.w3, to.score);
		};
		std::ranges::stable_sort(next.begin(), next.end(), std::less<>{}, &beam_entry::score); // stability appears to be important
		size_t keep = 1000;
		if(next.size() > keep)
			next.erase(next.begin() + keep, next.end());
		if(iters > 6 && next.front().score >= next.back().score * 0.95) { // no point in continuing
			break; // use cur, do not proceed to next
		}
		cur = std::exchange(next, {});
	}
	std::vector<std::bitset<128>> ret;
	auto push = [&ret][[gnu::used]](uint64_t a, uint64_t b)  {
		std::bitset<128> x;
		for(size_t i = 0; i < 64; i++) // of course this does not get optimized
			x[i] = a >> i & 1;
		for(size_t i = 0; i < 64; i++)
			x[64 + i] = b >> i & 1;
		ret.push_back(x);
	};
	// first returned element = mask where something is known
	// all others = candidates
	uint64_t itermask = (1ull << iters) - 1;
	uint64_t km0 = itermask << (64 - iters) | itermask << (64 - iters - 17), km1 = itermask << (64 - iters - 17);
	push(km0, km1);
	for(auto[nb,w1,w2,w3,score] : cur) {
		uint64_t c0 = w1 << (64 - iters) | w2 << (64 - iters - 17), c1 = w3 << (64 - iters - 17);
		assert((c0 &~ km0) == 0);
		assert((c1 &~ km1) == 0);
		push(c0, c1);
	}
	return ret;
}
enum block_type : int {
	BEDROCK, RAW = 1, ORE = 2, FILL = 3, STONE = 4, DEEPSLATE = 5
};
struct block_entry {
	int x, y, z;
	block_type typ;
};
static_assert(sizeof(block_entry) == 16);
using point3 = std::tuple<int,int,int>;

struct kissat* kissat_ctx;
int next_var = 1;
int Zero, One;
template<std::same_as<int>... T>
void ADD(T... lits)
{
	//(printf("%d%c", lits, lits == 0 ? '\n' : ' '), ...);
	(kissat_add(kissat_ctx, lits), ...);
}
int VAR()
{
	return next_var++;
}
std::vector<int> Xor(std::span<const int> x, std::span<const int> y)
{
	assert(x.size() == y.size());
	std::vector<int> z(x.size());
	for(int i = 0; i < x.size(); i++) {
		z[i] = VAR();
		ADD(-x[i], -y[i], -z[i], 0);
		ADD(-x[i], y[i], z[i], 0);
		ADD(x[i], -y[i], z[i], 0);
		ADD(x[i], y[i], -z[i], 0);
	}
	return z;
}
std::vector<int> And(std::span<const int> x, std::span<const int> y)
{
	assert(x.size() == y.size());
	std::vector<int> z(x.size());
	for(int i = 0; i < x.size(); i++) {
		z[i] = VAR();
		ADD(-x[i], -y[i], z[i], 0);
		ADD(x[i], -z[i], 0);
		ADD(y[i], -z[i], 0);
	}
	return z;
}
void AssertEq(std::vector<int> x, std::vector<int> y)
{
	assert(x.size() == y.size());
	for(int i = 0; i < x.size(); i++) {
		ADD(x[i], -y[i], 0);
		ADD(-x[i], y[i], 0);
	}
}
std::vector<int> Const(uint64_t x)
{
	std::vector<int> ret(64);
	for(int i = 0; i < 64; i++) {
		ret[i] = x >> i & 1 ? One : Zero;
	}
	return ret;
}
std::vector<int> Unk()
{
	std::vector<int> ret(64);
	for(int i = 0; i < 64; i++) {
		ret[i] = VAR();
	}
	return ret;
}
uint64_t Value(std::span<const int> a) {
	assert(a.size() == 64);
	uint64_t ret = 0;
	for(int i = 0; i < 64; i++)
		ret |= uint64_t(kissat_value(kissat_ctx, a[i]) >= 0) << i;
	return ret;
}
std::vector<int> Rotr(std::span<const int> a, size_t r)
{
	assert(a.size() == 64 && r < 64);
	std::vector<int> ret(64);
	for(int i = 0; i < 64; i++) {
		ret[i] = a[(i + r) % 64];
	}
	return ret;
}
std::vector<int> Rotl(std::span<const int> a, size_t r)
{
	return Rotr(a, 64 - r);
}
std::vector<int> Shl(std::span<const int> a, size_t r)
{
	assert(a.size() == 64 && r < 64);
	std::vector<int> ret(64, Zero);
	for(int i = r; i < 64; i++) {
		ret[i] = a[i - r];
	}
	return ret;
}
std::vector<int> Shr(std::span<const int> a, size_t r)
{
	assert(a.size() == 64 && r < 64);
	std::vector<int> ret(64, Zero);
	for(int i = 0; i < 64 - r; i++) {
		ret[i] = a[i + r];
	}
	return ret;
}
int Maj(int x, int y, int z)
{
	return Xor(And({x}, {y}), Xor(And({x}, {z}), And({y},{z})))[0];
}
std::vector<int> Add(std::span<const int> x, std::span<const int> y)
{
	assert(x.size() == y.size());
	std::vector<int> z(x.size());
	int c_in = Zero;
	for(int i = 0; i < x.size(); i++) {
		z[i] = Xor({x[i]}, Xor({y[i]}, {c_in}))[0];
		if(i != x.size() - 1)
			c_in = Maj(x[i], y[i], c_in);
	}
	return z;
}
std::vector<int> Rnd(std::span<const int> S0, std::span<const int> S1)
{
	return Add(Rotl(Add(S0, S1), 17), S0);
}
std::pair<std::vector<int>, std::vector<int>> Step(std::span<const int> S0, std::span<const int> S1)
{
	auto S12 = Xor(S0, S1);
	return {Xor(Xor(Rotl(S0, 49), S12), Shl(S12, 21)), Rotl(S12, 28)};
}


int main(int argc, char*argv[])
{
	setlinebuf(stdout);
	FILE* bdat = fopen(argv[1], "rb");
	int mode = atoi(argv[2]);
	assert(1 <= mode && mode <= 31);
	std::map<point3, block_type> blocks;
	std::vector<std::tuple<uint64_t, double, bool>> xvs1, xvs2, xvs3, xvs4, xvs5;
	while(true)
	{
		block_entry e;
		if(fread(&e, sizeof(e), 1, bdat) != 1)
			break;
		e.x = std::byteswap(e.x);
		e.y = std::byteswap(e.y);
		e.z = std::byteswap(e.z);
		e.typ = (block_type)std::byteswap((int)e.typ);
		blocks[{e.x, e.y, e.z}] = e.typ;
	}
	std::set<point3> maybe_veins;
	for(auto[coord, typ] : blocks) {
		auto[x,y,z] = coord;
		uint64_t xv = getSeed(x, y, z);
		if(mode & 1) {
			if(-63 <= y && y <= -60)
			{
				float thrs[] {0.8, 0.6, 0.4, 0.19999999999999996 };
				float thr = thrs[y + 63];
				if(typ == BEDROCK) {
					printf("%d %d %d: bedrock xv=%lld\n", x, y, z, xv);
					xvs1.push_back({xv, thr, 1});
				} else if(typ == DEEPSLATE) {
					printf("%d %d %d: deepslate xv=%lld\n", x, y, z, xv);
					xvs1.push_back({xv, thr, 0});
				}
			}
		}
		if(mode & 2) {
			if(1 <= y && y <= 7)
			{
				float thrs[] {0.875, 0.75, 0.625, 0.5, 0.375, 0.25, 0.125};
				float thr = thrs[y - 1];
				if(typ == DEEPSLATE) {
					printf("%d %d %d: deepslate xv=%lld\n", x, y, z, xv);
					xvs2.push_back({xv, thr, 1});
				} else if(typ == STONE) {
					printf("%d %d %d: stone xv=%lld\n", x, y, z, xv);
					xvs2.push_back({xv, thr, 0});
				}
			}
		}
		// identify veins. heuristic 1: investigate only 7x7x7 boxes around raw ore blocks for being parts of veins
		if(typ == RAW) {
			printf("Raw: %d %d %d\n", x, y, z);
			for(int dx = -3; dx <= 3; dx++)
			for(int dy = -3; dy <= 3; dy++)
			for(int dz = -3; dz <= 3; dz++)
				maybe_veins.insert({x + dx, y + dy, z + dz});
		}
	}
	for(auto[x,y,z] : maybe_veins) {
		// check if the block is actually part of a vein, based on what it is adjacent to
		int adj_ore = 0, adj_filler = 0, adj_raw = 0;
		for(int dx = -1; dx <= 1; dx++)
		for(int dy = -1; dy <= 1; dy++)
		for(int dz = -1; dz <= 1; dz++)
		{
			auto it = blocks.find({x+dx, y+dy, z+dz});
			if(it == blocks.end())
				continue;
			block_type typ = it->second;
			adj_raw += typ == RAW;
			adj_ore += typ == ORE;
			adj_filler += typ == FILL;
		}
		if(adj_ore + adj_filler + 3 * adj_raw < 18)
			continue;
		if(adj_filler == 0 || (adj_ore == 0 && adj_raw == 0)) // if we passed above but not this, we're probably in nearby normal ore / filler
			continue;
		// vein confirmed. 
		uint64_t xv = getSeed(x, y, z);
		auto it = blocks.find({x, y, z});
		if(mode & 4) { // identify vein vs skipped. 70% is replaced by vein (filler / ore / raw), 30% is left as is
			bool vein = false;
			if(it != blocks.end()) {
				block_type typ = it->second;
				vein |= typ == RAW || typ == ORE || typ == FILL;
			}
			printf("%d %d %d: vein (%d %d %d), replaced=%d xv=%lld\n", x, y, z, adj_ore, adj_filler, adj_raw, (int)vein, xv);
			xvs3.push_back({xv, 0.7, vein});
		}
		if(mode & 8) { // identify filler vs ore among those that passed previous step
			// vein richness varies from 0.1 to 0.3 so assume the worst
			// ore is placed if rand() < richness
			if(it == blocks.end())
				continue; // not needed by next either
			if(it->second == RAW || it->second == ORE) {
				printf("%d %d %d: vein (%d %d %d), ore xv=%lld\n", x, y, z, adj_ore, adj_filler, adj_raw, xv);
				xvs4.push_back({xv, 0.3, true});
			}
			else if(it->second == FILL) {
				printf("%d %d %d: vein (%d %d %d), fill xv=%lld\n", x, y, z, adj_ore, adj_filler, adj_raw, xv);
				xvs4.push_back({xv, 0.1, false});
			}
		}
		if(mode & 16) { // identify ore vs raw block
			if(it == blocks.end())
				continue;
			if(it->second == RAW || it->second == ORE) {
				printf("%d %d %d: vein (%d %d %d), raw=%d xv=%lld\n", x, y, z, adj_ore, adj_filler, adj_raw, int(it->second == RAW), xv);
				xvs5.push_back({xv, 0.02, it->second == RAW});
			}
		}
	}
	auto sol1 = solve(xvs1, 1);
	auto sol2 = solve(xvs2, 2);
	auto sol3 = solve(xvs3, 3);
	auto sol4 = solve(xvs4, 4);
	auto sol5 = solve(xvs5, 5);
	// encode problem as CNF
	// I = unknown
	// S = I ^ BEDROCK, T = I ^ DEEPSLATE, V = I ^ ORE
	// O0 = forkPositional(S), O1 = forkPositional(T), O2 = forkPositional(V)
	// O3 = step(O2), O4 = step(O3)
	// our solutions above leak data from Os
	// kissat is smart enough that we don't need to optimize the encoding
	kissat_ctx = kissat_init();
	Zero = VAR();
	One = VAR();
	ADD(-Zero, 0);
	ADD(One, 0);
	auto I0 = Unk();
	auto I1 = Unk();
	auto S0 = Xor(I0, Const(13544455532117611141ull));
	auto S1 = Xor(I1, Const(14185350335435586452ull));
	auto T0 = Xor(I0, Const(10411719568726253007ull));
	auto T1 = Xor(I1, Const(14964796469053385315ull));
	auto V0 = Xor(I0, Const(11207227798492025197ull));
	auto V1 = Xor(I1, Const(3091299299654006625ull));
	auto O0 = Rnd(S0, S1);
	auto[S2, S3] = Step(S0, S1);
	auto O1 = Rnd(S2, S3);
	auto O2 = Rnd(T0, T1);
	auto[T2, T3] = Step(T0, T1);
	auto O3 = Rnd(T2, T3);
	auto O4 = Rnd(V0, V1);
	auto[V2, V3] = Step(V0, V1);
	auto O5 = Rnd(V2, V3);
	auto[O6, O7] = Step(O4, O5);
	auto[O8, O9] = Step(O6, O7);

	auto constrain = [&](std::span<const std::bitset<128>> cons, std::span<const int> v0, std::span<const int> v1) {
		std::vector<int> cands(cons.size() - 1);
		for(int& x : cands) {
			x = VAR();
			ADD(x);
		}
		ADD(0);
		for(int i = 0; i < cands.size(); i++) {
			const std::bitset<128>& mask = cons[0];
			const std::bitset<128>& cur = cons[i + 1];
			for(int j = 0; j < 64; j++)
				if(mask[j]) ADD(-cands[i], v0[j] * (cur[j] ? 1 : -1), 0);
			for(int j = 0; j < 64; j++)
				if(mask[64 + j]) ADD(-cands[i], v1[j] * (cur[64 + j] ? 1 : -1), 0);
		}
	};
	if(sol1.size()) constrain(sol1, O0, O1);
	if(sol2.size()) constrain(sol2, O2, O3);
	if(sol3.size()) constrain(sol3, O4, O5);
	if(sol4.size()) constrain(sol4, O6, O7);
	if(sol5.size()) constrain(sol5, O8, O9);
	int res = kissat_solve(kissat_ctx);
	assert(res == 10); // satisfiable
	uint64_t i0 = Value(I0), i1 = Value(I1);
	printf("got RandomState.random: lo=%llu hi=%llu. searching for seed\n", i0, i1);
	crack(i0, i1);
	return 0;
}
