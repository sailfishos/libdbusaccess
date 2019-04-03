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

#include "dbusaccess_policy.h"
#include "dbusaccess_parser.h"
#include "dbusaccess_log.h"

#include <gutil_macros.h>

typedef struct da_policy_entry DAPolicyEntry;
typedef struct da_policy_expr DAPolicyExpr;

typedef struct da_policy_check {
    const DACred* cred;
    guint action;
    const char* arg;
} DAPolicyCheck;

typedef struct da_policy_expr_type {
    gboolean (*match)(const DAPolicyExpr* x, const DAPolicyCheck* pc);
    gboolean (*equal)(const DAPolicyExpr* x1, const DAPolicyExpr* x2);
    void (*free)(DAPolicyExpr* expr);
} DAPolicyExprType;

struct da_policy_expr {
    const DAPolicyExprType* type;
};

typedef struct da_policy_expr_unary {
    DAPolicyExpr expr;
    DAPolicyExpr* operand;
} DAPolicyExprUnary;

typedef struct da_policy_expr_binary {
    DAPolicyExpr expr;
    DAPolicyExpr* left;
    DAPolicyExpr* right;
} DAPolicyExprBinary;

typedef struct da_policy_expr_custom {
    DAPolicyExpr expr;
    guint action;
    GPatternSpec* pattern;
} DAPolicyExprCustom;

typedef struct da_policy_expr_identity {
    DAPolicyExpr expr;
    int uid;
    int gid;
} DAPolicyExprIdentity;

struct da_policy_entry {
    DAPolicyEntry* next;
    DA_ACCESS access;
    DAPolicyExpr* expr; /* NULL if wildcard */
};

struct da_policy {
    gint ref_count;
    DAPolicyEntry* entries;
};

/* Expressions */

static inline
gboolean
da_policy_expr_match(
    const DAPolicyExpr* expr,
    const DAPolicyCheck* pc)
{
    /*
     * NULL expresson matches everything! da_policy_check relies on that
     * to handle wildcard entries. Lower-level expressions should never
     * be NULL, the parser's grammar ensures that.
     */
    return !expr || expr->type->match(expr, pc);
}

static
gboolean
da_policy_expr_equal(
    const DAPolicyExpr* x1,
    const DAPolicyExpr* x2)
{
    if (x1 == x2) {
        return TRUE;
    } else if (!x1 || !x2) {
        return FALSE;
    } else {
        return x1->type == x2->type && x1->type->equal(x1, x2);
    }
}

static inline
void
da_policy_expr_free(
    DAPolicyExpr* expr)
{
    if (expr) {
        expr->type->free(expr);
    }
}

/* Binary operation */

static inline
DAPolicyExprBinary*
da_policy_expr_binary_cast(
    const DAPolicyExpr* expr)
{
    return G_CAST(expr, DAPolicyExprBinary, expr);
}

static
gboolean
da_policy_expr_binary_and_match(
    const DAPolicyExpr* expr,
    const DAPolicyCheck* pc)
{
    DAPolicyExprBinary* x = da_policy_expr_binary_cast(expr);
    return da_policy_expr_match(x->left, pc) &&
        da_policy_expr_match(x->right, pc);
}

static
gboolean
da_policy_expr_binary_or_match(
    const DAPolicyExpr* expr,
    const DAPolicyCheck* pc)
{
    DAPolicyExprBinary* x = da_policy_expr_binary_cast(expr);
    return da_policy_expr_match(x->left, pc) ||
        da_policy_expr_match(x->right, pc);
}

static
gboolean
da_policy_expr_binary_equal(
    const DAPolicyExpr* expr1,
    const DAPolicyExpr* expr2)
{
    /* Types have been compared by the caller */
    const DAPolicyExprBinary* x1 = da_policy_expr_binary_cast(expr1);
    const DAPolicyExprBinary* x2 = da_policy_expr_binary_cast(expr2);
    /* Our binary operations are commutative */
    return (da_policy_expr_equal(x1->left, x2->left) &&
            da_policy_expr_equal(x1->right, x2->right)) ||
           (da_policy_expr_equal(x1->left, x2->right) &&
            da_policy_expr_equal(x1->right, x2->left));
}

static
void
da_policy_expr_binary_free(
    DAPolicyExpr* expr)
{
    DAPolicyExprBinary* x = da_policy_expr_binary_cast(expr);
    da_policy_expr_free(x->left);
    da_policy_expr_free(x->right);
    g_slice_free(DAPolicyExprBinary, x);
}

static
DAPolicyExpr*
da_policy_expr_binary_new(
    const DAPolicyExprType* type,
    DAPolicyExpr* left,
    DAPolicyExpr* right)
{
    DAPolicyExprBinary* x = g_slice_new0(DAPolicyExprBinary);
    x->expr.type = type;
    x->left = left;
    x->right = right;
    return &x->expr;
}

static
DAPolicyExpr*
da_policy_expr_binary_and_new(
    DAPolicyExpr* left,
    DAPolicyExpr* right)
{
    static const DAPolicyExprType expr_type_and = {
        da_policy_expr_binary_and_match,
        da_policy_expr_binary_equal,
        da_policy_expr_binary_free
    };
    return da_policy_expr_binary_new(&expr_type_and, left, right);
}

static
DAPolicyExpr*
da_policy_expr_binary_or_new(
    DAPolicyExpr* left,
    DAPolicyExpr* right)
{
    static const DAPolicyExprType expr_type_or = {
        da_policy_expr_binary_or_match,
        da_policy_expr_binary_equal,
        da_policy_expr_binary_free
    };
    return da_policy_expr_binary_new(&expr_type_or, left, right);
}

/* Unary operation */

static inline
DAPolicyExprUnary*
da_policy_expr_unary_cast(
    const DAPolicyExpr* expr)
{
    return G_CAST(expr, DAPolicyExprUnary, expr);
}

static
gboolean
da_policy_expr_unary_not_match(
    const DAPolicyExpr* expr,
    const DAPolicyCheck* pc)
{
    DAPolicyExprUnary* x = da_policy_expr_unary_cast(expr);
    return !da_policy_expr_match(x->operand, pc);
}

static
gboolean
da_policy_expr_unary_equal(
    const DAPolicyExpr* expr1,
    const DAPolicyExpr* expr2)
{
    /* Types have been compared by the caller */
    DAPolicyExprUnary* x1 = da_policy_expr_unary_cast(expr1);
    DAPolicyExprUnary* x2 = da_policy_expr_unary_cast(expr2);
    return da_policy_expr_equal(x1->operand, x2->operand);
}

static
void
da_policy_expr_unary_free(
    DAPolicyExpr* expr)
{
    DAPolicyExprUnary* x = da_policy_expr_unary_cast(expr);
    da_policy_expr_free(x->operand);
    g_slice_free(DAPolicyExprUnary, x);
}

static
DAPolicyExpr*
da_policy_expr_unary_not_new(
    DAPolicyExpr* operand)
{
    static const DAPolicyExprType expr_type_not = {
        da_policy_expr_unary_not_match,
        da_policy_expr_unary_equal,
        da_policy_expr_unary_free
    };
    DAPolicyExprUnary* x = g_slice_new0(DAPolicyExprUnary);
    x->expr.type = &expr_type_not;
    x->operand = operand;
    return &x->expr;
}

/* Identity match */

static inline
DAPolicyExprIdentity*
da_policy_expr_identity_cast(
    const DAPolicyExpr* expr)
{
    return G_CAST(expr, DAPolicyExprIdentity, expr);
}

static
gboolean
da_policy_expr_identity_match_user(
    int uid,
    const DACred* cred)
{
    if (uid == DA_WILDCARD) {
        /* Wild card matches everything */
        return TRUE;
    } else if (uid == DA_INVALID || !cred) {
        return FALSE;
    } else {
        return (uid == cred->euid);
    }
}

static
gboolean
da_policy_expr_identity_match_group(
    int gid,
    const DACred* cred)
{
    if (gid == DA_WILDCARD) {
        /* Wild card matches everything */
        return TRUE;
    } else if (gid == DA_INVALID || !cred) {
        return FALSE;
    } else if (gid == cred->egid) {
        return TRUE;
    } else {
        guint i;
        for (i=0; i<cred->ngroups; i++) {
            if (cred->groups[i] == gid) {
                return TRUE;
            }
        }
        return FALSE;
    }
}

static
gboolean
da_policy_expr_identity_match(
    const DAPolicyExpr* expr,
    const DAPolicyCheck* pc)
{
    DAPolicyExprIdentity* x = da_policy_expr_identity_cast(expr);
    return da_policy_expr_identity_match_user(x->uid, pc->cred) &&
        da_policy_expr_identity_match_group(x->gid, pc->cred);
}

static
gboolean
da_policy_expr_identity_equal(
    const DAPolicyExpr* expr1,
    const DAPolicyExpr* expr2)
{
    /* Types have been compared by the caller */
    DAPolicyExprIdentity* x1 = da_policy_expr_identity_cast(expr1);
    DAPolicyExprIdentity* x2 = da_policy_expr_identity_cast(expr2);
    return x1->uid == x2->uid && x1->gid == x2->gid;
}

static
void
da_policy_expr_identity_free(
    DAPolicyExpr* expr)
{
    DAPolicyExprIdentity* x = da_policy_expr_identity_cast(expr);
    g_slice_free(DAPolicyExprIdentity, x);
}

static
DAPolicyExpr*
da_policy_expr_identity_new(
    int uid,
    int gid)
{
    static const DAPolicyExprType expr_type_identity = {
        da_policy_expr_identity_match,
        da_policy_expr_identity_equal,
        da_policy_expr_identity_free
    };
    DAPolicyExprIdentity* x = g_slice_new0(DAPolicyExprIdentity);
    x->expr.type = &expr_type_identity;
    x->uid = uid;
    x->gid = gid;
    return &x->expr;
}

/* Custom match */

static inline
DAPolicyExprCustom*
da_policy_expr_custom_cast(
    const DAPolicyExpr* expr)
{
    return G_CAST(expr, DAPolicyExprCustom, expr);
}

static
gboolean
da_policy_expr_custom_match(
    const DAPolicyExpr* expr,
    const DAPolicyCheck* pc)
{
    DAPolicyExprCustom* x = da_policy_expr_custom_cast(expr);
    if (pc->action == x->action) {
        if (pc->arg) {
            if (x->pattern) {
                guint len = strlen(pc->arg);
                return g_pattern_match(x->pattern, len, pc->arg, NULL);
            } else {
                /* This is a wildcard or we are not expecting any arguments */
                return TRUE;
            }
        } else {
            /* No arguments - ok if there's no pattern */
            return !x->pattern;
        }
    } else {
        /* Not our call */
        return FALSE;
    }
}

static
gboolean
da_policy_expr_custom_equal(
    const DAPolicyExpr* expr1,
    const DAPolicyExpr* expr2)
{
    /* Types have been compared by the caller */
    DAPolicyExprCustom* x1 = da_policy_expr_custom_cast(expr1);
    DAPolicyExprCustom* x2 = da_policy_expr_custom_cast(expr2);
    return x1->action == x2->action &&
        ((!x1->pattern && !x2->pattern) || (x1->pattern && x2->pattern &&
         g_pattern_spec_equal(x1->pattern, x2->pattern)));
}

static
void
da_policy_expr_custom_free(
    DAPolicyExpr* expr)
{
    DAPolicyExprCustom* x = da_policy_expr_custom_cast(expr);
    if (x->pattern) {
        g_pattern_spec_free(x->pattern);
    }
    g_slice_free(DAPolicyExprCustom, x);
}

static
DAPolicyExpr*
da_policy_expr_custom_new(
    guint action,
    const char* pattern)
{
    static const DAPolicyExprType expr_type_custom = {
        da_policy_expr_custom_match,
        da_policy_expr_custom_equal,
        da_policy_expr_custom_free
    };
    DAPolicyExprCustom* x = g_slice_new0(DAPolicyExprCustom);
    x->expr.type = &expr_type_custom;
    x->action = action;
    if (pattern && strcmp(pattern, "*")) {
        x->pattern = g_pattern_spec_new(pattern);
    }
    return &x->expr;
}

/* Policy */

static
DAPolicyExpr*
da_policy_expr_new(
    const DAParserExpr* expr)
{
    if (expr) {
        switch (expr->type) {
        case DA_PARSER_EXPR_IDENTITY:
            return da_policy_expr_identity_new(
                expr->data.identity.uid,
                expr->data.identity.gid);
        case DA_PARSER_EXPR_CUSTOM:
            return da_policy_expr_custom_new(
                expr->data.custom.action,
                expr->data.custom.param);
        case DA_PARSER_EXPR_NOT:
            return da_policy_expr_unary_not_new(
                da_policy_expr_new(expr->data.expr[0]));
        case DA_PARSER_EXPR_AND:
            return da_policy_expr_binary_and_new(
                da_policy_expr_new(expr->data.expr[0]),
                da_policy_expr_new(expr->data.expr[1]));
        case DA_PARSER_EXPR_OR:
            return da_policy_expr_binary_or_new(
                da_policy_expr_new(expr->data.expr[0]),
                da_policy_expr_new(expr->data.expr[1]));
        }
    }
    return NULL;
}

static
void
da_policy_add_entry(
    DAPolicy* policy,
    const DAParserEntry* parser_entry)
{
    DAPolicyEntry* entry = g_slice_new0(DAPolicyEntry);
    DAPolicyEntry* last = policy->entries;
    entry->access = parser_entry->access;
    entry->expr =  da_policy_expr_new(parser_entry->expr);
    if (last) {
        while (last->next) {
            last = last->next;
        }
        last->next = entry;
    } else {
        policy->entries = entry;
    }
}

DAPolicy*
da_policy_new_full(
    const char* spec,
    const DA_ACTION* actions)
{
    DAParser* parser = da_parser_compile(spec, actions);
    if (parser) {
        DAPolicy* policy = g_slice_new0(DAPolicy);
        GSList* entry = da_parser_get_result(parser);
        while (entry) {
            da_policy_add_entry(policy, entry->data);
            entry = entry->next;
        }
        policy->ref_count = 1;
        da_parser_delete(parser);
        return policy;
    }
    return NULL;
}

DAPolicy*
da_policy_new(
    const char* spec)
{
    return da_policy_new_full(spec, NULL);
}

static
void
da_policy_finalize(
    DAPolicy* policy)
{
    if (policy->entries) {
        DAPolicyEntry* entry = policy->entries;
        while (entry) {
            da_policy_expr_free(entry->expr);
            entry = entry->next;
        }
        g_slice_free_chain(DAPolicyEntry, policy->entries, next);
    }
}

DAPolicy*
da_policy_ref(
    DAPolicy* policy)
{
    if (policy) {
        g_atomic_int_inc(&policy->ref_count);
    }
    return policy;
}

void
da_policy_unref(
    DAPolicy* policy)
{
    if (policy) {
        if (g_atomic_int_dec_and_test(&policy->ref_count)) {
            da_policy_finalize(policy);
            g_slice_free(DAPolicy, policy);
        }
    }
}

gboolean
da_policy_equal(
    const DAPolicy* p1,
    const DAPolicy* p2)
{
    if (p1 == p2) {
        return TRUE;
    } else if (!p1 || !p2) {
        return FALSE;
    } else {
        DAPolicyEntry* e1 = p1->entries;
        DAPolicyEntry* e2 = p2->entries;
        while (e1 && e2) {
            if (!da_policy_expr_equal(e1->expr, e2->expr) ||
                e1->access != e2->access) {
                return FALSE;
            }
            e1 = e1->next;
            e2 = e2->next;
        }
        return !e1 && !e2;
    }
}

DA_ACCESS
da_policy_check(
    const DAPolicy* policy,
    const DACred* cred,
    guint action,
    const char* arg,
    DA_ACCESS def)
{
    DA_ACCESS result = def;
    if (cred && !cred->euid) {
        /* No checks for root user */
        result = DA_ACCESS_ALLOW;
    } else if (policy) {
        DAPolicyEntry* entry = policy->entries;
        DAPolicyCheck check;
        check.cred = cred;
        check.action = action;
        check.arg = arg;
        while (entry) {
            if (da_policy_expr_match(entry->expr, &check)) {
                result = entry->access;
            }
            entry = entry->next;
        }
    }
    return result;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */

