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

/* BMP280 instance used by all tests */
static BMP280 bmp280;
/* Init cfg used by all tests */
static BMP280InitCfg init_cfg;

/* Populated by mock object whenever mock_bmp280_read_reg is called */
static BMP280_IOCompleteCb read_regs_complete_cb;
static void *read_regs_complete_cb_user_data;

static void populate_default_init_cfg(BMP280InitCfg *const cfg)
{
    cfg->get_inst_buf = mock_bmp280_get_inst_buf;
    cfg->get_inst_buf_user_data = get_inst_buf_user_data;
    cfg->read_regs = mock_bmp280_read_regs;
    cfg->read_regs_user_data = read_regs_user_data;
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

        /* Pass pointers so that the mock object populates them with callbacks and user data, so that the test can simulate
         * calling these callbacks. */
        mock().setData("readRegsCompleteCb", (void *)&read_regs_complete_cb);
        mock().setData("readRegsCompleteCbUserData", &read_regs_complete_cb_user_data);

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

static void test_get_chip_id(uint8_t read_regs_data, uint8_t complete_cb_rc, uint8_t read_io_rc)
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
    uint8_t rc = bmp280_get_chip_id(bmp280, &chip_id, mock_bmp280_complete_cb, complete_cb_user_data);
    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc);

    mock()
        .expectOneCall("mock_bmp280_complete_cb")
        .withParameter("rc", complete_cb_rc)
        .withParameter("user_data", complete_cb_user_data);
    read_regs_complete_cb(read_io_rc, read_regs_complete_cb_user_data);
}

TEST(BMP280, GetChipIdReadFail)
{
    /* Value does not matter since read fails */
    uint8_t read_regs_data = 0x42;
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_IO_ERR;
    /* Fail read regs */
    uint8_t read_io_rc = BMP280_IO_RESULT_CODE_ERR;
    test_get_chip_id(read_regs_data, complete_cb_rc, read_io_rc);
}

TEST(BMP280, GetChipIdReadSuccess)
{
    /* 0x58 is the expected chip id */
    uint8_t read_regs_data = 0x58;
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_OK;
    uint8_t read_io_rc = BMP280_IO_RESULT_CODE_OK;
    test_get_chip_id(read_regs_data, complete_cb_rc, read_io_rc);
}

TEST(BMP280, GetChipIdReadWrongChipId)
{
    /* Chip id is not the expected one. Our function should still succeed, since it only reads out the chip id, but does
     * not check for its correctness. */
    uint8_t read_regs_data = 0x59;
    uint8_t complete_cb_rc = BMP280_RESULT_CODE_OK;
    uint8_t read_io_rc = BMP280_IO_RESULT_CODE_OK;
    test_get_chip_id(read_regs_data, complete_cb_rc, read_io_rc);
}
