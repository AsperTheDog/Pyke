#include <mylib/mylib.hpp>
#include <thread>
namespace mylib {
int answer() {
    int v = 40;
    std::thread worker([&] { v += 2; });
    worker.join();
    return v;
}
}
