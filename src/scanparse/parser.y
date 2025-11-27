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
 enum MonOpType     cmonop;
 enum DataType      ctype;
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
%token <id> VAR

%type <node> program declarations declaration 
%type <node> funDec funDef funHeader funBody localFunDefs localFunDef 
%type <node> globalDec globalDef
%type <node> optParams optArrayInit optVarInit optArrayExpr optExpr optForStep 
%type <node> params param varDecs varDec funBody_varDecs_localFunDefs
%type <node> statements statement statement_no_else
%type <node> arrayInits arrayInit arrayVar
%type <node> block block_if
%type <node> Exprs expr expr_binop expr_binop_OR expr_binop_AND expr_binop_EQNE
%type <node> expr_binop_LTLEGTGE expr_binop_PLUSMINUS expr_monopcast
%type <node> constant floatval intval boolval dimensionVars var
%type <cbinop> binop_EQNE binop_LTLEGTGE binop_PLUSMINUS binop_STARSLASHPERCENT
%type <cmonop> monop
%type <ctype> basicType

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
              $$ = $1;
           } 
           | globalDec 
           {
              $$ = $1;  
           } 
           | funDef 
           {
              $$ = $1;
           } 
           | funDec
           {
              $$ = $1;
           };

funDec: EXTERN funHeader SEMICOLON 
      {
        $$ = ASTfundec($2);
      };

funDef: EXPORT funHeader CURLY_L funBody CURLY_R 
      {
        $$ = ASTfundef($2, $4, true);
      }
      | funHeader CURLY_L funBody CURLY_R
      {
        $$ = ASTfundef($1, $3, false);
      };

funHeader: VOID var BRACKET_L optParams BRACKET_R 
         {
           $$ = ASTfunheader($2, $4, DT_void);
         }
         | basicType var BRACKET_L optParams BRACKET_R 
         {
           $$ = ASTfunheader($2, $4, $1);
         };

funBody: funBody_varDecs_localFunDefs statements
       {
            FUNBODY_STMTS($1) = $2;
            $$ = $1;
       }
       | varDecs statements
       {
            $$ = ASTfunbody($1, NULL, $2);
       }
       | localFunDefs statements
       {
            $$ = ASTfunbody(NULL, $1, $2);
       }
       | statements
       {
            $$ = ASTfunbody(NULL, NULL, $1);
       }
       | varDecs 
       {
            $$ = ASTfunbody($1, NULL, NULL);
       }
       | localFunDefs
       {
            $$ = ASTfunbody(NULL, $1, NULL);
       };


localFunDefs: localFunDef localFunDefs
            {
                $$ = ASTlocalfundefs($1, $2);
            }
            | localFunDef
            {
                $$ = ASTlocalfundefs($1, NULL);
            };

localFunDef: funHeader CURLY_L funBody CURLY_R
           {
            $$ = ASTlocalfundef($1, $3);
           };

basicType: BOOL 
         {
            $$ = DT_bool;
         }
         | INT 
         {
            $$ = DT_int;
         }
         | FLOAT
         {
            $$ = DT_float;
         };

globalDec: EXTERN basicType arrayVar SEMICOLON
         {
            $$ = ASTglobaldec($3, $2);
         }
         | EXTERN basicType var SEMICOLON
         {
            $$ = ASTglobaldec($3, $2);
         };

globalDef: EXPORT varDec
         {
            $$ = ASTglobaldef($2, true);
         }
         | varDec
         {
            $$ = ASTglobaldef($1, false);
         };

params: param COMMA params
      {
        $$ = ASTparams($1, $3);
      }
      | param
      {
        $$ = ASTparams($1, NULL);
      };

param: basicType arrayVar
     {
        $$ = ASTparam($2, $1);
     }
     | basicType var
     {
        $$ = ASTparam($2, $1);
     };

optParams: params
         {
            $$ = $1;
         }
         |
         {
            $$ = NULL;
         };


varDecs: varDec varDecs
    {
      $$ = ASTvardecs($1, $2);
    }
    | varDec
    {
      $$ = ASTvardecs($1, NULL);
    };

// Funbody that only contains VarDecs and localFunDefs
funBody_varDecs_localFunDefs: varDec funBody_varDecs_localFunDefs
                    {
                        FUNBODY_VARDECS($2) = ASTvardecs($1, FUNBODY_VARDECS($2));
                        $$ = $2;
                    }
                    | varDec localFunDefs
                    {
                        $$ = ASTfunbody(ASTvardecs($1, NULL), $2, NULL);
                    };

varDec: basicType SQUARE_L Exprs SQUARE_R var optArrayInit SEMICOLON
      {
        $$ = ASTvardec(ASTarrayexpr($3, $5), $6, $1);
      }
      | basicType var optVarInit SEMICOLON
      {
          $$ = ASTvardec($2, $3, $1);
      };

optVarInit: LET expr
          {
            $$ = $2;
          }
          |
          {
            $$ = NULL;
          };


statements: statement statements
          {
          $$ = ASTstatements($1, $2);
          }
          | statement
          {
          $$ = ASTstatements($1, NULL);
          };

statement: statement_no_else
         {
            $$ = $1;
         }
         | IF BRACKET_L expr BRACKET_R block_if ELSE block
         {
            $$ = ASTifstatement($3, $5, $7);
         };

statement_no_else: var LET expr SEMICOLON
                 {
                    $$ = ASTassign($1, $3);
                 }
                 | var BRACKET_L optArrayExpr BRACKET_R SEMICOLON
                 {
                    $$ = ASTproccall($1, $3);
                 }
                 | IF BRACKET_L expr BRACKET_R block_if
                 {
                    $$ = ASTifstatement($3, $5, NULL);
                 }
                 | WHILE BRACKET_L expr BRACKET_R block_if
                 {
                    $$ = ASTwhileloop($3, $5);
                 }
                 | DO block WHILE BRACKET_L expr BRACKET_R SEMICOLON
                 {
                    $$ = ASTdowhileloop($2, $5);
                 }
                 | FOR BRACKET_L INT var LET expr COMMA expr optForStep BRACKET_R block_if
                 {
                    $$ = ASTforloop(ASTassign($4, $6), $8, $9, $11);
                 }
                 | RETURN optExpr SEMICOLON
                 {
                    $$ = ASTretstatement($2);
                 }
                 | var SQUARE_L Exprs SQUARE_R LET expr SEMICOLON
                 {
                    $$ = ASTarrayassign(ASTarrayexpr($3, $1), $6);
                 };

optForStep: COMMA expr
          {
            $$ = $2;
          }
          |
          {
            $$ = NULL;
          };

optArrayExpr: Exprs
        {
            $$ = $1;
        }
        |
        {
            $$ = NULL;
        };

optExpr: expr
       {
          $$ = $1;
       }
       |
       {
          $$ = NULL;
       };

arrayVar: SQUARE_L dimensionVars SQUARE_R var
        {
            $$ = ASTarrayvar($2, $4);
        };

arrayInits: arrayInit COMMA arrayInits
          {
            $$ = ASTarrayinit($1, $3);
          }
          | arrayInit
          {
            $$ = ASTarrayinit($1, NULL);
          };

arrayInit: SQUARE_L arrayInits SQUARE_R
         {
            $$ = $2;
         }
         | expr
         {
            $$ = $1;
         }

optArrayInit: LET arrayInit
            {
                $$ = $2;
            }
            |
            {
                $$ = NULL;
            };


block: CURLY_L statements CURLY_R
     {
        $$ = $2;
     }
     | statement
     {
        $$ = $1;
     };

block_if: CURLY_L statements CURLY_R
        {
            $$ = $2;
        }
        | statement_no_else
        {
            $$ = $1;
        };

Exprs: expr COMMA Exprs
     {
        $$ = ASTexprs($1, $3);
     }
     | expr
     {
        $$ = ASTexprs($1, NULL);
     };

expr: expr_monopcast
    {
        $$ = $1;
    }
    | expr_binop
    {
        $$ = $1;
    };

// Extract binops based on precedence OR > AND > EQ, NE > LT, LE, GT, GE > PLUS, MINUS > STAR, SLASH, PERCENT
// Naming follows the logic: We come from the binop_<OP> and process one precedence lower
expr_binop: expr_binop_OR 
          {
            $$ = $1;
          }
          // with this the left associativity is ensured
          | expr_binop OR expr_binop_OR
          {
            $$ = ASTbinop($1, $3, BO_or);
          };

expr_binop_OR: expr_binop_AND
             {
                $$ = $1;
             }
             | expr_binop_OR AND expr_binop_AND
             {
                $$ = ASTbinop($1, $3, BO_and);
             };

expr_binop_AND: expr_binop_EQNE
              {
                $$ = $1;
              }
              | expr_binop_AND binop_EQNE expr_binop_EQNE
              {
                $$ = ASTbinop($1, $3, $2);
              };

expr_binop_EQNE: expr_binop_LTLEGTGE
               {
                  $$ = $1;
               }
               | expr_binop_EQNE binop_LTLEGTGE expr_binop_LTLEGTGE
               {
                  $$ = ASTbinop($1, $3, $2);
               };

expr_binop_LTLEGTGE: expr_binop_PLUSMINUS
                   {
                    $$ = $1;
                   }
                   | expr_binop_LTLEGTGE binop_PLUSMINUS expr_binop_PLUSMINUS
                   {
                    $$ = ASTbinop($1, $3, $2);
                   };

// expr_monopcast = all Expression except binop.
expr_binop_PLUSMINUS: expr_monopcast binop_STARSLASHPERCENT expr_monopcast
                    {
                        $$ = ASTbinop($1, $3, $2);
                    }
                    | expr_binop_PLUSMINUS binop_STARSLASHPERCENT expr_monopcast
                    {
                        $$ = ASTbinop($1, $3, $2);
                    };

expr_monopcast: BRACKET_L expr BRACKET_R
          {
            $$ = $2;
          }
          | monop expr_monopcast
          {
            $$ = ASTmonop($2, $1);
          }
          | BRACKET_L basicType BRACKET_R expr_monopcast
          {
            $$ = ASTcast($4, $2);
          }
          | var BRACKET_L Exprs BRACKET_R
          {
            $$ = ASTproccall($1, $3);
          }
          | var BRACKET_L BRACKET_R
          {
            $$ = ASTproccall($1, NULL);
          }
          | var
          {
            $$ = $1;
          }
          | constant
          {
            $$ = $1;
          }
          | var SQUARE_L Exprs SQUARE_R
          {
            $$ = ASTarrayexpr($3, $1);
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
      $$ = ASTint($1);
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

dimensionVars: var COMMA dimensionVars
   {
      $$ = ASTdimensionvars($1, $3);
   }
   | var
   {
      $$ = ASTdimensionvars($1, NULL);
   };

var: VAR
   {
    $$ = ASTvar($1);
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
