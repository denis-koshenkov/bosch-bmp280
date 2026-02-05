#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "bmp280.h"
/* To include the definition of struct BMP280Struct, so that we can define an instance to return from
 * mock_bmp280_get_inst_buf. */
#include "bmp280_private.h"
#include "mock_cfg_functions.h"
#include "mock_complete_cb.h"

/* Example calib values from the datasheet p. 23. */
static uint8_t default_calib_data[24] = {
    0x70, 0x6B, // dig_T1 = 27504
    0x43, 0x67, // dig_T2 = 26435
    0x18, 0xFC, // dig_T3 = -1000
    0x7D, 0x8E, // dig_P1 = 36477
    0x43, 0xD6, // dig_P2 = -10685
    0xD0, 0x0B, // dig_P3 = 3024
    0x27, 0x0B, // dig_P4 = 2855
    0x8C, 0x00, // dig_P5 = 140
    0xF9, 0xFF, // dig_P6 = -7
    0x8C, 0x3C, // dig_P7 = 15500
    0xF8, 0xC6, // dig_P8 = -14600
    0x70, 0x17  // dig_P9 = 6000
};

static uint8_t alt_calib_data[24] = {
    0x82, 0x6B, // dig_T1 = 27522
    0x53, 0x67, // dig_T2 = 26451
    0x18, 0xFB, // dig_T3 = -1256
    0x7F, 0x8E, // dig_P1 = 36479
    0x43, 0xD6, // dig_P2 = -10685
    0xD0, 0x0B, // dig_P3 = 3024
    0x27, 0x0B, // dig_P4 = 2855
    0x9C, 0x00, // dig_P5 = 156
    0xF9, 0xFF, // dig_P6 = -7
    0x80, 0x3C, // dig_P7 = 15488
    0xF8, 0xC6, // dig_P8 = -14600
    0x70, 0x17  // dig_P9 = 6000
};

/* To return from mock_bmp280_get_inst_buf */
static struct BMP280Struct inst_buf;

/* User data parameters to pass to bmp280_create in the init cfg */
static void *get_inst_buf_user_data = (void *)0x90;
static void *read_regs_user_data = (void *)0x91;
static void *write_reg_user_data = (void *)0x92;
static void *start_timer_user_data = (void *)0x93;

/* BMP280 instance used by all tests */
static BMP280 bmp280;
/* Init cfg used by all tests */
static BMP280InitCfg init_cfg;

/* Populated by mock object whenever mock_bmp280_read_reg is called */
static BMP280_IOCompleteCb read_regs_complete_cb;
static void *read_regs_complete_cb_user_data;
/* Populated by mock object whenever mock_bmp280_write_reg is called */
static BMP280_IOCompleteCb write_reg_complete_cb;
static void *write_reg_complete_cb_user_data;
/* Populated by mock object whenever mock_bmp280_start_timer is called */
static BMP280TimerExpiredCb timer_expired_cb;
static void *timer_expired_cb_user_data;

static void populate_default_init_cfg(BMP280InitCfg *const cfg)
{
    cfg->get_inst_buf = mock_bmp280_get_inst_buf;
    cfg->get_inst_buf_user_data = get_inst_buf_user_data;
    cfg->read_regs = mock_bmp280_read_regs;
    cfg->read_regs_user_data = read_regs_user_data;
    cfg->write_reg = mock_bmp280_write_reg;
    cfg->write_reg_user_data = write_reg_user_data;
    cfg->start_timer = mock_bmp280_start_timer;
    cfg->start_timer_user_data = start_timer_user_data;
}

// clang-format off
TEST_GROUP(BMP280){
    void setup() {
        /* Order of expected calls is important for these tests. Fail the test if the expected mock calls do not happen
        in the specified order. */
        mock().strictOrder();

        /* Reset all values populated by mock object */
        read_regs_complete_cb = NULL;
        read_regs_complete_cb_user_data = NULL;
        write_reg_complete_cb = NULL;
        write_reg_complete_cb_user_data = NULL;

        /* Pass pointers so that the mock object populates them with callbacks and user data, so that the test can simulate
         * calling these callbacks. */
        mock().setData("readRegsCompleteCb", (void *)&read_regs_complete_cb);
        mock().setData("readRegsCompleteCbUserData", &read_regs_complete_cb_user_data);
        mock().setData("writeRegCompleteCb", (void *)&write_reg_complete_cb);
        mock().setData("writeRegCompleteCbUserData", &write_reg_complete_cb_user_data);
        mock().setData("timerExpiredCb", (void *)&timer_expired_cb);
        mock().setData("timerExpiredCbUserData", &timer_expired_cb_user_data);

        bmp280 = NULL;
        memset(&init_cfg, 0, sizeof(BMP280InitCfg));

        populate_default_init_cfg(&init_cfg);

        /* Every test must call bmp280_create. It is not done in setup so that each test has an opportunity to adjust init cfg. */
        mock()
            .expectOneCall("mock_bmp280_get_inst_buf")
            .withParameter("user_data", get_inst_buf_user_data)
            .andReturnValue((void *)&inst_buf);
    }
};
// clang-format on

static void test_get_chip_id(uint8_t read_regs_data, uint8_t complete_cb_rc, uint8_t read_io_rc,
                             BMP280CompleteCb complete_cb)
{
    mock()
        .expectOneCall("mock_bmp280_read_regs")
        .withParameter("start_addr", 0xD0)
        .withParameter("num_regs", 1)
        .withOutputParameterReturning("data", &read_regs_data, 1)
        .withParameter("user_data", read_regs_user_data)
        .ignoreOtherParameters();

    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t chip_id;
    void *complete_cb_user_data = (void *)0xA0;
    uint8_t rc = bmp280_get_chip_id(bmp280, &chip_id, complete_cb, complete_cb_user_data);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc);

    if (complete_cb) {
        mock()
            .expectOneCall("mock_bmp280_complete_cb")
            .withParameter("rc", complete_cb_rc)
            .withParameter("user_data", complete_cb_user_data);
    }
    read_regs_complete_cb(read_io_rc, read_regs_complete_cb_user_data);
}

TEST(BMP280, GetChipIdReadFail)
{
    /* Value does not matter since read fails */
    uint8_t read_regs_data = 0x42;
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_IO_ERR;
    /* Fail read regs */
    uint8_t read_io_rc = BMP280_IO_RESULT_CODE_ERR;
    test_get_chip_id(read_regs_data, complete_cb_rc, read_io_rc, mock_bmp280_complete_cb);
}

TEST(BMP280, GetChipIdReadSuccess)
{
    /* 0x58 is the expected chip id */
    uint8_t read_regs_data = 0x58;
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_OK;
    uint8_t read_io_rc = BMP280_IO_RESULT_CODE_OK;
    test_get_chip_id(read_regs_data, complete_cb_rc, read_io_rc, mock_bmp280_complete_cb);
}

TEST(BMP280, GetChipIdReadWrongChipId)
{
    /* Chip id is not the expected one. Our function should still succeed, since it only reads out the chip id, but does
     * not check for its correctness. */
    uint8_t read_regs_data = 0x59;
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_OK;
    uint8_t read_io_rc = BMP280_IO_RESULT_CODE_OK;
    test_get_chip_id(read_regs_data, complete_cb_rc, read_io_rc, mock_bmp280_complete_cb);
}

TEST(BMP280, GetChipIdCompleteCbNull)
{
    /* 0x58 is the expected chip id */
    uint8_t read_regs_data = 0x58;
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_OK;
    uint8_t read_io_rc = BMP280_IO_RESULT_CODE_OK;
    test_get_chip_id(read_regs_data, complete_cb_rc, read_io_rc, NULL);
}

TEST(BMP280, GetChipIdSelfNull)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t chip_id;
    uint8_t rc = bmp280_get_chip_id(NULL, &chip_id, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280, GetChipIdChipIdNull)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t rc = bmp280_get_chip_id(bmp280, NULL, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

static void test_reset_with_delay(uint8_t complete_cb_rc, uint8_t write_io_rc, BMP280CompleteCb complete_cb)
{
    void *complete_cb_user_data = (void *)0xA1;
    /* Called from bmp280_reset_with_delay */
    mock()
        .expectOneCall("mock_bmp280_write_reg")
        .withParameter("addr", 0xE0)
        .withParameter("reg_val", 0xB6)
        .withParameter("user_data", write_reg_user_data)
        .ignoreOtherParameters();
    if (write_io_rc == BMP280_IO_RESULT_CODE_OK) {
        /* Called from write_reg_complete_cb */
        mock()
            .expectOneCall("mock_bmp280_start_timer")
            .withParameter("duration_ms", 2)
            .withParameter("user_data", start_timer_user_data)
            .ignoreOtherParameters();
    }
    if (complete_cb) {
        /* Called either from write_reg_complete_cb (if write reg failed), or from timer_expired_cb (if write reg
         * succeeded) */
        mock()
            .expectOneCall("mock_bmp280_complete_cb")
            .withParameter("rc", complete_cb_rc)
            .withParameter("user_data", complete_cb_user_data);
    }

    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t rc = bmp280_reset_with_delay(bmp280, complete_cb, complete_cb_user_data);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc);

    write_reg_complete_cb(write_io_rc, write_reg_complete_cb_user_data);
    if (write_io_rc == BMP280_IO_RESULT_CODE_OK) {
        timer_expired_cb(timer_expired_cb_user_data);
    }
}

TEST(BMP280, ResetWithDelayWriteFail)
{
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_IO_ERR;
    uint8_t write_io_rc = BMP280_IO_RESULT_CODE_ERR;
    test_reset_with_delay(complete_cb_rc, write_io_rc, mock_bmp280_complete_cb);
}

TEST(BMP280, ResetWithDelayWriteSuccess)
{
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_OK;
    uint8_t write_io_rc = BMP280_IO_RESULT_CODE_OK;
    test_reset_with_delay(complete_cb_rc, write_io_rc, mock_bmp280_complete_cb);
}

TEST(BMP280, ResetWithDelayCbNull)
{
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_OK;
    uint8_t write_io_rc = BMP280_IO_RESULT_CODE_OK;
    test_reset_with_delay(complete_cb_rc, write_io_rc, NULL);
}

TEST(BMP280, ResetWithDelaySelfNull)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t rc = bmp280_reset_with_delay(NULL, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

typedef struct {
    /** Must point to 24 bytes that simulate calibration data to be read out from registers 0x88...0x9F. */
    const uint8_t *const calib_data;
    /** Measurement type to pass to bmp280_read_meas_forced_mode. Must be one of @ref BMP280MeasType. */
    uint8_t meas_type;
    /** Data to return from first read regs that reads the ctrl_meas register. */
    uint8_t read_1_data;
    /** IO result code of first read regs that reads the ctrl_meas register. */
    uint8_t read_1_io_rc;
    /** Data to write to ctrl_meas register. */
    uint8_t write_2_data;
    /** IO result code of write reg that writes the ctrl_meas register to set the mode to forced mode. */
    uint8_t write_2_io_rc;
    /** Measurement time to pass to bmp280_read_meas_forced_mode. It is also the expected timer period with which
     * start_timer is called after forced mode is set in ctrl_meas register. */
    uint32_t meas_time_ms;
    /** Data to return from read regs 3 which reads temperature and/or pressure value registers. Must point to buffer of
     * size read_3_data_size. If meas_type is ONLY_TEMP, must point to three bytes to read from temperature registers.
     * If meas_type is TEMP_AND_PRES, must point to 6 bytes to read from temperature and pressure registers (in that
     * order). */
    const uint8_t *const read_3_data;
    /** Must be 3 if meas_type is ONLY_TEMP, or 6 if meas_type is TEMP_AND_PRES. */
    size_t read_3_data_size;
    /** IO result code of read reg that reads the temperature and/or pressure registers. */
    uint8_t read_3_io_rc;
    /** Complete cb to pass to bmp280_read_meas_forced_mode. Two options: mock_bmp280_complete_cb or NULL. If it is
     * mock_bmp280_complete_cb, then the test checks that that function is called with complete_cb_rc. */
    BMP280CompleteCb complete_cb;
    /** Result code to pass to complete_cb. If complete_cb is NULL, this value is not used in the test. */
    uint8_t complete_cb_rc;
    /** Expected temperature measurement value. If NULL, check is not performed - useful when testing error scenarios.
     */
    int32_t *temperature;
    /** Expected pressure measurement value. If NULL, check is not performed - useful when testing error scenarios. */
    uint32_t *pressure;
} ReadMeasForcedModeTestCfg;

static void call_init_meas(const uint8_t *const calib_data)
{
    void *complete_cb_user_data = (void *)0xA3;
    /* Called from bmp280_init_meas */
    mock()
        .expectOneCall("mock_bmp280_read_regs")
        .withParameter("start_addr", 0x88)
        .withParameter("num_regs", 24)
        .withOutputParameterReturning("data", calib_data, 24)
        .withParameter("user_data", read_regs_user_data)
        .ignoreOtherParameters();
    /* Called from read_regs_complete_cb */
    mock()
        .expectOneCall("mock_bmp280_complete_cb")
        .withParameter("rc", BMP280_RESULT_CODE_OK)
        .withParameter("user_data", complete_cb_user_data);

    uint8_t rc_init_meas = bmp280_init_meas(bmp280, mock_bmp280_complete_cb, complete_cb_user_data);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_init_meas);

    read_regs_complete_cb(BMP280_IO_RESULT_CODE_OK, read_regs_complete_cb_user_data);
}

static void test_read_meas_forced_mode(const ReadMeasForcedModeTestCfg *const cfg)
{
    void *complete_cb_user_data = (void *)0xA2;

    /* Calling these before recording mock expectations so that the order of mock calls is preserved */
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);
    call_init_meas(cfg->calib_data);

    /* Called from bmp280_read_meas_forced_mode */
    mock()
        .expectOneCall("mock_bmp280_read_regs")
        .withParameter("start_addr", 0xF4)
        .withParameter("num_regs", 1)
        .withOutputParameterReturning("data", &cfg->read_1_data, 1)
        .withParameter("user_data", read_regs_user_data)
        .ignoreOtherParameters();
    if (cfg->read_1_io_rc == BMP280_IO_RESULT_CODE_OK) {
        /* Called from read_reg_complete_cb */
        mock()
            .expectOneCall("mock_bmp280_write_reg")
            .withParameter("addr", 0xF4)
            .withParameter("reg_val", cfg->write_2_data)
            .withParameter("user_data", write_reg_user_data)
            .ignoreOtherParameters();
        if (cfg->write_2_io_rc == BMP280_IO_RESULT_CODE_OK) {
            /* Called from write_reg_complete_cb */
            mock()
                .expectOneCall("mock_bmp280_start_timer")
                .withParameter("duration_ms", cfg->meas_time_ms)
                .withParameter("user_data", start_timer_user_data)
                .ignoreOtherParameters();

            uint8_t start_addr;
            if (cfg->meas_type == BMP280_MEAS_TYPE_ONLY_TEMP) {
                start_addr = 0xFA;
            } else if (cfg->meas_type == BMP280_MEAS_TYPE_TEMP_AND_PRES) {
                start_addr = 0xF7;
            } else {
                FAIL_TEST("Invalis meas_type");
            }
            /* Called from timer_expired_cb */
            mock()
                .expectOneCall("mock_bmp280_read_regs")
                .withParameter("start_addr", start_addr)
                .withParameter("num_regs", cfg->read_3_data_size)
                .withOutputParameterReturning("data", cfg->read_3_data, cfg->read_3_data_size)
                .withParameter("user_data", read_regs_user_data)
                .ignoreOtherParameters();
        }
    }
    if (cfg->complete_cb) {
        mock()
            .expectOneCall("mock_bmp280_complete_cb")
            .withParameter("rc", cfg->complete_cb_rc)
            .withParameter("user_data", complete_cb_user_data);
    }

    BMP280Meas meas;
    uint8_t rc = bmp280_read_meas_forced_mode(bmp280, cfg->meas_type, cfg->meas_time_ms, &meas, cfg->complete_cb,
                                              complete_cb_user_data);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc);

    read_regs_complete_cb(cfg->read_1_io_rc, read_regs_complete_cb_user_data);
    if (cfg->read_1_io_rc == BMP280_IO_RESULT_CODE_OK) {
        write_reg_complete_cb(cfg->write_2_io_rc, write_reg_complete_cb_user_data);
        if (cfg->write_2_io_rc == BMP280_IO_RESULT_CODE_OK) {
            timer_expired_cb(timer_expired_cb_user_data);
            read_regs_complete_cb(cfg->read_3_io_rc, read_regs_complete_cb_user_data);
        }
    }
    if (cfg->temperature) {
        CHECK_EQUAL(*(cfg->temperature), meas.temperature);
    }
    if (cfg->pressure) {
        CHECK_EQUAL(*(cfg->pressure), meas.pressure);
    }
}

TEST(BMP280, ReadMeasForcedModeRead1Fail)
{
    ReadMeasForcedModeTestCfg cfg = {
        .calib_data = default_calib_data,
        /* Does not matter */
        .meas_type = BMP280_MEAS_TYPE_ONLY_TEMP,
        /* Does not matter, this read fails */
        .read_1_data = 0x42,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_ERR,
        /* Does not matter */
        .write_2_data = 0x42,
        /* Does not matter */
        .write_2_io_rc = BMP280_IO_RESULT_CODE_ERR,
        /* Does not matter */
        .meas_time_ms = 1,
        /* Does not matter */
        .read_3_data = NULL,
        /* Does not matter */
        .read_3_data_size = 0,
        /* Does not matter */
        .read_3_io_rc = BMP280_IO_RESULT_CODE_ERR,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_IO_ERR,
        .temperature = NULL,
        .pressure = NULL,
    };
    test_read_meas_forced_mode(&cfg);
}

TEST(BMP280, ReadMeasForcedModeWrite2Fail)
{
    ReadMeasForcedModeTestCfg cfg = {
        .calib_data = default_calib_data,
        /* Does not matter */
        .meas_type = BMP280_MEAS_TYPE_ONLY_TEMP,
        .read_1_data = 0x0,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Sets forced mode to ctrl_meas reg */
        .write_2_data = 0x1,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_ERR,
        /* Does not matter */
        .meas_time_ms = 1,
        /* Does not matter */
        .read_3_data = NULL,
        /* Does not matter */
        .read_3_data_size = 0,
        /* Does not matter */
        .read_3_io_rc = BMP280_IO_RESULT_CODE_ERR,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_IO_ERR,
        .temperature = NULL,
        .pressure = NULL,
    };
    test_read_meas_forced_mode(&cfg);
}

TEST(BMP280, ReadMeasForcedModeWrite2UsesRead1Val)
{
    ReadMeasForcedModeTestCfg cfg = {
        .calib_data = default_calib_data,
        /* Does not matter */
        .meas_type = BMP280_MEAS_TYPE_ONLY_TEMP,
        .read_1_data = 0xFF,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Keeps the 6 MSb the same as read_1_data, and sets the 2 LSb to 01 (forced mode) */
        .write_2_data = 0xFD,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_ERR,
        /* Does not matter */
        .meas_time_ms = 1,
        /* Does not matter */
        .read_3_data = NULL,
        /* Does not matter */
        .read_3_data_size = 0,
        /* Does not matter */
        .read_3_io_rc = BMP280_IO_RESULT_CODE_ERR,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_IO_ERR,
        .temperature = NULL,
        .pressure = NULL,
    };
    test_read_meas_forced_mode(&cfg);
}

TEST(BMP280, ReadMeasForcedModeRead3Fail)
{
    /* Does not matter, read 3 fails */
    uint8_t read_3_data[] = {0x80, 0x0, 0x0};
    ReadMeasForcedModeTestCfg cfg = {
        .calib_data = default_calib_data,
        .meas_type = BMP280_MEAS_TYPE_ONLY_TEMP,
        .read_1_data = 0x1F,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Keeps the 6 MSb the same as read_1_data, and sets the 2 LSb to 01 (forced mode) */
        .write_2_data = 0x1D,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .meas_time_ms = 5,
        .read_3_data = read_3_data,
        .read_3_data_size = 3,
        .read_3_io_rc = BMP280_IO_RESULT_CODE_ERR,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_IO_ERR,
        .temperature = NULL,
        .pressure = NULL,
    };
    test_read_meas_forced_mode(&cfg);
}

TEST(BMP280, ReadMeasForcedModeOnlyTemp)
{
    /* 519888, example from datasheet p.23 */
    uint8_t read_3_data[] = {0x7E, 0xED, 0x0};
    int32_t temperature = 2508;
    ReadMeasForcedModeTestCfg cfg = {
        .calib_data = default_calib_data,
        .meas_type = BMP280_MEAS_TYPE_ONLY_TEMP,
        .read_1_data = 0x03,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Keeps the 6 MSb the same as read_1_data, and sets the 2 LSb to 01 (forced mode) */
        .write_2_data = 0x01,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .meas_time_ms = 5,
        .read_3_data = read_3_data,
        .read_3_data_size = 3,
        .read_3_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
        .temperature = &temperature,
        .pressure = NULL,
    };
    test_read_meas_forced_mode(&cfg);
}

TEST(BMP280, ReadMeasForcedModeTempAndPres)
{
    /* Pres 415148, temp 519888, example from datasheet p.23 */
    uint8_t read_3_data[] = {0x65, 0x5A, 0xC0, 0x7E, 0xED, 0x0};
    int32_t temperature = 2508;
    /* Should be 25767236 according to the example, but a small difference is expected due to integer calculation
     * rounding errors. */
    uint32_t pressure = 25767233;
    ReadMeasForcedModeTestCfg cfg = {
        .calib_data = default_calib_data,
        .meas_type = BMP280_MEAS_TYPE_TEMP_AND_PRES,
        .read_1_data = 0x01,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Keeps the 6 MSb the same as read_1_data, and sets the 2 LSb to 01 (forced mode) */
        .write_2_data = 0x01,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .meas_time_ms = 5,
        .read_3_data = read_3_data,
        .read_3_data_size = 6,
        .read_3_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
        .temperature = &temperature,
        .pressure = &pressure,
    };
    test_read_meas_forced_mode(&cfg);
}

TEST(BMP280, ReadMeasForcedModeOnlyTemp2)
{
    /* Temp 500000 */
    uint8_t read_3_data[] = {0x7A, 0x12, 0x0};
    int32_t temperature = 1885;
    ReadMeasForcedModeTestCfg cfg = {
        .calib_data = default_calib_data,
        .meas_type = BMP280_MEAS_TYPE_ONLY_TEMP,
        .read_1_data = 0x30,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Keeps the 6 MSb the same as read_1_data, and sets the 2 LSb to 01 (forced mode) */
        .write_2_data = 0x31,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .meas_time_ms = 5,
        .read_3_data = read_3_data,
        .read_3_data_size = 3,
        .read_3_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
        .temperature = &temperature,
        .pressure = NULL,
    };
    test_read_meas_forced_mode(&cfg);
}

TEST(BMP280, ReadMeasForcedModeTempAndPres2)
{
    /* Pres 350000, temp 500000 */
    uint8_t read_3_data[] = {0x55, 0x73, 0x0, 0x7A, 0x12, 0x0};
    int32_t temperature = 1885;
    /* Should be 28376769 according to the example, but a small difference is expected due to integer calculation
     * rounding errors. */
    uint32_t pressure = 28376756;
    ReadMeasForcedModeTestCfg cfg = {
        .calib_data = default_calib_data,
        .meas_type = BMP280_MEAS_TYPE_TEMP_AND_PRES,
        .read_1_data = 0xA6,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Keeps the 6 MSb the same as read_1_data, and sets the 2 LSb to 01 (forced mode) */
        .write_2_data = 0xA5,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .meas_time_ms = 5,
        .read_3_data = read_3_data,
        .read_3_data_size = 6,
        .read_3_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
        .temperature = &temperature,
        .pressure = &pressure,
    };
    test_read_meas_forced_mode(&cfg);
}

TEST(BMP280, ReadMeasForcedModeTempAndPresAltCalib)
{
    /* Pres 415148, temp 519888, example from datasheet p.23 */
    uint8_t read_3_data[] = {0x65, 0x5A, 0xC0, 0x7E, 0xED, 0x0};
    /* These results are taken from the code calculation result, we did not perform the calculation with alternative
     * calibraiton values ourselves. */
    int32_t temperature = 2499;
    uint32_t pressure = 25761933;
    ReadMeasForcedModeTestCfg cfg = {
        .calib_data = alt_calib_data,
        .meas_type = BMP280_MEAS_TYPE_TEMP_AND_PRES,
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Keeps the 6 MSb the same as read_1_data, and sets the 2 LSb to 01 (forced mode) */
        .write_2_data = 0x81,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .meas_time_ms = 5,
        .read_3_data = read_3_data,
        .read_3_data_size = 6,
        .read_3_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
        .temperature = &temperature,
        .pressure = &pressure,
    };
    test_read_meas_forced_mode(&cfg);
}

TEST(BMP280, ReadMeasForcedModeTempAndPresMeasTime50)
{
    /* Pres 415148, temp 519888, example from datasheet p.23 */
    uint8_t read_3_data[] = {0x65, 0x5A, 0xC0, 0x7E, 0xED, 0x0};
    int32_t temperature = 2508;
    /* Should be 25767236 according to the example, but a small difference is expected due to integer calculation
     * rounding errors. */
    uint32_t pressure = 25767233;
    ReadMeasForcedModeTestCfg cfg = {
        .calib_data = default_calib_data,
        .meas_type = BMP280_MEAS_TYPE_TEMP_AND_PRES,
        .read_1_data = 0x01,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Keeps the 6 MSb the same as read_1_data, and sets the 2 LSb to 01 (forced mode) */
        .write_2_data = 0x01,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .meas_time_ms = 50,
        .read_3_data = read_3_data,
        .read_3_data_size = 6,
        .read_3_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
        .temperature = &temperature,
        .pressure = &pressure,
    };
    test_read_meas_forced_mode(&cfg);
}

TEST(BMP280, ReadMeasForcedModeSelfNull)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);
    call_init_meas(default_calib_data);

    BMP280Meas meas;
    uint32_t meas_time_ms = 5;
    uint8_t rc = bmp280_read_meas_forced_mode(NULL, BMP280_MEAS_TYPE_TEMP_AND_PRES, meas_time_ms, &meas,
                                              mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280, ReadMeasForcedModeMeasNull)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);
    call_init_meas(default_calib_data);

    uint32_t meas_time_ms = 5;
    uint8_t rc = bmp280_read_meas_forced_mode(bmp280, BMP280_MEAS_TYPE_TEMP_AND_PRES, meas_time_ms, NULL,
                                              mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280, ReadMeasForcedModeMeasTimeZero)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);
    call_init_meas(default_calib_data);

    BMP280Meas meas;
    uint32_t meas_time_ms = 0;
    uint8_t rc = bmp280_read_meas_forced_mode(bmp280, BMP280_MEAS_TYPE_TEMP_AND_PRES, meas_time_ms, &meas,
                                              mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280, ReadMeasForcedModeInvalidMeasType)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);
    call_init_meas(default_calib_data);

    uint8_t invalid_meas_type = 0x5A;
    BMP280Meas meas;
    uint32_t meas_time_ms = 10;
    uint8_t rc =
        bmp280_read_meas_forced_mode(bmp280, invalid_meas_type, meas_time_ms, &meas, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280, ReadMeasForcedModeCalledBeforeInitMeas)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    BMP280Meas meas;
    uint32_t meas_time_ms = 20;
    uint8_t rc = bmp280_read_meas_forced_mode(bmp280, BMP280_MEAS_TYPE_ONLY_TEMP, meas_time_ms, &meas,
                                              mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_USAGE, rc);
}

TEST(BMP280, ReadMeasForcedModeCalledAfterInitMeasFailed)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    void *complete_cb_user_data = (void *)0xA6;
    /* Called from bmp280_init_meas */
    mock()
        .expectOneCall("mock_bmp280_read_regs")
        .withParameter("start_addr", 0x88)
        .withParameter("num_regs", 24)
        .withOutputParameterReturning("data", default_calib_data, 24)
        .withParameter("user_data", read_regs_user_data)
        .ignoreOtherParameters();
    /* Called from read_regs_complete_cb */
    mock()
        .expectOneCall("mock_bmp280_complete_cb")
        .withParameter("rc", BMP280_RESULT_CODE_IO_ERR)
        .withParameter("user_data", complete_cb_user_data);

    uint8_t rc_init_meas = bmp280_init_meas(bmp280, mock_bmp280_complete_cb, complete_cb_user_data);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_init_meas);
    /* Fail read of calibration values */
    read_regs_complete_cb(BMP280_IO_RESULT_CODE_ERR, read_regs_complete_cb_user_data);

    BMP280Meas meas;
    uint32_t meas_time_ms = 20;
    uint8_t rc = bmp280_read_meas_forced_mode(bmp280, BMP280_MEAS_TYPE_TEMP_AND_PRES, meas_time_ms, &meas,
                                              mock_bmp280_complete_cb, complete_cb_user_data);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_USAGE, rc);
}

static void test_init_meas(uint8_t complete_cb_rc, const uint8_t *const calib_data, uint8_t read_io_rc,
                           BMP280CompleteCb complete_cb)
{
    void *complete_cb_user_data = (void *)0xA4;
    /* Called from bmp280_init_meas */
    mock()
        .expectOneCall("mock_bmp280_read_regs")
        .withParameter("start_addr", 0x88)
        .withParameter("num_regs", 24)
        .withOutputParameterReturning("data", calib_data, 24)
        .withParameter("user_data", read_regs_user_data)
        .ignoreOtherParameters();
    if (complete_cb) {
        /* Called either from read_regs_complete_cb */
        mock()
            .expectOneCall("mock_bmp280_complete_cb")
            .withParameter("rc", complete_cb_rc)
            .withParameter("user_data", complete_cb_user_data);
    }

    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t rc = bmp280_init_meas(bmp280, complete_cb, complete_cb_user_data);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc);

    read_regs_complete_cb(read_io_rc, read_regs_complete_cb_user_data);
}

TEST(BMP280, InitMeasReadFail)
{
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_IO_ERR;
    uint8_t *calib_data = default_calib_data;
    uint8_t read_io_rc = BMP280_IO_RESULT_CODE_ERR;
    BMP280CompleteCb complete_cb = mock_bmp280_complete_cb;
    test_init_meas(complete_cb_rc, calib_data, read_io_rc, complete_cb);
}

TEST(BMP280, InitMeasReadSuccess)
{
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_OK;
    uint8_t *calib_data = default_calib_data;
    uint8_t read_io_rc = BMP280_IO_RESULT_CODE_OK;
    BMP280CompleteCb complete_cb = mock_bmp280_complete_cb;
    test_init_meas(complete_cb_rc, calib_data, read_io_rc, complete_cb);
}

TEST(BMP280, InitMeasCompleteCbNull)
{
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_OK;
    uint8_t *calib_data = default_calib_data;
    uint8_t read_io_rc = BMP280_IO_RESULT_CODE_OK;
    BMP280CompleteCb complete_cb = NULL;
    test_init_meas(complete_cb_rc, calib_data, read_io_rc, complete_cb);
}

TEST(BMP280, InitMeasSelfNull)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t rc = bmp280_init_meas(NULL, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

typedef enum {
    /** Test bmp280_set_temp_oversampling function. */
    SET_OVERSAMPLING_TEST_TYPE_TEMP,
    /** Test bmp280_set_pres_oversampling function. */
    SET_OVERSAMPLING_TEST_TYPE_PRES,
} SetOversamplingTestType;

typedef struct {
    /** One of @ref SetOversamplingTestType. Determines whether setting temperature or pressure oversampling is tested.
     */
    uint8_t test_type;
    /** Oversampling option to pass to bmp280_set_temp_oversampling. Must be one of @ref BMP280Oversampling. */
    uint8_t oversampling;
    /** Data to return from first read regs that reads the ctrl_meas register. */
    uint8_t read_1_data;
    /** IO result code of first read regs that reads the ctrl_meas register. */
    uint8_t read_1_io_rc;
    /** Data to write to ctrl_meas register. */
    uint8_t write_2_data;
    /** IO result code of write reg that writes the ctrl_meas register to set the oversampling option. */
    uint8_t write_2_io_rc;
    /** Complete cb to pass to bmp280_set_temp_oversampling. Two options: mock_bmp280_complete_cb or NULL. If it is
     * mock_bmp280_complete_cb, then the test checks that that function is called with complete_cb_rc. */
    BMP280CompleteCb complete_cb;
    /** Result code to pass to complete_cb. If complete_cb is NULL, this value is not used in the test. */
    uint8_t complete_cb_rc;
} SetOversamplingTestCfg;

static void test_set_oversampling(const SetOversamplingTestCfg *const cfg)
{
    void *complete_cb_user_data = (void *)0xA8;
    /* Called from bmp280_set_temp/pres_oversamlping */
    mock()
        .expectOneCall("mock_bmp280_read_regs")
        .withParameter("start_addr", 0xF4)
        .withParameter("num_regs", 1)
        .withOutputParameterReturning("data", &cfg->read_1_data, 1)
        .withParameter("user_data", read_regs_user_data)
        .ignoreOtherParameters();
    if (cfg->read_1_io_rc == BMP280_IO_RESULT_CODE_OK) {
        /* Called from read_reg_complete_cb */
        mock()
            .expectOneCall("mock_bmp280_write_reg")
            .withParameter("addr", 0xF4)
            .withParameter("reg_val", cfg->write_2_data)
            .withParameter("user_data", write_reg_user_data)
            .ignoreOtherParameters();
    }
    if (cfg->complete_cb) {
        mock()
            .expectOneCall("mock_bmp280_complete_cb")
            .withParameter("rc", cfg->complete_cb_rc)
            .withParameter("user_data", complete_cb_user_data);
    }

    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t rc;
    if (cfg->test_type == SET_OVERSAMPLING_TEST_TYPE_TEMP) {
        bmp280_set_temp_oversampling(bmp280, cfg->oversampling, cfg->complete_cb, complete_cb_user_data);
    } else if (cfg->test_type == SET_OVERSAMPLING_TEST_TYPE_PRES) {
        bmp280_set_pres_oversampling(bmp280, cfg->oversampling, cfg->complete_cb, complete_cb_user_data);
    } else {
        FAIL_TEST("Invalid set oversampling test type");
    }
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc);

    read_regs_complete_cb(cfg->read_1_io_rc, read_regs_complete_cb_user_data);
    if (cfg->read_1_io_rc == BMP280_IO_RESULT_CODE_OK) {
        write_reg_complete_cb(cfg->write_2_io_rc, write_reg_complete_cb_user_data);
    }
}

TEST(BMP280, SetTempOversamplingReadFail)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_TEMP,
        .oversampling = BMP280_OVERSAMPLING_4,
        /* Does not matter, read fails */
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_ERR,
        /* Does not matter */
        .write_2_data = 0x81,
        /* Does not matter */
        .write_2_io_rc = BMP280_IO_RESULT_CODE_ERR,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_IO_ERR,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetTempOversamplingWriteFail)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_TEMP,
        .oversampling = BMP280_OVERSAMPLING_4,
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set 3 MSb to 011 (oversampling x4), keep other bits the same */
        .write_2_data = 0x60,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_ERR,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_IO_ERR,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetTempOversamplingOversampling4)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_TEMP,
        .oversampling = BMP280_OVERSAMPLING_4,
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set 3 MSb to 011 (oversampling x4), keep other bits the same */
        .write_2_data = 0x60,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetTempOversamplingOversampling4AltReadData)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_TEMP,
        .oversampling = BMP280_OVERSAMPLING_4,
        .read_1_data = 0x1C,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set 3 MSb to 011 (oversampling x4), keep other bits the same */
        .write_2_data = 0x7C,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetTempOversamplingOversampling2)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_TEMP,
        .oversampling = BMP280_OVERSAMPLING_2,
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set 3 MSb to 010 (oversampling x2), keep other bits the same */
        .write_2_data = 0x40,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetTempOversamplingOversampling1)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_TEMP,
        .oversampling = BMP280_OVERSAMPLING_1,
        .read_1_data = 0xFF,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set 3 MSb to 001 (oversampling x1), keep other bits the same */
        .write_2_data = 0x3F,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetTempOversamplingOversampling8)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_TEMP,
        .oversampling = BMP280_OVERSAMPLING_8,
        .read_1_data = 0x5A,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set 3 MSb to 100 (oversampling x8), keep other bits the same */
        .write_2_data = 0x9A,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetTempOversamplingOversampling16)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_TEMP,
        .oversampling = BMP280_OVERSAMPLING_16,
        .read_1_data = 0x33,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set 3 MSb to 101 (oversampling x16), keep other bits the same */
        .write_2_data = 0xB3,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetTempOversamplingOversamplingSkipped)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_TEMP,
        .oversampling = BMP280_OVERSAMPLING_SKIPPED,
        .read_1_data = 0x6A,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set 3 MSb to 000 (oversampling skipped), keep other bits the same */
        .write_2_data = 0x0A,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetTempOversamplingCbNull)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_TEMP,
        .oversampling = BMP280_OVERSAMPLING_SKIPPED,
        .read_1_data = 0x6A,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set 3 MSb to 000 (oversampling skipped), keep other bits the same */
        .write_2_data = 0x0A,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = NULL,
        /* Does not matter */
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetTempOversamlpingSelfNull)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t rc = bmp280_set_temp_oversampling(NULL, BMP280_OVERSAMPLING_1, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280, SetTempOversamlpingInvalidOversampling)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t invalid_oversampling = 0x42;
    uint8_t rc = bmp280_set_temp_oversampling(bmp280, invalid_oversampling, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280, SetPresOversamplingReadFail)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_PRES,
        .oversampling = BMP280_OVERSAMPLING_4,
        /* Does not matter, read fails */
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_ERR,
        /* Does not matter */
        .write_2_data = 0x81,
        /* Does not matter */
        .write_2_io_rc = BMP280_IO_RESULT_CODE_ERR,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_IO_ERR,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetPresOversamplingWriteFail)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_PRES,
        .oversampling = BMP280_OVERSAMPLING_4,
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 011 (oversampling x4), keep other bits the same */
        .write_2_data = 0x8C,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_ERR,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_IO_ERR,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetPresOversamplingOversampling4)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_PRES,
        .oversampling = BMP280_OVERSAMPLING_4,
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 011 (oversampling x4), keep other bits the same */
        .write_2_data = 0x8C,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetPresOversamplingOversampling4AltReadData)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_PRES,
        .oversampling = BMP280_OVERSAMPLING_4,
        .read_1_data = 0x1C,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 011 (oversampling x4), keep other bits the same */
        .write_2_data = 0x0C,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetPresOversamplingOversampling2)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_PRES,
        .oversampling = BMP280_OVERSAMPLING_2,
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 010 (oversampling x2), keep other bits the same */
        .write_2_data = 0x88,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetPresOversamplingOversampling1)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_PRES,
        .oversampling = BMP280_OVERSAMPLING_1,
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 001 (oversampling x1), keep other bits the same */
        .write_2_data = 0x84,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetPresOversamplingOversampling8)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_PRES,
        .oversampling = BMP280_OVERSAMPLING_8,
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 100 (oversampling x8), keep other bits the same */
        .write_2_data = 0x90,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetPresOversamplingOversampling16)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_PRES,
        .oversampling = BMP280_OVERSAMPLING_16,
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 101 (oversampling x16), keep other bits the same */
        .write_2_data = 0x94,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetPresOversamplingOversamplingSkipped)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_PRES,
        .oversampling = BMP280_OVERSAMPLING_SKIPPED,
        .read_1_data = 0x98,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 000 (oversampling skipped), keep other bits the same */
        .write_2_data = 0x80,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetPresOversamplingCbNull)
{
    SetOversamplingTestCfg cfg = {
        .test_type = SET_OVERSAMPLING_TEST_TYPE_PRES,
        .oversampling = BMP280_OVERSAMPLING_SKIPPED,
        .read_1_data = 0x98,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 000 (oversampling skipped), keep other bits the same */
        .write_2_data = 0x80,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = NULL,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_oversampling(&cfg);
}

TEST(BMP280, SetPresOversamlpingSelfNull)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t rc = bmp280_set_pres_oversampling(NULL, BMP280_OVERSAMPLING_1, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280, SetPresOversamlpingInvalidOversampling)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t invalid_oversampling = 0x24;
    uint8_t rc = bmp280_set_pres_oversampling(bmp280, invalid_oversampling, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

typedef struct {
    /** Filter coefficient to pass to bmp280_set_filter_coefficient. Must be one of @ref BMP280FilterCoeff. */
    uint8_t filter_coeff;
    /** Data to return from first read regs that reads the config register. */
    uint8_t read_1_data;
    /** IO result code of first read regs that reads the config register. */
    uint8_t read_1_io_rc;
    /** Data to write to config register. */
    uint8_t write_2_data;
    /** IO result code of write reg that writes the config register to set the filter coefficient. */
    uint8_t write_2_io_rc;
    /** Complete cb to pass to bmp280_set_temp_oversampling. Two options: mock_bmp280_complete_cb or NULL. If it is
     * mock_bmp280_complete_cb, then the test checks that that function is called with complete_cb_rc. */
    BMP280CompleteCb complete_cb;
    /** Result code to pass to complete_cb. If complete_cb is NULL, this value is not used in the test. */
    uint8_t complete_cb_rc;
} SetFilterCoeffTestCfg;

static void test_set_filter_coefficient(const SetFilterCoeffTestCfg *const cfg)
{
    void *complete_cb_user_data = (void *)0xA9;
    /* Called from bmp280_set_filter_coefficient */
    mock()
        .expectOneCall("mock_bmp280_read_regs")
        .withParameter("start_addr", 0xF5)
        .withParameter("num_regs", 1)
        .withOutputParameterReturning("data", &cfg->read_1_data, 1)
        .withParameter("user_data", read_regs_user_data)
        .ignoreOtherParameters();
    if (cfg->read_1_io_rc == BMP280_IO_RESULT_CODE_OK) {
        /* Called from read_reg_complete_cb */
        mock()
            .expectOneCall("mock_bmp280_write_reg")
            .withParameter("addr", 0xF5)
            .withParameter("reg_val", cfg->write_2_data)
            .withParameter("user_data", write_reg_user_data)
            .ignoreOtherParameters();
    }
    if (cfg->complete_cb) {
        mock()
            .expectOneCall("mock_bmp280_complete_cb")
            .withParameter("rc", cfg->complete_cb_rc)
            .withParameter("user_data", complete_cb_user_data);
    }

    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t rc = bmp280_set_filter_coefficient(bmp280, cfg->filter_coeff, cfg->complete_cb, complete_cb_user_data);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc);

    read_regs_complete_cb(cfg->read_1_io_rc, read_regs_complete_cb_user_data);
    if (cfg->read_1_io_rc == BMP280_IO_RESULT_CODE_OK) {
        write_reg_complete_cb(cfg->write_2_io_rc, write_reg_complete_cb_user_data);
    }
}

TEST(BMP280, SetFiterCoeffReadFail)
{
    SetFilterCoeffTestCfg cfg = {
        .filter_coeff = BMP280_FILTER_COEFF_FILTER_OFF,
        /* Does not matter, read fails */
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_ERR,
        /* Does not matter */
        .write_2_data = 0x81,
        /* Does not matter */
        .write_2_io_rc = BMP280_IO_RESULT_CODE_ERR,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_IO_ERR,
    };
    test_set_filter_coefficient(&cfg);
}

TEST(BMP280, SetFiterCoeffWriteFail)
{
    SetFilterCoeffTestCfg cfg = {
        .filter_coeff = BMP280_FILTER_COEFF_FILTER_OFF,
        .read_1_data = 0x88,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 000 (filter off), keep other bits the same */
        .write_2_data = 0x80,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_ERR,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_IO_ERR,
    };
    test_set_filter_coefficient(&cfg);
}

TEST(BMP280, SetFiterCoeffFilterOff)
{
    SetFilterCoeffTestCfg cfg = {
        .filter_coeff = BMP280_FILTER_COEFF_FILTER_OFF,
        .read_1_data = 0x88,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 000 (filter off), keep other bits the same */
        .write_2_data = 0x80,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_filter_coefficient(&cfg);
}

TEST(BMP280, SetFiterCoeffFilterOffAltReadData)
{
    SetFilterCoeffTestCfg cfg = {
        .filter_coeff = BMP280_FILTER_COEFF_FILTER_OFF,
        .read_1_data = 0xFF,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 000 (filter off), keep other bits the same */
        .write_2_data = 0xE3,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_filter_coefficient(&cfg);
}

TEST(BMP280, SetFiterCoeff2)
{
    SetFilterCoeffTestCfg cfg = {
        .filter_coeff = BMP280_FILTER_COEFF_2,
        .read_1_data = 0x5A,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 001 (filter coeff 2), keep other bits the same */
        .write_2_data = 0x46,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_filter_coefficient(&cfg);
}

TEST(BMP280, SetFiterCoeff4)
{
    SetFilterCoeffTestCfg cfg = {
        .filter_coeff = BMP280_FILTER_COEFF_4,
        .read_1_data = 0x00,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 010 (filter coeff 4), keep other bits the same */
        .write_2_data = 0x08,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_filter_coefficient(&cfg);
}

TEST(BMP280, SetFiterCoeff8)
{
    SetFilterCoeffTestCfg cfg = {
        .filter_coeff = BMP280_FILTER_COEFF_8,
        .read_1_data = 0x33,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 011 (filter coeff 8), keep other bits the same */
        .write_2_data = 0x2F,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_filter_coefficient(&cfg);
}

TEST(BMP280, SetFiterCoeff16)
{
    SetFilterCoeffTestCfg cfg = {
        .filter_coeff = BMP280_FILTER_COEFF_16,
        .read_1_data = 0x44,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 100 (filter coeff 16), keep other bits the same */
        .write_2_data = 0x50,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_filter_coefficient(&cfg);
}

TEST(BMP280, SetFiterCoeffCbNull)
{
    SetFilterCoeffTestCfg cfg = {
        .filter_coeff = BMP280_FILTER_COEFF_16,
        .read_1_data = 0x44,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bits[4:2] to 100 (filter coeff 16), keep other bits the same */
        .write_2_data = 0x50,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = NULL,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_filter_coefficient(&cfg);
}

TEST(BMP280, SetFiterCoeffSelfNull)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t rc = bmp280_set_filter_coefficient(NULL, BMP280_FILTER_COEFF_4, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280, SetFiterCoeffInvalidFilterCoeff)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t invalid_filter_coeff = 0x56;
    uint8_t rc = bmp280_set_filter_coefficient(bmp280, invalid_filter_coeff, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

typedef struct {
    /** SPI 3 wire option to pass to bmp280_set_spi_3_wire_interface. Must be one of @ref BMP280Spi3Wire. */
    uint8_t spi_3_wire;
    /** Data to return from first read regs that reads the config register. */
    uint8_t read_1_data;
    /** IO result code of first read regs that reads the config register. */
    uint8_t read_1_io_rc;
    /** Data to write to config register. */
    uint8_t write_2_data;
    /** IO result code of write reg that writes the config register to set the SPI 3 wire mode. */
    uint8_t write_2_io_rc;
    /** Complete cb to pass to bmp280_set_temp_oversampling. Two options: mock_bmp280_complete_cb or NULL. If it is
     * mock_bmp280_complete_cb, then the test checks that that function is called with complete_cb_rc. */
    BMP280CompleteCb complete_cb;
    /** Result code to pass to complete_cb. If complete_cb is NULL, this value is not used in the test. */
    uint8_t complete_cb_rc;
} SetSpi3WireTestCfg;

static void test_set_spi_3_wire_interface(const SetSpi3WireTestCfg *const cfg)
{
    void *complete_cb_user_data = (void *)0xAA;
    /* Called from bmp280_set_spi_3_wire_interface */
    mock()
        .expectOneCall("mock_bmp280_read_regs")
        .withParameter("start_addr", 0xF5)
        .withParameter("num_regs", 1)
        .withOutputParameterReturning("data", &cfg->read_1_data, 1)
        .withParameter("user_data", read_regs_user_data)
        .ignoreOtherParameters();
    if (cfg->read_1_io_rc == BMP280_IO_RESULT_CODE_OK) {
        /* Called from read_reg_complete_cb */
        mock()
            .expectOneCall("mock_bmp280_write_reg")
            .withParameter("addr", 0xF5)
            .withParameter("reg_val", cfg->write_2_data)
            .withParameter("user_data", write_reg_user_data)
            .ignoreOtherParameters();
    }
    if (cfg->complete_cb) {
        mock()
            .expectOneCall("mock_bmp280_complete_cb")
            .withParameter("rc", cfg->complete_cb_rc)
            .withParameter("user_data", complete_cb_user_data);
    }

    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t rc = bmp280_set_spi_3_wire_interface(bmp280, cfg->spi_3_wire, cfg->complete_cb, complete_cb_user_data);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc);

    read_regs_complete_cb(cfg->read_1_io_rc, read_regs_complete_cb_user_data);
    if (cfg->read_1_io_rc == BMP280_IO_RESULT_CODE_OK) {
        write_reg_complete_cb(cfg->write_2_io_rc, write_reg_complete_cb_user_data);
    }
}

TEST(BMP280, SetSpi3WireReadFail)
{
    SetSpi3WireTestCfg cfg = {
        .spi_3_wire = BMP280_SPI_3_WIRE_DIS,
        /* Does not matter, read fails */
        .read_1_data = 0x80,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_ERR,
        /* Does not matter */
        .write_2_data = 0x81,
        /* Does not matter */
        .write_2_io_rc = BMP280_IO_RESULT_CODE_ERR,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_IO_ERR,
    };
    test_set_spi_3_wire_interface(&cfg);
}

TEST(BMP280, SetSpi3WireWriteFail)
{
    SetSpi3WireTestCfg cfg = {
        .spi_3_wire = BMP280_SPI_3_WIRE_DIS,
        .read_1_data = 0x89,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bit 0 to 0 (spi 3 wire disabled), keep other bits the same */
        .write_2_data = 0x88,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_ERR,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_IO_ERR,
    };
    test_set_spi_3_wire_interface(&cfg);
}

TEST(BMP280, SetSpi3WireDis)
{
    SetSpi3WireTestCfg cfg = {
        .spi_3_wire = BMP280_SPI_3_WIRE_DIS,
        .read_1_data = 0x89,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bit 0 to 0 (spi 3 wire disabled), keep other bits the same */
        .write_2_data = 0x88,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_spi_3_wire_interface(&cfg);
}

TEST(BMP280, SetSpi3WireDisAltReadData)
{
    SetSpi3WireTestCfg cfg = {
        .spi_3_wire = BMP280_SPI_3_WIRE_DIS,
        .read_1_data = 0xF0,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bit 0 to 0 (spi 3 wire disabled), keep other bits the same */
        .write_2_data = 0xF0,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_spi_3_wire_interface(&cfg);
}

TEST(BMP280, SetSpi3WireEn)
{
    SetSpi3WireTestCfg cfg = {
        .spi_3_wire = BMP280_SPI_3_WIRE_EN,
        .read_1_data = 0xFE,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bit 0 to 1 (spi 3 wire enabled), keep other bits the same */
        .write_2_data = 0xFF,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_spi_3_wire_interface(&cfg);
}

TEST(BMP280, SetSpi3WireEnAltReadData)
{
    SetSpi3WireTestCfg cfg = {
        .spi_3_wire = BMP280_SPI_3_WIRE_EN,
        .read_1_data = 0xE1,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bit 0 to 1 (spi 3 wire enabled), keep other bits the same */
        .write_2_data = 0xE1,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = mock_bmp280_complete_cb,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_spi_3_wire_interface(&cfg);
}

TEST(BMP280, SetSpi3WireCbNull)
{
    SetSpi3WireTestCfg cfg = {
        .spi_3_wire = BMP280_SPI_3_WIRE_EN,
        .read_1_data = 0xE1,
        .read_1_io_rc = BMP280_IO_RESULT_CODE_OK,
        /* Set bit 0 to 1 (spi 3 wire enabled), keep other bits the same */
        .write_2_data = 0xE1,
        .write_2_io_rc = BMP280_IO_RESULT_CODE_OK,
        .complete_cb = NULL,
        .complete_cb_rc = BMP280_RESULT_CODE_OK,
    };
    test_set_spi_3_wire_interface(&cfg);
}

TEST(BMP280, SetSpi3WireSelfNull)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t rc = bmp280_set_spi_3_wire_interface(NULL, BMP280_SPI_3_WIRE_DIS, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280, SetSpi3WireInvalidSpi3WireOption)
{
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    uint8_t invalid_spi_3_wire = 0x99;
    uint8_t rc = bmp280_set_spi_3_wire_interface(bmp280, invalid_spi_3_wire, mock_bmp280_complete_cb, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

typedef uint8_t (*BMP280Function)();

static void test_busy_if_seq_in_progress(BMP280Function function)
{
    void *complete_cb_user_data = (void *)0xA9;

    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);
    call_init_meas(default_calib_data);

    /* Does not matter */
    uint8_t read_data = 0x55;
    /* Called from bmp280_set_filter_coefficient */
    mock()
        .expectOneCall("mock_bmp280_read_regs")
        .withParameter("start_addr", 0xF5)
        .withParameter("num_regs", 1)
        .withOutputParameterReturning("data", &read_data, 1)
        .withParameter("user_data", read_regs_user_data)
        .ignoreOtherParameters();

    uint8_t rc_set_filter_coeff = bmp280_set_filter_coefficient(bmp280, BMP280_FILTER_COEFF_2, NULL, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_set_filter_coeff);
    /* Read regs complete callback is not executed yet, so "set filter coefficient" sequence is still in progres. The
     * driver should reject attempts to start new sequences. */

    uint8_t rc = function();
    CHECK_EQUAL(BMP280_RESULT_CODE_BUSY, rc);
}

static uint8_t get_chip_id()
{
    uint8_t chip_id;
    return bmp280_get_chip_id(bmp280, &chip_id, mock_bmp280_complete_cb, NULL);
}

TEST(BMP280, GetChipIdBusy)
{
    test_busy_if_seq_in_progress(get_chip_id);
}

static uint8_t reset_with_delay()
{
    return bmp280_reset_with_delay(bmp280, mock_bmp280_complete_cb, NULL);
}

TEST(BMP280, ResetWithDelayBusy)
{
    test_busy_if_seq_in_progress(reset_with_delay);
}

static uint8_t init_meas()
{
    return bmp280_init_meas(bmp280, mock_bmp280_complete_cb, NULL);
}

TEST(BMP280, InitMeasBusy)
{
    test_busy_if_seq_in_progress(init_meas);
}

static uint8_t read_meas_forced_mode()
{
    BMP280Meas meas;
    return bmp280_read_meas_forced_mode(bmp280, BMP280_MEAS_TYPE_TEMP_AND_PRES, 20, &meas, mock_bmp280_complete_cb,
                                        NULL);
}

TEST(BMP280, ReadMeasForcedModeBusy)
{
    test_busy_if_seq_in_progress(read_meas_forced_mode);
}

static uint8_t set_temp_oversampling()
{
    return bmp280_set_temp_oversampling(bmp280, BMP280_OVERSAMPLING_1, mock_bmp280_complete_cb, NULL);
}

TEST(BMP280, SetTempOversamplingBusy)
{
    test_busy_if_seq_in_progress(set_temp_oversampling);
}

static uint8_t set_pres_oversampling()
{
    return bmp280_set_pres_oversampling(bmp280, BMP280_OVERSAMPLING_4, mock_bmp280_complete_cb, NULL);
}

TEST(BMP280, SetPresOversamplingBusy)
{
    test_busy_if_seq_in_progress(set_pres_oversampling);
}

static uint8_t set_filter_coefficient()
{
    return bmp280_set_filter_coefficient(bmp280, BMP280_FILTER_COEFF_16, mock_bmp280_complete_cb, NULL);
}

TEST(BMP280, SetFilterCoefficientBusy)
{
    test_busy_if_seq_in_progress(set_filter_coefficient);
}

static uint8_t set_spi_3_wire_interface()
{
    return bmp280_set_spi_3_wire_interface(bmp280, BMP280_SPI_3_WIRE_EN, mock_bmp280_complete_cb, NULL);
}

TEST(BMP280, SetSpi3WireInterfaceBusy)
{
    test_busy_if_seq_in_progress(set_spi_3_wire_interface);
}

static void test_read_seq_cannot_be_interrupted(uint8_t read_1_start_reg, size_t read_1_num_regs, uint8_t *read_1_data,
                                                uint8_t read_1_rc, BMP280Function start_seq)
{
    if (!start_seq) {
        FAIL_TEST("Invalid args");
    }
    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

    mock()
        .expectOneCall("mock_bmp280_read_regs")
        .withParameter("start_addr", read_1_start_reg)
        .withParameter("num_regs", read_1_num_regs)
        .withOutputParameterReturning("data", read_1_data, 1)
        .withParameter("user_data", read_regs_user_data)
        .ignoreOtherParameters();

    uint8_t rc = start_seq();
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc);

    uint8_t other_cmd_rc;
    other_cmd_rc = bmp280_set_temp_oversampling(bmp280, BMP280_OVERSAMPLING_1, NULL, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_BUSY, other_cmd_rc);

    mock().expectOneCall("mock_bmp280_complete_cb").ignoreOtherParameters();
    read_regs_complete_cb(read_1_rc, read_regs_complete_cb_user_data);

    /* Sequence finished, other operations are now allowed */
    /* Exact value does not matter */
    uint8_t read_2_data = 0x46;
    mock()
        .expectOneCall("mock_bmp280_read_regs")
        .withParameter("start_addr", 0xF4)
        .withParameter("num_regs", 1)
        .withOutputParameterReturning("data", &read_2_data, 1)
        .withParameter("user_data", read_regs_user_data)
        .ignoreOtherParameters();
    other_cmd_rc = bmp280_set_temp_oversampling(bmp280, BMP280_OVERSAMPLING_1, NULL, NULL);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, other_cmd_rc);
}

TEST(BMP280, GetChipIdCannotBeInterruptedReadFail)
{
    uint8_t read_1_start_reg = 0xD0;
    size_t read_1_num_regs = 1;
    uint8_t read_1_data = 0x58;
    uint8_t read_1_rc = BMP280_IO_RESULT_CODE_ERR;
    test_read_seq_cannot_be_interrupted(read_1_start_reg, read_1_num_regs, &read_1_data, read_1_rc, get_chip_id);
}

TEST(BMP280, GetChipIdCannotBeInterruptedReadSuccess)
{
    uint8_t read_1_start_reg = 0xD0;
    size_t read_1_num_regs = 1;
    uint8_t read_1_data = 0x58;
    uint8_t read_1_rc = BMP280_IO_RESULT_CODE_OK;
    test_read_seq_cannot_be_interrupted(read_1_start_reg, read_1_num_regs, &read_1_data, read_1_rc, get_chip_id);
}

TEST(BMP280, InitMeasCannotBeInterruptedReadFail)
{
    uint8_t read_1_start_reg = 0x88;
    size_t read_1_num_regs = 24;
    uint8_t *read_1_data = default_calib_data;
    uint8_t read_1_rc = BMP280_IO_RESULT_CODE_ERR;
    test_read_seq_cannot_be_interrupted(read_1_start_reg, read_1_num_regs, read_1_data, read_1_rc, init_meas);
}

TEST(BMP280, InitMeasCannotBeInterruptedReadSuccess)
{
    uint8_t read_1_start_reg = 0x88;
    size_t read_1_num_regs = 24;
    uint8_t *read_1_data = default_calib_data;
    uint8_t read_1_rc = BMP280_IO_RESULT_CODE_OK;
    test_read_seq_cannot_be_interrupted(read_1_start_reg, read_1_num_regs, read_1_data, read_1_rc, init_meas);
}
