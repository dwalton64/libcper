/**
 * Describes functions for converting Info-action-ppr sections from binary and JSON format
 * into an intermediate format.
 *
 * Author: Custom Section Generator
 **/
#include <stdio.h>
#include <string.h>
#include <json.h>
#include <libcper/Cper.h>
#include <libcper/sections/cper-section-platform-action-event.h>
#include <libcper/cper-utils.h>
#include <libcper/log.h>
#include <libcper/base64.h>

//FIXME: Change to use the format defined in Cper.h

//Converts the given Info-action-ppr CPER section into JSON IR.
json_object *cper_section_platform_action_event_to_ir(const UINT8 *section, UINT32 size, char **desc_string)
{
	int outstr_len = 0;

	*desc_string = NULL;
	if (size < sizeof(EFI_PLATFORM_ACTION_EVENT)) {
		cper_print_log("Error: Platform Action Event section too small\n");
		return NULL;
	}

	*desc_string = calloc(1, SECTION_DESC_STRING_SIZE);
	if (*desc_string == NULL) {
		cper_print_log(
			"Error: Failed to allocate Platform Action Event desc string\n");
		return NULL;
	}
	outstr_len = snprintf(*desc_string, SECTION_DESC_STRING_SIZE,
			      "A Platform Action Event occurred");
	if (outstr_len < 0) {
		cper_print_log(
			"Error: Could not write to Platform Action Event description string\n");
	} else if (outstr_len > SECTION_DESC_STRING_SIZE) {
		cper_print_log(
			"Error: Platform Action Event description string truncated\n");
	}

	EFI_PLATFORM_ACTION_EVENT *action_event = (EFI_PLATFORM_ACTION_EVENT *)section;
	json_object *section_ir = json_object_new_object();
	
	//UINT8      ValidationBits;
	add_bool(section_ir, "recordIdValid", action_event->ValidationBits & EFI_PLATFORM_ACTION_RECORD_ID_VALID);
	add_bool(section_ir, "sectionIndexValid", action_event->ValidationBits & EFI_PLATFORM_ACTION_CPAD_SECTION_INDEX_VALID);
	add_bool(section_ir, "actionIdValid", action_event->ValidationBits & EFI_PLATFORM_ACTION_ACTION_ID_VALID);
	add_bool(section_ir, "actionReturnCodeValid", action_event->ValidationBits & EFI_PLATFORM_ACTION_ACTION_RETURN_CODE_VALID);
	add_bool(section_ir, "additionalContextValid", action_event->ValidationBits & EFI_PLATFORM_ACTION_ADDITIONAL_CONTEXT_VALID);	

	//UINT8      ActionReturnCode;
	if (action_event->ValidationBits & EFI_PLATFORM_ACTION_ACTION_RETURN_CODE_VALID) {
		add_int_hex_8(section_ir, "actionReturnCode", action_event->ActionReturnCode);
	}
	
	// Add GUIDs from the original CPAD that triggered this event
	add_guid(section_ir, "cpadPlatformID", &action_event->CpadPlatformId);
	add_guid(section_ir, "cpadPartitionID", &action_event->CpadPartitionId);
	add_guid(section_ir, "cpadCreatorID", &action_event->CpadCreatorId);

	//UINT64     CpadRecordId;
	if (action_event->ValidationBits & EFI_PLATFORM_ACTION_RECORD_ID_VALID) {
		add_int_hex_64(section_ir, "cpadRecordId", action_event->CpadRecordId);
	}

	//UINT16     CpadActionId;
	if (action_event->ValidationBits & EFI_PLATFORM_ACTION_ACTION_ID_VALID) {
		add_int_hex_16(section_ir, "cpadActionId", action_event->CpadActionId);
	}

	//UINT32     CpadSectionDescriptorIndex;
	if (action_event->ValidationBits & EFI_PLATFORM_ACTION_CPAD_SECTION_INDEX_VALID) {
		add_uint(section_ir, "cpadSectionIndex", action_event->CpadSectionDescriptorIndex);
	}

	size_t additional_context_size = size - sizeof(EFI_PLATFORM_ACTION_EVENT);

	if (action_event->ValidationBits & EFI_PLATFORM_ACTION_ADDITIONAL_CONTEXT_VALID) {
	   // Base64Encode any remaining data as "additionalContext" in the JSON IR.
	   if (additional_context_size > 0) {
		INT32 encoded_len = 0;
		char *encoded_context = base64_encode((const UINT8*)(action_event) + sizeof(EFI_PLATFORM_ACTION_EVENT), 
									additional_context_size, 
									&encoded_len);

			if (encoded_context == NULL) {
				cper_print_log(
					"Error: Failed to allocate encode output buffer for Platform Action Event additional context. \n");
			} else {
				add_string_len(section_ir, "additionalContext", encoded_context, encoded_len);
				free(encoded_context);
			}
	   }
	} else if (additional_context_size > 0) {
		cper_print_log(
			"Warning: Additional context data present in Platform Action Event, "\
			" but additionalContextValid bit is not set. Additional context data will be ignored.\n");
	}

	return section_ir;
}

//Converts a single Info-action-ppr JSON IR section into CPER binary, outputting to the given stream.
void ir_section_platform_action_event_to_cper(json_object *section, FILE *out)
{
	EFI_PLATFORM_ACTION_EVENT *section_cper = calloc(1, sizeof(EFI_PLATFORM_ACTION_EVENT));
	if (section_cper == NULL) {
		cper_print_log(
			"Error: Failed to allocate memory for Platform Action Event section conversion.\n");
		return;
	}

	section_cper->ValidationBits = 0;
	int additional_context_valid = 0;  // remember if there is additional context after freeing section_cper
	struct json_object *valid_obj;

	if (json_object_object_get_ex(section, "recordIdValid", &valid_obj) &&
	    json_object_get_boolean(valid_obj)) {
		section_cper->ValidationBits |= EFI_PLATFORM_ACTION_RECORD_ID_VALID;	
	}
	if (json_object_object_get_ex(section, "sectionIndexValid", &valid_obj) &&
	    json_object_get_boolean(valid_obj)) {
		section_cper->ValidationBits |= EFI_PLATFORM_ACTION_CPAD_SECTION_INDEX_VALID;
	}
	if (json_object_object_get_ex(section, "actionIdValid", &valid_obj) &&
	    json_object_get_boolean(valid_obj)) {	
		section_cper->ValidationBits |= EFI_PLATFORM_ACTION_ACTION_ID_VALID;
	}
	if (json_object_object_get_ex(section, "actionReturnCodeValid", &valid_obj) &&
	    json_object_get_boolean(valid_obj)) {
		section_cper->ValidationBits |= EFI_PLATFORM_ACTION_ACTION_RETURN_CODE_VALID;
	}
	if (json_object_object_get_ex(section, "additionalContextValid", &valid_obj) &&
	    json_object_get_boolean(valid_obj)) {
		section_cper->ValidationBits |= EFI_PLATFORM_ACTION_ADDITIONAL_CONTEXT_VALID;
		additional_context_valid = 1;
	}

	// Action Return Code
	get_value_hex_8(section, "actionReturnCode", &section_cper->ActionReturnCode);

	// Read GUIDs from the JSON IR and populate the CPER structure
	string_to_guid(&section_cper->CpadPlatformId, json_object_get_string(json_object_object_get(section, "cpadPlatformID")));
	string_to_guid(&section_cper->CpadPartitionId, json_object_get_string(json_object_object_get(section, "cpadPartitionID")));
	string_to_guid(&section_cper->CpadCreatorId, json_object_get_string(json_object_object_get(section, "cpadCreatorID")));

	// CPAD Record ID
	get_value_hex_64(section, "cpadRecordId", &section_cper->CpadRecordId);

	// CPAD Action ID
	get_value_hex_16(section, "cpadActionId", &section_cper->CpadActionId);

	// CPAD Section Descriptor Index
	struct json_object *cpad_section_index_obj;
	if (json_object_object_get_ex(section, "cpadSectionIndex", &cpad_section_index_obj)) {
		section_cper->CpadSectionDescriptorIndex = (UINT32)json_object_get_uint64(cpad_section_index_obj);
	}
	
	// Write the fixed-size portion of the structure to the output stream.
	fwrite(section_cper, sizeof(EFI_PLATFORM_ACTION_EVENT), 1, out);
	fflush(out);

	// Free the allocated structure now that it's written to the stream.
	free(section_cper);

	// If the JSON IR indicates that additional context is valid, read the base64-encoded additional context and write it to the stream after decoding.
	if (additional_context_valid) {
		struct json_object *additional_context_obj;
		if (json_object_object_get_ex(section, "additionalContext", &additional_context_obj)) {
			const char *encoded_context = json_object_get_string(additional_context_obj);
			if (encoded_context == NULL) {
				cper_print_log(
					"Error: Additional context valid bit is set, but no additionalContext field found in JSON IR.\n");
				return;
			}
			INT32 decoded_len = 0;
			UINT8 *decoded_context = base64_decode(encoded_context, strlen(encoded_context), &decoded_len);
			if (decoded_context == NULL) {
				cper_print_log(
					"Error: Failed to allocate decode buffer for Platform Action Event additional context. \n");
			} else {
				fwrite(decoded_context, decoded_len, 1, out);
				fflush(out);
				free(decoded_context);
			}
		}
	}

}
