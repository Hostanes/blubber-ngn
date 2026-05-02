#pragma once
#include <stdbool.h>

// Minimal JSON reader for flat objects with string, number, and string-array values.

// Reads a string value for key into out (null-terminated, maxLen includes null).
// Returns chars written, or -1 if the key is not found.
int JsonReadString(const char *json, const char *key, char *out, int maxLen);

// Reads a float value for key. Returns true on success.
bool JsonReadFloat(const char *json, const char *key, float *out);

// Reads an array of strings for key into a flat buffer.
// out must be at least maxCount * strLen bytes.
// Access the i-th string as: out + i * strLen
// Returns the number of strings read, 0 if the key is not found.
int JsonReadStringArray(const char *json, const char *key,
                        char *out, int maxCount, int strLen);
