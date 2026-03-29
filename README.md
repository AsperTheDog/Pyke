# Pyke

A Python-like configuration language that compiles to CMake. Write one `.pyke` file, get all the `CMakeLists.txt` files for your project.

This started as a personal tool to avoid writing CMake by hand, thus it is catered to my uses, needs and preferences. It covers the things I deal with most in a syntax that feels more natural to me.

If someone stumbles upon this project and likes it enough to use it, I am open to contributions or suggestions.

```python
from packages import fmt, Threads

project("MyApp", version="1.0.0", lang="c++20")

@SharedLibrary("lib")
target core(PRIVATE Threads):
    def configure(self):
        self.sources = ["src/*.cpp"]
        self.exports.includes = ["include/"]

@Executable
target app(PRIVATE core, PRIVATE fmt):
    def configure(self):
        self.sources = ["src/main.cpp"]
```

## Building

```bash
cd pyke
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Usage

```bash
pyke my_project.pyke output_dir/    # generate CMake files
pyke --init my_project/             # scaffold .pyke from existing directory
pyke --validate my_project.pyke     # check without generating
pyke --fmt my_project.pyke          # format
pyke --upgrade my_project.pyke      # list FetchContent deps and their tags
```

## Features

- **Imports:** `from packages import`, `from github import` (FetchContent), `from env import`, conditional imports
- **Target types:** `@Executable`, `@SharedLibrary`, `@StaticLibrary`, `@HeaderOnly`
- **Decorator options:** `path`, `source_groups`, `copy_dlls`, `test`, `unity_build`
- **Conditionals:** `if`/`elif`/`else` on `platform`, `compiler`, `build_type`, and user options
- **Attributes:** `sources`, `includes`, `definitions`, `flags`, `link`, `link_dirs`, `copy_files`, `pch`, `assets`, `commands`, and `exports.*` variants
- **Project settings:** `output_dir`, `presets=True` for CMakePresets.json generation
- **Stub creation:** missing source files are created as empty stubs automatically

## VS Code Extension

Copy `vscode-extension/` to your extensions directory for syntax highlighting:

```bash
cp -r vscode-extension ~/.vscode/extensions/pyke.pyke-language-0.1.0
```

## Examples

| File | Description |
|---|---|
| [`hello_world.pyke`](examples/hello_world.pyke) | Minimal project |
| [`fetchcontent.pyke`](examples/fetchcontent.pyke) | GitHub dependencies |
| [`library_with_tests.pyke`](examples/library_with_tests.pyke) | Library + CTest |
| [`game_engine.pyke`](examples/game_engine.pyke) | Multi-target with conditionals |
| [`vulkan_app.pyke`](examples/vulkan_app.pyke) | Vulkan SDK, env vars, DLL copying |
| [`full_showcase.pyke`](examples/full_showcase.pyke) | Everything at once |

Full language reference in [`LANGUAGE_SPEC.md`](LANGUAGE_SPEC.md).

## License

[MIT](LICENSE)
