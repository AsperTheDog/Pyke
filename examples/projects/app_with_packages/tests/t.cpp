#include <hdr/clamp.hpp>
int main() { return hdr::clamp(5, 0, 3) == 3 ? 0 : 1; }
