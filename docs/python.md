# Python bindings

`libcper` ships an optional CPython extension module named `cper` that exposes
decode and encode for both CPER and CPAD records.

## Enabling the build

The Python extension is off by default. Enable it with the `python` option:

```sh
meson setup build -Dpython=enabled
ninja -C build
```

This produces `cper.cpython-*.so` in the build directory. To use it directly
from a build tree you must put both the module and the shared library on the
relevant paths:

```sh
PYTHONPATH=build LD_LIBRARY_PATH=build python3 -c "import cper; print(dir(cper))"
```

Alternatively, build and install a wheel with the bundled
[meson-python](https://meson-python.readthedocs.io/) backend (see
`pyproject.toml`):

```sh
pip install .
```

## API

The module provides four functions:

| Function                          | Description                               |
| --------------------------------- | ----------------------------------------- |
| `parse(data: bytes) -> dict`      | Decode a CPER binary record to an IR dict |
| `parse_cpad(data: bytes) -> dict` | Decode a CPAD binary record to an IR dict |
| `to_cper(ir: dict) -> bytes`      | Encode a CPER IR dict to a binary record  |
| `to_cpad(ir: dict) -> bytes`      | Encode a CPAD IR dict to a binary record  |

The IR dictionaries are the same structure described in
[concepts.md](concepts.md) and the JSON schemas. Invalid input raises
`ValueError`.

## Example

```python
import cper

# Decode a CPER binary record into a dict.
with open("sample.cper", "rb") as f:
    record = cper.parse(f.read())

print(record["header"]["severity"])

# Modify and re-encode it.
encoded = cper.to_cper(record)
with open("roundtrip.cper", "wb") as f:
    f.write(encoded)

# CPADs use the parse_cpad / to_cpad pair.
with open("sample.cpad", "rb") as f:
    action = cper.parse_cpad(f.read())
cpad_bytes = cper.to_cpad(action)
```

A runnable sample is provided in `parse_example.py` at the repository root.
The Python tests in `tests/test_pycper.py` show round-trip usage and run as
part of `meson test` when the `python` option is enabled (see
[testing.md](testing.md)).
