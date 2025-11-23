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
 char               *var;
 int                 cint;
 float               cflt;
 enum BinOpType     cbinop;
 enum MonOpType     cmonop;
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
%token <var> VAR

%type <node> program declarations declaration 
%type <node> funDec funDef funHeader funBody localFunDefs localFunDef 
%type <node> basicType 
%type <node> globalDec globalDef 
%type <node> params param varDecs varDec varDecs_localFunDefs
%type <node> statements statement statement_no_else
%type <node> arrayInits arrayInit 
%type <node> block block_if
%type <node> exprs expr expr_binop expr_binop_no_binop expr_binop_OR expr_binop_AND expr_binop_EQNE
%type <node> expr_binop_LTLEGTGE expr_binop_PLUSMINUS expr_monop expr_cast
%type <node> constant floatval intval boolval vars 
%type <cbinop> binop_EQNE binop_LTLEGTGE binop_PLUSMINUS binop_STARSLASHPERCENT
%type <cmonop> monop

%start program

%%

program: declarations 
         {
           parseresult = ASTprogram($1);
         };

declarations: declaration declarations 
            {
                $$ = ASTdeclarations($1, $2);
            }
            | declaration 
            {
                $$ = ASTdeclarations($1, NULL);
            };

declaration: globalDef 
           {
           } 
           | globalDec 
           {
           } 
           | funDef 
           {
           } 
           | funDec
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

funHeader: VOID VAR BRACKET_L params BRACKET_R 
         {
         }
         | basicType VAR BRACKET_L params BRACKET_R 
         {
         }
         | VOID VAR BRACKET_L BRACKET_R 
         {
         }
         | basicType VAR BRACKET_L BRACKET_R 
         {
         };

funBody: varDecs_localFunDefs statements
       {
       }
       | varDecs statements
       {
       }
       | localFunDefs statements
       {
       }
       | statements
       {
       }
       | varDecs 
       {
       }
       | localFunDefs
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

basicType: BOOL 
         {
         }
         | INT 
         {
         }
         | FLOAT
         {
         };

globalDec: EXTERN basicType SQUARE_L vars SQUARE_R VAR SEMICOLON
         {
         }
         | EXTERN basicType VAR SEMICOLON
         {
         };

globalDef: EXPORT basicType SQUARE_L exprs SQUARE_R VAR LET arrayInit SEMICOLON
         {
         }
         | EXPORT basicType SQUARE_L exprs SQUARE_R VAR SEMICOLON
         {
         }
         | EXPORT basicType VAR SEMICOLON
         {
         }
         | EXPORT basicType VAR LET expr SEMICOLON
         {
         }
         | basicType VAR SEMICOLON
         {
         }
         | basicType VAR LET expr SEMICOLON   
         {
         };

params: param COMMA params
      {
      }
      | param
      {
      };

param: basicType SQUARE_L vars SQUARE_R VAR
     {
     }
     | basicType VAR
     {
     };

varDecs: varDec varDecs
    {
    }
    | varDec
    {
    };

varDecs_localFunDefs: varDec varDecs_localFunDefs
                    {
                    }
                    | varDec localFunDefs
                    {
                    };

varDec: basicType SQUARE_L exprs SQUARE_R VAR LET arrayInit SEMICOLON
      {
      }
      | basicType SQUARE_L exprs SQUARE_R VAR SEMICOLON
      {
      }
      | basicType VAR LET expr SEMICOLON
      {
      }
      | basicType VAR SEMICOLON
      {
      };

statements: statement statements
          {
          $$ = ASTstatements($1, $2);
          }
          | statement
          {
          $$ = ASTstatements($1, NULL);
          };

statement: VAR LET expr SEMICOLON
         {
         }
         | VAR BRACKET_L exprs BRACKET_R SEMICOLON
         {
         }
         | VAR BRACKET_L BRACKET_R SEMICOLON
         {
         }
         | IF BRACKET_L expr BRACKET_R block_if ELSE block
         {
         }
         | IF BRACKET_L expr BRACKET_R block_if
         {
         }
         | WHILE BRACKET_L expr BRACKET_R block
         {
         }
         | DO block WHILE BRACKET_L expr BRACKET_R SEMICOLON
         {
         }
         | FOR BRACKET_L INT VAR LET expr COMMA expr COMMA expr BRACKET_R block
         {
         }
         | FOR BRACKET_L INT VAR LET expr COMMA expr BRACKET_R block
         {
         }
         | RETURN expr SEMICOLON
         {
         }
         | RETURN SEMICOLON
         {
         }
         | VAR SQUARE_L exprs SQUARE_R LET expr SEMICOLON
         {
         };

statement_no_else: VAR LET expr SEMICOLON
                 {
                 }
                 | VAR BRACKET_L exprs BRACKET_R SEMICOLON
                 {
                 }
                 | VAR BRACKET_L BRACKET_R SEMICOLON
                 {
                 }
                 | IF BRACKET_L expr BRACKET_R block_if
                 {
                 }
                 | WHILE BRACKET_L expr BRACKET_R block_if
                 {
                 }
                 | DO block WHILE BRACKET_L expr BRACKET_R SEMICOLON
                 {
                 }
                 | FOR BRACKET_L INT VAR LET expr COMMA expr COMMA expr BRACKET_R block_if
                 {
                 }
                 | FOR BRACKET_L INT VAR LET expr COMMA expr BRACKET_R block_if
                 {
                 }
                 | RETURN expr SEMICOLON
                 {
                 }
                 | RETURN SEMICOLON
                 {
                 }
                 | VAR SQUARE_L exprs SQUARE_R LET expr SEMICOLON
                 {
                 };

arrayInits: arrayInit COMMA arrayInits
          {
          }
          | arrayInit
          {
          };

arrayInit: SQUARE_L arrayInits SQUARE_R
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

block_if: CURLY_L statements CURLY_R
        {
        }
        | statement_no_else
        {
        };

exprs: expr COMMA exprs
     {
     }
     | expr
     {
     };

expr: BRACKET_L expr BRACKET_R
    {
    }
    | expr_binop
    {
      $$ = $1;
    }
    | monop expr_monop
    {
    }
    | BRACKET_L basicType BRACKET_R expr_cast
    {
    }
    | VAR BRACKET_L exprs BRACKET_R
    {
    }
    | VAR BRACKET_L BRACKET_R
    {
    }
    | VAR
    {
      $$ = ASTvar($1);
    }
    | constant
    {
      $$ = $1;
    }
    | VAR SQUARE_L exprs SQUARE_R
    {
    };

// Extract binops based on predecence OR > AND > EQ, NE > LT, LE, GT, GE > PLUS, MINUS > STAR, SLASH, PERCENT
// Naming follows the logic: We come from the binop_<OP> and process one predecnece lower
expr_binop: expr_binop_OR 
          {
          }
          // with this the left associativity is ensured
          | expr_binop OR expr_binop_OR
          {
          };

expr_binop_OR: expr_binop_AND
             {
             }
             | expr_binop_OR AND expr_binop_AND
             {
             };

expr_binop_AND: expr_binop_EQNE
              {
              }
              | expr_binop_AND binop_EQNE expr_binop_EQNE
              {
              };

expr_binop_EQNE: expr_binop_LTLEGTGE
               {
               }
               | expr_binop_EQNE binop_LTLEGTGE expr_binop_LTLEGTGE
               {
               };

expr_binop_LTLEGTGE: expr_binop_PLUSMINUS
                   {
                   }
                   | expr_binop_LTLEGTGE binop_PLUSMINUS expr_binop_PLUSMINUS
                   {
                   };

expr_binop_PLUSMINUS: expr_binop_no_binop binop_STARSLASHPERCENT expr_binop_no_binop
                    {
                    }
                    | expr_binop_PLUSMINUS binop_STARSLASHPERCENT expr_binop_no_binop
                    {
                    };

// Expression from binopt that does not include the binop itself.
expr_binop_no_binop: BRACKET_L expr BRACKET_R
                   {
                   }
                   | monop expr_monop
                   {
                   }
                   | BRACKET_L basicType BRACKET_R expr_cast
                   {
                   }
                   | VAR BRACKET_L exprs BRACKET_R
                   {
                   }
                   | VAR BRACKET_L BRACKET_R
                   {
                   }
                   | VAR
                   {
                     $$ = ASTvar($1);
                   }
                   | constant
                   {
                     $$ = $1;
                   }
                   | VAR SQUARE_L exprs SQUARE_R
                   {
                   };


expr_monop: BRACKET_L expr BRACKET_R
          {
          }
          | monop expr_monop
          {
          }
          | BRACKET_L basicType BRACKET_R expr_cast
          {
          }
          | VAR BRACKET_L exprs BRACKET_R
          {
          }
          | VAR BRACKET_L BRACKET_R
          {
          }
          | VAR
          {
            $$ = ASTvar($1);
          }
          | constant
          {
            $$ = $1;
          }
          | VAR SQUARE_L exprs SQUARE_R
          {
          };

expr_cast: BRACKET_L expr BRACKET_R
         {
         }
         | monop expr_monop
         {
         }
         | BRACKET_L basicType BRACKET_R expr_cast
         {
         }
         | VAR BRACKET_L exprs BRACKET_R
         {
         }
         | VAR BRACKET_L BRACKET_R
         {
         }
         | VAR
         {
           $$ = ASTvar($1);
         }
         | constant
         {
           $$ = $1;
         }
         | VAR SQUARE_L exprs SQUARE_R
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
        };

floatval: FLOATCONST
        {
        $$ = ASTfloat($1);
        };

intval: NUMCONST
      {
      $$ = ASTnum($1);
      };

boolval: TRUEVAL 
       {
       $$ = ASTbool(true);
       }
       | FALSEVAL
       {
       $$ = ASTbool(false);
       };

binop_EQNE: EQ        { $$ = BO_eq; }
          | NE        { $$ = BO_ne; };

binop_LTLEGTGE: LT        { $$ = BO_lt; }
              | LE        { $$ = BO_le; }
              | GT        { $$ = BO_gt; }
              | GE        { $$ = BO_ge; };

binop_PLUSMINUS: PLUS      { $$ = BO_add; }
     | MINUS     { $$ = BO_sub; };

binop_STARSLASHPERCENT:  STAR     { $$ = BO_mul; }
                      | SLASH     { $$ = BO_div; }
                      | PERCENT   { $$ = BO_mod; } ;

monop: MINUS 
     {
     $$ = MO_neg;
     }
     | EXCLAMATION_MARK
     {
     $$ = MO_not;
     };

vars: VAR COMMA vars
   {
   }
   | VAR
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
