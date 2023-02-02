#pragma once

#include "gtest/gtest.h"
#include "scenario.h"

class cbox_test : public ::testing::Test {
  protected:
    std::unique_ptr<cbox::scenario> cbox;
    virtual void SetUp();
    virtual void TearDown();
};
