# AST shape

The AST is defined in `src/parser/ast.h`. Expression and statement nodes are
modelled as `std::variant` alternatives wrapped in a tagged struct, so traversal
is done with `std::visit` and `if constexpr` dispatch rather than a virtual
visitor hierarchy. Nodes are owned through `std::shared_ptr` (`ExprPtr`,
`StmtPtr`) because closures and methods retain subtrees beyond a single walk.

## Expressions

| Node           | Fields                                  |
|----------------|-----------------------------------------|
| `LiteralExpr`  | `value` (number/string/bool/nil), `line`|
| `UnaryExpr`    | `op`, `right`                           |
| `BinaryExpr`   | `left`, `op`, `right`                   |
| `LogicalExpr`  | `left`, `op` (`and`/`or`), `right`      |
| `GroupingExpr` | `expr`                                  |
| `VariableExpr` | `name`                                  |
| `AssignExpr`   | `name`, `value`                         |
| `CallExpr`     | `callee`, `paren`, `arguments`          |
| `GetExpr`      | `object`, `name`                        |
| `SetExpr`      | `object`, `name`, `value`               |
| `ThisExpr`     | `keyword`                               |
| `SuperExpr`    | `keyword`, `method`                     |

## Statements

| Node           | Fields                                     |
|----------------|--------------------------------------------|
| `ExprStmt`     | `expression`                               |
| `PrintStmt`    | `expression`                               |
| `VarStmt`      | `name`, `initializer` (nullable)           |
| `BlockStmt`    | `statements`                               |
| `IfStmt`       | `condition`, `then_branch`, `else_branch`  |
| `WhileStmt`    | `condition`, `body`                        |
| `FunctionStmt` | `name`, `params`, `body`                   |
| `ReturnStmt`   | `keyword`, `value` (nullable)              |
| `ClassStmt`    | `name`, `superclass` (nullable), `methods` |

`make_expr<T>(...)` and `make_stmt<T>(...)` are the construction helpers; they
allocate the wrapper and emplace the chosen alternative.
