#include <cstring>
#include <iostream>
#include <string>

#include "repl/driver.h"

namespace {

void print_usage() {
  std::cerr << "usage: lumen [--engine=tree|bytecode] [script]\n"
            << "  with no script, starts an interactive REPL\n";
}

}  // namespace

int main(int argc, char** argv) {
  lumen::Engine engine = lumen::Engine::Tree;
  std::string script;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg.rfind("--engine=", 0) == 0) {
      std::string value = arg.substr(std::strlen("--engine="));
      if (value == "tree") {
        engine = lumen::Engine::Tree;
      } else if (value == "bytecode") {
        engine = lumen::Engine::Bytecode;
      } else {
        std::cerr << "lumen: unknown engine '" << value << "'\n";
        return 64;
      }
    } else if (arg == "-h" || arg == "--help") {
      print_usage();
      return 0;
    } else if (!arg.empty() && arg[0] == '-') {
      std::cerr << "lumen: unknown option '" << arg << "'\n";
      print_usage();
      return 64;
    } else {
      script = arg;
    }
  }

  if (script.empty()) {
    return lumen::run_repl(engine);
  }
  return lumen::run_file(script, engine);
}
