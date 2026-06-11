# CPER / CPAD generators

The generator libraries (`libcper-generate`, `libcpad-generate`) and the
`cper-generate` / `cpad-generate` tools produce pseudo-random, spec-compliant
records for testing.

- For command-line usage, see [../docs/cli.md](../docs/cli.md).
- For adding OEM/vendor section generators, see
  [../docs/extending.md](../docs/extending.md).

## Caveats

The generators are intentionally not fully random, to keep testing
deterministic and round-trippable:

- Validation bits for optional fields are set so that a binary -> JSON ->
  binary round trip reproduces the original bytes, rather than dropping
  fields. The `cper-generate --valid-bits` option (`all`, `some`, `random`)
  controls this.
- Sections defined by external specifications (outside UEFI Appendix N) are
  often emitted as representative random bytes rather than fully modelled
  structures.
