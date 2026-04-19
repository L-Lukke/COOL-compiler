/*
 *  The scanner definition for COOL.
 */
%option noyywrap

%{
#include <cool-parse.h>
#include <stringtab.h>
#include <utilities.h>
#include <stdlib.h>

#define yylval cool_yylval
#define yylex  cool_yylex

#define MAX_STR_CONST 1025
#define YY_NO_UNPUT

extern FILE *fin;

#undef YY_INPUT
#define YY_INPUT(buf,result,max_size) \
  if ( (result = fread((char*)buf, sizeof(char), max_size, fin)) < 0) \
    YY_FATAL_ERROR("read() in flex scanner failed");

char string_buf[MAX_STR_CONST];
int string_len = 0;

extern int curr_lineno;
extern int verbose_flag;
extern YYSTYPE cool_yylval;

static int comment_level = 0;

static int append_string_char(char c) {
  if (string_len >= MAX_STR_CONST - 1) {
    cool_yylval.error_msg = (char *)"String constant too long";
    return 0;
  }
  string_buf[string_len++] = c;
  return 1;
}
%}

%x NESTED_COMMENT
%x LINE_COMMENT
%x STRING
%x STRING_ERROR

DIGIT           [0-9]
LOWER           [a-z]
UPPER           [A-Z]
LETTER          ({LOWER}|{UPPER}|_)
WHITESPACE      [ \f\r\t\v]

CLASS           [cC][lL][aA][sS][sS]
ELSE            [eE][lL][sS][eE]
FI              [fF][iI]
IF              [iI][fF]
IN              [iI][nN]
INHERITS        [iI][nN][hH][eE][rR][iI][tT][sS]
ISVOID          [iI][sS][vV][oO][iI][dD]
LET             [lL][eE][tT]
LOOP            [lL][oO][oO][pP]
POOL            [pP][oO][oO][lL]
THEN            [tT][hH][eE][nN]
WHILE           [wW][hH][iI][lL][eE]
CASE            [cC][aA][sS][eE]
ESAC            [eE][sS][aA][cC]
OF              [oO][fF]
NEW             [nN][eE][wW]
NOT             [nN][oO][tT]

TRUE_TKN        t[rR][uU][eE]
FALSE_TKN       f[aA][lL][sS][eE]

TYPEID          {UPPER}({LETTER}|{DIGIT})*
OBJECTID        {LOWER}({LETTER}|{DIGIT})*
INT_TKN         {DIGIT}+

DARROW          =>
LE              <=
ASSIGN          <-

N_COMMENT_START \(\*
N_COMMENT_END   \*\)
L_COMMENT       --

SINGLES         [~=(){}.@;:,]
OPERATORS       [-+*/<]

%%
{CLASS}         { return CLASS; }
{ELSE}          { return ELSE; }
{FI}            { return FI; }
{IF}            { return IF; }
{IN}            { return IN; }
{INHERITS}      { return INHERITS; }
{ISVOID}        { return ISVOID; }
{LET}           { return LET; }
{LOOP}          { return LOOP; }
{POOL}          { return POOL; }
{THEN}          { return THEN; }
{WHILE}         { return WHILE; }
{CASE}          { return CASE; }
{ESAC}          { return ESAC; }
{OF}            { return OF; }
{NEW}           { return NEW; }
{NOT}           { return NOT; }

{DARROW}        { return DARROW; }
{LE}            { return LE; }
{ASSIGN}        { return ASSIGN; }

{N_COMMENT_START} {
  comment_level = 1;
  BEGIN(NESTED_COMMENT);
}

<NESTED_COMMENT>{N_COMMENT_START} { ++comment_level; }

<NESTED_COMMENT>{N_COMMENT_END} {
  if (--comment_level == 0)
    BEGIN(INITIAL);
}

<NESTED_COMMENT>\n { ++curr_lineno; }

<NESTED_COMMENT><<EOF>> {
  cool_yylval.error_msg = (char *)"EOF in comment";
  BEGIN(INITIAL);
  return ERROR;
}

<NESTED_COMMENT>. { }

{N_COMMENT_END} {
  cool_yylval.error_msg = (char *)"Unmatched *)";
  return ERROR;
}

{L_COMMENT} { BEGIN(LINE_COMMENT); }

<LINE_COMMENT>\n {
  curr_lineno++;
  BEGIN(INITIAL);
}

<LINE_COMMENT><<EOF>> { yyterminate(); }

<LINE_COMMENT>. { }

\" {
  string_len = 0;
  BEGIN(STRING);
}

<STRING>\" {
  string_buf[string_len] = '\0';
  cool_yylval.symbol = stringtable.add_string(string_buf);
  BEGIN(INITIAL);
  return STR_CONST;
}

<STRING><<EOF>> {
  cool_yylval.error_msg = (char *)"EOF in string constant";
  BEGIN(INITIAL);
  return ERROR;
}

<STRING>\n {
  curr_lineno++;
  cool_yylval.error_msg = (char *)"Unterminated string constant";
  BEGIN(INITIAL);
  return ERROR;
}

<STRING>\\\0 {
  cool_yylval.error_msg = (char *)"String contains escaped null character";
  BEGIN(STRING_ERROR);
  return ERROR;
}

<STRING>\\\n {
  curr_lineno++;
  if (!append_string_char('\n')) {
    BEGIN(STRING_ERROR);
    return ERROR;
  }
}

<STRING>\\n {
  if (!append_string_char('\n')) {
    BEGIN(STRING_ERROR);
    return ERROR;
  }
}

<STRING>\\t {
  if (!append_string_char('\t')) {
    BEGIN(STRING_ERROR);
    return ERROR;
  }
}

<STRING>\\b {
  if (!append_string_char('\b')) {
    BEGIN(STRING_ERROR);
    return ERROR;
  }
}

<STRING>\\f {
  if (!append_string_char('\f')) {
    BEGIN(STRING_ERROR);
    return ERROR;
  }
}

<STRING>\0 {
  cool_yylval.error_msg = (char *)"String contains null character";
  BEGIN(STRING_ERROR);
  return ERROR;
}

<STRING>\\. {
  if (!append_string_char(yytext[1])) {
    BEGIN(STRING_ERROR);
    return ERROR;
  }
}

<STRING>. {
  if (!append_string_char(yytext[0])) {
    BEGIN(STRING_ERROR);
    return ERROR;
  }
}

<STRING_ERROR>\" { BEGIN(INITIAL); }

<STRING_ERROR>\n {
  curr_lineno++;
  BEGIN(INITIAL);
}

<STRING_ERROR><<EOF>> {
  BEGIN(INITIAL);
  yyterminate();
}

<STRING_ERROR>. { }

{SINGLES} { return (int)yytext[0]; }
{OPERATORS} { return (int)yytext[0]; }

{TRUE_TKN} {
  cool_yylval.boolean = true;
  return BOOL_CONST;
}

{FALSE_TKN} {
  cool_yylval.boolean = false;
  return BOOL_CONST;
}

{INT_TKN} {
  cool_yylval.symbol = inttable.add_string(yytext);
  return INT_CONST;
}

{TYPEID} {
  cool_yylval.symbol = idtable.add_string(yytext);
  return TYPEID;
}

{OBJECTID} {
  cool_yylval.symbol = idtable.add_string(yytext);
  return OBJECTID;
}

\n { curr_lineno++; }
{WHITESPACE}+ { }

. {
  cool_yylval.error_msg = yytext;
  return ERROR;
}

%%