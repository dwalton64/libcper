#ifndef CPER_IR_TEST_UTILS_H
#define CPER_IR_TEST_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <libcper/BaseTypes.h>
#include <libcper/generator/sections/gen-section.h>
#include <json.h>

// Controls whether required properties are added to the majority of property
// definitions.  This is useful for unit tests that are validating JSON where
// all fields are valid
enum AddRequiredProps { AddRequired, NoModify };

int schema_validate_from_file(json_object *to_test, int single_section,
			      int all_valid_bits);

int schema_validate_cpad_from_file(json_object *to_test, int single_section,
				   int all_valid_bits);

#ifdef __cplusplus
}
#endif

#endif
