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
#include "dbusaccess_log.h"

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>

static guint self_shared_timeout_id;
static DASelf* self_shared;

#define DBUSACCESS_SELF_TIMEOUT_SEC (30)
#define DBUSACCESS_SELF_TIMEOUT_SEC_ENV "DBUSACCESS_SELF_TIMEOUT_SEC"

DASelf*
da_self_new(
    void)
{
    return da_proc_new(getpid());
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

DASelf*
da_self_ref(
    DASelf* self)
{
    return da_proc_ref(self);
}

void
da_self_unref(
    DASelf* self)
{
    da_proc_unref(self);
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
