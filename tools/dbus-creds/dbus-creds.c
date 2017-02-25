/*
 * Copyright (C) 2017 Jolla Ltd.
 * Contact: Slava Monich <slava.monich@jolla.com>
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
 *   3. Neither the name of Jolla Ltd nor the names of its contributors may
 *      be used to endorse or promote products derived from this software
 *      without specific prior written permission.
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

#include "dbusaccess_peer.h"
#include <gutil_log.h>

#define HELP_SUMMARY "Shows D-Bus credentials for the client specified by NAME"

#define RET_OK          (0)
#define RET_NOTFOUND    (1)
#define RET_ERR         (2)

typedef struct app {
    DA_BUS bus;

    const char* name;
} App;

static
int
app_run(
    App* app)
{
    DAPeer* peer = da_peer_get(app->bus, app->name);
    if (peer) {
        GDEBUG("%s bus client \"%s\"", (peer->bus == DA_BUS_SYSTEM) ?
            "System" : "Session", peer->name);
        printf("Pid: %d\n", peer->pid);
        printf("Uid: %d\n", peer->cred.euid);
        printf("Gid: %d\n", peer->cred.egid);
        GASSERT(da_peer_get(app->bus, app->name) == peer);
        if (peer->cred.flags & DBUSACCESS_CRED_GROUPS) {
            GString* buf = g_string_new(NULL);
            guint i;
            for (i=0; i<peer->cred.ngroups; i++) {
                if (buf->len > 0) {
                    g_string_append_c(buf, ' ');
                }
                g_string_append_printf(buf, "%d", peer->cred.groups[i]);
            }
            printf("Groups: %s\n", buf->str);
            g_string_free(buf, TRUE);
        }
        if (peer->cred.flags & DBUSACCESS_CRED_CAPS) {
            printf("Caps: 0x%016llx\n", (unsigned long long)peer->cred.caps);
        }
        da_peer_flush(app->bus, app->name);
        return RET_OK;
    } else {
        GERR("D-Bus name \"%s\" nit found", app->name);
        return RET_NOTFOUND;
    }
}

static
gboolean
app_log_verbose(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    gutil_log_default.level = GLOG_LEVEL_VERBOSE;
    return TRUE;
}

static
gboolean
app_log_quiet(
    const gchar* name,
    const gchar* value,
    gpointer data,
    GError** error)
{
    gutil_log_default.level = GLOG_LEVEL_NONE;
    return TRUE;
}

static
gboolean
app_init(
    App* app,
    int argc,
    char* argv[])
{
    gboolean ok = FALSE;
    gboolean system_bus = FALSE;
    GOptionEntry entries[] = {
        { "system", 0, 0, G_OPTION_ARG_NONE, &system_bus,
          "Use system bus (default is session)", NULL },
        { "verbose", 'v', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          app_log_verbose, "Enable verbose output", NULL },
        { "quiet", 'q', G_OPTION_FLAG_NO_ARG, G_OPTION_ARG_CALLBACK,
          app_log_quiet, "Be quiet", NULL },
        { NULL }
    };
    GError* error = NULL;
    GOptionContext* options = g_option_context_new("NAME");
    g_option_context_set_summary(options, HELP_SUMMARY);
    g_option_context_add_main_entries(options, entries, NULL);
    if (g_option_context_parse(options, &argc, &argv, &error)) {
        if (argc == 2) {
            app->bus = system_bus ? DA_BUS_SYSTEM : DA_BUS_SESSION;
            app->name = argv[1];
            ok = TRUE;
        } else {
            char* help = g_option_context_get_help(options, TRUE, NULL);
            fprintf(stderr, "%s", help);
            g_free(help);
        }
    } else {
        GERR("%s", error->message);
        g_error_free(error);
    }
    g_option_context_free(options);
    return ok;
}

int main(int argc, char* argv[])
{
    int ret = RET_ERR;
    App app;
    memset(&app, 0, sizeof(app));
    gutil_log_timestamp = FALSE;
    gutil_log_set_type(GLOG_TYPE_STDERR, "dbus-creds");
    gutil_log_default.level = GLOG_LEVEL_DEFAULT;
    if (app_init(&app, argc, argv)) {
        ret = app_run(&app);
    }
    return ret;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
