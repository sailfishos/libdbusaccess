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

#include "test_common.h"

#include "dbusaccess_self.h"

#include <stdlib.h>

static TestOpt test_opt;

static
gboolean
test_loop_quit(
    gpointer data)
{
    g_main_loop_quit(data);
    return G_SOURCE_REMOVE;
}

/*==========================================================================*
 * Invalid
 *==========================================================================*/

static
void
test_proc_invalid(
    void)
{
    /* Test NULL resistance */
    g_assert(!da_proc_ref(NULL));
    g_assert(!da_proc_new(0));
    da_proc_unref(NULL);

    /* Try to query credentials for invalid pid */
    g_assert(!da_proc_new((pid_t)0xffffffff));
}

/*==========================================================================*
 * Self
 *==========================================================================*/

static
void
test_proc_self(
    void)
{
    DASelf* self;

    /* Test NULL resistance */
    g_assert(!da_self_ref(NULL));
    da_self_unref(NULL);

    /* Actually query credentials */
    self = da_self_new();
    g_assert(self);
    da_self_unref(da_self_ref(self));
    da_self_unref(self);
}

/*==========================================================================*
 * SelfShared
 *==========================================================================*/

static
void
test_proc_self_shared(
    void)
{
    GMainLoop* loop = g_main_loop_new(NULL, TRUE);

    /* Allocate two instances */
    DASelf* self1 = da_self_new_shared();
    DASelf* self2 = da_self_new_shared();
    g_assert(self1);
    g_assert(self1 == self2);

    /* Unref both */
    da_self_unref(self1);
    da_self_unref(self2);

    /* Next call still returns the same pointer */
    self1 = da_self_new_shared();
    g_assert(self1 == self2);
    da_self_unref(self1);

    /* Clear the shared instance */
    da_self_flush();
    da_self_flush();

    /* Set shared instance timeout to the minimum */
    setenv("DBUSACCESS_SELF_TIMEOUT_SEC", "0", TRUE);

    /* This allocates a new shared instance */
    self1 = da_self_new_shared();
    g_assert(self1);
    /* Disabled because with glib 2.78.4 the pointers are the same */
    //g_assert(self1 != self2);

    /* Let it expire */
    g_timeout_add_seconds(0, test_loop_quit, loop);
    g_main_loop_run(loop);

    /* This allocates a new shared instance */
    self2 = da_self_new_shared();
    g_assert(self2);
    g_assert(self1 != self2);

    da_self_unref(self1);
    da_self_unref(self2);
    da_self_flush();
    g_main_loop_unref(loop);
}

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_(t) "/proc/" t

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_("invalid"), test_proc_invalid);
    g_test_add_func(TEST_("self"), test_proc_self);
    g_test_add_func(TEST_("self_shared"), test_proc_self_shared);
    test_init(&test_opt, argc, argv);
    return g_test_run();
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
