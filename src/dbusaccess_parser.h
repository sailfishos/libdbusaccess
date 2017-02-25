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

#ifndef DBUSACCESS_PARSER_H
#define DBUSACCESS_PARSER_H

#include "dbusaccess_policy.h"

typedef struct da_parser DAParser;
typedef struct da_parser_expr DAParserExpr;

typedef enum {
    DA_PARSER_EXPR_IDENTITY,
    DA_PARSER_EXPR_CUSTOM,
    DA_PARSER_EXPR_NOT,
    DA_PARSER_EXPR_AND,
    DA_PARSER_EXPR_OR
} DA_PARSER_EXPR;

struct da_parser_expr {
    DA_PARSER_EXPR type;
    union {
        struct {
            int uid;
            int gid;
        } identity;
        struct {
            guint action;
            const char* param;
        } custom;
        DAParserExpr* expr[2];
    } data;
};

typedef struct da_parser_entry {
    DAParserExpr* expr; /* NULL if wildcard */
    DA_ACCESS access;
} DAParserEntry;

DAParser*
da_parser_compile(
    const char* spec,
    const DA_ACTION* actions);

GSList*
da_parser_get_result(
    DAParser* parser);

void
da_parser_delete(
    DAParser* parser);

#endif /* DBUSACCESS_PARSER_H */

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
