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

TEST(CacheFindAcknowledgement, CongratulatesFirstFinder) {
  char reply[128];
  cache::formatFindAcknowledgement(1, reply, sizeof(reply));
  EXPECT_STREQ(
      "Congratulations! You are the first to find this cache. Your log was saved.",
      reply);
}

TEST(CacheFindAcknowledgement, ReportsFindNumber) {
  char reply[64];
  cache::formatFindAcknowledgement(12, reply, sizeof(reply));
  EXPECT_STREQ("Log saved. You are find #12.", reply);
}

TEST(CacheRssiReset, AllowsUsbOrAuthenticatedRadioAdmin) {
  EXPECT_TRUE(cache::canResetRssi(true, false, false));
  EXPECT_TRUE(cache::canResetRssi(false, true, true));
}

TEST(CacheRssiReset, RejectsUnauthenticatedRadioRequests) {
  EXPECT_FALSE(cache::canResetRssi(false, true, false));
  EXPECT_FALSE(cache::canResetRssi(false, false, false));
}

TEST(CacheRssiCommand, ReportsCurrentReading) {
  char reply[64];
  cache::formatCurrentRssi(-87, reply, sizeof(reply));
  EXPECT_STREQ("Current RSSI: -87 dBm.", reply);
}

TEST(CacheRssiStatus, ReportsSavedCalibration) {
  char reply[96];
  cache::formatRssiStatus(true, -40, -91, -94, reply, sizeof(reply));
  EXPECT_STREQ("Near -40 | Far -91 | Limit -94 dBm", reply);
}

TEST(CacheRssiStatus, ReportsUncalibratedState) {
  char reply[96];
  cache::formatRssiStatus(false, 0, 0, 0, reply, sizeof(reply));
  EXPECT_STREQ("RSSI not calibrated. Use rssi near, then rssi far.", reply);
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
