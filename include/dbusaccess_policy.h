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

#ifndef DBUSACCESS_POLICY_H
#define DBUSACCESS_POLICY_H

#include "dbusaccess_types.h"

G_BEGIN_DECLS

/*
 * The policy grammar is described in the .y file. Here is an example:
 *
 * 1;group(privileged)&get(Passphrase)=allow
 *
 * It's basically the format version (1) followed by rules separated
 * by semicolons. Each rule is a logical expression optionally followed
 * by the access specifier =allow or =deny (default is =allow). The
 * rules are applied in the order in this they appear in the policy.
 * The following functions are built into the parser:
 *
 * user(uid)
 * group(gid)
 * user(uid:gid) is equivalent to (user(uid)&group(gid))
 *
 * Three logical operators are supported: ! (not), | (or), & (and) as
 * as parentheses for complex expressions. If the pattern contains spaces,
 * it has to be enclosed in quotes, either single or double.
 */

#define DA_POLICY_VERSION "1"

typedef struct da_action {
    const char* name;   /* Action name defined by the caller */
    guint id;           /* Non-zero unique id for compiled representation */
    guint args;         /* Number of arguments (currently only 0 or 1) */
} DA_ACTION;

DAPolicy*
da_policy_new(
    const char* spec);

DAPolicy*
da_policy_new_full(
    const char* spec,
    const DA_ACTION* actions);

DAPolicy*
da_policy_ref(
    DAPolicy* policy);

void
da_policy_unref(
    DAPolicy* policy);

gboolean
da_policy_equal(
    const DAPolicy* policy1,
    const DAPolicy* policy2);

DA_ACCESS
da_policy_check(
    const DAPolicy* policy,
    const DACred* cred,
    guint action,
    const char* arg,
    DA_ACCESS def);

G_END_DECLS

#endif /* DBUSACCESS_POLICY_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
