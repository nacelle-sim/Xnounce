/// @file
/// The window's content: the titlebar row and the tabs. Pure ImGui.
#pragma once

#include "ui/model.h"

namespace xn::ui {

/// The tab ids, saved as `[ui] tab`. Id 4 was the Inspector (removed in
/// 0.3.0).
enum TabId {
  TAB_STATUS = 0,
  TAB_TIMELINE = 1,
  TAB_PACKS = 2,
  TAB_SETTINGS = 3,
  TAB_LOG = 5
};
constexpr int kTabIds[] = {TAB_STATUS, TAB_TIMELINE, TAB_PACKS, TAB_SETTINGS,
                           TAB_LOG};
const char* tabName(int tab);
/// Whether a tab is shown: the Timeline and the Log can be turned off.
bool tabVisible(int tab, const Settings* s);

void drawPanel(UiModel& m, UiState& s);

}  // namespace xn::ui
