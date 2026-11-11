/// @file
/// The B612 fonts (fonts/, SIL OFL 1.1), added to any atlas with the same
/// metrics in the plugin and the headless test.
#include "b612_bold.inc"
#include "b612_regular.inc"
#include "b612mono_regular.inc"
#include "ui/theme.h"

namespace xn::ui {

namespace {
Fonts gFonts;
constexpr float kBaseSize = 15.0f;  // pixels at text size 1.0
}  // namespace

Fonts& fonts() { return gFonts; }

void addFonts(ImFontAtlas* atlas) {
  ImFontConfig cfg;
  cfg.FontDataOwnedByAtlas = false;
  gFonts.regular = atlas->AddFontFromMemoryCompressedTTF(
      b612_regular_compressed_data,
      static_cast<int>(b612_regular_compressed_size), kBaseSize, &cfg);
  gFonts.bold = atlas->AddFontFromMemoryCompressedTTF(
      b612_bold_compressed_data, static_cast<int>(b612_bold_compressed_size),
      kBaseSize, &cfg);
  gFonts.mono = atlas->AddFontFromMemoryCompressedTTF(
      b612mono_regular_compressed_data,
      static_cast<int>(b612mono_regular_compressed_size), kBaseSize, &cfg);
}

}  // namespace xn::ui
