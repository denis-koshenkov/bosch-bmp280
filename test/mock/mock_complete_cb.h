#ifndef TEST_MOCK_MOCK_COMPLETE_CB_H
#define TEST_MOCK_MOCK_COMPLETE_CB_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "bmp280.h"

void mock_bmp280_complete_cb(uint8_t rc, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* TEST_MOCK_MOCK_COMPLETE_CB_H */
