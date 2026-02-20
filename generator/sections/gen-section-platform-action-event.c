/**
 * Functions for generating pseudo-random CPER platform-action-event error sections.
 *
 * Author: drewwalton@microsoft.com
 **/

#include <stdlib.h>
#include <libcper/BaseTypes.h>
#include <libcper/generator/gen-utils.h>
#include <libcper/generator/sections/gen-section.h>
#include <string.h>
#include <stdio.h>


//FIXME: Update to use the format defined in Cper.h

//Generates a single pseudo-random platform-action-event error section, saving the resulting address to the given
//location. Returns the size of the newly created section.
size_t generate_section_platform_action_event(void **location,
				 GEN_VALID_BITS_TEST_TYPE validBitsType)
{
	UINT32 additional_context_size = 0;
	UINT32 total_section_size = 0;  // EFI_PLATFORM_ACTION_EVENT struct size + additional context size if valid
	UINT8 section_validation_bits = 0;
	// Set valid bits according to the provided validBitsType.
	if (validBitsType == SOME_VALID) {
		// Set some valid bits (for example, let's set the first and third bits).
		// You can adjust which bits to set based on your needs.
	} else if (validBitsType == ALL_VALID) {
		section_validation_bits = EFI_PLATFORM_ACTION_VALID_MASK;
	} else if (validBitsType == RANDOM_VALID) {
		section_validation_bits = cper_rand() & EFI_PLATFORM_ACTION_VALID_MASK;
	} else if (validBitsType == SOME_VALID) {
		// This event can still be useful with no additional context
		section_validation_bits = EFI_PLATFORM_ACTION_ACTION_RETURN_CODE_VALID | 
								   EFI_PLATFORM_ACTION_CPAD_SECTION_INDEX_VALID | 
								   EFI_PLATFORM_ACTION_RECORD_ID_VALID | 
									   EFI_PLATFORM_ACTION_ACTION_ID_VALID;
	}
	
	printf("Generating Platform Action Event section with validation bits: 0x%02X\n", section_validation_bits);

	if (section_validation_bits & EFI_PLATFORM_ACTION_ADDITIONAL_CONTEXT_VALID) {
		// Determine how much additional context to include
		additional_context_size = cper_rand() % 1024; // between 0 and 1024 bytes
		printf("Including additional context of size %u bytes in generated Platform Action Event section.\n", additional_context_size);
	}
	total_section_size = sizeof(EFI_PLATFORM_ACTION_EVENT) + additional_context_size;
		
	EFI_PLATFORM_ACTION_EVENT *section_cper = calloc(1, total_section_size);
	if (section_cper == NULL) {
		printf(
			"Error: Failed to allocate memory for Platform Action Event section generation.\n");
		return 0;
	}

	section_cper->ValidationBits = section_validation_bits;
	section_cper->ActionReturnCode = (UINT8)cper_rand();

	// Create Random GUIDs for the CPAD that triggered this event
	section_cper->CpadPlatformId = generate_random_guid();
	section_cper->CpadPartitionId = generate_random_guid();
	section_cper->CpadCreatorId = generate_random_guid();

	section_cper->CpadRecordId = cper_rand64();
	section_cper->CpadActionId = (UINT16)cper_rand();
	section_cper->CpadSectionDescriptorIndex = cper_rand();
	
	// If additional context is valid, fill the additional context area with random bytes.
	if (additional_context_size > 0) {
		memcpy((char *)section_cper + sizeof(EFI_PLATFORM_ACTION_EVENT), generate_random_bytes(additional_context_size), additional_context_size);
	}
	
	*location = section_cper;
	return total_section_size;
}
