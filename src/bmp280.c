#include <stddef.h>
#include <stdbool.h>

#include "bmp280.h"
#include "bmp280_private.h"

#define BMP280_CALIB_DATA_START_REG_ADDR 0x88
#define BMP280_CHIP_ID_REG_ADDR 0xD0
#define BMP280_RESET_REG_ADDR 0xE0
#define BMP280_CTRL_MEAS_REG_ADDR 0xF4
#define BMP280_PRES_MSB_REG_ADDR 0xF7
#define BMP280_TEMP_MSB_REG_ADDR 0xFA

#define BMP280_BIT_MSK_POWER_MODE_FORCED 0x01U

/**
 * @brief Get temperature oversampling bit mask.
 *
 * @param x Temperature oversampling option. One of @ref BMP280Oversampling. E.g. oversampling_2 = 2, oversampling_4 =
 * 3, oversampling_8 = 4.
 */
#define BMP280_BIT_MSK_TEMP_OVERSAMPLING(x) ((uint8_t)(((uint8_t)x) << 5))

/**
 * @brief Get pressure oversampling bit mask.
 *
 * @param x Pressure oversampling option. One of @ref BMP280Oversampling. E.g. oversampling_2 = 2, oversampling_4 =
 * 3, oversampling_8 = 4.
 */
#define BMP280_BIT_MSK_PRES_OVERSAMPLING(x) ((uint8_t)(((uint8_t)x) << 2))

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
 * @brief Check if measurement type is valid.
 *
 * @param meas_type Measurement type.
 *
 * @retval true Measurement type is valid.
 * @retval false Measurement type is invalid.
 */
static bool is_valid_meas_type(uint8_t meas_type)
{
    return (meas_type == BMP280_MEAS_TYPE_ONLY_TEMP) || (meas_type == BMP280_MEAS_TYPE_TEMP_AND_PRES);
}

/**
 * @brief Check if oversampling option is valid.
 *
 * @param oversampling Oversampling option.
 *
 * @retval true Oversampling option is valid.
 * @retval false Oversampling option is invalid.
 */
static bool is_valid_oversampling(uint8_t oversampling)
{
    // clang-format off
    return (
        (oversampling == BMP280_OVERSAMPLING_SKIPPED)
        || (oversampling == BMP280_OVERSAMPLING_1)
        || (oversampling == BMP280_OVERSAMPLING_2)
        || (oversampling == BMP280_OVERSAMPLING_4)
        || (oversampling == BMP280_OVERSAMPLING_8)
        || (oversampling == BMP280_OVERSAMPLING_16)
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
 * @brief Read calibration data.
 *
 * @param[in] self BMP280 instance.
 * @param[out] calib_vals Must be a buffer of 24 bytes. Calibration register values are written to this parameter.
 * @param[in] cb Callback to execute once IO transaction to read the register is complete.
 * @param[in] user_data User data to pass to @p cb.
 */
static void read_calib_data(BMP280 self, uint8_t *const calib_vals, BMP280_IOCompleteCb cb, void *user_data)
{
    self->read_regs(BMP280_CALIB_DATA_START_REG_ADDR, 24, calib_vals, self->read_regs_user_data, cb, user_data);
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

/**
 * @brief Compensate temperature using raw temperature value and temperature calibration values.
 *
 * @param[in] calib_temp Temperature calibration values.
 * @param[in] temp_raw Raw temperature value.
 * @param[out] t_fine Fine resolution temperature value is written to this parameter, so that it can be used in pressure
 * compensation calculation.
 *
 * @return int32_t Temperature in DegC, resolution is 0.01 DegC. Output value of "5123" equals 51.23 DegC.
 */
static int32_t compensate_temp(const CalibTemp *const calib_temp, int32_t temp_raw, int32_t *const t_fine)
{
    uint16_t dig_T1 = calib_temp->dig_T1;
    int16_t dig_T2 = calib_temp->dig_T2;
    int16_t dig_T3 = calib_temp->dig_T3;

    int32_t var1 = ((((temp_raw >> 3) - ((int32_t)dig_T1 << 1))) * ((int32_t)dig_T2)) >> 11;
    int32_t var2 =
        (((((temp_raw >> 4) - ((int32_t)dig_T1)) * ((temp_raw >> 4) - ((int32_t)dig_T1))) >> 12) * ((int32_t)dig_T3)) >>
        14;
    *t_fine = var1 + var2;
    int32_t T = (*t_fine * 5 + 128) >> 8;
    return T;
}

/**
 * @brief Compensate pressure using raw pressure value and pressure calibration values.
 *
 * @param[in] calib Pressure calibration values.
 * @param[in] pres_raw Raw pressure value.
 * @param[in] t_fine Fine resolution temperature value from @ref compensate_temp.
 *
 * @return uint32_t Pressure in Pa in Q24.8 format (24 integer bits and 8 fractional bits). Output value of "24674867"
 * represents 24674867/256 = 96386.2 Pa = 963.862 hPa.
 */
static uint32_t compensate_pres(const CalibPres *const calib, int32_t pres_raw, int32_t t_fine)
{
    int64_t var1, var2, p;
    var1 = ((int64_t)t_fine) - 128000;
    var2 = var1 * var1 * (int64_t)calib->dig_P6;
    var2 = var2 + ((var1 * (int64_t)calib->dig_P5) << 17);
    var2 = var2 + (((int64_t)calib->dig_P4) << 35);
    var1 = ((var1 * var1 * (int64_t)calib->dig_P3) >> 8) + ((var1 * (int64_t)calib->dig_P2) << 12);
    var1 = (((((int64_t)1) << 47) + var1)) * ((int64_t)calib->dig_P1) >> 33;
    if (var1 == 0) {
        return 0; // avoid exception caused by division by zero
    }
    p = 1048576 - pres_raw;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (((int64_t)calib->dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (((int64_t)calib->dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (((int64_t)calib->dig_P7) << 4);
    return (uint32_t)p;
}

/**
 * @brief Convert temperature/pressure bytes from BMP280 registers to raw value.
 *
 * @param[in] data Must point to a buffer of 3 bytes that contains register values of temp/press_msb, temp/press_lsb,
 * temp/press_xlsb (in that order).
 *
 * @return int32_t Raw value.
 */
static int32_t temp_pres_bytes_to_raw_val(const uint8_t *const data)
{
    uint32_t t = (((uint32_t)data[0]) << 12) | (((uint32_t)data[1]) << 4) | (((uint32_t)(data[2] & 0xF0)) >> 4);
    return (int32_t)t;
}

/**
 * @brief Interpret two bytes in little endian as an unsigned 16-bit integer.
 *
 * @param[in] bytes Must point to two bytes. bytes[0] is the LSB, bytes[1] is the MSB.
 *
 * @return uint16_t Resulting unsigned 16-bit integer.
 */
uint16_t two_little_endian_bytes_to_uint16(const uint8_t *const bytes)
{
    return (((uint16_t)bytes[1]) << 8) | ((uint16_t)bytes[0]);
}

/**
 * @brief Interpret two bytes in little endian as a signed 16-bit integer.
 *
 * @param[in] bytes Must point to two bytes. bytes[0] is the LSB, bytes[1] is the MSB.
 *
 * @return int16_t Resulting signed 16-bit integer.
 */
int16_t two_little_endian_bytes_to_int16(const uint8_t *const bytes)
{
    uint16_t unsigned_val = (((uint16_t)bytes[1]) << 8) | ((uint16_t)bytes[0]);
    int16_t *signed_val_p = (int16_t *)&unsigned_val;
    return *signed_val_p;
}

/**
 * @brief Convert temperature calibration register values to calibration values.
 *
 * @param[in] data Must point to 6 bytes that contain the contents of registers 0x88...0x8D.
 * @param[out] calib_temp Temperature calibration values are written to this parameter.
 */
static void convert_temp_calib_reg_vals_to_calib_values(const uint8_t *const data, CalibTemp *const calib_temp)
{
    calib_temp->dig_T1 = two_little_endian_bytes_to_uint16(&data[0]);
    calib_temp->dig_T2 = two_little_endian_bytes_to_int16(&data[2]);
    calib_temp->dig_T3 = two_little_endian_bytes_to_int16(&data[4]);
}

/**
 * @brief Convert pressure calibration register values to calibration values.
 *
 * @param[in] data Must point to 18 bytes that contain the contents of registers 0x8E...0x9F.
 * @param[out] calib_pres Pressure calibration values are written to this parameter.
 */
static void convert_pres_calib_reg_vals_to_calib_values(const uint8_t *const data, CalibPres *const calib_pres)
{
    calib_pres->dig_P1 = two_little_endian_bytes_to_uint16(&data[0]);
    calib_pres->dig_P2 = two_little_endian_bytes_to_int16(&data[2]);
    calib_pres->dig_P3 = two_little_endian_bytes_to_int16(&data[4]);
    calib_pres->dig_P4 = two_little_endian_bytes_to_int16(&data[6]);
    calib_pres->dig_P5 = two_little_endian_bytes_to_int16(&data[8]);
    calib_pres->dig_P6 = two_little_endian_bytes_to_int16(&data[10]);
    calib_pres->dig_P7 = two_little_endian_bytes_to_int16(&data[12]);
    calib_pres->dig_P8 = two_little_endian_bytes_to_int16(&data[14]);
    calib_pres->dig_P9 = two_little_endian_bytes_to_int16(&data[16]);
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

    bool calculate_pres;
    if (self->meas_type == BMP280_MEAS_TYPE_ONLY_TEMP) {
        calculate_pres = false;
    } else if (self->meas_type == BMP280_MEAS_TYPE_TEMP_AND_PRES) {
        calculate_pres = true;
    } else {
        /* Invalid meas type */
        execute_complete_cb(self, BMP280_RESULT_CODE_DRIVER_ERR);
        return;
    }

    /* If we also read out pressure, then the first three bytes in read_buf are pressure register values */
    size_t temp_start_idx = calculate_pres ? 3 : 0;
    int32_t temp_raw = temp_pres_bytes_to_raw_val(&self->read_buf[temp_start_idx]);
    int32_t t_fine;
    (self->meas)->temperature = compensate_temp(&self->calib_temp, temp_raw, &t_fine);

    if (calculate_pres) {
        /* Pressure reg values always start at index 0 of read_buf */
        int32_t pres_raw = temp_pres_bytes_to_raw_val(self->read_buf);
        (self->meas)->pressure = compensate_pres(&self->calib_pres, pres_raw, t_fine);
    }
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

    self->start_timer(self->timer_period_ms, self->start_timer_user_data, read_meas_forced_mode_part_4, (void *)self);
}

static void read_meas_forced_mode_part_2(uint8_t io_rc, void *user_data)
{
    BMP280 self = (BMP280)user_data;
    if (io_rc != BMP280_IO_RESULT_CODE_OK) {
        execute_complete_cb(self, BMP280_RESULT_CODE_IO_ERR);
        return;
    }

    uint8_t write_val = self->read_buf[0];
    /* Clear the two LSb of ctrl_meas register value */
    write_val = write_val & ~((uint8_t)0x3U);
    /* Set the two LSb of ctrl_meas register value to forced mode */
    write_val = write_val | (uint8_t)BMP280_BIT_MSK_POWER_MODE_FORCED;

    write_ctrl_meas_reg(self, write_val, read_meas_forced_mode_part_3, (void *)self);
}

static void read_set_temp_oversamlping_part_2(uint8_t io_rc, void *user_data)
{
    BMP280 self = (BMP280)user_data;
    if (io_rc != BMP280_IO_RESULT_CODE_OK) {
        execute_complete_cb(self, BMP280_RESULT_CODE_IO_ERR);
        return;
    }

    uint8_t write_val = self->read_buf[0];
    /* Clear the three MSb of ctrl_meas register value */
    write_val = write_val & ~((uint8_t)0xE0U);
    /* Set the three MSb of ctrl_meas register value to oversampling option */
    write_val = write_val | BMP280_BIT_MSK_TEMP_OVERSAMPLING(self->oversamlping);

    write_ctrl_meas_reg(self, write_val, generic_io_complete_cb, (void *)self);
}

static void read_set_pres_oversamlping_part_2(uint8_t io_rc, void *user_data)
{
    BMP280 self = (BMP280)user_data;
    if (io_rc != BMP280_IO_RESULT_CODE_OK) {
        execute_complete_cb(self, BMP280_RESULT_CODE_IO_ERR);
        return;
    }

    uint8_t write_val = self->read_buf[0];
    /* Clear bits[4:2] of ctrl_meas register value */
    write_val = write_val & ~((uint8_t)0x1CU);
    /* Set bits[4:2] of ctrl_meas register value to oversampling option */
    write_val = write_val | BMP280_BIT_MSK_PRES_OVERSAMPLING(self->oversamlping);

    write_ctrl_meas_reg(self, write_val, generic_io_complete_cb, (void *)self);
}

static void init_meas_part_2(uint8_t io_rc, void *user_data)
{
    BMP280 self = (BMP280)user_data;
    if (io_rc != BMP280_IO_RESULT_CODE_OK) {
        execute_complete_cb(self, BMP280_RESULT_CODE_IO_ERR);
        return;
    }

    /* First 6 bytes are from temperature calibration registers */
    convert_temp_calib_reg_vals_to_calib_values(&self->read_buf[0], &self->calib_temp);
    /* Last 18 bytes are from pressure calibration registers */
    convert_pres_calib_reg_vals_to_calib_values(&self->read_buf[6], &self->calib_pres);

    self->is_meas_init = true;
    execute_complete_cb(self, BMP280_RESULT_CODE_OK);
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
    (*inst)->is_meas_init = false;

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

uint8_t bmp280_init_meas(BMP280 self, BMP280CompleteCb cb, void *user_data)
{
    if (!self) {
        return BMP280_RESULT_CODE_INVAL_ARG;
    }

    start_sequence(self, cb, user_data);
    read_calib_data(self, self->read_buf, init_meas_part_2, (void *)self);
    return BMP280_RESULT_CODE_OK;
}

uint8_t bmp280_read_meas_forced_mode(BMP280 self, uint8_t meas_type, uint32_t meas_time_ms, BMP280Meas *const meas,
                                     BMP280CompleteCb cb, void *user_data)
{
    if (!self || !meas || (meas_time_ms == 0) || !is_valid_meas_type(meas_type)) {
        return BMP280_RESULT_CODE_INVAL_ARG;
    }
    if (!self->is_meas_init) {
        return BMP280_RESULT_CODE_INVAL_USAGE;
    }

    start_sequence(self, cb, user_data);
    self->meas = meas;
    self->meas_type = meas_type;
    self->timer_period_ms = meas_time_ms;
    read_ctrl_meas_reg(self, self->read_buf, read_meas_forced_mode_part_2, (void *)self);
    return BMP280_RESULT_CODE_OK;
}

uint8_t bmp280_set_temp_oversampling(BMP280 self, uint8_t oversampling, BMP280CompleteCb cb, void *user_data)
{
    if (!self || !is_valid_oversampling(oversampling)) {
        return BMP280_RESULT_CODE_INVAL_ARG;
    }

    start_sequence(self, cb, user_data);
    self->oversamlping = oversampling;
    read_ctrl_meas_reg(self, self->read_buf, read_set_temp_oversamlping_part_2, (void *)self);
    return BMP280_RESULT_CODE_OK;
}

uint8_t bmp280_set_pres_oversampling(BMP280 self, uint8_t oversampling, BMP280CompleteCb cb, void *user_data)
{
    if (!self || !is_valid_oversampling(oversampling)) {
        return BMP280_RESULT_CODE_INVAL_ARG;
    }

    start_sequence(self, cb, user_data);
    self->oversamlping = oversampling;
    read_ctrl_meas_reg(self, self->read_buf, read_set_pres_oversamlping_part_2, (void *)self);
    return BMP280_RESULT_CODE_OK;
}
