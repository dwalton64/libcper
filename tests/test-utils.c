/**
 * Defines utility functions for testing CPER-JSON IR output from the cper-parse library.
 *
 * Author: Lawrence.Tang@arm.com
 **/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "test-utils.h"

#include <libcper/BaseTypes.h>
#include <libcper/generator/cper-generate.h>

#include <validate.h>
#include <json.h>
#include <libcper/log.h>

// Objects that have mutually exclusive fields (and thereforce can't have both
// required at the same time) can be added to this list.
// Truly optional properties that shouldn't be added to "required" field for
// validating the entire schema with validationbits=1
// In most cases making sure examples set all valid bits is preferable to adding to this list
static const char *optional_props[] = {
	// Some sections don't parse header correctly?
	"header",

	// Each section is optional
	"GenericProcessor", "Ia32x64Processor", "ArmProcessor", "ArmRas",
	"Memory", "Memory2", "Pcie", "PciBus", "PciComponent", "Firmware",
	"GenericDmar", "VtdDmar", "IommuDmar", "CcixPer", "CxlProtocol",
	"CxlComponent", "Nvidia", "Ampere", "PlatformActionEvent", "Unknown",

	// CXL?  might have a bug?
	"partitionID",

	// CXL protocol
	"capabilityStructure", "deviceSerial",

	// CXL component
	"cxlComponentEventLog", "addressSpace", "errorType",
	"participationType", "timedOut", "level", "operation", "preciseIP",
	"restartableIP", "overflow", "uncorrected", "transactionType",

	// PCIe AER
	"addressSpace", "errorType", "participationType", "timedOut", "level",
	"operation", "preciseIP", "restartableIP", "overflow", "uncorrected",
	"transactionType"
};

// Optional / mutually-exclusive properties for CPAD schema validation.
// Only one section body key is present per section, so they cannot all be
// required at once.
static const char *cpad_optional_props[] = {
	"GenericOS",
	"Unknown",
};

//Returns a ready-for-use memory stream containing a CPER record with the given sections inside.
//(Defined in ir-tests.c, which links the CPER generate library.)

//Returns a ready-for-use memory stream containing a CPAD record with the given sections inside.
//(Defined in cpad-ir-tests.c, which links the CPAD generate library.)

int iterate_make_required_props(json_object *jsonSchema, int all_valid_bits,
				const char **opt_props, size_t opt_props_len)
{
	//properties
	json_object *properties =
		json_object_object_get(jsonSchema, "properties");

	if (properties != NULL) {
		json_object *requrired_arr = json_object_new_array();

		json_object_object_foreach(properties, property_name,
					   property_value)
		{
			(void)property_value;
			int add_to_required = 1;
			for (size_t i = 0; i < opt_props_len; i++) {
				if (strcmp(opt_props[i], property_name) == 0) {
					add_to_required = 0;
					break;
				}
			}

			if (add_to_required) {
				//Add to list if property is not optional
				json_object_array_add(
					requrired_arr,
					json_object_new_string(property_name));
			}
		}

		json_object_object_foreach(properties, property_name2,
					   property_value2)
		{
			(void)property_name2;
			if (iterate_make_required_props(
				    property_value2, all_valid_bits, opt_props,
				    opt_props_len) < 0) {
				json_object_put(requrired_arr);
				return -1;
			}
		}

		if (all_valid_bits) {
			json_object_object_add(jsonSchema, "required",
					       requrired_arr);
		} else {
			json_object_put(requrired_arr);
		}
	}

	// ref
	json_object *ref = json_object_object_get(jsonSchema, "$ref");
	if (ref != NULL) {
		const char *ref_str = json_object_get_string(ref);
		if (ref_str != NULL) {
			if (strlen(ref_str) < 1) {
				cper_print_log("Failed seek filepath: %s\n",
					       ref_str);
				return -1;
			}
			size_t size =
				strlen(LIBCPER_JSON_SPEC) + strlen(ref_str);
			char *path = (char *)malloc(size);
			int n = snprintf(path, size, "%s%s", LIBCPER_JSON_SPEC,
					 ref_str + 1);
			if (n != (int)size - 1) {
				cper_print_log("Failed concat filepath: %s\n",
					       ref_str);
				free(path);
				return -1;
			}
			json_object *ref_obj = json_object_from_file(path);
			free(path);
			if (ref_obj == NULL) {
				cper_print_log("Failed to parse file: %s\n",
					       ref_str);
				return -1;
			}

			if (iterate_make_required_props(ref_obj, all_valid_bits,
							opt_props,
							opt_props_len) < 0) {
				json_object_put(ref_obj);
				return -1;
			}

			json_object_object_foreach(ref_obj, key, val)
			{
				// Use json_object_get to increment ref count properly
				json_object *val_copy = json_object_get(val);
				json_object_object_add(jsonSchema, key,
						       val_copy);
			}
			json_object_object_del(jsonSchema, "$ref");

			json_object_put(ref_obj);
		}
	}

	//oneOf
	const json_object *oneOf = json_object_object_get(jsonSchema, "oneOf");
	if (oneOf != NULL) {
		size_t num_elements = json_object_array_length(oneOf);

		for (size_t i = 0; i < num_elements; i++) {
			json_object *obj = json_object_array_get_idx(oneOf, i);
			if (iterate_make_required_props(obj, all_valid_bits,
							opt_props,
							opt_props_len) < 0) {
				return -1;
			}
		}
	}

	//items
	const json_object *items = json_object_object_get(jsonSchema, "items");
	if (items != NULL) {
		json_object_object_foreach(items, key, val)
		{
			(void)key;
			if (iterate_make_required_props(val, all_valid_bits,
							opt_props,
							opt_props_len) < 0) {
				return -1;
			}
		}
	}

	return 1;
}

static int schema_validate_with(const char *schema_file, json_object *to_test,
				int all_valid_bits, const char **opt_props,
				size_t opt_props_len)
{
	printf("start schema_validate_from_file\n");
	int size = strlen(schema_file) + 1 + strlen(LIBCPER_JSON_SPEC) + 1;
	char *schema_path = malloc(size);
	snprintf(schema_path, size, "%s/%s", LIBCPER_JSON_SPEC, schema_file);

	json_object *schema = json_object_from_file(schema_path);

	if (schema == NULL) {
		cper_print_log("Could not parse schema file: %s", schema_path);
		free(schema_path);
		return 0;
	}
	printf("end iterate_make_required_props\n");
	if (iterate_make_required_props(schema, all_valid_bits, opt_props,
					opt_props_len) < 0) {
		cper_print_log("Failed to make required props\n");
		json_object_put(schema);
		free(schema_path);
		return -1;
	}
	printf("start schemavalidator_validate\n");
	int err = schemavalidator_validate(to_test, schema);
	if (err == SCHEMAVALIDATOR_ERR_VALID) {
		cper_print_log("validation ok\n");
		json_object_put(schema);
		free(schema_path);
		return 1;
	}
	printf("end schemavalidator_validate\n");
	cper_print_log("validate failed %d: %s\n", err,
		       schemavalidator_errorstr(err));

	json_object_put(schema);
	free(schema_path);
	return 0;
}

int schema_validate_from_file(json_object *to_test, int single_section,
			      int all_valid_bits)
{
	const char *schema_file;
	if (single_section) {
		schema_file = "cper-json-section-log.json";
	} else {
		schema_file = "cper-json-full-log.json";
	}
	return schema_validate_with(
		schema_file, to_test, all_valid_bits, optional_props,
		sizeof(optional_props) / sizeof(optional_props[0]));
}

int schema_validate_cpad_from_file(json_object *to_test, int single_section,
				   int all_valid_bits)
{
	const char *schema_file;
	if (single_section) {
		schema_file = "cpad-json-section-log.json";
	} else {
		schema_file = "cpad-json-full-log.json";
	}
	return schema_validate_with(
		schema_file, to_test, all_valid_bits, cpad_optional_props,
		sizeof(cpad_optional_props) / sizeof(cpad_optional_props[0]));
}
