# Lumen grammar

The grammar below is the one the recursive-descent parser implements. It is an
LL(1) grammar with the usual expression precedence ladder. Lowercase names are
nonterminals; UPPERCASE names are terminals produced by the lexer.

## Program

```
program     -> declaration* EOF ;
declaration -> classDecl | funDecl | varDecl | statement ;
```

## Declarations

```
classDecl   -> "class" IDENTIFIER ( "<" IDENTIFIER )? "{" function* "}" ;
funDecl     -> "fun" function ;
function    -> IDENTIFIER "(" parameters? ")" block ;
parameters  -> IDENTIFIER ( "," IDENTIFIER )* ;
varDecl     -> "var" IDENTIFIER ( "=" expression )? ";" ;
```

## Statements

```
statement   -> exprStmt | forStmt | ifStmt | printStmt
             | returnStmt | whileStmt | block ;
exprStmt    -> expression ";" ;
forStmt     -> "for" "(" ( varDecl | exprStmt | ";" )
                         expression? ";"
                         expression? ")" statement ;
ifStmt      -> "if" "(" expression ")" statement ( "else" statement )? ;
printStmt   -> "print" expression ";" ;
returnStmt  -> "return" expression? ";" ;
whileStmt   -> "while" "(" expression ")" statement ;
block       -> "{" declaration* "}" ;
```

`for` is desugared into a block wrapping a `while` loop; there is no dedicated
`for` AST node.

## Expressions

Each rule sits one precedence level above the rule it calls, lowest first:

```
expression  -> assignment ;
assignment  -> ( call "." )? IDENTIFIER "=" assignment | logic_or ;
logic_or    -> logic_and ( "or" logic_and )* ;
logic_and   -> equality ( "and" equality )* ;
equality    -> comparison ( ( "!=" | "==" ) comparison )* ;
comparison  -> term ( ( ">" | ">=" | "<" | "<=" ) term )* ;
term        -> factor ( ( "-" | "+" ) factor )* ;
factor      -> unary ( ( "/" | "*" ) unary )* ;
unary       -> ( "!" | "-" ) unary | call ;
call        -> primary ( "(" arguments? ")" | "." IDENTIFIER )* ;
arguments   -> expression ( "," expression )* ;
primary     -> NUMBER | STRING | "true" | "false" | "nil"
             | "(" expression ")" | IDENTIFIER
             | "this" | "super" "." IDENTIFIER ;
```

Assignment is right-associative; every binary rule is left-associative.
Function calls and property access bind tighter than unary, which binds tighter
than the arithmetic and logical ladder.

## Error handling

The parser reports errors with the offending line and a short message, then
synchronizes to the next statement boundary (a `;` or a leading keyword) so a
single source file can surface multiple independent errors in one pass.
