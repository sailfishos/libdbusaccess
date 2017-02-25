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

#include "dbusaccess_parser_p.h"
#include "dbusaccess_log.h"

struct da_parser {
    const DA_ACTION* actions;
    GString* buf;
    GSList* alloc_list;
    GSList* link_list;
    GSList* entries;
};

void
da_parser_error(
    DAParser* parser,
    DAScanner* scanner,
    const char* error)
{
    GWARN("%s", error);
}

void
da_parser_start_string(
    DAParser* parser)
{
    g_string_set_size(parser->buf, 0);
}

void
da_parser_append_char(
    DAParser* parser,
    char c)
{
    g_string_append_c(parser->buf, c);
}

const char*
da_parser_finish_string(
    DAParser* parser)
{
    char* str = da_parser_new_string(parser, parser->buf->str);
    g_string_set_size(parser->buf, 0);
    return str;
}

static
const DA_ACTION*
da_parser_find_action(
    DAParser* parser,
    const char* name)
{
    if (name && parser->actions) {
        const DA_ACTION* action = parser->actions;
        while (action->name) {
            if (!g_strcmp0(name, action->name)) {
                return action;
            }
            action++;
        }
    }
    return NULL;
}

GSList*
da_parser_new_link(
    DAParser* parser,
    void* data)
{
    GSList* link = g_slist_append(NULL, NULL);
    parser->link_list = g_slist_prepend(parser->link_list, link);
    link->data = data;
    return link;
}

char*
da_parser_new_string(
    DAParser* parser,
    const char* str)
{
    char* copy = g_strdup(str);
    parser->alloc_list = g_slist_prepend(parser->alloc_list, copy);
    return copy;
}

DAParserExpr*
da_parser_new_expr_identity(
    DAParser* parser,
    int uid,
    int gid)
{
    DAParserExpr* expr = g_new(DAParserExpr,1);
    parser->alloc_list = g_slist_prepend(parser->alloc_list, expr);
    expr->type = DA_PARSER_EXPR_IDENTITY;
    expr->data.identity.uid = uid;
    expr->data.identity.gid = gid;
    return expr;
}

DAParserExpr*
da_parser_new_expr_custom(
    DAParser* parser,
    const char* name,
    const char* param)
{
    const DA_ACTION* action = da_parser_find_action(parser, name);
    if (!action) {
        GDEBUG("Unknown action \"%s\"", name);
    } else if (param && !action->args) {
        GDEBUG("Unexpected parameter \"%s\" for \"%s\"", param, name);
    } else if (!param && action->args) {
        GDEBUG("Missing parameter for \"%s\"", name);
    } else {
        DAParserExpr* expr = g_new(DAParserExpr,1);
        parser->alloc_list = g_slist_prepend(parser->alloc_list, expr);
        expr->type = DA_PARSER_EXPR_CUSTOM;
        expr->data.custom.action = action->id;
        expr->data.custom.param = param;
        return expr;
    }
    return NULL;
}

DAParserExpr*
da_parser_new_expr(
    DAParser* parser,
    DA_PARSER_EXPR type,
    DAParserExpr* left,
    DAParserExpr* right)
{
    DAParserExpr* expr = g_new0(DAParserExpr, 1);
    parser->alloc_list = g_slist_prepend(parser->alloc_list, expr);
    GASSERT(type != DA_PARSER_EXPR_IDENTITY);
    GASSERT(type != DA_PARSER_EXPR_CUSTOM);
    expr->type = type;
    expr->data.expr[0] = left;
    expr->data.expr[1] = right;
    return expr;
}

DAParserEntry*
da_parser_new_entry(
    DAParser* parser,
    DAParserExpr* expr,
    DA_ACCESS access)
{
    DAParserEntry* entry = g_new(DAParserEntry,1);
    parser->alloc_list = g_slist_prepend(parser->alloc_list, entry);
    entry->expr = expr;
    entry->access = access;
    return entry;
}

void
da_parser_add_entries(
    DAParser* parser,
    GSList* entries)
{
    parser->entries = g_slist_concat(parser->entries, entries);
}

static
DAParser*
da_parser_create(
    const DA_ACTION* actions)
{
    DAParser* parser = g_slice_new0(DAParser);
    parser->buf = g_string_new(NULL);
    parser->actions = actions;
    return parser;
}

static
void
da_parser_free_link(
    gpointer data)
{
    g_slist_free_1(data);
}

void
da_parser_delete(
    DAParser* parser)
{
    g_slist_free_full(parser->link_list, da_parser_free_link);
    g_slist_free_full(parser->alloc_list, g_free);
    g_string_free(parser->buf, TRUE);
    g_slice_free(DAParser, parser);
}

DAParser*
da_parser_compile(
    const char* spec,
    const DA_ACTION* actions)
{
    if (spec) {
        DAParser* parser = da_parser_create(actions);
        DAScanner* scanner = da_scanner_create();
        int result = -1;
        if (scanner) {
            DAScannerBuffer* buf = da_scanner_buffer_create(spec, scanner);
            if (buf) {
#ifdef DEBUG
                da_parser_debug = gutil_log_default.level >= GLOG_LEVEL_DEBUG;
#endif
                GDEBUG("Parsing \"%s\"", spec);
                result = da_parser_parse(parser, scanner);
                da_scanner_buffer_delete(buf, scanner);
            }
            da_scanner_delete(scanner);
        }
        if (result == 0) {
            return parser;
        }
        da_parser_delete(parser);
    }
    return NULL;
}

GSList*
da_parser_get_result(
    DAParser* parser)
{
    return parser->entries;
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
