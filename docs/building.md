# Building

## Prerequisites

- [Meson](https://mesonbuild.com/) >= 1.2.0
- [Ninja](https://ninja-build.org/)
- A C compiler supporting C18 (GCC or Clang)
- [json-c](https://github.com/json-c/json-c) (resolved as a Meson
  subproject if not found on the system)
- For the fuzzer: Clang (libFuzzer + AddressSanitizer)
- For the Python extension: a Python 3 installation with development
  headers

## Building

```sh
meson setup build
ninja -C build
```

## Build options

Options are defined in `meson.options` and set with `-D<option>=<value>` at
`meson setup` time (or changed later with `meson configure build
-D<option>=<value>`).

| Option                  | Type    | Default    | Description                                |
| ----------------------- | ------- | ---------- | ------------------------------------------ |
| `tests`                 | feature | `enabled`  | Build the unit test executables            |
| `fuzz`                  | feature | `enabled`  | Build the libFuzzer target (Clang only)    |
| `utility`               | feature | `enabled`  | Build the command-line tools               |
| `python`                | feature | `disabled` | Build the `cper` Python extension          |
| `install`               | feature | `enabled`  | Install libraries, headers, and tools      |
| `pkgconfig`             | feature | `enabled`  | Install a `libcper` pkg-config file        |
| `output-all-properties` | feature | `disabled` | Force-enable all IR properties (debugging) |

Note that `fuzz` only produces a target when the compiler is Clang; under GCC
it is silently skipped. To build the fuzzer:

```sh
CC=clang meson setup build-fuzz -Dfuzz=enabled
ninja -C build-fuzz
```

## Build outputs

A default build under `build/` produces:

### Libraries

| Artifact               | Description                                    |
| ---------------------- | ---------------------------------------------- |
| `libcper-parse.so*`    | CPER/CPAD decode and encode (the core library) |
| `libcper-generate.so*` | Pseudo-random CPER record generator            |
| `libcpad-generate.so*` | Pseudo-random CPAD record generator            |

Configure with `-Ddefault_library=static` to produce `.a` archives instead
(this is what the Python packaging uses).

### Command-line tools (when `utility` is enabled)

| Binary          | Description                            |
| --------------- | -------------------------------------- |
| `cper-convert`  | Convert CPER between binary and JSON   |
| `cpad-convert`  | Convert CPAD between binary and JSON   |
| `cper-generate` | Generate pseudo-random CPER records    |
| `cpad-generate` | Generate pseudo-random CPAD records    |
| `lscpad`        | List CPAD file contents (text or JSON) |

See [cli.md](cli.md) for usage.

### Python extension (when `python` is enabled)

| Artifact            | Description                                |
| ------------------- | ------------------------------------------ |
| `cper.cpython-*.so` | Python module — see [python.md](python.md) |

### Tests

The unit test executables (`tests/cper-tests`, `tests/cpad-tests`) and, on
Clang fuzz builds, `tests/fuzz_cper_buf_to_ir`. See [testing.md](testing.md).
