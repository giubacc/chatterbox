#pragma once

#include "gtest/gtest.h"
#include "chatterbox.h"

class cbox_test : public ::testing::Test {
  protected:
    std::unique_ptr<rest::chatterbox> cbox;
    virtual void SetUp();
    virtual void TearDown();
};
