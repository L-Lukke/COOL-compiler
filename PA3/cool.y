/*
 * cool.y
 * Parser definition for the COOL language.
 */

%{
#include <iostream>
#include "cool-tree.h"
#include "stringtab.h"
#include "utilities.h"

extern char *curr_filename;
extern int node_lineno;
extern int yylex();

void yyerror(char *s);

/* ------------------------------------------------------------------ */
/* Source locations                                                   */
/* ------------------------------------------------------------------ */

#define YYLTYPE int
#define cool_yylloc curr_lineno

#define YYLLOC_DEFAULT(Current, Rhs, N)        \
  do {                                         \
    if ((N) > 0) {                             \
      (Current) = (Rhs)[1];                    \
    } else {                                   \
      (Current) = 0;                           \
    }                                          \
    node_lineno = (Current);                   \
  } while (0)

#define SET_NODELOC(Current) \
  node_lineno = (Current)

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

static inline Symbol current_filename_symbol()
{
  return stringtable.add_string(curr_filename);
}

static inline Classes make_class_list(Class_ klass)
{
  return (klass == NULL) ? nil_Classes() : single_Classes(klass);
}

static inline Classes append_class_if_valid(Classes classes, Class_ klass)
{
  return (klass == NULL) ? classes : append_Classes(classes, single_Classes(klass));
}

static inline Features append_feature_if_valid(Features features, Feature feature)
{
  return (feature == NULL) ? features : append_Features(features, single_Features(feature));
}

Program ast_root;
Classes parse_results;
int omerrs = 0;
%}

/* ------------------------------------------------------------------ */
/* Semantic values                                                    */
/* ------------------------------------------------------------------ */

%union {
  Boolean boolean;
  Symbol symbol;

  Program program;
  Class_ class_;
  Classes classes;

  Feature feature;
  Features features;

  Formal formal;
  Formals formals;

  Case case_;
  Cases cases;

  Expression expression;
  Expressions expressions;

  char *error_msg;
}

/* ------------------------------------------------------------------ */
/* Tokens                                                             */
/* ------------------------------------------------------------------ */

%token CLASS 258
%token ELSE 259
%token FI 260
%token IF 261
%token IN 262
%token INHERITS 263
%token LET 264
%token LOOP 265
%token POOL 266
%token THEN 267
%token WHILE 268
%token CASE 269
%token ESAC 270
%token OF 271
%token DARROW 272
%token NEW 273
%token ISVOID 274

%token <symbol> STR_CONST 275
%token <symbol> INT_CONST 276
%token <boolean> BOOL_CONST 277
%token <symbol> TYPEID 278
%token <symbol> OBJECTID 279

%token ASSIGN 280
%token NOT 281
%token LE 282
%token ERROR 283

/* ------------------------------------------------------------------ */
/* Start symbol                                                       */
/* ------------------------------------------------------------------ */

%start program

/* ------------------------------------------------------------------ */
/* Nonterminal types                                                  */
/* ------------------------------------------------------------------ */

%type <program>     program

%type <classes>     class_list
%type <class_>      class

%type <features>    features
%type <feature>     feature
%type <feature>     method_declaration
%type <feature>     attribute_declaration

%type <formals>     opt_formals
%type <formals>     formal_list
%type <formal>      formal

%type <cases>       branches
%type <case_>       branch

%type <expression>  expression

%type <expression>  dispatch_expression
%type <expression>  let_expression
%type <expression>  opt_let_init
%type <expression>  assignment_expression
%type <expression>  if_expression
%type <expression>  while_expression
%type <expression>  block_expression
%type <expression>  case_expression
%type <expression>  new_expression
%type <expression>  isvoid_expression
%type <expression>  primary_expression
%type <expression>  let_binding_list

%type <expression>  block_item
%type <expressions> block_items
%type <expressions> opt_actuals
%type <expressions> actual_list

/* ------------------------------------------------------------------ */
/* Precedence                                                         */
/* ------------------------------------------------------------------ */

%right ASSIGN
%precedence NOT
%nonassoc '<' '=' LE
%left '+' '-'
%left '*' '/'
%precedence ISVOID
%precedence '~'
%precedence '@'
%precedence '.'

%%

/* ================================================================== */
/* Program                                                            */
/* ================================================================== */

program
  : class_list
    {
      @$ = @1;
      SET_NODELOC(@1);
      ast_root = program($1);
      parse_results = $1;
      $$ = ast_root;
    }
  ;

/* ================================================================== */
/* Classes                                                            */
/* ================================================================== */

class_list
  : class
    {
      @$ = @1;
      $$ = make_class_list($1);
    }
  | class_list class
    {
      @$ = @2;
      $$ = append_class_if_valid($1, $2);
    }
  ;

class
  : CLASS TYPEID '{' features '}' ';'
    {
      @$ = @6;
      SET_NODELOC(@1);
      $$ = class_($2, idtable.add_string("Object"), $4, current_filename_symbol());
    }
  | CLASS TYPEID INHERITS TYPEID '{' features '}' ';'
    {
      @$ = @8;
      SET_NODELOC(@1);
      $$ = class_($2, $4, $6, current_filename_symbol());
    }
  | CLASS TYPEID '{' error ';'
    {
      yyerrok;
      $$ = NULL;
    }
  | CLASS TYPEID INHERITS TYPEID '{' error ';'
    {
      yyerrok;
      $$ = NULL;
    }
  | error ';'
    {
      yyerrok;
      $$ = NULL;
    }
  ;

/* ================================================================== */
/* Features                                                           */
/* ================================================================== */

features
  : /* empty */
    {
      @$ = 0;
      $$ = nil_Features();
    }
  | features feature
    {
      @$ = @2;
      $$ = append_feature_if_valid($1, $2);
    }
  ;

feature
  : method_declaration ';'
    {
      @$ = @2;
      $$ = $1;
    }
  | attribute_declaration ';'
    {
      @$ = @2;
      $$ = $1;
    }
  | error ';'
    {
      yyerrok;
      $$ = NULL;
    }
  ;

method_declaration
  : OBJECTID '(' opt_formals ')' ':' TYPEID '{' expression '}'
    {
      @$ = @9;
      SET_NODELOC(@1);
      $$ = method($1, $3, $6, $8);
    }
  ;

attribute_declaration
  : OBJECTID ':' TYPEID
    {
      @$ = @3;
      SET_NODELOC(@1);
      $$ = attr($1, $3, no_expr());
    }
  | OBJECTID ':' TYPEID ASSIGN expression
    {
      @$ = @5;
      SET_NODELOC(@1);
      $$ = attr($1, $3, $5);
    }
  ;

/* ================================================================== */
/* Formals                                                            */
/* ================================================================== */

opt_formals
  : /* empty */
    {
      @$ = 0;
      $$ = nil_Formals();
    }
  | formal_list
    {
      @$ = @1;
      $$ = $1;
    }
  ;

formal_list
  : formal
    {
      @$ = @1;
      $$ = single_Formals($1);
    }
  | formal_list ',' formal
    {
      @$ = @3;
      $$ = append_Formals($1, single_Formals($3));
    }
  | formal_list ',' error
    {
      @$ = @2;
      yyerrok;
      $$ = $1;
    }
  ;

formal
  : OBJECTID ':' TYPEID
    {
      @$ = @3;
      SET_NODELOC(@1);
      $$ = formal($1, $3);
    }
  ;

/* ================================================================== */
/* Expressions                                                        */
/* ================================================================== */

expression
  : assignment_expression
    {
      @$ = @1;
      $$ = $1;
    }
  | dispatch_expression
    {
      @$ = @1;
      $$ = $1;
    }
  | let_expression
    {
      @$ = @1;
      $$ = $1;
    }
  | if_expression
    {
      @$ = @1;
      $$ = $1;
    }
  | while_expression
    {
      @$ = @1;
      $$ = $1;
    }
  | block_expression
    {
      @$ = @1;
      $$ = $1;
    }
  | case_expression
    {
      @$ = @1;
      $$ = $1;
    }
  | new_expression
    {
      @$ = @1;
      $$ = $1;
    }
  | isvoid_expression
    {
      @$ = @1;
      $$ = $1;
    }
  | expression '+' expression
    {
      @$ = @3;
      SET_NODELOC(@2);
      $$ = plus($1, $3);
    }
  | expression '-' expression
    {
      @$ = @3;
      SET_NODELOC(@2);
      $$ = sub($1, $3);
    }
  | expression '*' expression
    {
      @$ = @3;
      SET_NODELOC(@2);
      $$ = mul($1, $3);
    }
  | expression '/' expression
    {
      @$ = @3;
      SET_NODELOC(@2);
      $$ = divide($1, $3);
    }
  | '~' expression
    {
      @$ = @2;
      SET_NODELOC(@1);
      $$ = neg($2);
    }
  | NOT expression
    {
      @$ = @2;
      SET_NODELOC(@1);
      $$ = comp($2);
    }
  | expression LE expression
    {
      @$ = @3;
      SET_NODELOC(@2);
      $$ = leq($1, $3);
    }
  | expression '<' expression
    {
      @$ = @3;
      SET_NODELOC(@2);
      $$ = lt($1, $3);
    }
  | expression '=' expression
    {
      @$ = @3;
      SET_NODELOC(@2);
      $$ = eq($1, $3);
    }
  | primary_expression
    {
      @$ = @1;
      $$ = $1;
    }
  ;

/* ================================================================== */
/* Simple expression families                                         */
/* ================================================================== */

assignment_expression
  : OBJECTID ASSIGN expression
    {
      @$ = @3;
      SET_NODELOC(@1);
      $$ = assign($1, $3);
    }
  ;

if_expression
  : IF expression THEN expression ELSE expression FI
    {
      @$ = @7;
      SET_NODELOC(@1);
      $$ = cond($2, $4, $6);
    }
  ;

while_expression
  : WHILE expression LOOP expression POOL
    {
      @$ = @5;
      SET_NODELOC(@1);
      $$ = loop($2, $4);
    }
  ;

block_expression
  : '{' block_items '}'
    {
      @$ = @3;
      SET_NODELOC(@1);
      $$ = block($2);
    }
  ;

case_expression
  : CASE expression OF branches ESAC
    {
      @$ = @4;
      SET_NODELOC(@1);
      $$ = typcase($2, $4);
    }
  ;

new_expression
  : NEW TYPEID
    {
      @$ = @2;
      SET_NODELOC(@1);
      $$ = new_($2);
    }
  ;

isvoid_expression
  : ISVOID expression
    {
      @$ = @2;
      SET_NODELOC(@1);
      $$ = isvoid($2);
    }
  ;

primary_expression
  : '(' expression ')'
    {
      @$ = @2;
      $$ = $2;
    }
  | INT_CONST
    {
      @$ = @1;
      SET_NODELOC(@1);
      $$ = int_const($1);
    }
  | STR_CONST
    {
      @$ = @1;
      SET_NODELOC(@1);
      $$ = string_const($1);
    }
  | BOOL_CONST
    {
      @$ = @1;
      SET_NODELOC(@1);
      $$ = bool_const($1);
    }
  | OBJECTID
    {
      @$ = @1;
      SET_NODELOC(@1);
      $$ = object($1);
    }
  ;

/* ================================================================== */
/* Let expressions                                                    */
/* ================================================================== */

let_expression
  : LET let_binding_list
    {
      @$ = @2;
      $$ = $2;
    }
  ;

let_binding_list
  : OBJECTID ':' TYPEID opt_let_init IN expression
    {
      @$ = @6;
      SET_NODELOC(@1);
      $$ = let($1, $3, $4, $6);
    }
  | OBJECTID ':' TYPEID opt_let_init ',' let_binding_list
    {
      @$ = @6;
      SET_NODELOC(@1);
      $$ = let($1, $3, $4, $6);
    }
  | error ',' let_binding_list
    {
      @$ = @3;
      yyerrok;
      $$ = $3;
    }
  ;

opt_let_init
  : /* empty */
    {
      @$ = 0;
      $$ = no_expr();
    }
  | ASSIGN expression
    {
      @$ = @2;
      $$ = $2;
    }
  ;

/* ================================================================== */
/* Dispatch                                                           */
/* ================================================================== */

dispatch_expression
  : OBJECTID '(' opt_actuals ')'
    {
      @$ = @4;
      SET_NODELOC(@1);
      $$ = dispatch(object(idtable.add_string("self")), $1, $3);
    }
  | expression '.' OBJECTID '(' opt_actuals ')'
    {
      @$ = @6;
      SET_NODELOC(@2);
      $$ = dispatch($1, $3, $5);
    }
  | expression '@' TYPEID '.' OBJECTID '(' opt_actuals ')'
    {
      @$ = @8;
      SET_NODELOC(@2);
      $$ = static_dispatch($1, $3, $5, $7);
    }
  ;

/* ================================================================== */
/* Block expressions                                                  */
/* ================================================================== */

block_items
  : block_item
    {
      @$ = @1;
      $$ = single_Expressions($1);
    }
  | block_items block_item
    {
      @$ = @2;
      $$ = append_Expressions($1, single_Expressions($2));
    }
  | block_items error ';'
    {
      @$ = @2;
      yyerrok;
      $$ = $1;
    }
  ;

block_item
  : expression ';'
    {
      @$ = @2;
      $$ = $1;
    }
  ;

/* ================================================================== */
/* Case branches                                                      */
/* ================================================================== */

branches
  : branch
    {
      @$ = @1;
      $$ = single_Cases($1);
    }
  | branches branch
    {
      @$ = @2;
      $$ = append_Cases($1, single_Cases($2));
    }
  | branches error ';'
    {
      @$ = @2;
      yyerrok;
      $$ = $1;
    }
  ;

branch
  : OBJECTID ':' TYPEID DARROW expression ';'
    {
      @$ = @6;
      SET_NODELOC(@1);
      $$ = branch($1, $3, $5);
    }
  ;

/* ================================================================== */
/* Dispatch arguments                                                 */
/* ================================================================== */

opt_actuals
  : /* empty */
    {
      @$ = 0;
      $$ = nil_Expressions();
    }
  | actual_list
    {
      @$ = @1;
      $$ = $1;
    }
  ;

actual_list
  : expression
    {
      @$ = @1;
      $$ = single_Expressions($1);
    }
  | actual_list ',' expression
    {
      @$ = @3;
      $$ = append_Expressions($1, single_Expressions($3));
    }
  | actual_list ',' error
    {
      @$ = @2;
      yyerrok;
      $$ = $1;
    }
  ;

%%

void yyerror(char *s)
{
  extern int curr_lineno;

  std::cerr << "\"" << curr_filename << "\", line "
            << curr_lineno << ": " << s << " at or near ";
  print_cool_token(yychar);
  std::cerr << std::endl;

  ++omerrs;

  if (omerrs > 50) {
    fprintf(stdout, "More than 50 errors\n");
    exit(1);
  }
}