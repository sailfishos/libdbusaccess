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

#include "test_common.h"

#include "dbusaccess_cred_p.h"

static TestOpt test_opt;

static
void
test_cred_parse_and_compare(
    const DACred* expected,
    const char* data)
{
    DACred cred;
    DACredPriv priv;
    guint i;
    memset(&priv, 0, sizeof(priv));
    memset(&cred, 0, sizeof(cred));
    g_assert(da_cred_parse(&cred, &priv, data, strlen(data)));
    g_assert(cred.euid == expected->euid);
    g_assert(cred.egid == expected->egid);
    g_assert(cred.caps == expected->caps);
    g_assert(cred.flags == expected->flags);
    g_assert(cred.ngroups == expected->ngroups);
    for (i=0; i<cred.ngroups; i++) {
        g_assert(cred.groups[i] == expected->groups[i]);
    }
    da_cred_priv_cleanup(&priv);
}

/*==========================================================================*
 * Empty
 *==========================================================================*/

static
void
test_cred_empty(
    void)
{
    DACred cred;
    DACredPriv priv;
    memset(&priv, 0, sizeof(priv));
    memset(&cred, 0, sizeof(cred));
    g_assert(!da_cred_parse(&cred, &priv, "", 0));
}

/*==========================================================================*
 * Basic 1
 *==========================================================================*/

static
void
test_cred_basic1(
    void)
{
    static const gid_t groups[] = {
        39, 100, 993, 996, 997, 999, 1000, 1002, 1003, 1004,
        1005, 1006, 1024, 100000
    };
    static const DACred expected = {
        100000, 998,
        groups, G_N_ELEMENTS(groups),
        0,
        DBUSACCESS_CRED_CAPS | DBUSACCESS_CRED_GROUPS
    };
    test_cred_parse_and_compare(&expected, "\
Name:	jolla-settings\n\
State:	S (sleeping)\n\
Tgid:	9128\n\
Pid:	9128\n\
PPid:	1121\n\
TracerPid:	0\n\
Uid:	100000	100000	100000	100000\n\
Gid:	100000	998	998	998\n\
FDSize:	256\n\
Groups:	39 100 993 996 997 999 1000 1002 1003 1004 1005 1006 1024 100000 \n\
VmPeak:	  163272 kB\n\
VmSize:	  144492 kB\n\
VmLck:	       0 kB\n\
VmPin:	       0 kB\n\
VmHWM:	   59688 kB\n\
VmRSS:	   49480 kB\n\
VmData:	   59360 kB\n\
VmStk:	     268 kB\n\
VmExe:	      20 kB\n\
VmLib:	   68820 kB\n\
VmPTE:	     162 kB\n\
VmSwap:	       0 kB\n\
Threads:	11\n\
SigQ:	0/6452\n\
SigPnd:	0000000000000000\n\
ShdPnd:	0000000000000000\n\
SigBlk:	0000000000000000\n\
SigIgn:	0000000000001000\n\
SigCgt:	0000000180000000\n\
CapInh:	0000000000000000\n\
CapPrm:	0000000000000000\n\
CapEff:	0000000000000000\n\
CapBnd:	ffffffffffffffff\n\
Cpus_allowed:	3\n\
Cpus_allowed_list:	0-1\n\
voluntary_ctxt_switches:	17371\n\
nonvoluntary_ctxt_switches:	4891\n");
}

/*==========================================================================*
 * Basic 2
 *==========================================================================*/

static
void
test_cred_basic2(
    void)
{
    static const DACred expected = {
        0, 0,
        NULL, 0,
        G_GUINT64_CONSTANT(0xfffffff008003420),
        DBUSACCESS_CRED_CAPS | DBUSACCESS_CRED_GROUPS
    };
    test_cred_parse_and_compare(&expected, "\
Name:	connman-vpnd\n\
State:	S (sleeping)\n\
Tgid:	24054\n\
Pid:	24054\n\
PPid:	1\n\
TracerPid:	0\n\
Uid:	0	0	0	0\n\
Gid:	0	0	0	0\n\
FDSize:	256\n\
Groups:	\n\
VmPeak:	    8480 kB\n\
VmSize:	    8476 kB\n\
VmLck:	       0 kB\n\
VmPin:	       0 kB\n\
VmHWM:	    2172 kB\n\
VmRSS:	    2172 kB\n\
VmData:	     340 kB\n\
VmStk:	     136 kB\n\
VmExe:	     164 kB\n\
VmLib:	    5824 kB\n\
VmPTE:	      36 kB\n\
VmSwap:	       0 kB\n\
Threads:	1\n\
SigQ:	0/6452\n\
SigPnd:	0000000000000000\n\
ShdPnd:	0000000000000000\n\
SigBlk:	0000000000004002\n\
SigIgn:	0000000000001000\n\
SigCgt:	0000000180000000\n\
CapInh:	0000000000000000\n\
CapPrm:	fffffff008003420\n\
CapEff:	fffffff008003420\n\
CapBnd:	fffffff008003420\n\
Cpus_allowed:	3\n\
Cpus_allowed_list:	0-1\n\
voluntary_ctxt_switches:	48\n\
nonvoluntary_ctxt_switches:	27\n");
}

/*==========================================================================*
 * Garbage 1
 *==========================================================================*/

static
void
test_cred_garbage1(
    void)
{
    static const DACred expected = {
        0, 0,
        NULL, 0, 0,
        DBUSACCESS_CRED_GROUPS
    };
    test_cred_parse_and_compare(&expected, "\
\n\
Line without a colon\n\
Uid:	0	0	0	0\n\
Gid:	0	0	0	0\n\
CapEff:	garbage\n\
Group: this won't match Groups\n\
Groups : this one is ignored because of the space before :\n\
Groups:");
}

/*==========================================================================*
 * Garbage 2
 *==========================================================================*/

static
void
test_cred_garbage2(
    void)
{
    static const DACred expected = {
        0, 0,
        NULL, 0,
        G_GUINT64_CONSTANT(0xfffffff008003420),
        DBUSACCESS_CRED_CAPS
    };
    test_cred_parse_and_compare(&expected, "\
\n\
Line without a colon\n\
SingleWordWithColon:\n\
Uid:	0	0	0	0\n\
Gid:	0	0	0	0\n\
Group: this won't match Groups\n\
CapEff:	fffffff008003420");
}

/*==========================================================================*
 * Garbage 3
 *==========================================================================*/

static
void
test_cred_garbage3(
    void)
{
    static const DACred expected = {
        0, 0,
        NULL, 0,
        G_GUINT64_CONSTANT(0xfffffff008003420),
        DBUSACCESS_CRED_CAPS
    };
    test_cred_parse_and_compare(&expected, "\
Uid:	0	0	0	0\n\
Gid:	0	0	0	0\n\
CapEff:	fffffff008003420\n\
NoColonHere");
}

/*==========================================================================*
 * Garbage 4
 *==========================================================================*/

static
void
test_cred_garbage4(
    void)
{
    static const DACred expected = { 0 };
    test_cred_parse_and_compare(&expected, "\
Uid:	0	0	0	0\n\
Gid:	0	0	0	0\n\
CapEff:	fffffff008003420 fffffff008003420\n");
}

/*==========================================================================*
 * Upper case
 *==========================================================================*/

static
void
test_cred_upcase(
    void)
{
    static const DACred expected = {
        0, 0,
        NULL, 0,
        G_GUINT64_CONSTANT(0xfffffff008003420),
        DBUSACCESS_CRED_CAPS | DBUSACCESS_CRED_GROUPS
    };
    test_cred_parse_and_compare(&expected, "\
NAME:	CONNMAN-VPND\n\
STATE:	S (SLEEPING)\n\
TGID:	24054\n\
PID:	24054\n\
PPID:	1\n\
TRACERPID:	0\n\
UID:	0	0	0	0\n\
GID:	0	0	0	0\n\
FDSIZE:	256\n\
GROUPS:	\n\
VMPEAK:	    8480 KB\n\
VMSIZE:	    8476 KB\n\
VMLCK:	       0 KB\n\
VMPIN:	       0 KB\n\
VMHWM:	    2172 KB\n\
VMRSS:	    2172 KB\n\
VMDATA:	     340 KB\n\
VMSTK:	     136 KB\n\
VMEXE:	     164 KB\n\
VMLIB:	    5824 KB\n\
VMPTE:	      36 KB\n\
VMSWAP:	       0 KB\n\
THREADS:	1\n\
SIGQ:	0/6452\n\
SIGPND:	0000000000000000\n\
SHDPND:	0000000000000000\n\
SIGBLK:	0000000000004002\n\
SIGIGN:	0000000000001000\n\
SIGCGT:	0000000180000000\n\
CAPINH:	0000000000000000\n\
CAPPRM:	FFFFFFF008003420\n\
CAPEFF:	FFFFFFF008003420\n\
CAPBND:	FFFFFFF008003420\n\
CPUS_ALLOWED:	3\n\
CPUS_ALLOWED_LIST:	0-1\n\
VOLUNTARY_CTXT_SWITCHES:	48\n\
NONVOLUNTARY_CTXT_SWITCHES:	27\n");
}

/*==========================================================================*
 * Lower case
 *==========================================================================*/

static
void
test_cred_lowcase(
    void)
{
    static const DACred expected = {
        0, 0,
        NULL, 0,
        G_GUINT64_CONSTANT(0xfffffff008003420),
        DBUSACCESS_CRED_CAPS | DBUSACCESS_CRED_GROUPS
    };
    test_cred_parse_and_compare(&expected, "\
name:	connman-vpnd\n\
state:	s (sleeping)\n\
tgid:	24054\n\
pid:	24054\n\
ppid:	1\n\
tracerpid:	0\n\
uid:	0	0	0	0\n\
gid:	0	0	0	0\n\
fdsize:	256\n\
groups:	\n\
vmpeak:	    8480 kb\n\
vmsize:	    8476 kb\n\
vmlck:	       0 kb\n\
vmpin:	       0 kb\n\
vmhwm:	    2172 kb\n\
vmrss:	    2172 kb\n\
vmdata:	     340 kb\n\
vmstk:	     136 kb\n\
vmexe:	     164 kb\n\
vmlib:	    5824 kb\n\
vmpte:	      36 kb\n\
vmswap:	       0 kb\n\
threads:	1\n\
sigq:	0/6452\n\
sigpnd:	0000000000000000\n\
shdpnd:	0000000000000000\n\
sigblk:	0000000000004002\n\
sigign:	0000000000001000\n\
sigcgt:	0000000180000000\n\
capinh:	0000000000000000\n\
capprm:	fffffff008003420\n\
capeff:	fffffff008003420\n\
capbnd:	fffffff008003420\n\
cpus_allowed:	3\n\
cpus_allowed_list:	0-1\n\
voluntary_ctxt_switches:	48\n\
nonvoluntary_ctxt_switches:	27\n");
}

/*==========================================================================*
 * No groups
 *==========================================================================*/

static
void
test_cred_nogroups(
    void)
{
    static const DACred expected = { 0 };
    test_cred_parse_and_compare(&expected, "\
Uid:	0	0	0	0\n\
Gid:	0	0	0	0\n "); /* extra space on purpose */
}

/*==========================================================================*
 * Bad UID count
 *==========================================================================*/

static
void
test_cred_baduid1(
    void)
{
    static const char data[] = "\
Uid:	0	0	0\n\
Gid:	0	0	0\n";
    DACred cred;
    DACredPriv priv;
    memset(&priv, 0, sizeof(priv));
    memset(&cred, 0, sizeof(cred));
    g_assert(!da_cred_parse(&cred, &priv, data, strlen(data)));
}

/*==========================================================================*
 * Bad UID number
 *==========================================================================*/

static
void
test_cred_baduid2(
    void)
{
    static const char data[] = "\
Uid:	100000	10000000000	100000	100000\n\
Gid:	100000	998	998	998\n";
    DACred cred;
    DACredPriv priv;
    memset(&priv, 0, sizeof(priv));
    memset(&cred, 0, sizeof(cred));
    g_assert(!da_cred_parse(&cred, &priv, data, strlen(data)));
}

/*==========================================================================*
 * Bad GID number
 *==========================================================================*/

static
void
test_cred_badgid(
    void)
{
    static const char data[] = "\
Uid:	100000	100000	100000	100000\n\
Gid:	100000	xxx	998	998\n";
    DACred cred;
    DACredPriv priv;
    memset(&priv, 0, sizeof(priv));
    memset(&cred, 0, sizeof(cred));
    g_assert(!da_cred_parse(&cred, &priv, data, strlen(data)));
}

/*==========================================================================*
 * Bad group 1
 *==========================================================================*/

static
void
test_cred_badgroup1(
    void)
{
    static const gid_t groups[] = {
        39, 100, 100000
    };
    static const DACred expected = {
        0, 0,
        groups, G_N_ELEMENTS(groups),
        G_GUINT64_CONSTANT(0xfffffff008003420),
        DBUSACCESS_CRED_CAPS | DBUSACCESS_CRED_GROUPS
    };
    test_cred_parse_and_compare(&expected, "\
Uid:	0	0	0	0\n\
Gid:	0	0	0	0\n\
Groups:	39 100 garbage 100000 \n\
CapEff:	fffffff008003420\n");
}

/*==========================================================================*
 * Bad group 2
 *==========================================================================*/

static
void
test_cred_badgroup2(
    void)
{
    static const DACred expected = {
        0, 0,
        NULL, 0,
        G_GUINT64_CONSTANT(0xfffffff008003420),
        DBUSACCESS_CRED_CAPS | DBUSACCESS_CRED_GROUPS
    };
    test_cred_parse_and_compare(&expected, "\
Uid:	0	0	0	0\n\
Gid:	0	0	0	0\n\
Groups:	garbage \n\
CapEff:	fffffff008003420\n");
}

/*==========================================================================*
 * Common
 *==========================================================================*/

#define TEST_PREFIX "/cred/"

int main(int argc, char* argv[])
{
    g_test_init(&argc, &argv, NULL);
    g_test_add_func(TEST_PREFIX "empty", test_cred_empty);
    g_test_add_func(TEST_PREFIX "basic1", test_cred_basic1);
    g_test_add_func(TEST_PREFIX "basic2", test_cred_basic2);
    g_test_add_func(TEST_PREFIX "garbage1", test_cred_garbage1);
    g_test_add_func(TEST_PREFIX "garbage2", test_cred_garbage2);
    g_test_add_func(TEST_PREFIX "garbage3", test_cred_garbage3);
    g_test_add_func(TEST_PREFIX "garbage4", test_cred_garbage4);
    g_test_add_func(TEST_PREFIX "upcase", test_cred_upcase);
    g_test_add_func(TEST_PREFIX "lowcase", test_cred_lowcase);
    g_test_add_func(TEST_PREFIX "nogroups", test_cred_nogroups);
    g_test_add_func(TEST_PREFIX "baduid1", test_cred_baduid1);
    g_test_add_func(TEST_PREFIX "baduid2", test_cred_baduid2);
    g_test_add_func(TEST_PREFIX "badgid", test_cred_badgid);
    g_test_add_func(TEST_PREFIX "badgroup1", test_cred_badgroup1);
    g_test_add_func(TEST_PREFIX "badgroup2", test_cred_badgroup2);
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
