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

## Building Pyke

Needs a C++20 compiler and CMake 3.20+.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release     # unit tests + command-line tests
```

## Usage

```bash
pyke my_project.pyke output_dir/    # generate CMake files (only rewrites what changed)
pyke build                          # generate + configure + build in ./build (--debug, --target X, -j N)
pyke test                           # same, then run ctest
pyke --validate my_project.pyke     # check without generating
pyke --fmt my_project.pyke          # format (add --check to fail instead, for CI)
pyke --init my_project/             # scaffold a .pyke from an existing directory
pyke --clean my_project.pyke out/   # also delete files from removed targets
pyke --force my_project.pyke out/   # overwrite files pyke did not generate (refused by default)
pyke --upgrade my_project.pyke      # list GitHub dependencies and their tags
```

## Features

**Language**
- **Imports:** `from packages import Boost(1.78), Vulkan(optional=True)`, `from github import` (FetchContent), `from vendor import` (folders with their own CMakeLists.txt), `from env import`, all optionally conditional on an option
- **Targets:** `@Executable`, `@SharedLibrary`, `@StaticLibrary`, `@HeaderOnly`, with `path`, `source_groups`, `copy_dlls`, `test` and `unity_build` options
- **Attributes:** `sources` (globs, `**`), `includes`, `definitions`, `flags`, `link`, `link_dirs`, `copy_files`, `pch`, `assets`, `commands`, and `exports.*` variants
- **Quality settings:** `warnings = "strict"`, `warnings_as_errors`, `sanitize = ["address"]`, `lto = True`, translated per compiler
- **Conditionals:** `if`/`elif`/`else` on `platform`, `compiler`, `build_type`, options and optional packages, combined with `and`/`or`/`not`
- **Reuse:** top-level constants (`warnings = [...]`) and f-strings (`f"{self.name}-{version}"`)
- **Root-anchored paths:** `"//src/*.cpp"` instead of `"../../src/*.cpp"`
- **C++ modules:** `self.exports.modules = ["math.cppm"]` declares module interfaces; `import_std=True` for `import std;`
- **Installable libraries:** `self.export = "Name"` writes a `find_package()`-able package
- **Escape hatch:** `cmake("...")` and `self.cmake += [...]` for anything Pyke has no syntax for

**Tooling**
- **Good errors:** line/column, source snippet and "did you mean" suggestions
- **Safe regeneration:** rewrites only what changed, refuses to overwrite hand-written files, `--clean` removes stale ones
- **Warnings after generation:** empty globs, source files no pattern picks up
- **Stub creation:** missing source files are created as empty stubs automatically
- **Formatter:** `--fmt` / `--fmt --check`
- **Project settings:** `output_dir`, `presets=True` for CMakePresets.json generation

## VS Code Extension

Syntax highlighting only. Copy `vscode-extension/` to your extensions directory:

```bash
cp -r vscode-extension ~/.vscode/extensions/pyke.pyke-language-0.1.0
```

## Examples

Complete projects (sources included) live in [`examples/projects/`](examples/projects): a conventional `include/src/app/tests` library, a package-using app, a shared plugin, a C project, an installable library with a `find_package` consumer, an app with GitHub dependencies, and C++20 modules (`cxx_modules`, skipped when the `CI` environment variable is set because CI compilers are too old; run it locally with `tests/run_examples.ps1` or `.sh`).

## Tests

| What | How |
|---|---|
| Unit tests (lexer, parser, analyzer, generator, formatter) | `ctest --test-dir build -C Release` |
| Command-line tests (Linux/macOS) | `tests/run_cli_tests.sh <pyke binary>` (also a ctest entry) |
| Example projects, built with a real CMake | `tests/run_examples.sh <pyke binary>` or `tests/run_examples.ps1` on Windows; `PYKE_OFFLINE=1` / `-Offline` skips the ones that fetch from GitHub |

Single-file examples:

| File | Description |
|---|---|
| [`hello_world.pyke`](examples/hello_world.pyke) | Minimal project |
| [`fetchcontent.pyke`](examples/fetchcontent.pyke) | GitHub dependencies |
| [`library_with_tests.pyke`](examples/library_with_tests.pyke) | Library + CTest |
| [`game_engine.pyke`](examples/game_engine.pyke) | Multi-target with conditionals |
| [`vulkan_app.pyke`](examples/vulkan_app.pyke) | Vulkan SDK, env vars, DLL copying |
| [`variables_and_reuse.pyke`](examples/variables_and_reuse.pyke) | Constants, f-strings, `and`/`or`/`not`, raw CMake |
| [`full_showcase.pyke`](examples/full_showcase.pyke) | Everything at once |

Full language reference in [`LANGUAGE_SPEC.md`](LANGUAGE_SPEC.md).

## License

[MIT](LICENSE)
