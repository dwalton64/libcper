# Specification

`libcper` converts records to and from a JSON intermediate representation (IR)
whose shape is defined by [JSON Schema](https://json-schema.org/) documents
under `specification/json/`.

## Schemas

| Schema                              | Describes                                  |
| ----------------------------------- | ------------------------------------------ |
| `cper-json-full-log.json`           | A complete CPER record (header + sections) |
| `cper-json-section-log.json`        | A single CPER section (descriptor + body)  |
| `cper-json-header.json`             | The CPER record header                     |
| `cper-json-section-descriptor.json` | A CPER section descriptor                  |
| `cpad-json-full-log.json`           | A complete CPAD record                     |
| `cpad-json-section-log.json`        | A single CPAD section                      |
| `cpad-json-header.json`             | The CPAD record header                     |
| `cpad-json-section-descriptor.json` | A CPAD section descriptor                  |
| `sections/`                         | Per-section-type body schemas              |

These schemas are used by the test suites to validate generated IR (see
[testing.md](testing.md)).

## Source specifications

- **CPER** binary format:
  [UEFI Specification Appendix N](https://uefi.org/specs/UEFI/2.9_A/Apx_N_Common_Platform_Error_Record.html).
- **CPAD** and Platform Action Events: the OCP RAS API specification, available
  from the
  [Open Compute Project](https://www.opencompute.org/contributions?contributions%5Bquery%5D=RAS%20API).
