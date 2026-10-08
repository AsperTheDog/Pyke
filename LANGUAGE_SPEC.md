# Pyke Language Design Specification v0.2

## Overview

Pyke is a configuration language that compiles to CMake. It uses Python-like syntax tailored to build system concepts, making CMake's capabilities intuitive for Python programmers. The compiler is written in C++.

---

## File Structure

A `.pyke` file has four sections, in this order:

```python
# 1. Imports (external packages and environment variables)
from packages import Boost, fmt, Threads
from env import VULKAN_SDK

# 2. Project declaration
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

- `name` (positional): Project name string
- `version` (keyword): Semantic version string
- `lang` (keyword): Language standard (e.g. `"c++17"`, `"c++20"`)

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
- Optional `tag=` specifies a Git tag (branch, tag, or commit)
- Fetched targets use bare names (not `Package::Package`)

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
| copy_dlls | bool | True for Executable, False otherwise | Copy runtime DLLs to exe output dir (POST_BUILD) |
| test | bool | False | Register as CTest test (`enable_testing()` + `add_test()`) |

### Target Path (Output Location)

- `@SharedLibrary` - default path is the target name (e.g. target `Core` generates `Core/CMakeLists.txt`)
- `@SharedLibrary("libs/core")` - explicit path override, generates `libs/core/CMakeLists.txt`

The **root `CMakeLists.txt`** is always generated and contains:
- `project()` declaration
- `find_package()` calls for all imports
- `add_subdirectory()` calls for all targets

**No targets are ever placed in the root `CMakeLists.txt`.** Every target gets its own subdirectory.

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
            self.flags += ["/W4"]
        else:
            self.flags += ["-Wall", "-Wextra"]

        if build_type == "debug":
            self.definitions["DEBUG"] = True
```

**All paths (sources, includes) are relative to the target's directory.**

#### Available `self` attributes

| Attribute | Type | CMake Mapping |
|---|---|---|
| `self.sources` | list[str] | Source files, globs allowed |
| `self.includes` | list[str] | `target_include_directories(... PRIVATE)` |
| `self.definitions` | dict[str, value] | `target_compile_definitions(... PRIVATE)` |
| `self.flags` | list[str] | `target_compile_options(... PRIVATE)` |
| `self.link` | list[target] | `target_link_libraries(... PRIVATE)` - additional runtime deps |
| `self.link_dirs` | list[str] | `target_link_directories(... PRIVATE)` |
| `self.copy_files` | list[str] | `POST_BUILD copy_if_different` to exe output dir |
| `self.cmake` | list[str] | Raw CMake lines, copied verbatim |
| `self.exports.includes` | list[str] | `target_include_directories(... PUBLIC)` |
| `self.exports.definitions` | dict[str, value] | `target_compile_definitions(... PUBLIC)` |
| `self.exports.flags` | list[str] | `target_compile_options(... PUBLIC)` |

For `@HeaderOnly` targets, all attributes are treated as `INTERFACE` automatically.

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

        if compiler == "msvc":
            self.flags += ["/W4"]
        else:
            self.flags += ["-Wall", "-Wextra"]

    def install(self):
        self.library = "lib"
        self.headers = ("include/", "include/renderer")

@Executable
target Game(PRIVATE Core, PRIVATE Renderer, PRIVATE fmt, PRIVATE Threads):
    def configure(self):
        self.sources = ["src/*.cpp"]

        if compiler == "msvc":
            self.flags += ["/W4", "/WX"]
        else:
            self.flags += ["-Wall", "-Wextra", "-Werror"]

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

---

## Regenerating

- Generated CMake files start with `# Generated by pyke from <file> - do not edit`.
- A file is rewritten only when its content changes (`Unchanged:` otherwise), so regenerating never triggers needless rebuilds.
- `<out>/.pyke/generated` records what was generated. Files from removed or renamed targets are reported as stale; `pyke --clean input.pyke out/` deletes them (only if they still carry the header, so hand-edited files are never removed).

---

## Validation

`pyke` rejects invalid input before writing anything. Errors carry line numbers; warnings never block generation.

**Errors**
- Missing `project(...)`, invalid project name, non-numeric `version`, or `lang` outside `c++11`–`c++26`
- Unknown attribute names (e.g. `self.sorces`), attributes in the wrong method (`library` in `configure`), or wrong value types (`sources = 5`)
- Executable/library targets with no sources (`@HeaderOnly` is exempt)
- Target paths that are absolute, escape the output directory (`..`), or collide with another target's directory
- Unknown variables or invalid values in conditions (`platform == "beos"`)
- Option type/default mismatches and duplicate options
- Dependency cycles, including ones created through `self.link` (reported once, with the full chain)


**Generated-output guarantees**
- `source_group` works for sources outside the target directory (grouped under "External Sources")
- `copy_dlls` is only emitted on Windows, so Linux/macOS builds don't break
- `build_type` maps to CMake's real config names (`RelWithDebInfo`, `MinSizeRel`) and `compiler == "clang"` also matches AppleClang
- String literals are escaped; nothing is ever written outside the output directory
