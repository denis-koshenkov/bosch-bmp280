#include <stddef.h>
#include <stdbool.h>

#include "bmp280.h"
#include "bmp280_private.h"

#define BMP280_CHIP_ID_REG_ADDR 0xD0

/**
 * @brief Check if init config is valid.
 *
 * @param cfg Init config.
 *
 * @retval true Init config is valid.
 * @retval false Init config is invalid.
 */
static bool is_valid_cfg(const BMP280InitCfg *const cfg)
{
    // clang-format off
    return (
        cfg
        && cfg->get_inst_buf
        && cfg->read_regs
    );
    // clang-format on
}

/**
 * @brief Start a sequence.
 *
 * Saves @p cb and @p user_data as sequence complete callback and user data, so that they can be executed once the
 * sequence is complete.
 *
 * @pre @p self has been validated to not be NULL.
 *
 * @param[in] self BMP280 instance.
 * @param[in] cb Callback to execute once the sequence is complete.
 * @param[in] user_data User data to pass to @p cb.
 */
static void start_sequence(BMP280 self, BMP280CompleteCb cb, void *user_data)
{
    self->complete_cb = cb;
    self->complete_cb_user_data = user_data;
}

static void generic_io_complete_cb(uint8_t io_rc, void *user_data)
{
    BMP280 self = (BMP280)user_data;
    if (!self) {
        return;
    }

    uint8_t rc = (io_rc == BMP280_IO_RESULT_CODE_OK) ? BMP280_RESULT_CODE_OK : BMP280_RESULT_CODE_IO_ERR;
    self->complete_cb(rc, self->complete_cb_user_data);
}

uint8_t bmp280_create(BMP280 *const inst, const BMP280InitCfg *const cfg)
{
    if (!inst || !is_valid_cfg(cfg)) {
        return BMP280_RESULT_CODE_INVAL_ARG;
    }

    *inst = cfg->get_inst_buf(cfg->get_inst_buf_user_data);
    if (*inst == NULL) {
        return BMP280_RESULT_CODE_NO_MEM;
    }

    (*inst)->read_regs = cfg->read_regs;
    (*inst)->read_regs_user_data = cfg->read_regs_user_data;

    return BMP280_RESULT_CODE_OK;
}

uint8_t bmp280_get_chip_id(BMP280 self, uint8_t *const chip_id, BMP280CompleteCb cb, void *user_data)
{
    start_sequence(self, cb, user_data);
    self->read_regs(BMP280_CHIP_ID_REG_ADDR, 1, chip_id, self->read_regs_user_data, generic_io_complete_cb,
                    (void *)self);
    return BMP280_RESULT_CODE_OK;
}
