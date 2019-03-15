/*
 * Copyright (C) 2017-2019 Jolla Ltd.
 * Copyright (C) 2017-2019 Slava Monich <slava.monich@jolla.com>
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

#include "dbusaccess_peer.h"
#include "dbusaccess_cred_p.h"
#include "dbusaccess_log.h"

#include <gio/gio.h>

#include <gutil_macros.h>

/* Log module */
GLOG_MODULE_DEFINE("dbusaccess");

#define DBUSACCESS_PEER_TIMEOUT_SEC (30)

typedef struct da_peer_bus {
    GHashTable* peers;
    GDBusConnection* connection;
} DAPeerBus;

typedef struct da_peer_priv {
    DAPeer pub;
    DACredPriv cred;
    DAPeerBus* bus;
    char* name;
    gint ref_count;
    guint timeout_id;
    guint name_watch_id;
} DAPeerPriv;

static inline DAPeerPriv* da_peer_cast(DAPeer* peer)
    { return G_CAST(peer, DAPeerPriv, pub); }

static
void
da_peer_unref1(
    gpointer data)
{
    DAPeerPriv* priv = data;
    da_peer_unref(&priv->pub);
}

static
DAPeerBus*
da_peer_bus(
    DA_BUS type,
    gboolean initialize)
{
    static DAPeerBus da_bus[2];
    DAPeerBus* bus;
    GBusType bus_type;
    switch (type) {
    case DA_BUS_SYSTEM:
        bus = da_bus + 0;
        bus_type = G_BUS_TYPE_SYSTEM;
        break;
    case DA_BUS_SESSION:
        bus = da_bus + 1;
        bus_type = G_BUS_TYPE_SESSION;
        break;
    default:
        GERR("Invalid bus type %d", type);
        return NULL;
    }
    if (!bus->connection && initialize) {
        bus->connection = g_bus_get_sync(bus_type, NULL, NULL);
    }
    if (bus->connection) {
        if (!bus->peers) {
            bus->peers = g_hash_table_new_full(g_str_hash, g_str_equal,
                NULL, da_peer_unref1);
        }
        return bus;
    } else {
        return NULL;
    }
}

static
void
da_peer_dispose(
    DAPeerPriv* priv)
{
    if (priv->name_watch_id) {
        g_bus_unwatch_name(priv->name_watch_id);
        priv->name_watch_id = 0;
    }
    if (priv->timeout_id) {
        g_source_remove(priv->timeout_id);
        priv->timeout_id = 0;
    }
}

static
void
da_peer_cleanup(
    DAPeerBus* bus)
{
    if (g_hash_table_size(bus->peers) == 0) {
        g_hash_table_unref(bus->peers);
        bus->peers = NULL;
        if (bus->connection) {
            g_object_unref(bus->connection);
            bus->connection = NULL;
        }
    }
}

static
void
da_peer_remove(
    DAPeerPriv* priv)
{
    DAPeerBus* bus = priv->bus;
    da_peer_dispose(priv);
    g_hash_table_remove(bus->peers, priv->name);
    da_peer_cleanup(bus);
}

static
gboolean
da_peer_timeout(
    gpointer data)
{
    DAPeerPriv* priv = data;
    GDEBUG("Name '%s' timed out", priv->name);
    priv->timeout_id = 0;
    da_peer_remove(priv);
    return G_SOURCE_REMOVE;
}

static
void
da_peer_vanished(
    GDBusConnection* bus,
    const char* name,
    gpointer data)
{
    GDEBUG("Name '%s' has disappeared", name);
    da_peer_remove(data);
}
static
void
da_peer_reset_timeout(
    DAPeerPriv* priv)
{
    if (priv->timeout_id) {
        g_source_remove(priv->timeout_id);
    }
    priv->timeout_id = g_timeout_add_seconds(DBUSACCESS_PEER_TIMEOUT_SEC,
        da_peer_timeout, priv);
}

static
DAPeerPriv*
da_peer_new(
    DAPeerBus* bus,
    const char* name)
{
    DAPeerPriv* priv = g_slice_new0(DAPeerPriv);
    DAPeer* peer = &priv->pub;
    peer->name = priv->name = g_strdup(name);
    priv->bus = bus;
    priv->ref_count = 1;
    da_peer_reset_timeout(priv);
    priv->name_watch_id = g_bus_watch_name_on_connection(bus->connection, name,
        G_BUS_NAME_WATCHER_FLAGS_NONE, NULL, da_peer_vanished, priv, NULL);
    return priv;
}

static
void
da_peer_finalize(
    DAPeerPriv* priv)
{
    if (priv->name_watch_id) {
        g_bus_unwatch_name(priv->name_watch_id);
    }
    if (priv->timeout_id) {
        g_source_remove(priv->timeout_id);
    }
    da_cred_priv_cleanup(&priv->cred);
    g_free(priv->name);
}

DAPeer*
da_peer_ref(
    DAPeer* peer)
{
    if (peer) {
        DAPeerPriv* priv = da_peer_cast(peer);
        g_atomic_int_inc(&priv->ref_count);
    }
    return peer;
}

void
da_peer_unref(
    DAPeer* peer)
{
    if (peer) {
        DAPeerPriv* priv = da_peer_cast(peer);
        if (g_atomic_int_dec_and_test(&priv->ref_count)) {
            da_peer_finalize(priv);
            g_slice_free(DAPeerPriv, priv);
        }
    }
}

static
gboolean
da_peer_fill_cred(
    DAPeerPriv* priv,
    guint pid)
{
    char* fname = g_strdup_printf("/proc/%u/status", pid);
    GError* error = NULL;
    gchar* data = NULL;
    gsize len = 0;
    gboolean ok;
    if (g_file_get_contents(fname, &data, &len, &error)) {
        GDEBUG("Parsing %s", fname);
        ok = da_cred_parse(&priv->pub.cred, &priv->cred, data, len);
        g_free(data);
    } else {
        GDEBUG("%s: %s", fname, GERRMSG(error));
        g_error_free(error);
        ok = FALSE;
    }
    g_free(fname);
    return ok;
}

DAPeer*
da_peer_get(
    DA_BUS type,
    const char* name)
{
    if (name) {
        DAPeerBus* bus = da_peer_bus(type, TRUE);
        if (bus) {
            DAPeerPriv* priv = g_hash_table_lookup(bus->peers, name);
            if (priv) {
                /* Found cached entry */
                da_peer_reset_timeout(priv);
                return &priv->pub;
            } else {
                /* No information about this one */
                GError* error = NULL;
                /* This call will block */
                GVariant* ret = g_dbus_connection_call_sync(bus->connection,
                    "org.freedesktop.DBus", "/org/freedesktop/DBus",
                    "org.freedesktop.DBus", "GetConnectionUnixProcessID",
                    g_variant_new("(s)", name), NULL, G_DBUS_CALL_FLAGS_NONE,
                    -1, NULL, &error);
                if (ret) {
                    guint pid = 0;
                    DAPeerPriv* priv = da_peer_new(bus, name);
                    g_variant_get(ret, "(u)", &pid);
                    g_variant_unref(ret);
                    /* We've got the pid, read /proc/pid/status */
                    if (da_peer_fill_cred(priv, pid)) {
                        /* Cache this info */
                        priv->pub.pid = pid;
                        g_hash_table_replace(bus->peers, priv->name, priv);
                        return &priv->pub;
                    } else {
                        da_peer_unref(&priv->pub);
                    }
                } else {
                    GDEBUG("%s", GERRMSG(error));
                    g_error_free(error);
                }
                da_peer_cleanup(bus);
            }
        }
    }
    return NULL;
}

void
da_peer_flush(
    DA_BUS type,
    const char* name)
{
    DAPeerBus* bus = da_peer_bus(type, FALSE);
    if (bus) {
        if (name) {
            /* Flush information about the specific name */
            DAPeerPriv* priv = g_hash_table_lookup(bus->peers, name);
            if (priv) {
                da_peer_remove(priv);
            }
        } else {
            /* Flush everything for this bus */
            GHashTableIter it;
            gpointer value;
            /* Stop all the timers, unwatch the names etc. */
            g_hash_table_iter_init(&it, bus->peers);
            while (g_hash_table_iter_next(&it, NULL, &value)) {
                da_peer_dispose(value);
            }
            g_hash_table_remove_all(bus->peers);
            da_peer_cleanup(bus);
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
