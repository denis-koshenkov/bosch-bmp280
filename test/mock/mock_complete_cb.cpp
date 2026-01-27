#include "CppUTestExt/MockSupport.h"
#include "mock_complete_cb.h"

void mock_bmp280_complete_cb(uint8_t rc, void *user_data)
{
    mock().actualCall("mock_bmp280_complete_cb").withParameter("rc", rc).withParameter("user_data", user_data);
}
