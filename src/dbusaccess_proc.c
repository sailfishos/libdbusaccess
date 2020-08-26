/*
 * Copyright (C) 2020 Jolla Ltd.
 * Copyright (C) 2020 Slava Monich <slava.monich@jolla.com>
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

#include "dbusaccess_proc.h"
#include "dbusaccess_cred_p.h"
#include "dbusaccess_log.h"

#include <gutil_macros.h>

typedef struct da_proc_priv {
    DAProc pub;
    DACredPriv cred;
    gint ref_count;
} DAProcPriv;

static inline DAProcPriv* da_proc_cast(DAProc* proc)
    { return G_CAST(proc, DAProcPriv, pub); }

DAProc*
da_proc_new(
    pid_t pid)
{
    DAProc* proc = NULL;

    if (pid) {
        char* fname = g_strdup_printf("/proc/%u/status", (guint)pid);
        GError* error = NULL;
        gchar* data = NULL;
        gsize len = 0;

        if (g_file_get_contents(fname, &data, &len, &error)) {
            DAProcPriv* priv = g_slice_new0(DAProcPriv);

            priv->ref_count = 1;
            GDEBUG("Parsing %s", fname);
            if (da_cred_parse(&priv->pub.cred, &priv->cred, data, len)) {
                proc = &priv->pub;
                proc->pid = pid;
            } else {
                g_slice_free(DAProcPriv, priv);
            }
            g_free(data);
        } else {
            GDEBUG("%s: %s", fname, GERRMSG(error));
            g_error_free(error);
        }
        g_free(fname);
    }
    return proc;
}

static
void
da_proc_finalize(
    DAProcPriv* priv)
{
    da_cred_priv_cleanup(&priv->cred);
}

DAProc*
da_proc_ref(
    DAProc* proc)
{
    if (proc) {
        DAProcPriv* priv = da_proc_cast(proc);
        g_atomic_int_inc(&priv->ref_count);
    }
    return proc;
}

void
da_proc_unref(
    DAProc* proc)
{
    if (proc) {
        DAProcPriv* priv = da_proc_cast(proc);
        if (g_atomic_int_dec_and_test(&priv->ref_count)) {
            da_proc_finalize(priv);
            g_slice_free(DAProcPriv, priv);
        }
    }
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
