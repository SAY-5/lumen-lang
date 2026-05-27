#pragma once

#include <string>

namespace lumen {

enum class Engine { Tree, Bytecode };

// Lexes, parses, and evaluates a complete source string with the chosen
// engine. Writes program output to stdout and diagnostics to stderr. Returns a
// process exit code: 0 ok, 65 compile error, 70 runtime error.
int run_source(const std::string& source, Engine engine);

// Reads a file and runs it.
int run_file(const std::string& path, Engine engine);

// Interactive read-eval-print loop reading from stdin.
int run_repl(Engine engine);

}  // namespace lumen
