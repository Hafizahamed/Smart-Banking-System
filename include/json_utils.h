#ifndef JSON_UTILS_H
#define JSON_UTILS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    char *buffer;
    size_t capacity;
    size_t length;
} JsonBuilder;

// Json Builder functions
JsonBuilder* jb_create(size_t initial_cap);
void jb_free(JsonBuilder *jb);
void jb_reset(JsonBuilder *jb);
void jb_append(JsonBuilder *jb, const char *str);
void jb_append_format(JsonBuilder *jb, const char *fmt, ...);
const char* jb_str(JsonBuilder *jb);

// JSON Escaping
void json_escape_string(const char *src, char *dst, size_t max_len);

// JSON Parser Helpers
bool json_get_string(const char *json, const char *key, char *out, size_t max_len);
bool json_get_int(const char *json, const char *key, int *out);
bool json_get_int64(const char *json, const char *key, int64_t *out);
bool json_get_double(const char *json, const char *key, double *out);
bool json_get_bool(const char *json, const char *key, bool *out);

// Currency formatting: converts paise (e.g. 25432050) to formatted INR ("₹2,54,320.50")
void format_inr(int64_t paise, char *out, size_t max_len);

#endif // JSON_UTILS_H
