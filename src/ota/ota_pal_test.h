#ifndef _OTA_PAL_TEST_H_
#define _OTA_PAL_TEST_H_

/* Include for TimeDelayFunc */
#include "time_delay.h"

typedef void (* MemFreeFunc)( void *ptr );

typedef struct OtaPalTestParam
{
    MemFreeFunc pMemFree;
    TimeDelayFunc_t pDelay;
} OtaPalTestParam_t;

void SetupOtaPalTestParam( OtaPalTestParam_t * pTestParam );

int RunOtaPalTest( void );

#endif /* _OTA_PAL_TEST_H_ */
