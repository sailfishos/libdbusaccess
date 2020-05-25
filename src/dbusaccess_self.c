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

#include "dbusaccess_self.h"
#include "dbusaccess_cred_p.h"
#include "dbusaccess_log.h"

#include <gutil_macros.h>

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

typedef struct da_self_priv {
    DASelf pub;
    DACredPriv cred;
    gint ref_count;
} DASelfPriv;

static guint self_shared_timeout_id;
static DASelf* self_shared;

#define DBUSACCESS_SELF_TIMEOUT_SEC (30)
#define DBUSACCESS_SELF_TIMEOUT_SEC_ENV "DBUSACCESS_SELF_TIMEOUT_SEC"

static inline DASelfPriv* da_self_cast(DASelf* self)
    { return G_CAST(self, DASelfPriv, pub); }

DASelf*
da_self_new(
    void)
{
    DASelf* self = NULL;
    pid_t pid = getpid();
    char* fname = g_strdup_printf("/proc/%u/status", (guint)pid);
    GError* error = NULL;
    gchar* data = NULL;
    gsize len = 0;
    if (g_file_get_contents(fname, &data, &len, &error)) {
        DASelfPriv* priv = g_slice_new0(DASelfPriv);
        priv->ref_count = 1;
        GDEBUG("Parsing %s", fname);
        if (da_cred_parse(&priv->pub.cred, &priv->cred, data, len)) {
            self = &priv->pub;
            self->pid = pid;
        } else {
            g_slice_free(DASelfPriv, priv);
        }
        g_free(data);
    } else {
        GDEBUG("%s: %s", fname, GERRMSG(error));
        g_error_free(error);
    }
    g_free(fname);
    return self;
}

static
gboolean
da_self_shared_timeout(
    gpointer data)
{
    GVERBOSE_("resetting shared instance");
    GASSERT(self_shared_timeout_id);
    GASSERT(self_shared == data);
    self_shared_timeout_id = 0;
    self_shared = NULL;
    return G_SOURCE_REMOVE;
}

static
void
da_self_shared_unref(
    gpointer data)
{
    da_self_unref(data);
}

DASelf*
da_self_new_shared(
    void)
{
    if (!self_shared) {
        guint sec = DBUSACCESS_SELF_TIMEOUT_SEC;
        const char* env = getenv(DBUSACCESS_SELF_TIMEOUT_SEC_ENV);
        if (env) {
            int interval = atoi(env);
            if (interval >= 0) {
                sec = interval;
            }
        }
        GASSERT(!self_shared_timeout_id);
        self_shared = da_self_new();
        self_shared_timeout_id = g_timeout_add_seconds_full(G_PRIORITY_DEFAULT,
            sec, da_self_shared_timeout, self_shared, da_self_shared_unref);
    }
    return da_self_ref(self_shared);
}

static
void
da_self_finalize(
    DASelfPriv* priv)
{
    da_cred_priv_cleanup(&priv->cred);
}

DASelf*
da_self_ref(
    DASelf* self)
{
    if (self) {
        DASelfPriv* priv = da_self_cast(self);
        g_atomic_int_inc(&priv->ref_count);
    }
    return self;
}

void
da_self_unref(
    DASelf* self)
{
    if (self) {
        DASelfPriv* priv = da_self_cast(self);
        if (g_atomic_int_dec_and_test(&priv->ref_count)) {
            da_self_finalize(priv);
            g_slice_free(DASelfPriv, priv);
        }
    }
}

void
da_self_flush(
    void)
{
    self_shared = NULL;
    if (self_shared_timeout_id) {
        g_source_remove(self_shared_timeout_id);
        self_shared_timeout_id = 0;
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
