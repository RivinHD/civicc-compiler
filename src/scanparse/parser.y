%{


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "palm/memory.h"
#include "palm/ctinfo.h"
#include "palm/dbug.h"
#include "palm/str.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "global/globals.h"

static node_st *parseresult = NULL;
extern int yylex();
int yyerror(char *errname);
extern FILE *yyin;
void AddLocToNode(node_st *node, void *begin_loc, void *end_loc);


%}

%union {
 char               *id;
 int                 cint;
 float               cflt;
 enum BinOpType     cbinop;
 node_st             *node;
}

%locations

%token BRACKET_L BRACKET_R COMMA SEMICOLON CURLY_L CURLY_R SQUARE_L SQUARE_R
%token EXTERN EXPORT EXCLAMATION_MARK RETURN
%token VOID BOOL INT FLOAT
%token IF WHILE FOR DO ELSE
%token MINUS PLUS STAR SLASH PERCENT LE LT GE GT EQ NE OR AND
%token TRUEVAL FALSEVAL LET

%token <cint> NUMCONST
%token <cflt> FLOATCONST
%token <id> ID

%type <node> program declerations decleration 
%type <node> funDec funDef funHeader funBody localFunDefs localFunDef retType 
%type <node> type basicType 
%type <node> globalDec globalDef 
%type <node> params param varDecs varDec statements statement 
%type <node> arrExprs arrExpr 
%type <node> block varlet exprs expr 
%type <node> constant floatval intval boolval 
%type <node> monop ids 
%type <> binop

%start program

%%

program: declerations
         {
           parseresult = ASTprogram($1);
         }
         ;
declerations: decleration declerations {
                // $$ = ASTdeclerations($1, $2);
            }
            | decleration {
                // $$ = ASTdeclerations($1, NULL);
            };

decleration: funDec {
         } 
         | funDef 
         {
         } 
         | globalDec 
         {
         } 
         | globalDef 
         {
         };

funDec: EXTERN funHeader SEMICOLON 
      {
      };

funDef: EXPORT funHeader CURLY_L funBody CURLY_R 
      {
      }
      | funHeader CURLY_L funBody CURLY_R
      {
      };

funHeader: retType ID BRACKET_L params BRACKET_R 
         {
         }
         | retType ID BRACKET_L BRACKET_R 
         {
         };

funBody: varDecs localFuncDefs statements
       {
       }
       | varDecs statements
       {
       };

localFunDefs: localFunDef localFunDefs
            {
            }
            | localFunDef
            {
            };

localFunDef: funHeader CURLY_L funBody CURLY_R
           {
           };

retType: VOID | basicType;
type: basicType;
basicType: BOOL | INT | FLOAT;

globalDec: EXTERN type SQUARE_L ids SQUARE_R ID SEMICOLON
         {
         }
         | EXTERN type ID SEMICOLON
         {
         };
globalDef: EXPORT type SQUARE_L exprs SQUARE_R ID LET arrExpr SEMICOLON
         {
         }
         |EXPORT type SQUARE_L exprs SQUARE_R ID SEMICOLON
         {
         }
         | EXPORT type ID SEMICOLON
         {
         }
         | EXPORT type ID LET expr SEMICOLON
         {
         }
         | type ID SEMICOLON
         {
         }
         | type ID LET expr SEMICOLON   
         {
         };

params: param COMMA params
      {
      }
      | param
      {
      };

param: type SQUARE_L ids SQUARE_R ID
     {
     }
     | type ID
     {
     };

varDecs: varDec varDecs
    {
    }
    | varDec
    {
    };

varDec: type SQUARE_L exprs SQUARE_R ID LET arrExpr SEMICOLON
      {
      }
      | type SQUARE_L exprs SQUARE_R ID SEMICOLON
      {
      }
      | type ID LET expr SEMICOLON
      {
      }
      type ID SEMICOLON
      {
      };

statements: statement statements
        {
          $$ = ASTstmts($1, $2);
        }
      | statement
        {
          $$ = ASTstmts($1, NULL);
        }
        ;

statement: ID = LET expr SEMICOLON
       {
       }
       | ID BRACKET_L exprs BRACKET_R SEMICOLON
       {
       }
       | ID BRACKET_L BRACKET_R SEMICOLON
       {
       }
       | IF BRACKET_L expr BRACKET_R block ELSE block
       {
       }
       | IF BRACKET_L expr BRACKET_R block 
       {
       }
       | WHILE BRACKET_L expr BRACKET_R block
       {
       }
       | DO block while BRACKET_L expr BRACKET_R SEMICOLON
       {
       }
       | FOR BRACKET_L INT ID LET expr COMMA expr COMMA expr BRACKET_R block
       {
       }
       | FOR BRACKET_L INT ID LET expr COMMA expr BRACKET_R block
       {
       }
       | RETURN expr SEMICOLON
       {
       }
       | RETURN SEMICOLON
       {
       }
       | ID SQUARE_L exprs SQUARE_R LET expr SEMICOLON
       {
       };

arrExprs: arrExpr COMMA arrExprs
        {
        }
        | arrExpr
        {
        };

arrExpr: SQUARE_L arrExprs SQUARE_R
       {
       }
       | expr
       {
       }

block: CURLY_L statements CURLY_R
     {
     }
     | statement
     {
     };

varlet: ID
        {
          $$ = ASTvarlet($1);
          AddLocToNode($$, &@1, &@1);
        }
        ;

exprs: expr COMMA exprs
     {
     }
     | expr
     {
     };

expr: BRACKET_L expr BRACKET_R
      {
      }
      | expr[left] binop[type] expr[right] 
      {
        $$ = ASTbinop( $left, $right, $type);
        AddLocToNode($$, &@left, &@right);
      }
      | monop expr
      {
      }
      | BRACKET_L basicType BRACKET_R expr
      {
      }
      | ID BRACKET_L exprs BRACKET_R
      {
      }
      | ID BRACKET_L BRACKET_R
      {
      }
      | ID
      {
        $$ = ASTvar($1);
      }
      | constant
      {
        $$ = $1;
      }
      | ID SQUARE_L exprs SQUARE_R
      {
      };

constant: floatval
          {
            $$ = $1;
          }
        | intval
          {
            $$ = $1;
          }
        | boolval
          {
            $$ = $1;
          }
        ;

floatval: FLOATCONST
           {
             $$ = ASTfloat($1);
           }
         ;

intval: NUMCONST
        {
          $$ = ASTnum($1);
        }
      ;

boolval: TRUEVAL
         {
           $$ = ASTbool(true);
         }
       | FALSEVAL
         {
           $$ = ASTbool(false);
         }
       ;

binop: PLUS      { $$ = BO_add; }
     | MINUS     { $$ = BO_sub; }
     | STAR      { $$ = BO_mul; }
     | SLASH     { $$ = BO_div; }
     | PERCENT   { $$ = BO_mod; }
     | EQ        { $$ = BO_eq; }
     | NE        { $$ = BO_ne; }
     | LT        { $$ = BO_lt; }
     | LE        { $$ = BO_le; }
     | GT        { $$ = BO_gt; }
     | GE        { $$ = BO_ge; }
     | OR        { $$ = BO_or; }
     | AND       { $$ = BO_and; }
     ;

monop: MINUS 
     {
     }
     | EXCLAMATION_MARK
     {
     };

ids: ID COMMA ids
   {
   }
   | ID
   {
   };

%%

void AddLocToNode(node_st *node, void *begin_loc, void *end_loc)
{
    // Needed because YYLTYPE unpacks later than top-level decl.
    YYLTYPE *loc_b = (YYLTYPE*)begin_loc;
    YYLTYPE *loc_e = (YYLTYPE*)end_loc;
    NODE_BLINE(node) = loc_b->first_line;
    NODE_BCOL(node) = loc_b->first_column;
    NODE_ELINE(node) = loc_e->last_line;
    NODE_ECOL(node) = loc_e->last_column;
}

int yyerror(char *error)
{
  CTI(CTI_ERROR, true, "line %d, col %d\nError parsing source code: %s\n",
            global.line, global.col, error);
  CTIabortOnError();
  return 0;
}

node_st *SPdoScanParse(node_st *root)
{
    DBUG_ASSERT(root == NULL, "Started parsing with existing syntax tree.");
    yyin = fopen(global.input_file, "r");
    if (yyin == NULL) {
        CTI(CTI_ERROR, true, "Cannot open file '%s'.", global.input_file);
        CTIabortOnError();
    }
    yyparse();
    return parseresult;
}
