/// @file
/// The window's look: two themes, Dark (navy with Delta blue and red) and
/// White (Emirates red), the B612 fonts, and the colours of the flat cards
/// (docs/ARCHITECTURE.md § 7).
#pragma once

#include <imgui.h>

namespace xn::ui {

struct Fonts {
  ImFont* regular = nullptr;
  ImFont* bold = nullptr;
  ImFont* mono = nullptr;
};
Fonts& fonts();
/// Adds the three B612 fonts to an atlas: the plugin's or the headless
/// test's.
void addFonts(ImFontAtlas* atlas);
void loadFonts();  ///< At plugin enable: ImgWindow's shared atlas.

enum class Theme { Dark = 0, White = 1 };

struct Palette {
  // The page.
  ImU32 page, panel, field, fieldHover, border;
  ImU32 text, dim, hdr;
  ImU32 chrome;  ///< The selected sidebar item, a pressed button.
  ImU32 accent;  ///< Check marks, slider grabs, progress.
  ImU32 brand;   ///< The tab overline.
  ImU32 tab, tabHover, tabSelected;
  ImU32 ok, warn, bad;  ///< As on the ECAM: green normal, amber caution, red
                        ///< error.
  ImU32 track, chipBg;
  // Inside the cards: the panel tint with the page's colours.
  ImU32 cardBg, cardText, cardDim, cardOk, cardWarn, cardBad, cardButton,
      cardButtonHover;
  bool dark;
};
const Palette& pal();
void setTheme(Theme t, bool colorBlind);

/// Applies the theme to the current ImGui context. Cheap.
void applyStyle(float fontScale);

inline ImVec4 vec(ImU32 c) { return ImGui::ColorConvertU32ToFloat4(c); }
ImU32 withAlpha(ImU32 c, float a);

/// Pushes a font at a size relative to the body text. `f` may be null to
/// change only the size.
void pushFont(ImFont* f, float scale = 1.0f);

}  // namespace xn::ui
