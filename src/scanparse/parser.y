%{

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include "palm/memory.h"
#include "palm/ctinfo.h"
#include "palm/dbug.h"
#include "palm/str.h"
#include "ccngen/ast.h"
#include "ccngen/enum.h"
#include "global/globals.h"
#include "release_assert.h"
#include "utils.h"

static node_st *parseresult = NULL;
extern int yylex();
int yyerror(char *errname);
extern FILE *yyin;
typedef void *YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_bytes(char *, size_t);
extern void yy_delete_buffer(YY_BUFFER_STATE);
void AddLocToNode(node_st *node, void *begin_loc, void *end_loc);

#define assertSetType(node, setType) \
do { \
   node_st *_ast_node = (node); \
   if (_ast_node == NULL) break; \
   scanparse_fprintf(stdout, "Set: %d %d\n", NODE_TYPE(_ast_node), (int)(setType)); \
   uint64_t _ast_combined = nodessettype_to_nodetypes((setType)); \
   release_assert(((1ull << NODE_TYPE(_ast_node)) & _ast_combined) != 0); \
} while (0)

#define assertType(node, type) \
do { \
   node_st *_at_node = (node); \
   if (_at_node == NULL) break; \
   scanparse_fprintf(stdout, "Type: %d %d\n", NODE_TYPE(_at_node), (int)(type)); \
   release_assert(NODE_TYPE(_at_node) == (type)); \
} while (0)

%}

%union {
 char               *id;
 int                 cint;
 double               cflt;
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
%type <node> statements
%type <node> statement statementNoBlock statementLoop statementUnmatched statementMatched 
%type <node> statementLoopMatched statementDoWhile
%type <node> arrayInits arrayInit arrayVar
%type <node> block blockOrMatched
%type <node> exprs expr expr_binop expr_binop_OR expr_binop_AND expr_binop_EQNE
%type <node> expr_binop_LTLEGTGE expr_binop_PLUSMINUS expr_monopcast
%type <node> constant floatval intval boolval dimensionVars var
%type <cbinop> binop_EQNE binop_LTLEGTGE binop_PLUSMINUS binop_STARSLASHPERCENT
%type <cmonop> monop
%type <ctype> basicType

%start program

%%

// We do not use left recursion, because otherwise some parts of the tree would be structed upside down

program: declarations 
         {
           assertType($1, NT_DECLARATIONS);
           parseresult = ASTprogram($1);
           AddLocToNode($$, &@1, &@1);
         };

declarations: declaration declarations
            {
                assertSetType($1, NS_DECLARATION);
                assertType($2, NT_DECLARATIONS);
                $$ = ASTdeclarations($1, $2);
                AddLocToNode($$, &@1, &@2);
            }
            | declaration 
            {
                assertSetType($1, NS_DECLARATION);
                $$ = ASTdeclarations($1, NULL);
                AddLocToNode($$, &@1, &@1);
            };

declaration: globalDef 
           {
              assertType($1, NT_GLOBALDEF);
              $$ = $1;
           } 
           | globalDec 
           {
              assertType($1, NT_GLOBALDEC);
              $$ = $1;  
           } 
           | funDef 
           {
              assertType($1, NT_FUNDEF);
              $$ = $1;
           } 
           | funDec
           {
              assertType($1, NT_FUNDEC);
              $$ = $1;
           };

funDec: EXTERN funHeader SEMICOLON 
      {
        assertType($2, NT_FUNHEADER);
        $$ = ASTfundec($2);
        AddLocToNode($$, &@1, &@3);
      };

funDef: EXPORT funHeader CURLY_L funBody CURLY_R 
      {
        assertType($2, NT_FUNHEADER);
        assertType($4, NT_FUNBODY);
        $$ = ASTfundef($2, $4, true);
        AddLocToNode($$, &@1, &@5);
      }
      | funHeader CURLY_L funBody CURLY_R
      {
        assertType($1, NT_FUNHEADER);
        assertType($3, NT_FUNBODY);
        $$ = ASTfundef($1, $3, false);
        AddLocToNode($$, &@1, &@4);
      };

funHeader: VOID var BRACKET_L optParams BRACKET_R 
         {
           assertType($2, NT_VAR);
           $$ = ASTfunheader($2, $4, DT_void);
           AddLocToNode($$, &@1, &@5);
         }
         | basicType var BRACKET_L optParams BRACKET_R 
         {
           assertType($2, NT_VAR);
           $$ = ASTfunheader($2, $4, $1);
           AddLocToNode($$, &@1, &@5);
         };

funBody: funBody_varDecs_localFunDefs statements
       {
            assertType($1, NT_FUNBODY);
            assertType($2, NT_STATEMENTS);
            FUNBODY_STMTS($1) = $2;
            $$ = $1;
            AddLocToNode($$, &@1, &@2);
       }
       | funBody_varDecs_localFunDefs
       {
            assertType($1, NT_FUNBODY);
            $$ = $1;
       }
       | varDecs statements
       {
            assertType($1, NT_VARDECS);
            assertType($2, NT_STATEMENTS);
            $$ = ASTfunbody($1, NULL, $2);
            AddLocToNode($$, &@1, &@2);
       }
       | localFunDefs statements
       {
            assertType($1, NT_LOCALFUNDEFS);
            assertType($2, NT_STATEMENTS);
            $$ = ASTfunbody(NULL, $1, $2);
            AddLocToNode($$, &@1, &@2);
       }
       | statements
       {
            assertType($1, NT_STATEMENTS);
            $$ = ASTfunbody(NULL, NULL, $1);
            AddLocToNode($$, &@1, &@1);
       }
       | varDecs 
       {
            assertType($1, NT_VARDECS);
            $$ = ASTfunbody($1, NULL, NULL);
            AddLocToNode($$, &@1, &@1);
       }
       | localFunDefs
       {
            assertType($1, NT_LOCALFUNDEFS);
            $$ = ASTfunbody(NULL, $1, NULL);
            AddLocToNode($$, &@1, &@1);
       }
       |
       {
           // The funbody is allowed to be empty
           $$ = ASTfunbody(NULL, NULL, NULL);
       };


localFunDefs: localFunDef localFunDefs
            {
                assertType($1, NT_FUNDEF);
                assertType($2, NT_LOCALFUNDEFS);
                $$ = ASTlocalfundefs($1, $2);
                AddLocToNode($$, &@1, &@2);
            }
            | localFunDef
            {
                assertType($1, NT_FUNDEF);
                $$ = ASTlocalfundefs($1, NULL);
                AddLocToNode($$, &@1, &@1);
            };

localFunDef: funHeader CURLY_L funBody CURLY_R
           {
            assertType($1, NT_FUNHEADER);
            assertType($3, NT_FUNBODY);
            $$ = ASTfundef($1, $3, false);
            AddLocToNode($$, &@1, &@4);
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
            assertType($3, NT_ARRAYVAR);
            $$ = ASTglobaldec($3, $2);
            AddLocToNode($$, &@1, &@4);
         }
         | EXTERN basicType var SEMICOLON
         {
            assertType($3, NT_VAR);
            $$ = ASTglobaldec($3, $2);
            AddLocToNode($$, &@1, &@4);
         };

globalDef: EXPORT varDec
         {
            assertType($2, NT_VARDEC);
            $$ = ASTglobaldef($2, true);
            AddLocToNode($$, &@1, &@2);
         }
         | varDec
         {
            assertType($1, NT_VARDEC);
            $$ = ASTglobaldef($1, false);
            AddLocToNode($$, &@1, &@1);
         };

// We are not able to do left recursion otherwise we have memory leaks
params: param COMMA params
      {
        assertType($1, NT_PARAMS);
        assertType($3, NT_PARAMS);
        PARAMS_NEXT($1) = $3;
        AddLocToNode($$, &@1, &@3);
      }
      | param
      {
        assertType($1, NT_PARAMS);
        $$ = $1;
      };

param: basicType arrayVar
     {
        assertType($2, NT_ARRAYVAR);
        $$ = ASTparams($2, NULL, $1);
        AddLocToNode($$, &@1, &@2);
     }
     | basicType var
     {
        assertType($2, NT_VAR);
        $$ = ASTparams($2, NULL, $1);
        AddLocToNode($$, &@1, &@2);
     };

optParams: params
         {
            assertType($1, NT_PARAMS);
            $$ = $1;
         }
         |
         {
            $$ = NULL;
         };


// We are not able to do left recursion otherwise we have shift-reduce confictls
varDecs: varDec varDecs
    {
      assertType($1, NT_VARDEC);
      assertType($2, NT_VARDECS);
      $$ = ASTvardecs($1, $2);
      AddLocToNode($$, &@1, &@2);
    }
    | varDec
    {
      assertType($1, NT_VARDEC);
      $$ = ASTvardecs($1, NULL);
      AddLocToNode($$, &@1, &@1);
    };

// Funbody that only contains VarDecs and localFunDefs
funBody_varDecs_localFunDefs: varDec funBody_varDecs_localFunDefs
                    {
                        assertType($1, NT_VARDEC);
                        assertType($2, NT_FUNBODY);
                        node_st* vardec = ASTvardecs($1, FUNBODY_VARDECS($2));
                        AddLocToNode(vardec, &@1, &@1);
                        NODE_ELINE(vardec) = NODE_ELINE(FUNBODY_VARDECS($2));
                        NODE_ECOL(vardec) = NODE_ECOL(FUNBODY_VARDECS($2));

                        FUNBODY_VARDECS($2) = vardec;
                        $$ = $2;
                        AddLocToNode($$, &@1, &@2);
                    }
                    | varDec localFunDefs
                    {
                        assertType($1, NT_VARDEC);
                        assertType($2, NT_LOCALFUNDEFS);
                        node_st* vardec = ASTvardecs($1, NULL); 
                        AddLocToNode(vardec, &@1, &@1);

                        $$ = ASTfunbody(vardec, $2, NULL);
                        AddLocToNode($$, &@1, &@2);
                    };

varDec: basicType SQUARE_L exprs SQUARE_R var optArrayInit SEMICOLON
      {
          assertType($3, NT_EXPRS);
          assertType($5, NT_VAR);
          node_st* array = ASTarrayexpr($3, $5);
          AddLocToNode(array, &@2, &@5);

          $$ = ASTvardec(array, $6, $1);
          AddLocToNode($$, &@1, &@7);
      }
      | basicType var optVarInit SEMICOLON
      {
          assertType($2, NT_VAR);
          $$ = ASTvardec($2, $3, $1);
          AddLocToNode($$, &@1, &@4);
      };

optVarInit: LET expr
          {
            assertSetType($2, NS_EXPR);
            $$ = $2;
          }
          |
          {
            $$ = NULL;
          };


statements: statement statements
          {
              assertSetType($1, NS_STATEMENT);
              assertType($2, NT_STATEMENTS);
              $$ = ASTstatements($1, $2);
              AddLocToNode($$, &@1, &@2);
          }
          | statement
          {
              assertSetType($1, NS_STATEMENT);
              $$ = ASTstatements($1, NULL);
              AddLocToNode($$, &@1, &@1);
          };

statement: statementUnmatched
         {
            assertSetType($1, NS_STATEMENT);
            $$ = $1;
         }
         | statementMatched
         {
            assertSetType($1, NS_STATEMENT);
            $$ = $1;
         };


statementUnmatched: IF BRACKET_L expr BRACKET_R block ELSE statementUnmatched
         {
            assertSetType($3, NS_EXPR);
            assertType($5, NT_STATEMENTS);
            assertSetType($7, NS_STATEMENT);

            node_st* statements_else = ASTstatements($7, NULL);
            AddLocToNode(statements_else, &@7, &@7);

            $$ = ASTifstatement($3, $5, statements_else);
            AddLocToNode($$, &@1, &@7);
         }
         | IF BRACKET_L expr BRACKET_R statementMatched ELSE statementUnmatched
         {
            assertSetType($3, NS_EXPR);
            assertSetType($5, NS_STATEMENT);
            assertSetType($7, NS_STATEMENT);

            node_st* statement = ASTstatements($5, NULL);
            AddLocToNode(statement, &@5, &@5);

            node_st* statement_else = ASTstatements($7, NULL);
            AddLocToNode(statement_else, &@7, &@7);

            $$ = ASTifstatement($3, statement, statement_else);
            AddLocToNode($$, &@1, &@7);
         }
         | IF BRACKET_L expr BRACKET_R block
         {
            assertSetType($3, NS_EXPR);
            assertType($5, NT_STATEMENTS);
            $$ = ASTifstatement($3, $5, NULL);
            AddLocToNode($$, &@1, &@5);
         }
         | IF BRACKET_L expr BRACKET_R statement
         {
            assertSetType($3, NS_EXPR);
            assertSetType($5, NS_STATEMENT);

            node_st* statement = ASTstatements($5, NULL);
            AddLocToNode(statement, &@5, &@5);

            $$ = ASTifstatement($3, statement, NULL);
            AddLocToNode($$, &@1, &@5);
         }
         | statementLoop
         {
            assertSetType($1, NS_STATEMENT);
            $$ = $1;
         };
         

statementMatched: IF BRACKET_L expr BRACKET_R block ELSE block
                {
                    assertSetType($3, NS_EXPR);
                    assertType($5, NT_STATEMENTS);
                    assertType($7, NT_STATEMENTS);

                    $$ = ASTifstatement($3, $5, $7);
                    AddLocToNode($$, &@1, &@7);
                }
                | IF BRACKET_L expr BRACKET_R statementMatched ELSE block
                {
                    assertSetType($3, NS_EXPR);
                    assertSetType($5, NS_STATEMENT);
                    assertType($7, NT_STATEMENTS);

                    node_st* statement = ASTstatements($5, NULL);
                    AddLocToNode(statement, &@5, &@5);

                    $$ = ASTifstatement($3, statement, $7);
                    AddLocToNode($$, &@1, &@7);
                }
                | IF BRACKET_L expr BRACKET_R block ELSE statementMatched
                {
                    assertSetType($3, NS_EXPR);
                    assertType($5, NT_STATEMENTS);
                    assertSetType($7, NS_STATEMENT);

                    node_st* statement_else = ASTstatements($7, NULL);
                    AddLocToNode(statement_else, &@7, &@7);

                    $$ = ASTifstatement($3, $5, statement_else);
                    AddLocToNode($$, &@1, &@7);
                }
                | IF BRACKET_L expr BRACKET_R statementMatched ELSE statementMatched
                {
                    assertSetType($3, NS_EXPR);
                    assertSetType($5, NS_STATEMENT);
                    assertSetType($7, NS_STATEMENT);

                    node_st* statement = ASTstatements($5, NULL);
                    AddLocToNode(statement, &@5, &@5);

                    node_st* statement_else = ASTstatements($7, NULL);
                    AddLocToNode(statement_else, &@7, &@7);

                    $$ = ASTifstatement($3, statement, statement_else);
                    AddLocToNode($$, &@1, &@7);
                }
                | statementNoBlock
                {
                    assertSetType($1, NS_STATEMENT);
                    $$ = $1;
                }
                | statementLoopMatched
                {
                    assertSetType($1, NS_STATEMENT);
                    $$ = $1;
                }
                | statementDoWhile
                {
                    assertSetType($1, NS_STATEMENT);
                    $$ = $1;
                };

statementLoop: WHILE BRACKET_L expr BRACKET_R statementUnmatched
         {
            assertSetType($3, NS_EXPR);
            assertSetType($5, NS_STATEMENT);

            node_st* statement = ASTstatements($5, NULL);
            AddLocToNode(statement, &@5, &@5);

            $$ = ASTwhileloop($3, statement);
            AddLocToNode($$, &@1, &@5);
         }
         | FOR BRACKET_L INT var LET expr COMMA expr optForStep BRACKET_R statementUnmatched
         {
            assertType($4, NT_VAR);
            assertSetType($6, NS_EXPR);
            assertSetType($8, NS_EXPR);
            assertSetType($9, NS_EXPR);
            assertSetType($11, NS_STATEMENT);
            node_st* assign = ASTassign($4, $6); 
            AddLocToNode(assign, $4, $6);

            node_st* statement = ASTstatements($11, NULL);
            AddLocToNode(statement, &@11, &@11);
            
            $$ = ASTforloop(assign, $8, $9, statement);
            AddLocToNode($$, &@1, &@11);
         };

statementLoopMatched: WHILE BRACKET_L expr BRACKET_R blockOrMatched
         {
            assertSetType($3, NS_EXPR);
            assertType($5, NT_STATEMENTS);
            $$ = ASTwhileloop($3, $5);
            AddLocToNode($$, &@1, &@5);
         }
         | FOR BRACKET_L INT var LET expr COMMA expr optForStep BRACKET_R blockOrMatched
         {
            assertType($4, NT_VAR);
            assertSetType($6, NS_EXPR);
            assertSetType($8, NS_EXPR);
            assertSetType($9, NS_EXPR);
            assertType($11, NT_STATEMENTS);
            node_st* assign = ASTassign($4, $6); 
            AddLocToNode(assign, $4, $6);
            
            $$ = ASTforloop(assign, $8, $9, $11);
            AddLocToNode($$, &@1, &@11);
         };

// Do while act as a natural block with and without the curly braces thus we handel it seperatly
statementDoWhile: DO block WHILE BRACKET_L expr BRACKET_R SEMICOLON
         {
            assertType($2, NT_STATEMENTS);
            assertSetType($5, NS_EXPR);
            $$ = ASTdowhileloop($2, $5);
            AddLocToNode($$, &@1, &@7);
         }
         | DO statement WHILE BRACKET_L expr BRACKET_R SEMICOLON
         {
            assertSetType($2, NS_STATEMENT);
            assertSetType($5, NS_EXPR);

            node_st* statement = ASTstatements($2, NULL);
            AddLocToNode(statement, &@2, &@2);

            $$ = ASTdowhileloop(statement, $5);
            AddLocToNode($$, &@1, &@7);
         };

statementNoBlock: var LET expr SEMICOLON
                 {
                    assertType($1, NT_VAR);
                    assertSetType($3, NS_EXPR);
                    $$ = ASTassign($1, $3);
                    AddLocToNode($$, &@1, &@4);
                 }
                 | var BRACKET_L optArrayExpr BRACKET_R SEMICOLON
                 {
                    assertType($1, NT_VAR);
                    $$ = ASTproccall($1, $3);
                    AddLocToNode($$, &@1, &@5);
                 }
                 | RETURN optExpr SEMICOLON
                 {
                    $$ = ASTretstatement($2);
                    AddLocToNode($$, &@1, &@3);
                 }
                 | var SQUARE_L exprs SQUARE_R LET expr SEMICOLON
                 {
                    assertType($1, NT_VAR);
                    assertType($3, NT_EXPRS);
                    assertSetType($6, NS_EXPR);
                    node_st* arrayexpr = ASTarrayexpr($3, $1);
                    AddLocToNode(arrayexpr, &@1, &@4);

                    $$ = ASTarrayassign(arrayexpr, $6);
                    AddLocToNode($$, &@1, &@7);
                 };

optForStep: COMMA expr
          {
            assertSetType($2, NS_EXPR);
            $$ = $2;
          }
          |
          {
            $$ = NULL;
          };

optArrayExpr: exprs
        {
            assertType($1, NT_EXPRS);
            $$ = $1;
        }
        |
        {
            $$ = NULL;
        };

optExpr: expr
       {
          assertSetType($1, NS_EXPR);
          $$ = $1;
       }
       |
       {
          $$ = NULL;
       };

arrayVar: SQUARE_L dimensionVars SQUARE_R var
        {
            assertType($2, NT_DIMENSIONVARS);
            assertType($4, NT_VAR);
            $$ = ASTarrayvar($2, $4);
            AddLocToNode($$, &@1, &@4);
        };

arrayInits: arrayInit COMMA arrayInits
          {
            assertType($3, NT_ARRAYINIT);
            $$ = ASTarrayinit($1, $3);
            AddLocToNode($$, &@1, &@3);
          }
          | arrayInit
          {
            $$ = ASTarrayinit($1, NULL);
            AddLocToNode($$, &@1, &@1);
          };

arrayInit: SQUARE_L arrayInits SQUARE_R
         {
            assertType($2, NT_ARRAYINIT);
            $$ = $2;
         }
         | expr
         {
            assertSetType($1, NS_EXPR);
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

blockOrMatched: block
              {
                $$ = $1;
              }
              | statementMatched
              {
                assertSetType($1, NS_STATEMENT);
                $$ = ASTstatements($1, NULL);
                AddLocToNode($$, &@1, &@1);
              };

block: CURLY_L statements CURLY_R
     {
        assertType($2, NT_STATEMENTS);
        $$ = $2;
     }
     | CURLY_L CURLY_R
     {
         $$ = NULL;
     };


exprs: expr COMMA exprs
     {
        assertSetType($1, NS_EXPR);
        assertType($3, NT_EXPRS);
        $$ = ASTexprs($1, $3);
        AddLocToNode($$, &@1, &@3);
     }
     | expr
     {
        assertSetType($1, NS_EXPR);
        $$ = ASTexprs($1, NULL);
        AddLocToNode($$, &@1, &@1);
     };

expr: expr_monopcast
    {
        assertSetType($1, NS_EXPR);
        $$ = $1;
    }
    | expr_binop
    {
        assertSetType($1, NS_EXPR);
        $$ = $1;
    };

// Extract binops based on precedence OR > AND > EQ, NE > LT, LE, GT, GE > PLUS, MINUS > STAR, SLASH, PERCENT
// Naming follows the logic: We come from the binop_<OP> and process one precedence lower
expr_binop: expr_binop_OR 
          {
            assertSetType($1, NS_EXPR);
            $$ = $1;
          }
          // with this the left associativity is ensured
          | expr_binop OR expr_binop_OR
          {
            assertSetType($1, NS_EXPR);
            assertSetType($3, NS_EXPR);
            $$ = ASTbinop($1, $3, BO_or);
            AddLocToNode($$, &@1, &@3);
          }
          | expr_binop OR expr_monopcast 
          {
            assertSetType($1, NS_EXPR);
            assertSetType($3, NS_EXPR);
            $$ = ASTbinop($1, $3, BO_or);
            AddLocToNode($$, &@1, &@3);
          }
          | expr_monopcast OR expr_binop_OR
          {
            assertSetType($1, NS_EXPR);
            assertSetType($3, NS_EXPR);
            $$ = ASTbinop($1, $3, BO_or);
            AddLocToNode($$, &@1, &@3);
          }
          | expr_monopcast OR expr_monopcast
          {
            assertSetType($1, NS_EXPR);
            assertSetType($3, NS_EXPR);
            $$ = ASTbinop($1, $3, BO_or);
            AddLocToNode($$, &@1, &@3);
          };

expr_binop_OR: expr_binop_AND
             {
                assertSetType($1, NS_EXPR);
                $$ = $1;
             }
             | expr_binop_OR AND expr_binop_AND
             {
                assertSetType($1, NS_EXPR);
                assertSetType($3, NS_EXPR);
                $$ = ASTbinop($1, $3, BO_and);
                AddLocToNode($$, &@1, &@3);
             }
             | expr_binop_OR AND expr_monopcast 
             {
                assertSetType($1, NS_EXPR);
                assertSetType($3, NS_EXPR);
                $$ = ASTbinop($1, $3, BO_and);
                AddLocToNode($$, &@1, &@3);
             }
             | expr_monopcast AND expr_binop_AND
             {
                assertSetType($1, NS_EXPR);
                assertSetType($3, NS_EXPR);
                $$ = ASTbinop($1, $3, BO_and);
                AddLocToNode($$, &@1, &@3);
             }
             | expr_monopcast AND expr_monopcast
             {
                assertSetType($1, NS_EXPR);
                assertSetType($3, NS_EXPR);
                $$ = ASTbinop($1, $3, BO_and);
                AddLocToNode($$, &@1, &@3);
             };

expr_binop_AND: expr_binop_EQNE
              {
                  assertSetType($1, NS_EXPR);
                  $$ = $1;
              }
              | expr_binop_AND binop_EQNE expr_binop_EQNE
              {
                  assertSetType($1, NS_EXPR);
                  assertSetType($3, NS_EXPR);
                  $$ = ASTbinop($1, $3, $2);
                  AddLocToNode($$, &@1, &@3);
              }
              | expr_binop_AND binop_EQNE expr_monopcast 
              {
                  assertSetType($1, NS_EXPR);
                  assertSetType($3, NS_EXPR);
                  $$ = ASTbinop($1, $3, $2);
                  AddLocToNode($$, &@1, &@3);
              }
              | expr_monopcast binop_EQNE expr_binop_EQNE
              {
                  assertSetType($1, NS_EXPR);
                  assertSetType($3, NS_EXPR);
                  $$ = ASTbinop($1, $3, $2);
                  AddLocToNode($$, &@1, &@3);
              }
              | expr_monopcast binop_EQNE expr_monopcast
              {
                  assertSetType($1, NS_EXPR);
                  assertSetType($3, NS_EXPR);
                  $$ = ASTbinop($1, $3, $2);
                  AddLocToNode($$, &@1, &@3);
              };

expr_binop_EQNE: expr_binop_LTLEGTGE
               {
                    assertSetType($1, NS_EXPR);
                    $$ = $1;
               }
               | expr_binop_EQNE binop_LTLEGTGE expr_binop_LTLEGTGE
               {
                    assertSetType($1, NS_EXPR);
                    assertSetType($3, NS_EXPR);
                    $$ = ASTbinop($1, $3, $2);
                    AddLocToNode($$, &@1, &@3);
               }
               | expr_binop_EQNE binop_LTLEGTGE expr_monopcast 
               {
                    assertSetType($1, NS_EXPR);
                    assertSetType($3, NS_EXPR);
                    $$ = ASTbinop($1, $3, $2);
                    AddLocToNode($$, &@1, &@3);
               }
               | expr_monopcast binop_LTLEGTGE expr_binop_LTLEGTGE
               {
                    assertSetType($1, NS_EXPR);
                    assertSetType($3, NS_EXPR);
                    $$ = ASTbinop($1, $3, $2);
                    AddLocToNode($$, &@1, &@3);
               }
               | expr_monopcast binop_LTLEGTGE expr_monopcast
               {
                    assertSetType($1, NS_EXPR);
                    assertSetType($3, NS_EXPR);
                    $$ = ASTbinop($1, $3, $2);
                    AddLocToNode($$, &@1, &@3);
               };

expr_binop_LTLEGTGE: expr_binop_PLUSMINUS
                   {
                      assertSetType($1, NS_EXPR);
                      $$ = $1;
                   }
                   | expr_binop_LTLEGTGE binop_PLUSMINUS expr_binop_PLUSMINUS
                   {
                      assertSetType($1, NS_EXPR);
                      assertSetType($3, NS_EXPR);
                      $$ = ASTbinop($1, $3, $2);
                      AddLocToNode($$, &@1, &@3);
                   }
                   | expr_binop_LTLEGTGE binop_PLUSMINUS expr_monopcast 
                   {
                      assertSetType($1, NS_EXPR);
                      assertSetType($3, NS_EXPR);
                      $$ = ASTbinop($1, $3, $2);
                      AddLocToNode($$, &@1, &@3);
                   }
                   | expr_monopcast binop_PLUSMINUS expr_binop_PLUSMINUS
                   {
                      assertSetType($1, NS_EXPR);
                      assertSetType($3, NS_EXPR);
                      $$ = ASTbinop($1, $3, $2);
                      AddLocToNode($$, &@1, &@3);
                   }
                   | expr_monopcast binop_PLUSMINUS expr_monopcast
                   {
                      assertSetType($1, NS_EXPR);
                      assertSetType($3, NS_EXPR);
                      $$ = ASTbinop($1, $3, $2);
                      AddLocToNode($$, &@1, &@3);
                   };

// expr_monopcast = all Expression except binop.
expr_binop_PLUSMINUS: expr_monopcast binop_STARSLASHPERCENT expr_monopcast
                    {
                        assertSetType($1, NS_EXPR);
                        assertSetType($3, NS_EXPR);
                        $$ = ASTbinop($1, $3, $2);
                        AddLocToNode($$, &@1, &@3);
                    }
                    | expr_binop_PLUSMINUS binop_STARSLASHPERCENT expr_monopcast
                    {
                        assertSetType($1, NS_EXPR);
                        assertSetType($3, NS_EXPR);
                        $$ = ASTbinop($1, $3, $2);
                        AddLocToNode($$, &@1, &@3);
                    };

expr_monopcast: BRACKET_L expr BRACKET_R
          {
              assertSetType($2, NS_EXPR);
              $$ = $2;
          }
          | monop expr_monopcast
          {
              assertSetType($2, NS_EXPR);
              $$ = ASTmonop($2, $1);
              AddLocToNode($$, &@1, &@2);
          }
          | BRACKET_L basicType BRACKET_R expr_monopcast
          {
              assertSetType($4, NS_EXPR);
              $$ = ASTcast($4, $2);
              AddLocToNode($$, &@1, &@4);
          }
          | var BRACKET_L exprs BRACKET_R
          {
              assertType($1, NT_VAR);
              assertType($3, NT_EXPRS);
              $$ = ASTproccall($1, $3);
              AddLocToNode($$, &@1, &@4);
          }
          | var BRACKET_L BRACKET_R
          {
              assertType($1, NT_VAR);
              $$ = ASTproccall($1, NULL);
              AddLocToNode($$, &@1, &@3);
          }
          | var
          {
              assertType($1, NT_VAR);
              $$ = $1;
          }
          | constant
          {
              $$ = $1;
          }
          | var SQUARE_L exprs SQUARE_R
          {
              assertType($1, NT_VAR);
              assertType($3, NT_EXPRS);
              $$ = ASTarrayexpr($3, $1);
              AddLocToNode($$, &@1, &@4);
          };

constant: floatval
        {
            assertType($1, NT_FLOAT);
            $$ = $1;
        }
        | intval
        {
            assertType($1, NT_INT);
            $$ = $1;
        }
        | boolval
        {
            assertType($1, NT_BOOL);
            $$ = $1;
        };

floatval: FLOATCONST
        {
            $$ = ASTfloat($1);
            AddLocToNode($$, &@1, &@1);
        };

intval: NUMCONST
      {
          $$ = ASTint($1);
          AddLocToNode($$, &@1, &@1);
      };

boolval: TRUEVAL 
       {
          $$ = ASTbool(true);
          AddLocToNode($$, &@1, &@1);
       }
       | FALSEVAL
       {
          $$ = ASTbool(false);
          AddLocToNode($$, &@1, &@1);
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
      assertType($1, NT_VAR);
      assertType($3, NT_DIMENSIONVARS);
      $$ = ASTdimensionvars($1, $3);
      AddLocToNode($$, &@1, &@3);
   }
   | var
   {
      assertType($1, NT_VAR);
      $$ = ASTdimensionvars($1, NULL);
      AddLocToNode($$, &@1, &@1);
   };

var: VAR
   {
    scanparse_fprintf(stdout, "Variable: %s\n", $1);
    $$ = ASTvar($1);
    AddLocToNode($$, &@1, &@1);
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
    if (global.input_buf == NULL)
    {
        yyin = fopen(global.input_file, "r");
        if (yyin == NULL) {
            CTI(CTI_ERROR, true, "Cannot open file '%s'.", global.input_file);
            CTIabortOnError();
        }
        yyparse();
    }
    else
    {
        YY_BUFFER_STATE buffer = yy_scan_bytes(global.input_buf, global.input_buf_len);
        yyparse();
        yy_delete_buffer(buffer);
    }

    return parseresult;
}
