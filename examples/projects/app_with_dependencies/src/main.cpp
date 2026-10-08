#include <fmt/core.h>
#include <hdr/clamp.hpp>
#include "version.h"
int main() { fmt::print("v{} clamp={}\n", APP_VERSION, hdr::clamp(15, 0, 10)); }
