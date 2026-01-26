#include "bmp280.h"

uint8_t bmp280_create(BMP280 *const inst, const BMP280InitCfg *const cfg)
{
    *inst = cfg->get_inst_buf(cfg->get_inst_buf_user_data);
    return BMP280_RESULT_CODE_OK;
}
