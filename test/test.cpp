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
  env_.reset(new cbox::env());
  ASSERT_EQ(env_->init(), 0);
}

void cbox_test::TearDown()
{
  env_.release();
  spdlog::drop_all();
}

TEST_F(cbox_test, NoPathNonExistingInputFile)
{
  env_->event_log_->set_level(spdlog::level::level_enum::off);
  env_->cfg_.in_name = "in.json";
  ASSERT_EQ(env_->exec(), 1);
}

TEST_F(cbox_test, PathNonExistingInputFile)
{
  env_->event_log_->set_level(spdlog::level::level_enum::off);
  env_->cfg_.in_path = "empty";
  env_->cfg_.in_name = "in.json";
  ASSERT_EQ(env_->exec(), 1);
}

TEST_F(cbox_test, MalformedJson)
{
  env_->event_log_->set_level(spdlog::level::level_enum::off);
  env_->cfg_.in_path = "scenarios";
  env_->cfg_.in_name = "0_malformed.json";
  ASSERT_EQ(env_->exec(), 1);
}

TEST_F(cbox_test, HEAD_1Conv_1Req)
{
  env_->event_log_->set_level(spdlog::level::level_enum::off);
  env_->cfg_.no_out_ = true;
  env_->cfg_.in_path = "scenarios";
  env_->cfg_.in_name = "1_head1conv1req.json";
  ASSERT_EQ(env_->exec(), 0);
}
