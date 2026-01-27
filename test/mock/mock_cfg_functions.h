#ifndef TEST_MOCK_MOCK_CFG_FUNCTIONS_H
#define TEST_MOCK_MOCK_CFG_FUNCTIONS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "bmp280.h"

void *mock_bmp280_get_inst_buf(void *user_data);

void mock_bmp280_read_regs(uint8_t start_addr, size_t num_regs, uint8_t *data, void *user_data, BMP280_IOCompleteCb cb,
                           void *cb_user_data);

#ifdef __cplusplus
}
#endif

#endif /* TEST_MOCK_MOCK_CFG_FUNCTIONS_H */
