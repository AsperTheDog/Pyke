# Pyke Language Reference

## Overview

Pyke is a configuration language that compiles to CMake. It uses Python-like syntax tailored to build system concepts, so a whole project is described in one `.pyke` file. The compiler is written in C++.

**Contents:** [File structure](#file-structure) · [Project](#project-declaration) · [Imports](#imports) · [Options](#options) · [Targets](#targets) · [Variables](#variables-constants) · [f-strings](#f-strings) · [Conditions](#conditions) · [Raw CMake](#raw-cmake) · [Command line](#command-line) · [Diagnostics and validation](#diagnostics)

---

## File Structure

A `.pyke` file is made of these parts, usually in this order (constants and `cmake(...)` lines may appear anywhere before they are used):

```python
# 1. Imports: system packages, environment variables, GitHub and vendored projects
from packages import Boost, fmt, Threads
from env import VULKAN_SDK
from github import "gabime/spdlog" as spdlog, tag="v1.14.1"

# 2. Project declaration (required)
project("Name", version="1.0.0", lang="c++20")

# 3. Options (user-configurable values)
option use_opengl: bool = True
option build_tests: bool = False

# 4. Targets
@Executable
target MyApp(PRIVATE Lib1):
    def configure(self):
        ...
```

---

## Project Declaration

```python
project("GameEngine", version="0.1.0", lang="c++20")
```

- `name` (positional): project name string
- `version` (keyword): version string of numbers separated by dots
- `lang` (keyword): Language standard: C++ (`"c++11"` … `"c++26"`), C (`"c90"`, `"c99"`, `"c11"`, `"c17"`, `"c23"`), or a list for a mixed project, e.g. `lang=["c11", "c++17"]`. A C-only project enables only the C language, and `compiler == ...` conditions then test the C compiler.
- `import_std` (keyword, optional): `import_std=True` enables `import std;` for C++ modules
- `output_dir` (keyword, optional): folder inside the build directory that receives built executables and libraries
- `presets` (keyword, optional): `presets=True` also writes a `CMakePresets.json`
- The root file also sets `CMAKE_BUILD_TYPE` to `Release` when none is given (single-config generators) and turns on `compile_commands.json`.

---

## Imports

### Package Imports

External packages (things that map to CMake's `find_package`) are imported at the top of the file:

```python
from packages import Boost, fmt, OpenGL, Threads
```

- Each imported name maps to a `find_package()` call in the generated root `CMakeLists.txt`
- Sub-components are accessed with dot notation: `Boost.Filesystem`, `Boost.System`
  - These map to CMake's `Boost::filesystem`, `Boost::system` etc.
- Imported packages can be used in target dependency lists and inside `configure()` via `self.link`

#### Versions, optional and config packages

A package can carry settings in parentheses:

```python
from packages import Boost(1.78), OpenSSL("3.0"), Qt6(6.5, config=True), Vulkan(optional=True)
```

| Setting | Meaning | CMake |
|---|---|---|
| `Name(1.78)` | Minimum version (numbers separated by dots, or a string) | `find_package(Name 1.78 REQUIRED)` |
| `Name(optional=True)` | The build works without it; test it with `if Name:` | `find_package(Name)` and `if(Name_FOUND)` |
| `Name(config=True)` | Only look for the package's own config file | `find_package(Name REQUIRED CONFIG)` |

Settings combine: `Qt6(6.5, config=True)`. An optional package is a condition in `configure()`:

```python
    def configure(self):
        self.sources = ["main.cpp"]
        if Vulkan:
            self.definitions["HAS_VULKAN"] = 1
            self.link = [Vulkan.Vulkan]
```

Do not list an optional package in the target's dependency parentheses; link it inside the `if`.

Together with `if <option>` (see [conditional imports](#conditional-imports)) a whole import can depend on an option.

### Environment Variable Imports

Environment variables can be imported for use in paths:

```python
from env import VULKAN_SDK, QT_DIR
```

- Imported env vars become usable as expressions that resolve to `$ENV{NAME}` in CMake
- Supports string concatenation with `+`: `VULKAN_SDK + "/Include"` → `"$ENV{VULKAN_SDK}/Include"`
- Commonly used with `self.includes`, `self.link_dirs`, and `self.copy_files`

### GitHub Imports (FetchContent)

Dependencies can be fetched directly from GitHub:

```python
from github import "fmtlib/fmt" as fmt, tag="10.2.1"
from github import "gabime/spdlog" as spdlog
```

- Generates `FetchContent_Declare` + `FetchContent_MakeAvailable` in root CMakeLists.txt
- The `as` name is used as the target name in dependency lists
- Optional `tag=` specifies a Git tag (branch, tag, or commit); tags and branches are cloned shallowly
- Optional `options={...}` sets CMake cache variables **before** the project is added, to switch off its tests, docs and examples
- Fetched targets use bare names (not `Package::Package`); projects that define several targets are reached with a component, exactly like packages: `GTest.gtest_main` becomes `GTest::gtest_main`

```python
from github import "google/googletest" as GTest, tag="v1.15.2", options={"INSTALL_GTEST": False}
from github import "fmtlib/fmt" as fmt, tag="10.2.1", options={"FMT_TEST": False, "FMT_DOC": False}

@Executable("tests", test=True)
target my_tests(PRIVATE GTest.gtest_main, PRIVATE fmt):
    ...
```

Option values are bools (`ON`/`OFF`), numbers or strings.

#### Conditional imports

Any `packages` or `github` import can end with `if <bool option>`. The dependency is only fetched when the option is on, so link it inside `if use_fmt:` rather than in the target's parentheses (pyke warns when you don't):

```python
option use_fmt: bool = True
from github import "fmtlib/fmt" as fmt, tag="10.2.1" if use_fmt
from packages import Boost if use_boost
```

### Vendored Dependencies

Code that is already in your repository and has its own `CMakeLists.txt` (a `third_party/lz4` checkout, a git submodule) is imported with `vendor`:

```python
from vendor import "third_party/lz4" as lz4, options={"LZ4_BUILD_CLI": False}
```

- Generates `add_subdirectory(third_party/lz4)` in the root `CMakeLists.txt`; nothing is downloaded
- The folder is relative to the output directory, like target paths, and must not leave the project
- The `as` name is the CMake target to link (`lz4`); use `Name.component` when the folder defines several
- `options={...}` and `if <option>` work exactly as for `github` imports

---

## Options

```python
option use_opengl: bool = True
option install_prefix: path = "/usr/local"
```

- Declares user-configurable values (maps to CMake `option()` / `set(... CACHE ...)`)
- Available as variables inside any `configure()` method
- Types: `bool`, `str`, `path`

---

## Targets

### Declaration Syntax

```python
@Decorator
target Name(dependencies):
    def configure(self):
        ...
    def install(self):  # optional
        ...
```

### Target Types (Decorators)

| Decorator | CMake Equivalent |
|---|---|
| `@Executable` | `add_executable()` |
| `@SharedLibrary` | `add_library(... SHARED)` |
| `@StaticLibrary` | `add_library(... STATIC)` |
| `@HeaderOnly` | `add_library(... INTERFACE)` |

### Decorator Arguments

The decorator accepts optional arguments for path and IDE settings:

```python
@SharedLibrary                                    # default path = target name, source_groups on
@SharedLibrary("libs/core")                       # positional path
@SharedLibrary(path="libs/core")                  # named path
@Executable(source_groups=False)                  # disable VS source filters
@SharedLibrary("libs/core", source_groups=False)  # both
```

| Argument | Type | Default | Description |
|---|---|---|---|
| path (positional or named) | string | target name | Output directory for CMakeLists.txt |
| source_groups | bool | True | Generate `source_group(TREE ...)` for IDE folder structure |
| copy_dlls | bool | True for Executable, False otherwise | Copy runtime DLLs next to the executable after the build (Windows only) |
| test | bool | False | Register as CTest test (`enable_testing()` + `add_test()`) |
| unity_build | bool | False | Enable CMake unity (jumbo) builds for the target |

### Target Path (Output Location)

- `@SharedLibrary` - default path is the target name (e.g. target `Core` generates `Core/CMakeLists.txt`)
- `@SharedLibrary("libs/core")` - explicit path override, generates `libs/core/CMakeLists.txt`

The **root `CMakeLists.txt`** is always generated and contains:
- `project()` declaration
- `find_package()` calls for all imports
- `add_subdirectory()` calls for all targets

By default every target gets its own subdirectory, which is why `src/`, `app/` and `tests/` work as target paths in a conventional layout (sources are then relative to that folder).

For a small project that keeps everything in one folder, use the path `"."`. Those targets are written into the root `CMakeLists.txt` (after the subdirectories), and any number of targets can share it:

```python
@StaticLibrary(".")
target stack():
    def configure(self):
        self.sources = ["stack.c"]
```

### Dependencies (Parentheses)

Dependencies are declared in the target's parentheses, using CMake visibility modifiers:

```python
target Foo(PRIVATE Bar, PUBLIC Baz):    # explicit visibility
target Foo(Bar, PUBLIC Baz):            # PRIVATE is the default
target Foo():                           # no dependencies
```

- `PRIVATE` (default): dependency is used by this target only
- `PUBLIC`: dependency propagates to anything that links this target

Dependencies can be other targets defined in the same file, or imported packages.

### `configure(self)` - Required

**All** target configuration goes inside `configure()`. This is the only place to set sources, includes, definitions, flags, etc.

```python
@SharedLibrary
target Renderer(PRIVATE Core, PUBLIC Boost.System):
    def configure(self):
        self.sources = ["src/*.cpp"]
        self.exports.includes = ["include/renderer/"]

        if use_opengl:
            self.sources += ["src/gl/*.cpp"]
            self.link += [OpenGL]

        if platform == "windows":
            self.definitions["RENDERER_WIN32"] = True

        if compiler == "msvc":
            self.flags += ["/permissive-"]

        if build_type == "debug":
            self.definitions["DEBUG"] = True
```

**All paths (sources, includes) are relative to the target's directory.**

#### Globs and paths relative to the project root

`sources` accepts globs: `*` and `?` match within one folder, and `**` searches subfolders (`"src/**/*.cpp"` becomes `file(GLOB_RECURSE ...)`). Globs use `CONFIGURE_DEPENDS`, so new files are picked up on the next build.

Paths are relative to the target's own folder, which can mean `../` chains. Start a path with `//` to anchor it at the project root instead:

```python
@StaticLibrary("libs/core")
target core():
    def configure(self):
        self.sources = ["//src/core/*.cpp"]      # <root>/src/core/*.cpp
        self.exports.includes = ["//include"]    # <root>/include
```

`//` works in `sources`, `includes`, `link_dirs`, `copy_files`, `pch` and `assets`. It generates `${PROJECT_SOURCE_DIR}/...`.

#### Available `self` attributes

| Attribute | Type | CMake Mapping |
|---|---|---|
| `self.sources` | list[str] | Source files, globs allowed (`*`, `?`, `**`) |
| `self.includes` | list[str] | `target_include_directories(... PRIVATE)` |
| `self.definitions` | dict[str, value] | `target_compile_definitions(... PRIVATE)` |
| `self.flags` | list[str] | `target_compile_options(... PRIVATE)` |
| `self.link` | list[target] | `target_link_libraries(... PRIVATE)` - additional runtime deps |
| `self.link_dirs` | list[str] | `target_link_directories(... PRIVATE)` |
| `self.copy_files` | list[str] | `POST_BUILD copy_if_different` to exe output dir |
| `self.assets` | list[str] | `POST_BUILD copy_directory` of folders (relative to the target) next to the binary |
| `self.pch` | list[str] | `target_precompile_headers(... PRIVATE)` |
| `self.commands` | list[tuple] | `add_custom_command`: `("command", [outputs], [depends])`; the outputs are added to the target's sources |
| `self.output_name` | str | `OUTPUT_NAME` (file name without prefix/extension) |
| `self.version` / `self.soversion` | str | `VERSION` / `SOVERSION` (shared libraries) |
| `self.features` | list[str] | `target_compile_features(... PRIVATE)`, e.g. `["cxx_std_17"]` |
| `self.warnings` | str | `"none"`, `"default"`, `"all"`, `"strict"`: see below |
| `self.warnings_as_errors` | bool | `/WX` or `-Werror` |
| `self.sanitize` | list[str] | `"address"`, `"undefined"`, `"thread"`, `"leak"` |
| `self.lto` | bool | Link-time optimization (checked with `check_ipo_supported`; warns if unavailable) |
| `self.modules` | list[str] | C++ module files, private to the target: see [C++ modules](#c-modules) |
| `self.cmake` | list[str] | Raw CMake lines, copied verbatim |
| `self.exports.includes` | list[str] | `target_include_directories(... PUBLIC)` |
| `self.exports.definitions` | dict[str, value] | `target_compile_definitions(... PUBLIC)` |
| `self.exports.flags` | list[str] | `target_compile_options(... PUBLIC)` |
| `self.exports.modules` | list[str] | C++ module files that targets linking this one can `import` |

For `@HeaderOnly` targets, all attributes are treated as `INTERFACE` automatically (`warnings`, `warnings_as_errors`, `sanitize`, `lto` do not apply to them).

#### Warnings, sanitizers and LTO

One setting, translated for each compiler family:

| `self.warnings` | MSVC | GCC / Clang / AppleClang |
|---|---|---|
| `"none"` | `/W0` | `-w` |
| `"default"` | (nothing) | (nothing) |
| `"all"` | `/W3` | `-Wall` |
| `"strict"` | `/W4` | `-Wall -Wextra -Wpedantic` |

`warnings_as_errors = True` adds `/WX` or `-Werror`. `sanitize = ["address", "undefined"]` adds `-fsanitize=address,undefined -fno-omit-frame-pointer` (compile and link); MSVC supports `"address"` only (`/fsanitize=address`). `address` and `thread` cannot be combined. These flags are private to the target. To use them only in debug builds:

```python
        if build_type == "debug":
            self.sanitize = ["address", "undefined"]
```

#### C++ modules

List module interface files in `modules` (private to the target) or `exports.modules` (importable by targets that link it), the same way `includes` and `exports.includes` work:

```python
project("Calc", lang="c++23", import_std=True)

@StaticLibrary("math")
target math():
    def configure(self):
        self.sources = ["impl.cpp"]                         # module implementation units and ordinary files
        self.exports.modules = ["math.cppm", "ops/**/*.cppm"]

@Executable("app")
target app(PRIVATE math):
    def configure(self):
        self.sources = ["main.cpp"]                         # contains: import math;
```

- Becomes a `CXX_MODULES` file set (`target_sources(... FILE_SET CXX_MODULES ...)`); CMake scans the sources for `import`/`export module` itself, so linking the library is all a consumer needs.
- Globs (`*`, `**`) and `//` root-anchored paths work. Missing files get a stub (`export module name;`).
- Any use of modules raises the generated `cmake_minimum_required` to 3.28 and requires `lang="c++20"` or newer.
- `import_std=True` in `project(...)` enables `import std;` (needs `lang="c++23"`, CMake 3.30+, Clang 18.1.2+, GCC 15+ or MSVC 19.36+). CMake still treats this as experimental and changes its opt-in key between releases; pyke knows the keys for 3.30 to 4.4, so a newer CMake may need pyke updated. Visual Studio generators cannot build the std module: use Ninja.
- Modules need the Ninja or Visual Studio generators, and GCC 14+, Clang 16+ or MSVC 19.34+. `pyke build` picks Ninja when it is installed and reports an error when only Makefiles are available.
- A target can mix `modules` and `exports.modules` (they become two file sets, the private one named `private_modules`).
- Targets that are installed with `self.export` cannot contain modules yet.

#### Builtin variables available in `configure()`

| Variable | Values | CMake Equivalent |
|---|---|---|
| `platform` | `"windows"`, `"linux"`, `"macos"` | `CMAKE_SYSTEM_NAME` |
| `compiler` | `"msvc"`, `"gcc"`, `"clang"` | `CMAKE_CXX_COMPILER_ID` |
| `build_type` | `"debug"`, `"release"`, `"relwithdebinfo"`, `"minsizerel"` | `CMAKE_BUILD_TYPE` |
| Any `option` | User-defined | Cache variables |

### `install(self)` - Optional

Defines where build artifacts are installed. Only needed if the target should be installable.

```python
@SharedLibrary
target MyLib():
    def configure(self):
        self.sources = ["src/*.cpp"]
        self.exports.includes = ["include/"]

    def install(self):
        self.runtime = "bin"       # DLLs on Windows
        self.library = "lib"       # .so/.a files
        self.headers = ("include/", "include")  # (source, destination)
```

#### Making a library `find_package()`-able

`self.export = "Name"` in `install()` installs the library together with a `NameConfig.cmake`, a version file and the targets, so other CMake projects can write `find_package(Name 1.2)` and link `Name::mylib`:

```python
from packages import ZLIB

@StaticLibrary("mylib/src")
target mylib(PUBLIC ZLIB):
    def configure(self):
        self.sources = ["mylib.cpp"]
        self.exports.includes = ["../include"]       # build-tree path; the installed one is <prefix>/include
    def install(self):
        self.export = "MyLib"
        self.library = "lib"                          # defaults: lib, bin (runtime)
        self.headers = ("../include/", "include")
```

- Several targets can share one package name; the namespace is the package name (`MyLib::`).
- Imported packages that exported targets depend on publicly are re-found by consumers (`find_dependency`).
- Exported targets may only depend on targets exported under the same name. GitHub imports can't be exported (consumers couldn't find them again); a shared library may keep them as `PRIVATE` dependencies.
- Only libraries can be exported.

---

## Complete Example

```python
from packages import Boost, fmt, OpenGL, Threads

project("GameEngine", version="0.1.0", lang="c++20")

option use_opengl: bool = True

@SharedLibrary
target Core():
    def configure(self):
        self.sources = ["src/*.cpp"]
        self.includes = ["src/internal/"]
        self.exports.includes = ["include/"]
        self.exports.definitions = {"CORE_API": 1}

    def install(self):
        self.library = "lib"
        self.headers = ("include/", "include/core")

@SharedLibrary("libs/renderer")
target Renderer(PRIVATE Core, PUBLIC Boost.System):
    def configure(self):
        self.sources = ["src/*.cpp"]
        self.exports.includes = ["include/"]

        if use_opengl:
            self.sources += ["src/gl/*.cpp"]
            self.link += [OpenGL]

        if platform == "windows":
            self.definitions["RENDERER_WIN32"] = True

        self.warnings = "strict"

    def install(self):
        self.library = "lib"
        self.headers = ("include/", "include/renderer")

@Executable
target Game(PRIVATE Core, PRIVATE Renderer, PRIVATE fmt, PRIVATE Threads):
    def configure(self):
        self.sources = ["src/*.cpp"]
        self.warnings = "strict"
        self.warnings_as_errors = True

        if build_type == "debug":
            self.definitions["DEBUG"] = True
```

### Generated Output

```
CMakeLists.txt                    # project(), find_package(), add_subdirectory() calls
Core/CMakeLists.txt               # Core shared library target
libs/renderer/CMakeLists.txt      # Renderer shared library target
Game/CMakeLists.txt               # Game executable target
```

---

## Variables (constants)

Plain assignments at the **top level** of the file define constants you can use anywhere below them:

```python
warnings = ["-Wall", "-Wextra"]
shared_defs = {"APP_VERSION": "1.4.0"}

@Executable
target app():
    def configure(self):
        self.flags += warnings + ["-Werror"]       # lists are joined with +
        self.definitions = shared_defs + {"EXTRA": 1}   # so are dicts
```

- Resolved when `pyke` runs and substituted in place; they never become CMake variables.
- Assigned once: redefining a name is an error. They cannot reuse the name of a builtin (`platform`, `compiler`, `build_type`), option, package, env import, github import or target.
- Define a variable before using it.
- Inside a target, bare names cannot be assigned (`flags = [...]` is an error); use `self.flags`.

---

## f-strings

`f"..."` fills in `{names}` when `pyke` runs. They work in every string position.

```python
version = "1.4.0"
option flavor: str = "plain"
from env import SDK

self.includes = [f"third_party/{self.name}/include"]   # target name
self.definitions["VER"] = f"{version}"                  # constant
self.includes += [f"{SDK}/include"]                     # -> $ENV{SDK}/include
self.cmake += [f"message(STATUS {flavor})"]             # option -> ${flavor}
```

| Write | Result |
|---|---|
| `{name}` (a constant) | its value; lists are joined with spaces, bools become `ON`/`OFF` |
| `{self.name}` | the target's name (only inside a target) |
| `{some_option}` | `${some_option}`, the option's value at configure time |
| `{SOME_ENV_IMPORT}` | `$ENV{SOME_ENV_IMPORT}` |
| `${VAR}`, `$ENV{X}`, `$<...>` | left alone: CMake syntax is never interpolated |
| `{{` and `}}` | a literal `{` and `}` |

An unknown name is an error with a "did you mean" suggestion.

---

## Conditions

```python
if platform == "linux" and not use_wayland:
if compiler != "msvc" and (use_lto or build_type == "release"):
```

`==`, `!=`, `and`, `or`, `not` and parentheses, with Python's precedence (`not` > `and` > `or`). Operands are `platform`, `compiler` and `build_type` compared with string literals, str/path options compared with string literals, or bool options on their own.

---

## Raw CMake

For anything Pyke has no syntax for:

```python
cmake("set(CMAKE_EXPORT_COMPILE_COMMANDS ON)")      # root CMakeLists.txt, before add_subdirectory()

@Executable
target app():
    def configure(self):
        self.sources = ["src/main.cpp"]
        self.cmake += [
            f"set_target_properties({self.name} PROPERTIES WIN32_EXECUTABLE ON)",
        ]
```

Lines are copied verbatim (no validation) in the order written, and respect `if` blocks. Prefer a real attribute when one exists.

---

## Diagnostics

Every stage reports `file:line:col`, the offending line and a caret, and suggests a fix for typos:

```
app.pyke:10:14: error: target 'app': unknown attribute 'sorces' (did you mean 'sources'?)
  10 |         self.sorces = ["a.cpp"]
     |              ^
```

Pyke also checks the files after generating them (warnings, never errors):

- a glob such as `"src/*.cpp"` that matches no file names the pattern and the target;
- source files in a target's own folder that its `sources` do not mention are listed, which catches a forgotten `.cpp`;
- a `vendor` folder without a `CMakeLists.txt` is reported.

---

## Command line

```
pyke <input.pyke> [output_dir]    generate CMake files (default output: current directory)
pyke --clean|--force ...          modifiers for generation, see Regenerating
pyke --validate <input.pyke>      check without generating
pyke --init [directory]           scaffold a .pyke from an existing source tree
pyke --fmt [--check] <files>...   format
pyke build|test [input.pyke]      generate, configure, build (and run tests)
pyke --upgrade <input.pyke>       list GitHub dependencies and their tags
```

### Formatting

`pyke --fmt a.pyke b.pyke` rewrites files in canonical form: 4-space indentation, one space after commas and around `=`, `+=`, `==`, `!=`, `kwarg=value` without spaces, no padding inside brackets, at most one blank line in a row, LF line endings. Comments and strings are never touched. If formatting would change a file's tokens it is left alone and an error is printed.

`pyke --fmt --check a.pyke` changes nothing and exits with status 1 when a file is not formatted, for CI.

### Building

```bash
pyke build                 # the only .pyke file here: generate, configure, build (Release) in ./build
pyke build --debug         # Debug configuration
pyke build app.pyke --target app -j 8
pyke test                  # build, then run ctest
```

`pyke build` and `pyke test` generate next to the `.pyke` file, configure in `build/` (using Ninja when installed), then run `cmake --build` and, for `test`, `ctest --output-on-failure`. They need `cmake` on PATH.

### Regenerating

- Generated CMake files start with `# Generated by pyke from <file> - do not edit`.
- Pyke refuses to overwrite a file it didn't generate (for example a hand-written `CMakeLists.txt`): nothing is written and the files in the way are listed. Move them, pick another output directory, or pass `--force`.
- A file is rewritten only when its content changes (`Unchanged:` otherwise), so regenerating never triggers needless rebuilds.
- `<out>/.pyke/generated` records what was generated. Files from removed or renamed targets are reported as stale; `pyke --clean input.pyke out/` deletes them (only if they still carry the header, so hand-edited files are never removed).

---

## Validation

`pyke` rejects invalid input before writing anything. Errors carry line numbers; warnings never block generation.

**Errors**
- Missing `project(...)`, invalid project name, non-numeric `version`, or `lang` that isn't a C++ (`c++11`–`c++26`) or C (`c90`–`c23`) standard
- Unknown attribute names (e.g. `self.sorces`), attributes in the wrong method (`library` in `configure`), or wrong value types (`sources = 5`)
- Executable/library targets with no sources (`@HeaderOnly` is exempt)
- Target paths that are absolute, escape the output directory (`..`), or collide with another target's directory
- Unknown variables or invalid values in conditions (`platform == "beos"`)
- Option type/default mismatches and duplicate options
- Dependency cycles, including ones created through `self.link` (reported once, with the full chain)
- Bad package versions, unknown warning levels or sanitizers, unknown options in conditional imports, and `vendor` folders outside the project

**Generated-output guarantees**
- `source_group` works for sources outside the target directory (grouped under "External Sources")
- `copy_dlls` and its DLL list are only emitted on Windows and never fail when there are no DLLs to copy
- `build_type` maps to CMake's real config names (`RelWithDebInfo`, `MinSizeRel`) and `compiler == "clang"` also matches AppleClang
- String literals are escaped; nothing is ever written outside the output directory
