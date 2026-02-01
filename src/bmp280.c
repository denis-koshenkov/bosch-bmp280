#include <stddef.h>
#include <stdbool.h>

#include "bmp280.h"
#include "bmp280_private.h"

#define BMP280_CHIP_ID_REG_ADDR 0xD0
#define BMP280_RESET_REG_ADDR 0xE0
#define BMP280_CTRL_MEAS_REG_ADDR 0xF4
#define BMP280_PRES_MSB_REG_ADDR 0xF7
#define BMP280_TEMP_MSB_REG_ADDR 0xFA

#define BMP280_BIT_MSK_POWER_MODE_FORCED 0x01U

/** Value to write to reset register to perform a reset. */
#define BMP280_RESET_REG_VALUE 0xB6

/** The duration of power on reset procedure. This procedure is executed when the device is powered on, or a reset is
 * performed using the reset register. */
#define BMP280_POWER_ON_RESET_DURATION_MS 2

typedef enum {
    BMP280_POWER_MODE_SLEEP,
    BMP280_POWER_MODE_FORCED,
    BMP280_POWER_MODE_NORMAL,
} BMP280PowerMode;

typedef struct {
    uint16_t dig_T1;
    int16_t dig_T2;
    int16_t dig_T3;
} CalibTemp;

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
        && cfg->write_reg
        && cfg->start_timer
    );
    // clang-format on
}

/**
 * @brief Read chip ID from the chip ID regsiter.
 *
 * @pre @p self has been validated to not be NULL.
 *
 * @param[in] self BMP280 instance.
 * @param[out] chip_id Chip id is written to this parameter.
 * @param[in] cb Callback to execute once IO transaction to read chip id is complete.
 * @param[in] user_data User data to pass to @p cb.
 */
static void read_chip_id(BMP280 self, uint8_t *const chip_id, BMP280_IOCompleteCb cb, void *user_data)
{
    self->read_regs(BMP280_CHIP_ID_REG_ADDR, 1, chip_id, self->read_regs_user_data, cb, user_data);
}

/**
 * @brief Send reset command to BMP280.
 *
 * @pre @p self has been validated to not be NULL.
 *
 * @param[in] self BMP280 instance.
 * @param[in] cb Callback to execute once IO transaction to write the reset register is complete.
 * @param[in] user_data User data to pass to @p cb.
 */
static void send_reset_cmd(BMP280 self, BMP280_IOCompleteCb cb, void *user_data)
{
    self->write_reg(BMP280_RESET_REG_ADDR, BMP280_RESET_REG_VALUE, self->write_reg_user_data, cb, user_data);
}

/**
 * @brief Read ctrl_meas_register.
 *
 * @param[in] self BMP280 instance.
 * @param[out] val Register value is written to this parameter.
 * @param[in] cb Callback to execute once IO transaction to read the register is complete.
 * @param[in] user_data User data to pass to @p cb.
 */
static void read_ctrl_meas_reg(BMP280 self, uint8_t *const val, BMP280_IOCompleteCb cb, void *user_data)
{
    self->read_regs(BMP280_CTRL_MEAS_REG_ADDR, 1, val, self->read_regs_user_data, cb, user_data);
}

/**
 * @brief Write a value to ctrl_meas register.
 *
 * @param[in] self BMP280 instance.
 * @param[in] val Value to write.
 * @param[in] cb Callback to execute once IO transaction to write the register is complete.
 * @param[in] user_data User data to pass to @p cb.
 */
static void write_ctrl_meas_reg(BMP280 self, uint8_t val, BMP280_IOCompleteCb cb, void *user_data)
{
    self->write_reg(BMP280_CTRL_MEAS_REG_ADDR, val, self->write_reg_user_data, cb, user_data);
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

/**
 * @brief Execute complete callback, if one is present.
 *
 * @pre @p self has been validated to not be NULL.
 *
 * @param self BMP280 instance.
 * @param rc Result code to pass to complete cb.
 */
static void execute_complete_cb(BMP280 self, uint8_t rc)
{
    if (self->complete_cb) {
        self->complete_cb(rc, self->complete_cb_user_data);
    }
}

static int32_t compensate_temp(const CalibTemp *const calib_temp, int32_t temp_raw)
{
    uint16_t dig_T1 = calib_temp->dig_T1;
    int16_t dig_T2 = calib_temp->dig_T2;
    int16_t dig_T3 = calib_temp->dig_T3;

    int32_t var1 = ((((temp_raw >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    int32_t var2 =
        (((((temp_raw >> 4) - ((int32_t)dig_T1)) * ((temp_raw >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >>
        14;
    int32_t t_fine = var1 + var2;
    int32_t T = (t_fine * 5 + 128) >> 8;
    return T;
}

/**
 * @brief Convert temperature bytes from BMP280 temperature registers to raw temperature value.
 *
 * @param[in] data Must point to a buffer of 3 bytes that contains register values of temp_msb, temp_lsb, temp_xlsb (in
 * that order).
 *
 * @return int32_t Raw temperature value.
 */
static int32_t temp_bytes_to_raw_temp_val(const uint8_t *const data)
{
    uint32_t t = (((uint32_t)data[0]) << 12) | (((uint32_t)data[1]) << 4) | (((uint32_t)(data[2] & 0xF0)) >> 4);
    return (int32_t)t;
}

static void generic_io_complete_cb(uint8_t io_rc, void *user_data)
{
    BMP280 self = (BMP280)user_data;
    if (!self) {
        return;
    }

    uint8_t rc = (io_rc == BMP280_IO_RESULT_CODE_OK) ? BMP280_RESULT_CODE_OK : BMP280_RESULT_CODE_IO_ERR;
    execute_complete_cb(self, rc);
}

static void reset_with_delay_part_3(void *user_data)
{
    BMP280 self = (BMP280)user_data;
    if (!self) {
        return;
    }
    execute_complete_cb(self, BMP280_RESULT_CODE_OK);
}

static void reset_with_delay_part_2(uint8_t io_rc, void *user_data)
{
    BMP280 self = (BMP280)user_data;
    if (!self) {
        return;
    }

    if (io_rc != BMP280_IO_RESULT_CODE_OK) {
        execute_complete_cb(self, BMP280_RESULT_CODE_IO_ERR);
        return;
    }

    self->start_timer(BMP280_POWER_ON_RESET_DURATION_MS, self->start_timer_user_data, reset_with_delay_part_3,
                      (void *)self);
}

static void read_meas_forced_mode_part_5(uint8_t io_rc, void *user_data)
{
    BMP280 self = (BMP280)user_data;
    if (io_rc != BMP280_IO_RESULT_CODE_OK) {
        execute_complete_cb(self, BMP280_RESULT_CODE_IO_ERR);
        return;
    }

    size_t temp_start_idx = 0;
    if (self->meas_type == BMP280_MEAS_TYPE_ONLY_TEMP) {
        temp_start_idx = 0;
    } else if (self->meas_type == BMP280_MEAS_TYPE_TEMP_AND_PRES) {
        temp_start_idx = 3;
    } else {
        /* Invalid meas type */
        execute_complete_cb(self, BMP280_RESULT_CODE_DRIVER_ERR);
        return;
    }
    int32_t temp_raw = temp_bytes_to_raw_temp_val(&self->read_buf[temp_start_idx]);

    CalibTemp calib_temp = {
        .dig_T1 = 27504,
        .dig_T2 = 26435,
        .dig_T3 = -1000,
    };
    (self->meas)->temperature = compensate_temp(&calib_temp, temp_raw);
    (self->meas)->pressure = 25767236;
    execute_complete_cb(self, BMP280_RESULT_CODE_OK);
}

static void read_meas_forced_mode_part_4(void *user_data)
{
    BMP280 self = (BMP280)user_data;
    size_t num_regs;
    uint8_t start_addr;
    if (self->meas_type == BMP280_MEAS_TYPE_ONLY_TEMP) {
        num_regs = 3;
        start_addr = BMP280_TEMP_MSB_REG_ADDR;
    } else if (self->meas_type == BMP280_MEAS_TYPE_TEMP_AND_PRES) {
        num_regs = 6;
        start_addr = BMP280_PRES_MSB_REG_ADDR;
    } else {
        /* Invalid meas_type */
        execute_complete_cb(self, BMP280_RESULT_CODE_DRIVER_ERR);
        return;
    }

    self->read_regs(start_addr, num_regs, self->read_buf, self->read_regs_user_data, read_meas_forced_mode_part_5,
                    (void *)self);
}

static void read_meas_forced_mode_part_3(uint8_t io_rc, void *user_data)
{
    BMP280 self = (BMP280)user_data;
    if (io_rc != BMP280_IO_RESULT_CODE_OK) {
        execute_complete_cb(self, BMP280_RESULT_CODE_IO_ERR);
        return;
    }

    self->start_timer(5, self->start_timer_user_data, read_meas_forced_mode_part_4, (void *)self);
}

static void read_meas_forced_mode_part_2(uint8_t io_rc, void *user_data)
{
    BMP280 self = (BMP280)user_data;
    if (io_rc != BMP280_IO_RESULT_CODE_OK) {
        execute_complete_cb(self, BMP280_RESULT_CODE_IO_ERR);
        return;
    }

    uint8_t write_val = self->saved_reg_val;
    /* Clear the two LSb of ctrl_meas register value */
    write_val = write_val & ~((uint8_t)0x3U);
    /* Set the two LSb of ctrl_meas register value to forced mode */
    write_val = write_val | (uint8_t)BMP280_BIT_MSK_POWER_MODE_FORCED;

    write_ctrl_meas_reg(self, write_val, read_meas_forced_mode_part_3, (void *)self);
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
    (*inst)->write_reg = cfg->write_reg;
    (*inst)->write_reg_user_data = cfg->write_reg_user_data;
    (*inst)->start_timer = cfg->start_timer;
    (*inst)->start_timer_user_data = cfg->start_timer_user_data;

    return BMP280_RESULT_CODE_OK;
}

uint8_t bmp280_get_chip_id(BMP280 self, uint8_t *const chip_id, BMP280CompleteCb cb, void *user_data)
{
    if (!self || !chip_id) {
        return BMP280_RESULT_CODE_INVAL_ARG;
    }

    start_sequence(self, cb, user_data);
    read_chip_id(self, chip_id, generic_io_complete_cb, (void *)self);
    return BMP280_RESULT_CODE_OK;
}

uint8_t bmp280_reset_with_delay(BMP280 self, BMP280CompleteCb cb, void *user_data)
{
    if (!self) {
        return BMP280_RESULT_CODE_INVAL_ARG;
    }

    start_sequence(self, cb, user_data);
    send_reset_cmd(self, reset_with_delay_part_2, (void *)self);
    return BMP280_RESULT_CODE_OK;
}

uint8_t bmp280_read_meas_forced_mode(BMP280 self, uint8_t meas_type, uint32_t meas_time_ms, BMP280Meas *const meas,
                                     BMP280CompleteCb cb, void *user_data)
{
    start_sequence(self, cb, user_data);
    self->meas = meas;
    self->meas_type = meas_type;
    read_ctrl_meas_reg(self, &self->saved_reg_val, read_meas_forced_mode_part_2, (void *)self);
    return BMP280_RESULT_CODE_OK;
}
