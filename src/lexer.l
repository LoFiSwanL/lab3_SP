%option noyywrap yylineno noinput nounput
%x COMMENT
%{
#include "ast.h"
#include "parser.tab.h"
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;
static int comment_line;
%}
%%
[ \t\r\n]+                    ;
"//"[^\n]*                    ;
"/*"                          { comment_line = yylineno; BEGIN(COMMENT); }
<COMMENT>"*/"                 { BEGIN(INITIAL); }
<COMMENT>\n                   ;
<COMMENT>.                    ;
<COMMENT><<EOF>>              { fail(comment_line, "незакритий коментар"); }
"int"                         { return INT; }
"main"                        { return MAIN; }
"if"                          { return IF; }
"else"                        { return ELSE; }
"while"                       { return WHILE; }
"return"                      { return RETURN; }
(auto|break|case|char|const|continue|default|do|double|enum|extern|float|for|goto|inline|long|register|restrict|short|signed|sizeof|static|struct|switch|typedef|union|unsigned|void|volatile|_Alignas|_Alignof|_Atomic|_Bool|_Complex|_Generic|_Imaginary|_Noreturn|_Static_assert|_Thread_local) { fail(yylineno, "непідтримуване ключове слово C"); }
"=="                          { return EQ; }
"!="                          { return NE; }
"<="                          { return LE; }
">="                          { return GE; }
"&&"                          { return AND; }
"||"                          { return OR; }
("++"|"--"|"+="|"-="|"*="|"/="|"%="|"<<"|">>"|"&="|"|="|"^="|"->") { fail(yylineno, "непідтримуваний оператор C"); }
(0[xX][0-9a-fA-F]+|0[0-7]*|[1-9][0-9]*) { yylval.text = copy_text(yytext); return NUMBER; }
[0-9][a-zA-Z0-9_.]*            { fail(yylineno, "непідтримуваний числовий літерал"); }
[a-zA-Z_][a-zA-Z0-9_]*         { yylval.text = copy_text(yytext); return ID; }
[{}();=+*/%<>!\-]              { return yytext[0]; }
.                             { fail(yylineno, "невідомий символ"); }
<<EOF>>                       { yylloc.first_line = yylineno; return 0; }
%%
