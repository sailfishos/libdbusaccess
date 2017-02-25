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
%{
#include "dbusaccess_parser_p.h"
#include "dbusaccess_policy1.tab.h"
#include "dbusaccess_log.h"

#define YY_NO_INPUT 1

static
int
number(
    DAParser* parser,
    YYSTYPE* value,
    const char* str)
{
    char* endptr = NULL;
    unsigned long n = strtoul(str, &endptr, 0);
    if (!endptr[0]) {
        value->number = n;
        return NUMBER;
    } else {
        GDEBUG("Not a number: \"%s\"", str);
        return ERROR;
    }
}

%}
%option prefix="da_parser_"
%option never-interactive
%option nodefault
%option nounput
%option noyywrap
%option reentrant
%option bison-bridge 
%option warn

%x BEFORE_ARGS
%x ARGS
%x SQUOTE
%x QUOTE
%%
<QUOTE>\" {
     BEGIN(ARGS);
     yylval->string = da_parser_finish_string(parser);
     return STRING;
 }
<SQUOTE>"'" {
    BEGIN(ARGS);
    yylval->string = da_parser_finish_string(parser);
    return STRING;
 }
<QUOTE>\\\" {
    da_parser_append_char(parser, '"');
 }
<SQUOTE>"\\'" {
    da_parser_append_char(parser, '\'');
 }
<QUOTE,SQUOTE>\n {
    da_parser_append_char(parser, yytext[0]);
 }
<QUOTE,SQUOTE>. {
    da_parser_append_char(parser, yytext[0]);
 }
<BEFORE_ARGS>"(" {
    BEGIN(ARGS);
    return yytext[0];
 }
<ARGS>"'" {
    BEGIN(SQUOTE);
    da_parser_start_string(parser);
}
<ARGS>\" {
    BEGIN(QUOTE);
    da_parser_start_string(parser);
 }
<ARGS>")" {
    BEGIN(INITIAL);
    return yytext[0];
 }
<INITIAL>user {
    BEGIN(BEFORE_ARGS);
    return USER;
 }
<INITIAL>group {
    BEGIN(BEFORE_ARGS);
    return GROUP;
 }
<INITIAL>allow {
    return ALLOW;
 }
<INITIAL>deny {
    return DENY;
 }
<INITIAL,ARGS>[[:digit:]]+ {
    return number(parser, yylval, yytext);
 }
<ARGS>[[:alnum:]_\-?*]+ {
    yylval->string = da_parser_new_string(parser, yytext);
    return strcmp(yytext, "*") ? WORD : WILDCARD;
 }
<INITIAL>[[:alpha:]][[:alnum:]_\-]* {
    BEGIN(BEFORE_ARGS);
    yylval->string = da_parser_new_string(parser, yytext);
    return ID;
 }
<INITIAL,BEFORE_ARGS,ARGS>[[:space:]]+   /* Eat whitespaces */;
<INITIAL,BEFORE_ARGS,ARGS>. return yytext[0];
%%

DAScanner*
da_scanner_create()
{
    yyscan_t scanner;
    yylex_init(&scanner);
    return scanner;
}

void
da_scanner_delete(
    DAScanner* scanner)
{
    yylex_destroy(scanner);
}

DAScannerBuffer*
da_scanner_buffer_create(
    const char* str,
    DAScanner* scanner)
{
    return da_parser__scan_string(str, scanner);
}

void
da_scanner_buffer_delete(
    DAScannerBuffer* buffer,
    DAScanner* scanner)
{
    da_parser__delete_buffer(buffer, scanner);
}

/*
 * Local Variables:
 * mode: C
 * c-basic-offset: 4
 * indent-tabs-mode: nil
 * End:
 */
