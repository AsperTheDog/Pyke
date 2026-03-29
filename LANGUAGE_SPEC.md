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