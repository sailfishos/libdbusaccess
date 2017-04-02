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

#ifndef DBUSACCESS_TYPES_H
#define DBUSACCESS_TYPES_H

#include <gutil_types.h>

G_BEGIN_DECLS

#define DBUSACCESS_LOG_MODULE dbusaccess_log

typedef enum da_access {
    DA_ACCESS_DENY,
    DA_ACCESS_ALLOW
} DA_ACCESS;

typedef enum da_bus {
    DA_BUS_SYSTEM,
    DA_BUS_SESSION
} DA_BUS;

typedef struct da_cred {
    uid_t euid;
    gid_t egid;
    const gid_t* groups;
    guint ngroups;
    guint64 caps;
    guint32 flags;

#define DBUSACCESS_CRED_CAPS    (0x0001)
#define DBUSACCESS_CRED_GROUPS  (0x0002)

} DACred;

typedef struct da_self DASelf;
typedef struct da_peer DAPeer;
typedef struct da_policy /* opaque */ DAPolicy;

extern GLogModule DBUSACCESS_LOG_MODULE;

G_END_DECLS

#endif /* DBUSACCESS_TYPES_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

