//----------------------------------------------------------------
//
//  4190.307 Operating Systems (Spring 2024)
//
//  Project #4: KSM (Kernel Samepage Merging)
//
//  May 7, 2024
//
//  Jin-Soo Kim (jinsoo.kim@snu.ac.kr)
//  Systems Software & Architecture Laboratory
//  Dept. of Computer Science and Engineering
//  Seoul National University
//
//----------------------------------------------------------------

// This is a port of the xxh hashing algorithm used in Linux KSM,
// specifically tailored for 64-bit little-endian RISC-V CPUs.
// It assumes that the size of the data is a multiple of 8 bytes.

#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

uint64 seed = 4190307;        // Yes, our course number!

#define xxh_rotl64(x, r) ((x << r) | (x >> (64 - r)))

static const uint64 PRIME64_1 = 11400714785074694791ULL;
static const uint64 PRIME64_2 = 14029467366897019727ULL;
static const uint64 PRIME64_3 =  1609587929392839161ULL;
static const uint64 PRIME64_4 =  9650029242287828579ULL;

static uint64 
xxh64_round(uint64 acc, uint64 input)
{
    acc += input * PRIME64_2;
    acc = xxh_rotl64(acc, 31);
    acc *= PRIME64_1;
    return acc;
}

static uint64 
xxh64_merge_round(uint64 acc, uint64 val)
{
    val = xxh64_round(0, val);
    acc ^= val;
    acc = acc * PRIME64_1 + PRIME64_4;
    return acc;
}

uint64 
xxh64(void *input, unsigned int len)
{
	uint8 *p = (uint8 *) input;
	uint8 *b_end = p + len;
	uint64 h64;

	uint8 *limit = b_end - 32;
	uint64 v1 = seed + PRIME64_1 + PRIME64_2;
	uint64 v2 = seed + PRIME64_2;
	uint64 v3 = seed + 0;
	uint64 v4 = seed - PRIME64_1;

	do {
		v1 = xxh64_round(v1, *(uint64 *)p);
		p += 8;
		v2 = xxh64_round(v2, *(uint64 *)p);
		p += 8;
		v3 = xxh64_round(v3, *(uint64 *)p);
		p += 8;
		v4 = xxh64_round(v4, *(uint64 *)p);
		p += 8;
	} while (p <= limit);

	h64 = xxh_rotl64(v1, 1) + xxh_rotl64(v2, 7) + xxh_rotl64(v3, 12) + xxh_rotl64(v4, 18);
	h64 = xxh64_merge_round(h64, v1);
	h64 = xxh64_merge_round(h64, v2);
	h64 = xxh64_merge_round(h64, v3);
	h64 = xxh64_merge_round(h64, v4);

	h64 += (uint64) len;

	while (p + 8 <= b_end) {
		uint64 k1 = xxh64_round(0, *(uint64 *)p);
		
		h64 ^= k1;
		h64 = xxh_rotl64(h64, 27) * PRIME64_1 + PRIME64_4;
		p += 8;
	}

	h64 ^= h64 >> 33;
	h64 *= PRIME64_2;
	h64 ^= h64 >> 29;
	h64 *= PRIME64_3;
	h64 ^= h64 >> 32;

	return h64;
}

