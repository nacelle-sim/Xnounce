/// @file
/// The plugin's menu and commands, and the aircraft commands it watches.
///
/// Xnounce's own commands exist on every aircraft. Each is also registered
/// under its old `fxannounce/...` name, so key and joystick bindings made for
/// FXAnnounce keep working (docs/ARCHITECTURE.md § 5.1).
///
/// Aircraft commands, such as the cockpit's cabin call buttons, are only
/// observed: the handlers are registered ahead of the aircraft's own and
/// always let the press through (docs/ARCHITECTURE.md § 4, rule 4).
#pragma once

#include <XPLMMenus.h>
#include <XPLMUtilities.h>

#include <functional>
#include <string>
#include <vector>

#include "aircraft/module.h"

namespace xn {

class Commands {
 public:
  /// What the menu items and commands do.
  struct Actions {
    std::function<void()> toggleWindow, skip, advance, toggleMute,
        toggleOverwater, rescan, reenable;
    /// A watched aircraft command was pressed.
    std::function<void(const ObservedCommand&)> observed;
  };
  /// What the menu shows as ticked or greyed out.
  struct MenuState {
    bool muted = false, overwater = false, safeMode = false, active = false;
  };

  explicit Commands(Actions actions);
  Commands(const Commands&) = delete;
  Commands& operator=(const Commands&) = delete;

  /// XPluginEnable: creates the commands and the menu and starts handling
  /// them.
  void enable();
  /// XPluginDisable: the reverse.
  void disable();
  void updateMenu(const MenuState& state);

  /// Starts watching these aircraft commands. Logs the ones the aircraft
  /// doesn't have.
  void observe(const std::vector<ObservedCommand>& commands);
  /// Stops watching.
  void stopObserving();

 private:
  struct Own {
    const char* name;
    const char* description;
    std::function<void()> Actions::* action;
    XPLMCommandRef ref = nullptr;
  };
  struct Watched {
    ObservedCommand command;
    XPLMCommandRef ref = nullptr;
  };

  void buildMenu();
  void destroyMenu();
  static int ownHandler(XPLMCommandRef cmd, XPLMCommandPhase phase,
                        void* refcon);
  static int watchedHandler(XPLMCommandRef cmd, XPLMCommandPhase phase,
                            void* refcon);
  static void menuHandler(void* menuRef, void* itemRef);

  Actions actions_;
  std::vector<Own> own_;
  bool enabled_ = false;
  std::vector<Watched> watched_;

  XPLMMenuID menu_ = nullptr;
  int menuItem_ = -1;
  int miWindow_ = -1, miMute_ = -1, miOverwater_ = -1, miRescan_ = -1,
      miReenable_ = -1;
};

}  // namespace xn
