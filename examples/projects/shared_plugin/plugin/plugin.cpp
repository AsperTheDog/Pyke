int core_value();
#ifdef _WIN32
#define PLUGIN_API extern "C" __declspec(dllexport)
#else
#define PLUGIN_API extern "C"
#endif
PLUGIN_API int plugin_value() { return core_value() * 2; }
