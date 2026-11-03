%{
#include "ast.h"
#include <stdlib.h>
int yylex(void);
void yyerror(const char *message);
extern int yylineno;
Node *root;
%}
%locations
%error-verbose
%union { char *text; Node *node; }
%token INT MAIN IF ELSE WHILE RETURN
%token <text> ID NUMBER
%token EQ NE LE GE AND OR
%type <node> program block statements statement declaration expression
%destructor { free($$); } ID NUMBER
%nonassoc IF_WITHOUT_ELSE
%nonassoc ELSE
%left OR
%left AND
%left EQ NE
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right UNARY
%start program
%%
program : INT MAIN '(' ')' block { root = node("Program", "main", @1.first_line, $5, NULL, NULL); $$ = root; } ;
block : '{' statements '}' { $$ = node("Block", "", @1.first_line, $2, NULL, NULL); } ;
statements : /* empty */ { $$ = NULL; }
           | statements statement { $$ = append($1, $2); }
           | statements declaration { $$ = append($1, $2); } ;
declaration
    : INT ID ';' { $$ = node("Declare", $2, @1.first_line, NULL, NULL, NULL); free($2); }
    | INT ID '=' expression ';' { $$ = node("Declare", $2, @1.first_line, $4, NULL, NULL); free($2); }
    ;
statement
    : block { $$ = $1; }
    | ';' { $$ = node("Empty", "", @1.first_line, NULL, NULL, NULL); }
    | ID '=' expression ';' { $$ = node("Assign", $1, @1.first_line, $3, NULL, NULL); free($1); }
    | RETURN expression ';' { $$ = node("Return", "", @1.first_line, $2, NULL, NULL); }
    | IF '(' expression ')' statement %prec IF_WITHOUT_ELSE
      { $$ = node("If", "", @1.first_line, $3, $5, NULL); }
    | IF '(' expression ')' statement ELSE statement
      { $$ = node("If", "", @1.first_line, $3, $5, $7); }
    | WHILE '(' expression ')' statement
      { $$ = node("While", "", @1.first_line, $3, $5, NULL); }
    ;
expression
    : NUMBER { $$ = node("Number", $1, @1.first_line, NULL, NULL, NULL); free($1); }
    | ID { $$ = node("Variable", $1, @1.first_line, NULL, NULL, NULL); free($1); }
    | '(' expression ')' { $$ = $2; }
    | '-' expression %prec UNARY { $$ = node("Unary", "-", @1.first_line, $2, NULL, NULL); }
    | '+' expression %prec UNARY { $$ = node("Unary", "+", @1.first_line, $2, NULL, NULL); }
    | '!' expression %prec UNARY { $$ = node("Unary", "!", @1.first_line, $2, NULL, NULL); }
    | expression '+' expression { $$ = node("Binary", "+", @2.first_line, $1, $3, NULL); }
    | expression '-' expression { $$ = node("Binary", "-", @2.first_line, $1, $3, NULL); }
    | expression '*' expression { $$ = node("Binary", "*", @2.first_line, $1, $3, NULL); }
    | expression '/' expression { $$ = node("Binary", "/", @2.first_line, $1, $3, NULL); }
    | expression '%' expression { $$ = node("Binary", "%", @2.first_line, $1, $3, NULL); }
    | expression '<' expression { $$ = node("Binary", "<", @2.first_line, $1, $3, NULL); }
    | expression '>' expression { $$ = node("Binary", ">", @2.first_line, $1, $3, NULL); }
    | expression LE expression { $$ = node("Binary", "<=", @2.first_line, $1, $3, NULL); }
    | expression GE expression { $$ = node("Binary", ">=", @2.first_line, $1, $3, NULL); }
    | expression EQ expression { $$ = node("Binary", "==", @2.first_line, $1, $3, NULL); }
    | expression NE expression { $$ = node("Binary", "!=", @2.first_line, $1, $3, NULL); }
    | expression AND expression { $$ = node("Binary", "&&", @2.first_line, $1, $3, NULL); }
    | expression OR expression { $$ = node("Binary", "||", @2.first_line, $1, $3, NULL); }
    ;
%%
void yyerror(const char *message) { fail(yylloc.first_line, message); }
