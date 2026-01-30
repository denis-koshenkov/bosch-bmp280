#include <string.h>

#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "bmp280.h"
/* To include the definition of struct BMP280Struct, so that we can define an instance to return from
 * mock_bmp280_get_inst_buf. */
#include "bmp280_private.h"
#include "mock_cfg_functions.h"
#include "mock_complete_cb.h"

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

static void test_read_meas_forced_mode(const ReadMeasForcedModeTestCfg *const cfg)
{
    void *complete_cb_user_data = (void *)0xA2;
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
                .withOutputParameterReturning("data", &cfg->read_3_data, cfg->read_3_data_size)
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

    uint8_t rc_create = bmp280_create(&bmp280, &init_cfg);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc_create);

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
