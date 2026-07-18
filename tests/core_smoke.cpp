#include <cstring>

#include "cadenza/core/version.h"

int main() {
  const char* version = cadenza::coreVersion();
  return version != nullptr && std::strlen(version) > 0 ? 0 : 1;
}
