/// @file
/// Puts the fonts into ImgWindow's shared atlas. Separate from the fonts
/// themselves, so the UI library builds without ImgWindow.
#include <memory>

#include "ImgWindow.h"
#include "ui/theme.h"

namespace xn::ui {

void loadFonts() {
  if (ImgWindow::sFontAtlas) return;
  ImgWindow::sFontAtlas = std::make_shared<ImgFontAtlas>();
  addFonts(ImgWindow::sFontAtlas->getAtlas());
}

}  // namespace xn::ui
