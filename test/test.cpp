#include "test.h"

int argc_ = 0;
const char **argv_ = nullptr;

int main(int argc, char *argv[])
{
  argc_ = argc;
  argv_ = (const char **)argv;

  testing::InitGoogleTest(&argc, argv);
  EXPECT_TRUE(js::js_env::init_V8(argc_, argv_));

  int res = RUN_ALL_TESTS();

  js::js_env::stop_V8();
  return res;
}

void cbox_test::SetUp()
{
  cbox.reset(new rest::chatterbox());
  ASSERT_EQ(cbox->init(argc_, argv_), 0);
}

void cbox_test::TearDown()
{
  cbox.release();
  spdlog::drop_all();
}

TEST_F(cbox_test, NoPathNonExistingInputFile)
{
  cbox->event_log_->set_level(spdlog::level::level_enum::off);
  cbox->cfg_.in_scenario_name = "in.json";
  ASSERT_EQ(cbox->process_scenario(), 1);
}

TEST_F(cbox_test, PathNonExistingInputFile)
{
  cbox->event_log_->set_level(spdlog::level::level_enum::off);
  cbox->cfg_.in_scenario_path = "empty";
  cbox->cfg_.in_scenario_name = "in.json";
  ASSERT_EQ(cbox->process_scenario(), 1);
}

TEST_F(cbox_test, MalformedJson)
{
  cbox->event_log_->set_level(spdlog::level::level_enum::off);
  cbox->cfg_.in_scenario_path = "scenarios";
  cbox->cfg_.in_scenario_name = "0_malformed.json";
  ASSERT_EQ(cbox->process_scenario(), 1);
}

TEST_F(cbox_test, HEAD_1Conv_1Req)
{
  cbox->event_log_->set_level(spdlog::level::level_enum::off);
  cbox->output_->set_level(spdlog::level::level_enum::off);
  cbox->cfg_.in_scenario_path = "scenarios";
  cbox->cfg_.in_scenario_name = "1_head1conv1req.json";
  ASSERT_EQ(cbox->process_scenario(), 0);
}
