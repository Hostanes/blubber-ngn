
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  uint64_t *words;
  uint32_t bitCount;
  uint32_t wordCount;
} bitset_t;

/* lifecycle */
void BitsetInit(bitset_t *bitset, uint32_t bitCount);
void BitsetDestroy(bitset_t *bitset);

/* resize */
void BitsetResize(bitset_t *bitset, uint32_t newBitCount);

/* bit ops */
void BitsetSet(bitset_t *bitset, uint32_t bit);
void BitsetClear(bitset_t *bitset, uint32_t bit);
bool BitsetTest(const bitset_t *bitset, uint32_t bit);
bool BitsetEquals(const bitset_t *a, const bitset_t *b);

/* bulk ops */
void BitsetClearAll(bitset_t *bitset);
bool BitsetContainsAll(const bitset_t *bitset, const bitset_t *required);
bool BitsetContainsNone(const bitset_t *bitset, const bitset_t *excluded);
