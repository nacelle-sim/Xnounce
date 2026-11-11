/// @file
/// The window: the panel in an X-Plane window through ImgWindow, with Panel
/// Graphics on X-Plane 12.4.4 and later and OpenGL before. Pop-out and VR come
/// from ImgWindow.
#pragma once

#include <functional>

#include "ui/model.h"

namespace xn::ui {

/// Where the window gets its model; the provider returns null while the
/// plugin is not running.
void setModelProvider(std::function<UiModel*()> provider);

void showWindow(bool show);
void toggleWindow();
bool windowVisible();
void destroyWindow();  ///< Saves the window's geometry first.
/// Drops ImgWindow's atlas textures while XPLM is still valid (in
/// XPluginDisable), so no static destructor calls into X-Plane.
void shutdownUi();

}  // namespace xn::ui
