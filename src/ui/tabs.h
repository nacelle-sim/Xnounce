/// @file
/// The tabs of the window, one `tab_*.cpp` each.
#pragma once

#include "ui/model.h"

namespace xn::ui {

void drawStatus(UiModel& m, UiState& st);
void drawTimeline(UiModel& m, UiState& st);
void drawPacks(UiModel& m, UiState& st);
void drawSettings(UiModel& m, UiState& st);
void drawLog(UiModel& m, UiState& st);

/// Tells the plugin that the settings changed: apply and save them.
inline void settingsChanged(UiModel& m) {
  if (m.onSettingsChanged) m.onSettingsChanged();
}

}  // namespace xn::ui
