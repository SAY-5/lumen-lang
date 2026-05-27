#include "repl/driver.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>

#include "bytecode/compiler.h"
#include "bytecode/vm.h"
#include "eval/interpreter.h"
#include "lexer/lexer.h"
#include "parser/parser.h"

namespace lumen {

namespace {

// Reports lex/parse diagnostics to stderr in a stable, golden-friendly format.
template <typename ErrList>
void report(const ErrList& errors) {
  for (const auto& e : errors) {
    std::cerr << "[line " << e.line << "] error: " << e.message << "\n";
  }
}

}  // namespace

int run_source(const std::string& source, Engine engine) {
  Lexer lexer(source);
  auto tokens = lexer.scan_tokens();
  if (lexer.had_error()) {
    report(lexer.errors());
    return 65;
  }

  Parser parser(std::move(tokens));
  auto statements = parser.parse();
  if (parser.had_error()) {
    report(parser.errors());
    return 65;
  }

  if (engine == Engine::Bytecode) {
    Compiler compiler;
    FunctionObj script = compiler.compile(statements);
    if (compiler.had_error()) {
      report(compiler.errors());
      return 65;
    }
    VM vm;
    InterpretResult result = vm.run(script);
    std::cout << vm.output();
    std::cout.flush();
    if (result != InterpretResult::Ok) {
      std::cerr << vm.error_message() << "\n";
      return 70;
    }
    return 0;
  }

  Interpreter interp;
  bool ok = interp.interpret(statements);
  std::cout << interp.output();
  std::cout.flush();
  if (!ok) {
    std::cerr << interp.error_message() << "\n";
    return 70;
  }
  return 0;
}

int run_file(const std::string& path, Engine engine) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    std::cerr << "lumen: cannot open file '" << path << "'\n";
    return 66;
  }
  std::ostringstream ss;
  ss << in.rdbuf();
  return run_source(ss.str(), engine);
}

int run_repl(Engine engine) {
  std::string line;
  std::cout << "lumen repl (ctrl-d to exit)\n";
  while (true) {
    std::cout << "> ";
    std::cout.flush();
    if (!std::getline(std::cin, line))
      break;
    if (line.empty())
      continue;
    run_source(line, engine);
  }
  std::cout << "\n";
  return 0;
}

}  // namespace lumen
