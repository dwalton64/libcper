/**
 * Builds Platform Action Event CPER records from CPADs.
 *
 * Author: drewwalton@microsoft.com
 **/

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <libcper/Cper.h>
#include <libcper/Cpad.h>
#include <libcper/cper-utils.h>
#include <libcper/cpad-parse.h>
#include <libcper/platform-action-cper.h>

//Fills an EFI_ERROR_TIME_STAMP with the current UTC time, marked precise (it
//is the actual time the event was created).
static void fill_current_timestamp(EFI_ERROR_TIME_STAMP *timestamp)
{
	time_t now = time(NULL);
	struct tm utc;
	gmtime_r(&now, &utc);

	char iso[96];
	snprintf(iso, sizeof(iso), "%04d-%02d-%02dT%02d:%02d:%02d+00:00",
		 utc.tm_year + 1900, utc.tm_mon + 1, utc.tm_mday, utc.tm_hour,
		 utc.tm_min, utc.tm_sec);
	string_to_timestamp(timestamp, iso);
	timestamp->Flag = EFI_ERROR_TIME_STAMP_PRECISE;
}

//Returns a unique-ish record id for a newly created CPER (high-resolution
//creation time).
static UINT64 generate_record_id(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_REALTIME, &ts) != 0) {
		return (UINT64)time(NULL);
	}
	return (UINT64)ts.tv_sec * 1000000000ULL + (UINT64)ts.tv_nsec;
}

int cpad_to_platform_action_event_cper(
	const unsigned char *cpad_buf, size_t cpad_size,
	const PLATFORM_ACTION_EVENT_REQUEST *requests, UINT16 num_requests,
	FILE *out)
{
	if (cpad_buf == NULL || requests == NULL || out == NULL ||
	    num_requests == 0) {
		return -1;
	}
	if (!cpad_header_valid((const char *)cpad_buf, cpad_size)) {
		return -1;
	}

	const CPAD_HEADER *cpad = (const CPAD_HEADER *)cpad_buf;
	UINT16 cpad_section_count = cpad->SectionCount;

	//Validate every requested section index is in range and its descriptor
	//fits within the CPAD buffer.
	for (UINT16 i = 0; i < num_requests; i++) {
		UINT32 index = requests[i].CpadSectionIndex;
		if (index >= cpad_section_count) {
			return -1;
		}
		size_t offset = sizeof(CPAD_HEADER) +
				(size_t)index * sizeof(CPAD_SECTION_DESCRIPTOR);
		if (offset + sizeof(CPAD_SECTION_DESCRIPTOR) > cpad_size) {
			return -1;
		}
	}

	const size_t header_size = sizeof(EFI_COMMON_ERROR_RECORD_HEADER);
	const size_t descriptor_size = sizeof(EFI_ERROR_SECTION_DESCRIPTOR);
	const size_t body_size = sizeof(EFI_PLATFORM_ACTION_EVENT);
	const size_t section_data_offset =
		header_size + (size_t)num_requests * descriptor_size;
	const size_t total_size =
		section_data_offset + (size_t)num_requests * body_size;

	//Build and write the record header.
	EFI_COMMON_ERROR_RECORD_HEADER header;
	memset(&header, 0, sizeof(header));
	header.SignatureStart = EFI_ERROR_RECORD_SIGNATURE_START;
	header.Revision = EFI_ERROR_RECORD_REVISION;
	header.SignatureEnd = EFI_ERROR_RECORD_SIGNATURE_END;
	header.SectionCount = num_requests;
	header.ErrorSeverity = EFI_GENERIC_ERROR_PLATFORM_ACTION_EVENT;
	header.ValidationBits =
		(1u << EFI_ERROR_RECORD_HEADER_PLATFORM_ID_VALID) |
		(1u << EFI_ERROR_RECORD_HEADER_TIME_STAMP_VALID) |
		(1u << EFI_ERROR_RECORD_HEADER_PARTITION_ID_VALID);
	header.RecordLength = (UINT32)total_size;
	fill_current_timestamp(&header.TimeStamp);
	header.PlatformID = cpad->PlatformID;
	header.PartitionID = cpad->PartitionID;
	header.CreatorID = cpad->CreatorID;
	//NotificationType left zero by memset.
	header.RecordID = generate_record_id();
	//Flags, PersistenceInfo and Resv1 left zero by memset.

	if (fwrite(&header, sizeof(header), 1, out) != 1) {
		return -1;
	}

	//Write the section descriptors.
	for (UINT16 i = 0; i < num_requests; i++) {
		const CPAD_SECTION_DESCRIPTOR *cpad_descriptor =
			(const CPAD_SECTION_DESCRIPTOR
				 *)(cpad_buf + sizeof(CPAD_HEADER) +
				    (size_t)requests[i].CpadSectionIndex *
					    sizeof(CPAD_SECTION_DESCRIPTOR));

		EFI_ERROR_SECTION_DESCRIPTOR descriptor;
		memset(&descriptor, 0, sizeof(descriptor));
		descriptor.SectionOffset =
			(UINT32)(section_data_offset + (size_t)i * body_size);
		descriptor.SectionLength = (UINT32)body_size;
		descriptor.Revision = EFI_ERROR_SECTION_REVISION;
		descriptor.SecValidMask =
			(1u << EFI_ERROR_SECTION_FRU_ID_VALID_BIT) |
			(1u << EFI_ERROR_SECTION_FRU_STRING_VALID_BIT);
		descriptor.SectionFlags =
			(i == 0) ? EFI_ERROR_SECTION_FLAGS_PRIMARY : 0;
		descriptor.SectionType = gEfiPlatformActionEvent;
		descriptor.FruId = cpad_descriptor->FruId;
		descriptor.Severity = EFI_GENERIC_ERROR_PLATFORM_ACTION_EVENT;
		memcpy(descriptor.FruString, cpad_descriptor->FruString,
		       sizeof(descriptor.FruString));

		if (fwrite(&descriptor, sizeof(descriptor), 1, out) != 1) {
			return -1;
		}
	}

	//Write the Platform Action Event section bodies.
	for (UINT16 i = 0; i < num_requests; i++) {
		const CPAD_SECTION_DESCRIPTOR *cpad_descriptor =
			(const CPAD_SECTION_DESCRIPTOR
				 *)(cpad_buf + sizeof(CPAD_HEADER) +
				    (size_t)requests[i].CpadSectionIndex *
					    sizeof(CPAD_SECTION_DESCRIPTOR));

		EFI_PLATFORM_ACTION_EVENT body;
		memset(&body, 0, sizeof(body));
		body.ValidationBits =
			EFI_PLATFORM_ACTION_RECORD_ID_VALID |
			EFI_PLATFORM_ACTION_CPAD_SECTION_INDEX_VALID |
			EFI_PLATFORM_ACTION_ACTION_ID_VALID |
			EFI_PLATFORM_ACTION_ACTION_RETURN_CODE_VALID;
		body.ActionReturnCode = requests[i].ReturnCode;
		body.ActionReturnReasonCode = requests[i].ReasonCode;
		body.CpadActionId = cpad_descriptor->ActionID;
		body.CpadSectionDescriptorIndex = requests[i].CpadSectionIndex;
		body.CpadPlatformId = cpad->PlatformID;
		body.CpadPartitionId = cpad->PartitionID;
		body.CpadCreatorId = cpad->CreatorID;
		body.CpadRecordId = cpad->RecordID;

		if (fwrite(&body, sizeof(body), 1, out) != 1) {
			return -1;
		}
	}

	fflush(out);
	return 0;
}
