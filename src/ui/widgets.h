/// @file
/// The window's widgets. Every size follows the font size and the available
/// width.
#pragma once

#include <imgui.h>

#include <string>

namespace xn::ui {

void Chip(const char* text, ImU32 bg, ImU32 fg);
bool ToggleButton(const char* label, bool on, ImU32 onColor,
                  ImVec2 size = ImVec2(0, 0));
void KV(const char* label, const char* fmt, ...) IM_FMTARGS(2);
void KVColored(const char* label, ImU32 color, const char* fmt, ...)
    IM_FMTARGS(3);
bool BeginKVTable(const char* id, float labelWidthEm = 9.0f);
void EndKVTable();
/// A heading with a thin rule, and an optional note on the same line.
void Heading(const char* text, const char* help = nullptr,
             const char* note = nullptr);
void HelpMarker(const char* text);
/// A help marker "(?)" in the page's colours, also inside a card.
void Tooltip(const char* text);
/// A progress bar. 0 for the colours or the width: the theme's, the
/// available width.
void Progress(float frac, ImU32 track = 0, ImU32 fill = 0, float width = 0);

/// A card: a flat square panel in the theme's panel tint. `rule` marks an
/// alert card with a bar in that colour down its left edge. `minHeight` lines
/// the cards of a row up. Always pair it with `EndCard`, which returns the
/// card's natural height.
bool BeginCard(const char* id, float minHeight = 0, ImU32 rule = 0,
               float width = 0);
float EndCard();
void CardTitle(const char* text);
void BigText(const char* text, float scale = 1.5f, ImU32 color = 0);
/// The route bar: origin, the part flown, the aircraft as a dot, destination.
void RouteBar(const char* from, const char* to, float frac);
void TextCentered(const char* text, ImU32 color = 0);

/// A two-label switch; `on` means the right label. True when toggled.
bool PillSwitch(const char* id, const char* left, const char* right,
                bool* rightOn);

/// A time as "HH:MMZ" from seconds since midnight; negative is unknown.
std::string zuluText(double sec);
std::string localText(double sec);  ///< Local time, with the 12-hour time.
std::string mmssText(double sec);
/// A UTC time with the local time in its tooltip.
void TextZulu(double zuluSec, double localSec, ImU32 color = 0);

}  // namespace xn::ui
