#include <gtest/gtest.h>
#include "helpers/CacheCommandHelpers.h"

TEST(CacheEditCommand, AcceptsReplacementForExistingEntry) {
  const char* replacement = NULL;
  EXPECT_EQ(cache::EDIT_READY,
            cache::parseEditCommand("!edit Great cache!", true, &replacement));
  ASSERT_NE(replacement, nullptr);
  EXPECT_STREQ("Great cache!", replacement);
}

TEST(CacheEditCommand, RejectsMissingReplacementText) {
  EXPECT_EQ(cache::EDIT_MISSING_TEXT,
            cache::parseEditCommand("!edit", true, nullptr));
  EXPECT_EQ(cache::EDIT_MISSING_TEXT,
            cache::parseEditCommand("!edit   ", true, nullptr));
}

TEST(CacheEditCommand, RejectsOverlengthReplacementText) {
  char command[cache::MAX_LOG_ENTRY_LENGTH + 8];
  strcpy(command, "!edit ");
  memset(command + 6, 'x', cache::MAX_LOG_ENTRY_LENGTH + 1);
  command[6 + cache::MAX_LOG_ENTRY_LENGTH + 1] = 0;
  EXPECT_EQ(cache::EDIT_TOO_LONG,
            cache::parseEditCommand(command, true, nullptr));
}

TEST(CacheEditCommand, RejectsReplacementWhenNoEntryExists) {
  EXPECT_EQ(cache::EDIT_NO_ENTRY,
            cache::parseEditCommand("!edit Great cache!", false, nullptr));
}

TEST(CacheEditCommand, LeavesOtherMessagesAlone) {
  EXPECT_EQ(cache::EDIT_NOT_COMMAND,
            cache::parseEditCommand("ordinary log entry", true, nullptr));
  EXPECT_EQ(cache::EDIT_NOT_COMMAND,
            cache::parseEditCommand("!editor", true, nullptr));
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
