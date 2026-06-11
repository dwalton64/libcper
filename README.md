# libcper

`libcper` is part of [OpenBMC](https://github.com/openbmc). It converts UEFI
Common Platform Error Records (CPER, UEFI Specification Appendix N) and Common
Platform Action Descriptors (CPAD, OCP RAS API) between their binary form and a
human-readable JSON intermediate representation (IR).

It provides:

- A C library (`libcper-parse`) that decodes binary CPER/CPAD into JSON IR and
  encodes JSON IR back into binary.
- Generator libraries (`libcper-generate`, `libcpad-generate`) that produce
  pseudo-random, spec-compliant records for testing.
- Command-line tools: `cper-convert`, `cpad-convert`, `cper-generate`,
  `cpad-generate`.
- An optional Python extension exposing decode/encode for both formats.

## Quick start

```sh
meson setup build
ninja -C build

# Generate a sample CPER, decode it to JSON, then re-encode it to binary.
build/cper-generate --out sample.cper --sections generic
build/cper-convert to-json sample.cper --out sample.json
build/cper-convert to-cper sample.json --out roundtrip.cper

# The same round trip for a CPAD (Common Platform Action Descriptor).
build/cpad-generate --out sample.cpad --sections os-generic
build/cpad-convert to-json sample.cpad --out sample.cpad.json
build/cpad-convert to-cpad sample.cpad.json --out roundtrip.cpad
```

See [docs/building.md](docs/building.md) for prerequisites, build options, and
the full list of build outputs.

## Documentation

| Topic                                  | Description                                                |
| -------------------------------------- | ---------------------------------------------------------- |
| [Concepts](docs/concepts.md)           | What CPER and CPAD records are and how the IR maps to them |
| [Building](docs/building.md)           | Prerequisites, build options, and build outputs            |
| [CLI tools](docs/cli.md)               | Using `cper-convert`, `cpad-convert`, and the generators   |
| [Testing](docs/testing.md)             | Running the unit suites and the fuzzer                     |
| [C API](docs/c-api.md)                 | Using the library from C/C++ with an example               |
| [Python](docs/python.md)               | Enabling and using the Python bindings                     |
| [Extending](docs/extending.md)         | Adding OEM/vendor section parsers and generators           |
| [Specification](docs/specification.md) | The CPER-JSON / CPAD-JSON schemas and source specs         |

## License

See [LICENSE](LICENSE). Contributions follow the OpenBMC process; maintainers
are listed in [OWNERS](OWNERS).
