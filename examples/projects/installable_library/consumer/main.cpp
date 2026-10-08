#include <mylib/mylib.hpp>
#include <cstdio>
int main() {
    std::printf("%d\n", mylib::answer());
    return mylib::answer() == 42 ? 0 : 1;
}
