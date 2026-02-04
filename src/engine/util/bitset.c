
#include "bitset.h"
#include <stdlib.h>
#include <string.h>

#define BITS_PER_WORD 64

// round up for truncation
static uint32_t BitsetWordCount(uint32_t bitCount) {
  return (bitCount + BITS_PER_WORD - 1) / BITS_PER_WORD;
}

void BitsetInit(bitset_t *bitset, uint32_t bitCount) {
  bitset->bitCount = bitCount;
  bitset->wordCount = BitsetWordCount(bitCount);

  if (bitset->wordCount > 0) {
    bitset->words = calloc(bitset->wordCount, sizeof(uint64_t));
  } else {
    bitset->words = NULL;
  }
}

void BitsetDestroy(bitset_t *bitset) {
  free(bitset->words);
  bitset->words = NULL;
  bitset->bitCount = 0;
  bitset->wordCount = 0;
}

// shrinks and grows, new bits are set to 0
void BitsetResize(bitset_t *bitset, uint32_t newBitCount) {
  uint32_t newWordCount = BitsetWordCount(newBitCount);

  if (newWordCount == bitset->wordCount) {
    bitset->bitCount = newBitCount;
    return;
  }

  bitset->words = realloc(bitset->words, newWordCount * sizeof(uint64_t));

  if (newWordCount > bitset->wordCount) {
    uint32_t old = bitset->wordCount;
    memset(bitset->words + old, 0, (newWordCount - old) * sizeof(uint64_t));
  }

  bitset->bitCount = newBitCount;
  bitset->wordCount = newWordCount;
}

// sets bit at index "bit" to 1
void BitsetSet(bitset_t *bitset, uint32_t bit) {
  if (bit >= bitset->bitCount) {
    return;
  }

  uint32_t word = bit / BITS_PER_WORD;
  uint32_t offset = bit % BITS_PER_WORD;

  bitset->words[word] |= (1ULL << offset);
}

// sets bit at index "bit" to 0
void BitsetClear(bitset_t *bitset, uint32_t bit) {
  if (bit >= bitset->bitCount) {
    return;
  }

  uint32_t word = bit / BITS_PER_WORD;
  uint32_t offset = bit % BITS_PER_WORD;

  bitset->words[word] &= ~(1ULL << offset);
}

// checks if bit at index "bit" is 1,
// 1 -> true
// 0 -> false
bool BitsetTest(const bitset_t *bitset, uint32_t bit) {
  if (bit >= bitset->bitCount) {
    return false;
  }

  uint32_t word = bit / BITS_PER_WORD;
  uint32_t offset = bit % BITS_PER_WORD;

  return (bitset->words[word] & (1ULL << offset)) != 0;
}

bool BitsetEquals(const bitset_t *a, const bitset_t *b) {
  if (a->bitCount != b->bitCount) {
    return false;
  }

  uint32_t count = a->wordCount < b->wordCount ? a->wordCount : b->wordCount;

  for (uint32_t i = 0; i < count; ++i) {
    if (a->words[i] != b->words[i]) {
      return false;
    }
  }

  return true;
}

// 0 all bits in bitset
void BitsetClearAll(bitset_t *bitset) {
  memset(bitset->words, 0, bitset->wordCount * sizeof(uint64_t));
}

// check if required bits are set to 1
// given bitset should be same sizes for game case
// e.g.: if we have 50 components, we store them in 2 words
// each bitset has 2 words
bool BitsetContainsAll(const bitset_t *bitset, const bitset_t *required) {
  uint32_t count = bitset->wordCount < required->wordCount
                       ? bitset->wordCount
                       : required->wordCount;

  for (uint32_t i = 0; i < count; ++i) {
    if ((bitset->words[i] & required->words[i]) != required->words[i]) {
      return false;
    }
  }

  /* required has bits outside bitset */
  for (uint32_t i = count; i < required->wordCount; ++i) {
    if (required->words[i] != 0) {
      return false;
    }
  }

  return true;
}

// returns false if any bit is 1 in both bitset and excluded
// true otherwise
// aka return true if bitset shares NO bits with excluded
bool BitsetContainsNone(const bitset_t *bitset, const bitset_t *excluded) {
  uint32_t count = bitset->wordCount < excluded->wordCount
                       ? bitset->wordCount
                       : excluded->wordCount;

  for (uint32_t i = 0; i < count; ++i) {
    if ((bitset->words[i] & excluded->words[i]) != 0) {
      return false;
    }
  }

  return true;
}
