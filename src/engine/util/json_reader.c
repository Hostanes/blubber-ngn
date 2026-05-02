#include "json_reader.h"
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

static void skip_ws(const char **p) {
  while (**p == ' ' || **p == '\t' || **p == '\n' || **p == '\r')
    (*p)++;
}

// Parse a JSON string at *p (must be at opening '"').
// Advances *p past the closing '"'.
// Returns chars written (excl. null) or -1 on error.
static int parse_str(const char **p, char *out, int maxLen) {
  if (**p != '"')
    return -1;
  (*p)++;
  int i = 0;
  while (**p && **p != '"') {
    if (**p == '\\') {
      (*p)++;
      if (!**p)
        return -1;
    }
    if (i < maxLen - 1)
      out[i++] = **p;
    (*p)++;
  }
  if (**p != '"')
    return -1;
  (*p)++;
  out[i] = '\0';
  return i;
}

// Scans a flat JSON object for "key": and returns a pointer to the value start.
// Works by treating every quoted string as a potential key and checking for ':'.
// Value strings and array items won't have ':' immediately after, so they're skipped.
static const char *find_value(const char *json, const char *key) {
  const char *p = json;
  while (*p) {
    skip_ws(&p);
    if (*p != '"') {
      p++;
      continue;
    }
    char keybuf[128];
    if (parse_str(&p, keybuf, sizeof(keybuf)) < 0)
      break;
    skip_ws(&p);
    if (*p != ':')
      continue;
    p++;
    skip_ws(&p);
    if (strcmp(keybuf, key) == 0)
      return p;
  }
  return NULL;
}

int JsonReadString(const char *json, const char *key, char *out, int maxLen) {
  const char *p = find_value(json, key);
  if (!p)
    return -1;
  return parse_str(&p, out, maxLen);
}

bool JsonReadFloat(const char *json, const char *key, float *out) {
  const char *p = find_value(json, key);
  if (!p)
    return false;
  char *end;
  *out = strtof(p, &end);
  return end != p;
}

int JsonReadStringArray(const char *json, const char *key, char *out,
                        int maxCount, int strLen) {
  const char *p = find_value(json, key);
  if (!p || *p != '[')
    return 0;
  p++;

  int count = 0;
  while (*p && *p != ']' && count < maxCount) {
    skip_ws(&p);
    if (*p == '"') {
      if (parse_str(&p, out + count * strLen, strLen) >= 0)
        count++;
    } else if (*p != ']') {
      p++;
    }
    skip_ws(&p);
    if (*p == ',')
      p++;
  }
  return count;
}
