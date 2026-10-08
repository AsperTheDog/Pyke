#include <cstdio>
#ifdef _WIN32
extern "C" __declspec(dllimport) int plugin_value();
#else
extern "C" int plugin_value();
#endif
int main() {
    int v = plugin_value();
    std::printf("%d\n", v);
    return v == 14 ? 0 : 1;
}
