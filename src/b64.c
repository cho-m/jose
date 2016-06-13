/* vim: set tabstop=8 shiftwidth=4 softtabstop=4 expandtab smarttab colorcolumn=80: */

#include "b64.h"
#include "key.h"

#include <stdbool.h>
#include <string.h>

static const char table[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    "abcdefghijklmnopqrstuvwxyz"
    "0123456789-_";

static void __attribute__((nonnull(1, 3)))
b64url_enc(const uint8_t buf[], size_t len, char b64[])
{
    uint8_t rem = 0;

    for (size_t i = 0; i < len; i++) {
        uint8_t c = buf[i];

        switch (i % 3) {
        case 0:
            *b64++ = table[c >> 2];
            *b64++ = table[rem = (c & 0x03) << 4];
            break;

        case 1:
            b64[-1] = table[rem | (c >> 4)];
            *b64++ = table[rem = (c & 0x0F) << 2];
            break;

        case 2:
            b64[-1] = table[rem | (c >> 6)];
            *b64++ = table[c & 0x3F];
            break;
        }
    }

    *b64 = 0;
}

static bool __attribute__((warn_unused_result, nonnull(1, 2)))
b64url_dec(const char b64[], uint8_t buf[])
{
    uint8_t rem = 0;
    size_t len = 0;

    for (size_t i = 0; b64[i]; i++) {
        uint8_t v = 0;

        for (char c = b64[i]; v < sizeof(table) && table[v] != c; v++)
            continue;

        if (v >= sizeof(table))
            return false;

        switch (i % 4) {
        case 0:
            if (!b64[i+1])
                return false;

            rem = v << 2;
            break;

        case 1:
            buf[len++] = rem | (v >> 4);
            rem = v << 4;
            break;

        case 2:
            buf[len++] = rem | (v >> 2);
            rem = v << 6;
            break;

        case 3:
            buf[len++] = rem | v;
            break;
        }
    }

    return true;
}

size_t
jose_b64_dlen(size_t elen)
{
    switch (elen % 4) {
    case 0: return elen / 4 * 3;
    case 2: return elen / 4 * 3 + 1;
    case 3: return elen / 4 * 3 + 2;
    default: return 0;
    }
}

size_t
jose_b64_elen(size_t dlen)
{
    switch (dlen % 3) {
    case 0: return dlen / 3 * 4;
    case 1: return dlen / 3 * 4 + 2;
    case 2: return dlen / 3 * 4 + 3;
    default: return 0;
    }
}

bool
jose_b64_decode(const json_t *json, uint8_t buf[])
{
    if (!json_is_string(json))
        return false;

    return b64url_dec(json_string_value(json), buf);
}

jose_key_t *
jose_b64_decode_key(const json_t *json)
{
    jose_key_t *key = NULL;

    key = jose_key_new(jose_b64_dlen(json_string_length(json)));
    if (!key)
        return NULL;

    if (jose_b64_decode(json, key->key))
        return key;

    jose_key_free(key);
    return NULL;
}

json_t *
jose_b64_decode_json(const json_t *json)
{
    jose_key_t *key = NULL;
    json_t *out = NULL;

    key = jose_b64_decode_key(json);
    if (key)
        out = json_loadb((char *) key->key, key->len, 0, NULL);

    jose_key_free(key);
    return out;
}

json_t *
jose_b64_encode(const uint8_t buf[], size_t len)
{
    jose_key_t *tmp = NULL;
    json_t *json = NULL;

    tmp = jose_key_new(jose_b64_elen(len));
    if (!tmp)
        return NULL;

    b64url_enc(buf, len, (char *) tmp->key);

    json = json_stringn((char *) tmp->key, tmp->len);
    jose_key_free(tmp);
    return json;
}

json_t *
jose_b64_encode_key(const jose_key_t *key)
{
    return jose_b64_encode(key->key, key->len);
}

json_t *
jose_b64_encode_json(const json_t *json)
{
    json_t *out = NULL;
    char *tmp = NULL;

    tmp = json_dumps(json, JSON_SORT_KEYS | JSON_COMPACT);
    if (!tmp)
        return NULL;

    out = jose_b64_encode((uint8_t *) tmp, strlen(tmp));
    free(tmp);
    return out;
}
