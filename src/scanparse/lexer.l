%{

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <limits.h>
#include <float.h>

#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "parser.h"
#include "palm/str.h"
#include "global/globals.h"
#include "palm/ctinfo.h"
#include <stdio.h>
#include "utils.h"

// Use for error messages, see parser.y
extern int yyerror(char *errname);

#define FILTER(token) \
  scanparse_fprintf(stdout, "'%d': '%s'\n", token, yytext); \
  global.col += yyleng;  \
  return( token);

#define LINEBUF_SIZE 256
#define YY_NO_UNPUT
#define YY_NO_INPUT

static inline void token_action();


int yycolumn = 1;
#define YY_USER_ACTION token_action();

static inline void convert_int(int base);

%}

%p 6000 /* set positions to 6000 */
%e 2000 /* set nodes to 1100 */
%option noyywrap
%option nounput
%option yylineno

SEP [ \t\r\n]
SEPB ({SEP}|\()
SEPC ({SEP}|\{)

%x MULTI_COMMENT

%%


"/*"                       { BEGIN(MULTI_COMMENT); }
"//"[^\n]*$                { global.col += yyleng; }

"("                        { FILTER( BRACKET_L); }
")"                        { FILTER( BRACKET_R); }
","                        { FILTER( COMMA); }
";"                        { FILTER( SEMICOLON); }
"{"                        { FILTER( CURLY_L); }
"}"                        { FILTER( CURLY_R); }
"["                        { FILTER( SQUARE_L); }
"]"                        { FILTER( SQUARE_R); }

"-"                        { FILTER( MINUS); }
"+"                        { FILTER( PLUS);  }
"*"                        { FILTER( STAR);  }
"/"                        { FILTER( SLASH);  }
"%"                        { FILTER( PERCENT);  }

"<="                       { FILTER( LE);    }
"<"                        { FILTER( LT);    }
">="                       { FILTER( GE);    }
">"                        { FILTER( GT);    }
"=="                       { FILTER( EQ);    }
"!="                       { FILTER( NE);    }

"&&"                       { FILTER( AND);    }
"||"                       { FILTER( OR);    }

"!"                        { FILTER( EXCLAMATION_MARK)}

"="                        { FILTER( LET);    }

"true"[^A-Za-z0-9_]            { 
                            yyleng = yyleng - 1; 
                            yyless(yyleng); 
                            FILTER( TRUEVAL); }
"false"[^A-Za-z0-9_]           { 
                            yyleng = yyleng - 1; 
                            yyless(yyleng); 
                            FILTER( FALSEVAL); }

"extern"{SEP}                   { 
                            yyleng = yyleng - 1; 
                            yyless(yyleng); 
                            FILTER( EXTERN); }
"export"{SEP}                   { 
                            yyleng = yyleng - 1; 
                            yyless(yyleng); 
                            FILTER( EXPORT); }
"return"({SEPB}|;)             { 
                             yyleng = yyleng - 1; 
                             yyless(yyleng); 
                             FILTER( RETURN); }


"void"{SEP}                { 
                            yyleng = yyleng - 1; 
                            yyless(yyleng); 
                            FILTER( VOID); }
"bool"({SEP}|\[|\))                { 
                            yyleng = yyleng - 1; 
                            yyless(yyleng); 
                            FILTER( BOOL); }
"int"({SEP}|\[|\))                 { 
                            yyleng = yyleng - 1; 
                            yyless(yyleng); 
                            FILTER( INT); }
"float"({SEP}|\[|\))               { 
                            yyleng = yyleng - 1; 
                            yyless(yyleng); 
                            FILTER( FLOAT);}


"if"{SEPB}                 { 
                             yyleng = yyleng - 1; 
                             yyless(yyleng);
                             FILTER( IF);
                            }
"while"{SEPB}               { 
                             yyleng = yyleng - 1; 
                             yyless(yyleng);
                             FILTER( WHILE);
                            }
"for"{SEPB}                 { 
                             yyleng = yyleng - 1; 
                             yyless(yyleng);
                             FILTER( FOR);
                            }
"do"{SEPC}                 { 
                             yyleng = yyleng - 1; 
                             yyless(yyleng);
                             FILTER( DO);
                            }
"else"{SEPC}              {
                             yyleng = yyleng - 1; 
                             yyless(yyleng);
                             FILTER( ELSE);
                            }

[A-Za-z][A-Za-z0-9_]*      { yylval.id = STRcpy(yytext);
                             FILTER( VAR);
                           }

[0-9]+\.[0-9]+(("e"|"E")("+"|"-")?[0-9]+)? {
    double value = strtod(yytext, NULL);

    if(errno == ERANGE)
    {
        char* filename = STRcpy(global.input_file);
        struct ctinfo info = {global.line, yycolumn - yyleng, 0, yycolumn, filename, NULL};
        CTIobj(CTI_WARN, true, info, "Float out of range. Will clamp the value '%s' to '%f'.\n", yytext, value);
        if(filename != NULL)
        {
            free(filename);
        }
    }

    yylval.cflt = value;
    FILTER( FLOATCONST);
}

[1-9][0-9]*                     { 
    convert_int(10);
    FILTER( NUMCONST);
}

"0"[0-7]*                     { 
    convert_int(8);
    FILTER( NUMCONST);
}

"0x"[0-9a-fA-F]+                     { 
    convert_int(16);
    FILTER( NUMCONST);
}

"0b"[0-1]+                     { 
    convert_int(2);
    FILTER( NUMCONST);
}

\n.*                       {
                             yycolumn = 1;
                             global.line += 1;
                             global.col = 1;
                             yyless(1);
                           }

[ \t]+                      { global.col += yyleng; }

"#"[^\n]*\n {
    // Preprocessor linemarker
    // Reset the line on the linemarkers
    // Also enable use to skip preprocesor stuff if it is disabled
    yycolumn = 1;
    global.line = 1;
    global.col = 1;
    printf("Linemarker: '%s'\n", yytext);
}

. {                             
    scanparse_fprintf(stdout, "UNDEF_CHAR: '%s'\n", yytext);
    yyerror("invalid character");
}

<MULTI_COMMENT>{
    [^*\n]*         { global.col += yyleng; }
    "*"+[^*\/\n]*   { global.col += yyleng; }
    \n            { 
                         yycolumn = 1;
                         global.line += 1;
                         global.col = 1;
                    }
    "*"+"/"         { BEGIN(INITIAL); }
}
%%

static inline void token_action() {
    yylloc.first_line = yylloc.last_line = global.line;
    yylloc.first_column = yycolumn;
    yylloc.last_column = yycolumn + yyleng - 1;
    yycolumn += yyleng;
}

static inline void convert_int(int base) 
{
    long value = strtol(yytext, NULL, base);

    if(value > INT_MAX || value < INT_MIN)
    {
        char* filename = STRcpy(global.input_file);
        value = (value > INT_MAX) ? INT_MAX : INT_MIN;
        struct ctinfo info = {yylineno, yycolumn - yyleng, 0, yycolumn - 1, filename, NULL};
        CTIobj(CTI_WARN, true, info, "Int out of range. Will clamp the value '%s' to '%d'.\n", yytext, (int)value);
        if(filename != NULL)
        {
            free(filename);
        }
    }

    yylval.cint= (int)value;
}
