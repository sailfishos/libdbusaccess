/*
 * Copyright (C) 2017-2020 Jolla Ltd.
 * Copyright (C) 2017-2020 Slava Monich <slava.monich@jolla.com>
 *
 * You may use this file under the terms of BSD license as follows:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *   3. Neither the names of the copyright holders nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "dbusaccess_cred_p.h"

/* Process status file parsing */

#define PROC_PARSE_UID      (0x0001)
#define PROC_PARSE_GID      (0x0002)
#define PROC_PARSE_GROUPS   (0x0004)
#define PROC_PARSE_CAP_EFF  (0x0008)
#define PROC_PARSE_ALL      (0x000f)
#define PROC_PARSE_UID_OK   (0x0010)
#define PROC_PARSE_GID_OK   (0x0020)

static
gboolean
da_cred_parse_uint32(
    const char* str,
    guint32* result)
{
    char* end = NULL;
    guint64 val64 = g_ascii_strtoull(str, &end, 0);
    /* The input string is guaranteed to be non-empty */
    if (!end[0]) {
        /* Check for overflow */
        guint32 val32 = (guint32)val64;
        if ((guint64)val32 == val64) {
            *result = val32;
            return TRUE;
        }
    }
    return FALSE;
}

static
gboolean
da_cred_parse_uint64(
    const char* str,
    guint64* result)
{
    char* end = NULL;
    guint64 val = g_ascii_strtoull(str, &end, 16);
    /* The input string is guaranteed to be non-empty */
    if (!end[0]) {
        *result = val;
        return TRUE;
    }
    return FALSE;
}

static
const char*
da_cred_parse_values(
    GPtrArray* vals,
    const char* data,
    const char* end)
{
    const char* ptr = data;
    const char* eol = data;
    g_ptr_array_set_size(vals, 0);
    g_ptr_array_set_free_func(vals, g_free);
    /* Find the end of line */
    while (eol < end && *eol != '\n') eol++;
    /* Skip spaces before the first word */
    while (ptr < eol && g_ascii_isspace(*ptr)) ptr++;
    /* Parse values word by word */
    while (ptr < eol) {
        const char* word = ptr;
        while (ptr < eol && !g_ascii_isspace(*ptr)) ptr++;
        g_ptr_array_add(vals, g_strndup(word, ptr - word));
        /* Skip spaces after the word */
        while (ptr < eol && g_ascii_isspace(*ptr)) ptr++;
    }
    return ptr;
}

static
gboolean
da_cred_match(
    const char* key,
    const char* str,
    gsize len)
{
    return (!g_ascii_strncasecmp(key, str, len) && !key[len]);
}

gboolean
da_cred_parse(
    DACred* cred,
    DACredPriv* priv,
    const char* data,
    gsize len)
{
    guint flags = 0;
    const char* ptr = data;
    const char* eof = data + len;
    GPtrArray* val = g_ptr_array_new_with_free_func(g_free);
    while (ptr < eof && (flags & PROC_PARSE_ALL) != PROC_PARSE_ALL) {
        while (ptr < eof && g_ascii_isspace(*ptr)) ptr++;
        if (ptr < eof) {
            /* We are at the first non-empty character of the line */
            const char* start = ptr;
            while (ptr < eof && !g_ascii_isspace(*ptr) && *ptr != ':') ptr++;
            if (ptr < eof) {
                if (*ptr == ':') {
                    /* We've got the key */
                    const gsize keylen = ptr - start;
                    /* Skip the delimiter */
                    ptr++;
                    if (!(flags & PROC_PARSE_UID) &&
                        da_cred_match("Uid", start, keylen)) {
                        /* Real, effective, saved set, and filesystem UIDs */
                        guint32 uid;
                        flags |= PROC_PARSE_UID;
                        ptr = da_cred_parse_values(val, ptr, eof);
                        if (val->len == 4 &&
                            da_cred_parse_uint32(val->pdata[1], &uid)) {
                            flags |= PROC_PARSE_UID_OK;
                            cred->euid = uid;
                        }
                    } else if (!(flags & PROC_PARSE_GID) &&
                               da_cred_match("Gid", start, keylen)) {
                        /* Real, effective, saved set, and filesystem GIDs */
                        guint32 gid;
                        flags |= PROC_PARSE_GID;
                        ptr = da_cred_parse_values(val, ptr, eof);
                        if (val->len == 4 &&
                            da_cred_parse_uint32(val->pdata[1], &gid)) {
                            flags |= PROC_PARSE_GID_OK;
                            cred->egid = gid;
                        }
                    } else if (!(flags & PROC_PARSE_GROUPS) &&
                               da_cred_match("Groups", start, keylen)) {
                        /* Supplementary group list */
                        flags |= PROC_PARSE_GROUPS;
                        cred->flags |= DBUSACCESS_CRED_GROUPS;
                        ptr = da_cred_parse_values(val, ptr, eof);
                        if (val->len > 0) {
                            guint i;
                            priv->groups = g_new(gid_t, val->len);
                            for (i=0; i<val->len; i++) {
                                guint32 g;
                                /* Should we clear the DBUSACCESS_CRED_GROUPS
                                 * if parsing fails? */
                                if (da_cred_parse_uint32(val->pdata[i], &g)) {
                                    priv->groups[cred->ngroups++] = g;
                                }
                            }
                            if (cred->ngroups > 0) {
                                cred->groups = priv->groups;
                            }
                        }
                    } else if (!(flags & PROC_PARSE_CAP_EFF) &&
                               da_cred_match("CapEff", start, keylen)) {
                        /* Effective capability set */
                        flags |= PROC_PARSE_CAP_EFF;
                        ptr = da_cred_parse_values(val, ptr, eof);
                        if (val->len == 1 &&
                            da_cred_parse_uint64(val->pdata[0], &cred->caps)) {
                            cred->flags |= DBUSACCESS_CRED_CAPS;
                        }
                    }
                }
                /* Skip to the end of line, eat one EOL character */
                while (ptr < eof && *ptr != '\n') ptr++;
                if (ptr < eof) ptr++;
            }
        }
    }
    g_ptr_array_unref(val); 
    return (flags & PROC_PARSE_UID_OK) && (flags & PROC_PARSE_GID_OK);
}

void
da_cred_priv_cleanup(
    DACredPriv* priv)
{
    if (priv->groups) {
        g_free(priv->groups);
        priv->groups = NULL;
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
