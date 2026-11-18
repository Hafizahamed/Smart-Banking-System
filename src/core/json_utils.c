#include "json_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

JsonBuilder* jb_create(size_t initial_cap) {
    if (initial_cap == 0) initial_cap = 1024;
    JsonBuilder *jb = (JsonBuilder*)malloc(sizeof(JsonBuilder));
    if (!jb) return NULL;
    jb->buffer = (char*)malloc(initial_cap);
    if (!jb->buffer) {
        free(jb);
        return NULL;
    }
    jb->capacity = initial_cap;
    jb->length = 0;
    jb->buffer[0] = '\0';
    return jb;
}

void jb_free(JsonBuilder *jb) {
    if (jb) {
        if (jb->buffer) free(jb->buffer);
        free(jb);
    }
}

void jb_reset(JsonBuilder *jb) {
    if (jb) {
        jb->length = 0;
        if (jb->buffer) jb->buffer[0] = '\0';
    }
}

static bool jb_ensure_capacity(JsonBuilder *jb, size_t needed) {
    if (jb->length + needed + 1 > jb->capacity) {
        size_t new_cap = jb->capacity * 2;
        while (new_cap < jb->length + needed + 1) {
            new_cap *= 2;
        }
        char *new_buf = (char*)realloc(jb->buffer, new_cap);
        if (!new_buf) return false;
        jb->buffer = new_buf;
        jb->capacity = new_cap;
    }
    return true;
}

void jb_append(JsonBuilder *jb, const char *str) {
    if (!jb || !str) return;
    size_t len = strlen(str);
    if (!jb_ensure_capacity(jb, len)) return;
    memcpy(jb->buffer + jb->length, str, len);
    jb->length += len;
    jb->buffer[jb->length] = '\0';
}

void jb_append_format(JsonBuilder *jb, const char *fmt, ...) {
    if (!jb || !fmt) return;
    va_list args;
    va_start(args, fmt);
    va_list args_copy;
    va_copy(args_copy, args);
    int needed = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (needed > 0) {
        if (jb_ensure_capacity(jb, (size_t)needed)) {
            vsnprintf(jb->buffer + jb->length, needed + 1, fmt, args);
            jb->length += needed;
        }
    }
    va_end(args);
}

const char* jb_str(JsonBuilder *jb) {
    return (jb && jb->buffer) ? jb->buffer : "";
}

void json_escape_string(const char *src, char *dst, size_t max_len) {
    if (!src || !dst || max_len == 0) return;
    size_t d = 0;
    for (size_t s = 0; src[s] != '\0' && d + 6 < max_len; s++) {
        unsigned char c = (unsigned char)src[s];
        if (c == '"') {
            dst[d++] = '\\'; dst[d++] = '"';
        } else if (c == '\\') {
            dst[d++] = '\\'; dst[d++] = '\\';
        } else if (c == '\b') {
            dst[d++] = '\\'; dst[d++] = 'b';
        } else if (c == '\f') {
            dst[d++] = '\\'; dst[d++] = 'f';
        } else if (c == '\n') {
            dst[d++] = '\\'; dst[d++] = 'n';
        } else if (c == '\r') {
            dst[d++] = '\\'; dst[d++] = 'r';
        } else if (c == '\t') {
            dst[d++] = '\\'; dst[d++] = 't';
        } else if (c < 32) {
            d += snprintf(dst + d, max_len - d, "\\u%04x", c);
        } else {
            dst[d++] = src[s];
        }
    }
    dst[d] = '\0';
}

static const char* find_key_pos(const char *json, const char *key) {
    if (!json || !key) return NULL;
    char search_pattern[128];
    snprintf(search_pattern, sizeof(search_pattern), "\"%s\"", key);
    const char *p = strstr(json, search_pattern);
    if (!p) return NULL;
    p += strlen(search_pattern);
    while (*p && (isspace((unsigned char)*p) || *p == ':')) {
        p++;
    }
    return p;
}

bool json_get_string(const char *json, const char *key, char *out, size_t max_len) {
    if (!out || max_len == 0) return false;
    out[0] = '\0';
    const char *p = find_key_pos(json, key);
    if (!p) return false;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p != '"') return false;
    p++; // skip open quote
    size_t o = 0;
    while (*p && *p != '"' && o + 1 < max_len) {
        if (*p == '\\') {
            p++;
            if (*p == '"') out[o++] = '"';
            else if (*p == '\\') out[o++] = '\\';
            else if (*p == 'n') out[o++] = '\n';
            else if (*p == 'r') out[o++] = '\r';
            else if (*p == 't') out[o++] = '\t';
            else if (*p) out[o++] = *p;
        } else {
            out[o++] = *p;
        }
        if (*p) p++;
    }
    out[o] = '\0';
    return true;
}

bool json_get_int(const char *json, const char *key, int *out) {
    int64_t val;
    if (json_get_int64(json, key, &val)) {
        *out = (int)val;
        return true;
    }
    return false;
}

bool json_get_int64(const char *json, const char *key, int64_t *out) {
    if (!out) return false;
    const char *p = find_key_pos(json, key);
    if (!p) return false;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '"') p++; // accept string-encoded integer
    char num_buf[64];
    size_t idx = 0;
    if (*p == '-') {
        num_buf[idx++] = *p++;
    }
    while (*p && isdigit((unsigned char)*p) && idx + 1 < sizeof(num_buf)) {
        num_buf[idx++] = *p++;
    }
    if (idx == 0 || (idx == 1 && num_buf[0] == '-')) return false;
    num_buf[idx] = '\0';
    *out = _strtoi64(num_buf, NULL, 10);
    return true;
}

bool json_get_double(const char *json, const char *key, double *out) {
    if (!out) return false;
    const char *p = find_key_pos(json, key);
    if (!p) return false;
    while (*p && isspace((unsigned char)*p)) p++;
    if (*p == '"') p++;
    char num_buf[64];
    size_t idx = 0;
    while (*p && (isdigit((unsigned char)*p) || *p == '.' || *p == '-') && idx + 1 < sizeof(num_buf)) {
        num_buf[idx++] = *p++;
    }
    if (idx == 0) return false;
    num_buf[idx] = '\0';
    *out = atof(num_buf);
    return true;
}

bool json_get_bool(const char *json, const char *key, bool *out) {
    if (!out) return false;
    const char *p = find_key_pos(json, key);
    if (!p) return false;
    while (*p && isspace((unsigned char)*p)) p++;
    if (strncmp(p, "true", 4) == 0 || strncmp(p, "\"true\"", 6) == 0 || strncmp(p, "1", 1) == 0) {
        *out = true;
        return true;
    }
    if (strncmp(p, "false", 5) == 0 || strncmp(p, "\"false\"", 7) == 0 || strncmp(p, "0", 1) == 0) {
        *out = false;
        return true;
    }
    return false;
}

void format_inr(int64_t paise, char *out, size_t max_len) {
    if (!out || max_len == 0) return;
    bool is_neg = (paise < 0);
    int64_t abs_paise = is_neg ? -paise : paise;
    int64_t rupees = abs_paise / 100;
    int coins = (int)(abs_paise % 100);

    char raw[64];
    snprintf(raw, sizeof(raw), "%lld", (long long)rupees);
    int len = (int)strlen(raw);

    char formatted[64];
    int f_idx = 0;

    if (len <= 3) {
        strcpy(formatted, raw);
    } else {
        // Indian numbering system: last 3 digits, then groups of 2 digits
        // e.g. 1234567 -> 12,34,567
        int remainder = (len - 3) % 2;
        int i = 0;
        if (remainder > 0) {
            for (; i < remainder; i++) {
                formatted[f_idx++] = raw[i];
            }
            formatted[f_idx++] = ',';
        }
        for (; i < len - 3; i += 2) {
            formatted[f_idx++] = raw[i];
            formatted[f_idx++] = raw[i + 1];
            formatted[f_idx++] = ',';
        }
        // Last 3 digits
        formatted[f_idx++] = raw[len - 3];
        formatted[f_idx++] = raw[len - 2];
        formatted[f_idx++] = raw[len - 1];
        formatted[f_idx] = '\0';
    }

    snprintf(out, max_len, "%s₹%s.%02d", is_neg ? "-" : "", formatted, coins);
}
