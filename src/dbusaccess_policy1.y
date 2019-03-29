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

%{
#include "dbusaccess_parser_p.h"
#include "dbusaccess_system.h"
#include "dbusaccess_log.h"
#define FORMAT_VERSION 1
%}

%union 
{
    int number;
    const char* string;
    DA_ACCESS access;
    DAParserEntry* entry;
    DAParserExpr* expr;
    GSList* list;
}

/* BISON Declarations */
%pure-parser
%name-prefix "da_parser_"
%lex-param { void* yyscanner }
%lex-param { DAParser* parser }
%parse-param { DAParser* parser }
%parse-param { DAScanner* yyscanner }

%token <number> NUMBER
%token <string> ID
%token <string> WORD
%token <string> WILDCARD
%token <string> STRING   /* Quoted string, may include spaces */
%token USER
%token GROUP
%token ALLOW
%token DENY
%token ERROR

%type <number> version
%type <number> user
%type <number> group
%type <access> access
%type <list> entries
%type <entry> entry
%type <expr> expr
%type <expr> term
%type <string> param

%left '!'

/* Grammar follows */
%%
policy:
    version
    | version ';' entries
    {
        da_parser_add_entries(parser, $3);
    }
    | version ';' entries ';'
    {
        da_parser_add_entries(parser, $3);
    }

version:
    NUMBER
    {
        if ($1 != FORMAT_VERSION) {
            GDEBUG("Unsupported version %d", $1);
            YYERROR;
        }
    }

entries:
    entry
    {
        $$ = da_parser_new_link(parser, $1);
    }
    | entries ';' entry
    {
        $$ = g_slist_concat($1, da_parser_new_link(parser, $3));
    }

entry:
    '*' '=' access
    {
        $$ = da_parser_new_entry(parser, NULL, $3);
    }
    | expr '=' access
    {
        $$ = da_parser_new_entry(parser, $1, $3);
    }
    | expr
    {
        $$ = da_parser_new_entry(parser, $1, DA_ACCESS_ALLOW);
    }

access:
    ALLOW
    {
        $$ = DA_ACCESS_ALLOW;
    }
    | DENY
    {
        $$ = DA_ACCESS_DENY;
    }

user:
    NUMBER
    {
        $$ = ($1 < 0) ? DA_WILDCARD : $1;
    }
    | WILDCARD
    {
        $$ = DA_WILDCARD;
    }
    | WORD
    {
        $$ = da_system_uid($1);
        if ($$ < 0) {
            GWARN("Unknown user \"%s\"", $1);
            $$ = DA_INVALID;
        }
    }

group:
    NUMBER
    {
        $$ = ($1 < 0) ? DA_WILDCARD : $1;
    }
    | WILDCARD
    {
        $$ = DA_WILDCARD;
    }
    | WORD
    {
        $$ = da_system_gid($1);
        if ($$ < 0) {
            GWARN("Unknown group \"%s\"", $1);
            $$ = DA_INVALID;
        }
    }

expr:
    term
    | '!' expr
    {
        $$ = da_parser_new_expr(parser, DA_PARSER_EXPR_NOT, $2, NULL);
    }
    | '(' expr ')'
    {
        $$ = $2;
    }
    | expr '|' expr
    {
        $$ = da_parser_new_expr(parser, DA_PARSER_EXPR_OR, $1, $3);
    }
    | expr '&' expr
    {
        $$ = da_parser_new_expr(parser, DA_PARSER_EXPR_AND, $1, $3);
    }

term:
    USER '(' user ')'
    {
        $$ = da_parser_new_expr_identity(parser, $3, DA_WILDCARD);
    }
    | USER '(' user ':' group ')'
    {
        $$ = da_parser_new_expr_identity(parser, $3, $5);
    }
    | GROUP '(' group ')'
    {
        $$ = da_parser_new_expr_identity(parser, DA_WILDCARD, $3);
    }
    | ID '(' ')'
    {
        $$ = da_parser_new_expr_custom(parser, $1, NULL);
        if (!$$) {
            YYERROR;
        }
    }
    | ID '(' param ')'
    {
        $$ = da_parser_new_expr_custom(parser, $1, $3);
        if (!$$) {
            YYERROR;
        }
    }

param:
    WORD
    | STRING
    | WILDCARD

;
%%

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
