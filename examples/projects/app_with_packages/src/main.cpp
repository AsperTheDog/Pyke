#include <hdr/clamp.hpp>
#include <cstdio>
#include <thread>
#include "version.h"
int main() {
    int threads = 0;
    std::thread worker([&] { threads = 1; });
    worker.join();
    std::printf("v%s clamp=%d threads=%d\n", APP_VERSION, hdr::clamp(15, 0, 10), threads);
}
