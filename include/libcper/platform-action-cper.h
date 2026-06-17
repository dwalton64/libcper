#ifndef PLATFORM_ACTION_CPER_H
#define PLATFORM_ACTION_CPER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stddef.h>
#include <libcper/BaseTypes.h>

//A single Platform Action Event to emit for one CPAD section.
typedef struct {
	UINT32 CpadSectionIndex; //0-based index of the source CPAD section
	UINT8 ReturnCode;	 //Action Return Code
	UINT8 ReasonCode;	 //Action Return Reason Code
} PLATFORM_ACTION_EVENT_REQUEST;

//Builds a Platform Action Event CPER from the given binary CPAD and writes it
//to `out`. The output record contains one Platform Action Event section per
//request, each reporting on the referenced CPAD section. The CPER header
//copies the PlatformID, PartitionID and CreatorID from the CPAD (CreatorID is
//used to route the CPER back to the analyzer).
//
//Returns 0 on success, or a negative value if the arguments are invalid (NULL
//pointers, zero requests, an invalid CPAD, or a request referencing a section
//index outside the CPAD).
int cpad_to_platform_action_event_cper(
	const unsigned char *cpad_buf, size_t cpad_size,
	const PLATFORM_ACTION_EVENT_REQUEST *requests, UINT16 num_requests,
	FILE *out);

#ifdef __cplusplus
}
#endif

#endif
