#include <ThaiBrk.h>

#include <iostream>
#include <string>

// Reads UTF-8 lines on stdin, prints each with '|' at every break opportunity.
int main() {
  std::ios::sync_with_stdio(false);
  std::string line;
  while (std::getline(std::cin, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    const auto breaks = ThaiBrk::wordBreakByteOffsets(line);
    size_t prev = 0;
    for (const size_t b : breaks) {
      std::cout << line.substr(prev, b - prev) << '|';
      prev = b;
    }
    std::cout << line.substr(prev) << '\n';
  }
  return 0;
}
