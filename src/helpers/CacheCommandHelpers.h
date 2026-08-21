#pragma once

#include <stddef.h>
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

}  // namespace cache
