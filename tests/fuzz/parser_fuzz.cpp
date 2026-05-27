// libFuzzer entry point for the lexer + parser pipeline.
//
// Feeds arbitrary bytes through the full front end. The parser must never
// crash, hang, or trip a sanitizer on any input: malformed programs are
// expected to be reported as errors, not to abort the process.

#include <cstddef>
#include <cstdint>
#include <string>

#include "lexer/lexer.h"
#include "parser/parser.h"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  std::string source(reinterpret_cast<const char*>(data), size);

  lumen::Lexer lexer(source);
  auto tokens = lexer.scan_tokens();

  lumen::Parser parser(std::move(tokens));
  parser.parse();  // Result intentionally discarded; we only care about safety.

  return 0;
}
