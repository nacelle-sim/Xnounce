/// @file
/// Implements `src/app/commands.h`; the interface is documented there.
#include "app/commands.h"

#include <cstdint>

#include "core/guard.h"
#include "core/log.h"

namespace xn {

namespace {

/// X-Plane hands a handler one pointer. There is one `Commands` object, and
/// that pointer carries the index of the command.
Commands* gCommands = nullptr;

void* asRefcon(intptr_t k) { return reinterpret_cast<void*>(k); }

}  // namespace

Commands::Commands(Actions actions) : actions_(std::move(actions)) {
  // The first six are the commands the menu uses. The rest are the same
  // actions under their FXAnnounce names.
  own_ = {
      {"xnounce/window_toggle", "Xnounce: show/hide the window",
       &Actions::toggleWindow},
      {"xnounce/skip", "Xnounce: skip the current announcement",
       &Actions::skip},
      {"xnounce/advance", "Xnounce: fire the next pending event now",
       &Actions::advance},
      {"xnounce/mute_toggle", "Xnounce: mute/unmute", &Actions::toggleMute},
      {"xnounce/overwater_toggle", "Xnounce: toggle overwater flight",
       &Actions::toggleOverwater},
      {"xnounce/rescan", "Xnounce: rescan Airline Packs", &Actions::rescan},
      {"fxannounce/window_toggle",
       "Xnounce (old FXAnnounce name): show/hide the window",
       &Actions::toggleWindow},
      {"fxannounce/skip",
       "Xnounce (old FXAnnounce name): skip the current announcement",
       &Actions::skip},
      {"fxannounce/advance",
       "Xnounce (old FXAnnounce name): fire the next pending event now",
       &Actions::advance},
      {"fxannounce/mute_toggle", "Xnounce (old FXAnnounce name): mute/unmute",
       &Actions::toggleMute},
      {"fxannounce/overwater_toggle",
       "Xnounce (old FXAnnounce name): toggle overwater flight",
       &Actions::toggleOverwater},
      {"fxannounce/rescan",
       "Xnounce (old FXAnnounce name): rescan Airline Packs", &Actions::rescan},
  };
}

void Commands::enable() {
  if (enabled_) return;
  gCommands = this;
  for (intptr_t k = 0; k < static_cast<intptr_t>(own_.size()); ++k) {
    Own& c = own_[static_cast<size_t>(k)];
    c.ref = XPLMCreateCommand(c.name, c.description);
    if (c.ref) XPLMRegisterCommandHandler(c.ref, ownHandler, 1, asRefcon(k));
  }
  buildMenu();
  enabled_ = true;
}

void Commands::disable() {
  if (!enabled_) return;
  stopObserving();
  for (intptr_t k = 0; k < static_cast<intptr_t>(own_.size()); ++k) {
    Own& c = own_[static_cast<size_t>(k)];
    if (c.ref) XPLMUnregisterCommandHandler(c.ref, ownHandler, 1, asRefcon(k));
  }
  destroyMenu();
  enabled_ = false;
}

int Commands::ownHandler(XPLMCommandRef, XPLMCommandPhase phase, void* refcon) {
  if (phase != xplm_CommandBegin) return 1;
  guarded("command", [&] {
    Commands* self = gCommands;
    const intptr_t k = reinterpret_cast<intptr_t>(refcon);
    if (!self || k < 0 || k >= static_cast<intptr_t>(self->own_.size())) return;
    const auto& action =
        self->actions_.*(self->own_[static_cast<size_t>(k)].action);
    if (action) action();
  });
  return 1;
}

void Commands::observe(const std::vector<ObservedCommand>& commands) {
  stopObserving();
  std::string missing;
  watched_.reserve(commands.size());
  for (const ObservedCommand& c : commands) {
    Watched w{c, XPLMFindCommand(c.name.c_str())};
    if (!w.ref) missing += std::string(missing.empty() ? "" : ", ") + c.name;
    watched_.push_back(std::move(w));
  }
  // Registered after the list is complete: the handler gets an index into it.
  for (intptr_t k = 0; k < static_cast<intptr_t>(watched_.size()); ++k)
    if (watched_[static_cast<size_t>(k)].ref)
      XPLMRegisterCommandHandler(watched_[static_cast<size_t>(k)].ref,
                                 watchedHandler, 1, asRefcon(k));
  if (!missing.empty())
    logInfo("purser call commands not on this aircraft: %s", missing.c_str());
}

void Commands::stopObserving() {
  for (intptr_t k = 0; k < static_cast<intptr_t>(watched_.size()); ++k)
    if (watched_[static_cast<size_t>(k)].ref)
      XPLMUnregisterCommandHandler(watched_[static_cast<size_t>(k)].ref,
                                   watchedHandler, 1, asRefcon(k));
  watched_.clear();
}

int Commands::watchedHandler(XPLMCommandRef, XPLMCommandPhase phase,
                             void* refcon) {
  // Observe only. Returning 1 lets the aircraft handle the press as usual.
  if (phase != xplm_CommandBegin) return 1;
  guarded("purser call", [&] {
    Commands* self = gCommands;
    const intptr_t k = reinterpret_cast<intptr_t>(refcon);
    if (!self || k < 0 || k >= static_cast<intptr_t>(self->watched_.size()))
      return;
    if (self->actions_.observed)
      self->actions_.observed(self->watched_[static_cast<size_t>(k)].command);
  });
  return 1;
}

void Commands::menuHandler(void*, void* itemRef) {
  guarded("menu", [&] {
    Commands* self = gCommands;
    if (!self) return;
    const Actions& a = self->actions_;
    switch (reinterpret_cast<intptr_t>(itemRef)) {
      case 1:
        if (a.toggleMute) a.toggleMute();
        break;
      case 2:
        if (a.toggleOverwater) a.toggleOverwater();
        break;
      case 3:
        if (a.rescan) a.rescan();
        break;
      case 4:
        if (a.reenable) a.reenable();
        break;
      default:
        break;
    }
  });
}

void Commands::buildMenu() {
  if (menu_) return;
  menuItem_ = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Xnounce", nullptr, 0);
  menu_ = XPLMCreateMenu("Xnounce", XPLMFindPluginsMenu(), menuItem_,
                         menuHandler, nullptr);
  miWindow_ =
      XPLMAppendMenuItemWithCommand(menu_, "Show/hide window", own_[0].ref);
  miMute_ = XPLMAppendMenuItem(menu_, "Mute", asRefcon(1), 0);
  miOverwater_ = XPLMAppendMenuItem(menu_, "Overwater flight", asRefcon(2), 0);
  miRescan_ = XPLMAppendMenuItem(menu_, "Rescan Airline Packs", asRefcon(3), 0);
  XPLMAppendMenuSeparator(menu_);
  miReenable_ =
      XPLMAppendMenuItem(menu_, "Re-enable after fault", asRefcon(4), 0);
}

void Commands::destroyMenu() {
  if (!menu_) return;
  XPLMDestroyMenu(menu_);
  XPLMRemoveMenuItem(XPLMFindPluginsMenu(), menuItem_);
  menu_ = nullptr;
  menuItem_ = -1;
}

void Commands::updateMenu(const MenuState& state) {
  if (!menu_) return;
  XPLMCheckMenuItem(menu_, miMute_,
                    state.muted ? xplm_Menu_Checked : xplm_Menu_Unchecked);
  XPLMCheckMenuItem(menu_, miOverwater_,
                    state.overwater ? xplm_Menu_Checked : xplm_Menu_Unchecked);
  XPLMEnableMenuItem(menu_, miReenable_, state.safeMode ? 1 : 0);
  XPLMEnableMenuItem(menu_, miMute_, state.active ? 1 : 0);
  XPLMEnableMenuItem(menu_, miOverwater_, state.active ? 1 : 0);
}

}  // namespace xn
