#pragma once

#include <stddef.h>
#include <stdio.h>
#include <string.h>

namespace cache {

static const size_t MAX_LOG_ENTRY_LENGTH = 151;

enum EditCommandResult {
  EDIT_NOT_COMMAND,
  EDIT_MISSING_TEXT,
  EDIT_TOO_LONG,
  EDIT_NO_ENTRY,
  EDIT_READY,
};

inline EditCommandResult parseEditCommand(const char* text, bool has_entry,
                                          const char** replacement) {
  if (replacement) *replacement = NULL;
  if (!text || strncmp(text, "!edit", 5) != 0 || (text[5] != 0 && text[5] != ' ')) {
    return EDIT_NOT_COMMAND;
  }

  const char* value = text + 5;
  while (*value == ' ') value++;
  if (*value == 0) return EDIT_MISSING_TEXT;
  if (strlen(value) > MAX_LOG_ENTRY_LENGTH) return EDIT_TOO_LONG;
  if (!has_entry) return EDIT_NO_ENTRY;
  if (replacement) *replacement = value;
  return EDIT_READY;
}

inline void formatFindAcknowledgement(unsigned int find_count, char* reply,
                                      size_t reply_size) {
  if (!reply || reply_size == 0) return;
  if (find_count == 1) {
    snprintf(reply, reply_size,
             "Congratulations! You are the first to find this cache. Your log was saved.");
  } else {
    snprintf(reply, reply_size, "Log saved. You are find #%u.", find_count);
  }
}

inline bool canResetRssi(bool is_usb, bool has_sender, bool is_admin) {
  return is_usb || (has_sender && is_admin);
}

inline void formatCurrentRssi(int rssi, char* reply, size_t reply_size) {
  if (!reply || reply_size == 0) return;
  snprintf(reply, reply_size, "Current RSSI: %d dBm.", rssi);
}

inline void formatRssiStatus(bool calibrated, int near_rssi, int far_rssi,
                             int limit_rssi, char* reply, size_t reply_size) {
  if (!reply || reply_size == 0) return;
  if (calibrated) {
    snprintf(reply, reply_size, "Near %d | Far %d | Limit %d dBm", near_rssi,
             far_rssi, limit_rssi);
  } else {
    snprintf(reply, reply_size,
             "RSSI not calibrated. Use rssi near, then rssi far.");
  }
}

}  // namespace cache
