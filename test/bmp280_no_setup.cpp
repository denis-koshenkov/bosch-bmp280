#include "CppUTest/TestHarness.h"
#include "CppUTestExt/MockSupport.h"

#include "bmp280.h"
/* To include the definition of struct BMP280Struct, so that we can define an instance to return from
 * mock_bmp280_get_inst_buf. */
#include "bmp280_private.h"
#include "mock_cfg_functions.h"

/* To return from mock_bmp280_get_inst_buf */
static struct BMP280Struct inst_buf;

/* User data parameters to pass to bmp280_create in the init cfg */
static void *get_inst_buf_user_data = (void *)0x80;
static void *read_regs_user_data = (void *)0x81;
static void *write_reg_user_data = (void *)0x82;
static void *start_timer_user_data = (void *)0x83;

// clang-format off
TEST_GROUP(BMP280NoSetup){
    void setup() {
        /* Order of expected calls is important for these tests. Fail the test if the expected mock calls do not happen
        in the specified order. */
        mock().strictOrder();
    }
};
// clang-format on

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

TEST(BMP280NoSetup, CreateReturnsBufReturnedFromGetInstBuf)
{
    mock()
        .expectOneCall("mock_bmp280_get_inst_buf")
        .withParameter("user_data", get_inst_buf_user_data)
        .andReturnValue((void *)&inst_buf);

    BMP280 bmp280;
    BMP280InitCfg cfg;
    populate_default_init_cfg(&cfg);
    uint8_t rc = bmp280_create(&bmp280, &cfg);

    CHECK_EQUAL(BMP280_RESULT_CODE_OK, rc);
    CHECK_EQUAL((void *)&inst_buf, (void *)bmp280);
}

TEST(BMP280NoSetup, CreateReturnsNoMemWhenGetInstBufReturnsNull)
{
    mock()
        .expectOneCall("mock_bmp280_get_inst_buf")
        .withParameter("user_data", get_inst_buf_user_data)
        .andReturnValue((void *)NULL);

    BMP280 bmp280;
    BMP280InitCfg cfg;
    populate_default_init_cfg(&cfg);
    uint8_t rc = bmp280_create(&bmp280, &cfg);

    CHECK_EQUAL(BMP280_RESULT_CODE_NO_MEM, rc);
}

TEST(BMP280NoSetup, CreateReturnsInvalArgWhenCfgIsNull)
{
    BMP280 bmp280;
    uint8_t rc = bmp280_create(&bmp280, NULL);

    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280NoSetup, CreateReturnsInvalArgInstNull)
{
    BMP280InitCfg cfg;
    populate_default_init_cfg(&cfg);
    uint8_t rc = bmp280_create(NULL, &cfg);

    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280NoSetup, CreateGetInstBufNull)
{
    BMP280 bmp280;
    BMP280InitCfg cfg;
    populate_default_init_cfg(&cfg);
    cfg.get_inst_buf = NULL;
    uint8_t rc = bmp280_create(&bmp280, &cfg);

    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280NoSetup, CreateReadRegsNull)
{
    BMP280 bmp280;
    BMP280InitCfg cfg;
    populate_default_init_cfg(&cfg);
    cfg.read_regs = NULL;
    uint8_t rc = bmp280_create(&bmp280, &cfg);

    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280NoSetup, CreateWriteRegNull)
{
    BMP280 bmp280;
    BMP280InitCfg cfg;
    populate_default_init_cfg(&cfg);
    cfg.write_reg = NULL;
    uint8_t rc = bmp280_create(&bmp280, &cfg);

    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}

TEST(BMP280NoSetup, CreateStartTimerNull)
{
    BMP280 bmp280;
    BMP280InitCfg cfg;
    populate_default_init_cfg(&cfg);
    cfg.start_timer = NULL;
    uint8_t rc = bmp280_create(&bmp280, &cfg);

    CHECK_EQUAL(BMP280_RESULT_CODE_INVAL_ARG, rc);
}
