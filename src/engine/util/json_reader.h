#pragma once

// Minimal JSON reader for flat objects with string and string-array values.
// Does not support nested objects, numbers, booleans, or null.

// Reads a string value for key into out (null-terminated, maxLen includes null).
// Returns chars written, or -1 if the key is not found.
int JsonReadString(const char *json, const char *key, char *out, int maxLen);

// Reads an array of strings for key into a flat buffer.
// out must be at least maxCount * strLen bytes.
// Access the i-th string as: out + i * strLen
// Returns the number of strings read, 0 if the key is not found.
int JsonReadStringArray(const char *json, const char *key,
                        char *out, int maxCount, int strLen);
