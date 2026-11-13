/// @file
/// The Log tab (Settings > Advanced > Show log): the live inputs and the
/// recent log, where a repeated line collapses into one row with a count.
/// Xnounce.log keeps every line.
#include <imgui.h>

#include <string>

#include "core/strings.h"
#include "ui/tabs.h"
#include "ui/theme.h"
#include "ui/widgets.h"

namespace xn::ui {

void drawLog(UiModel& m, UiState& st) {
  const Palette& p = pal();
  const float em = ImGui::GetFontSize();
  if (ImGui::CollapsingHeader("Live inputs")) {
    if (m.liveInputs.empty()) {
      ImGui::TextDisabled(
          "No supported aircraft: nothing is read from the sim.");
    } else if (BeginKVTable("##inputs", 11.0f)) {
      for (const auto& [k, v] : m.liveInputs) KV(k.c_str(), "%s", v.c_str());
      EndKVTable();
    }
    ImGui::Spacing();
  }
  if (!m.checks.empty() &&
      ImGui::CollapsingHeader("Aircraft check",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::TextDisabled(
        "Try runs the aircraft's command once, on the next tick.");
    HelpMarker(
        "For checking the cabin crew's actions on this aircraft. The reading "
        "is the aircraft's own value; the result shows it before and after "
        "the try. Every try is written to Xnounce.log and the trace.");
    if (ImGui::BeginTable(
            "##checks", 4,
            ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("check", ImGuiTableColumnFlags_WidthStretch,
                              2.0f);
      ImGui::TableSetupColumn("reading", ImGuiTableColumnFlags_WidthStretch,
                              0.9f);
      ImGui::TableSetupColumn("try", ImGuiTableColumnFlags_WidthFixed,
                              em * 2.6f);
      ImGui::TableSetupColumn("result", ImGuiTableColumnFlags_WidthStretch,
                              1.8f);
      for (size_t k = 0; k < m.checks.size(); ++k) {
        const UiModel::Check& c = m.checks[k];
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted(c.label.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::TextDisabled("%s", c.reading.c_str());
        ImGui::TableSetColumnIndex(2);
        if (c.canTry) {
          ImGui::PushID(static_cast<int>(k));
          if (ImGui::SmallButton("Try") && m.onTryCheck)
            m.onTryCheck(static_cast<int>(k));
          ImGui::PopID();
        }
        ImGui::TableSetColumnIndex(3);
        ImGui::TextWrapped("%s", c.result.c_str());
      }
      ImGui::EndTable();
    }
    ImGui::Spacing();
  }
  ImGui::SetNextItemWidth(em * 16.0f);
  ImGui::InputTextWithHint("##filter", "filter", st.logFilter,
                           sizeof(st.logFilter));
  ImGui::SameLine();
  ImGui::AlignTextToFramePadding();
  ImGui::TextDisabled("%d entries", static_cast<int>(m.log.size()));
  HelpMarker(
      "The newest entries. A line that comes again moves to the bottom with a "
      "count (x5) instead of a new row. "
      "Xnounce.log in X-Plane's Output folder has every line. Settings > "
      "Advanced > Log detail sets how "
      "much is written.");
  ImGui::SameLine(0, em);
  ImGui::TextDisabled("window %.2f ms, model %.2f ms", st.frameMs, m.modelMs);
  HelpMarker(
      "The time Xnounce takes to build each frame of this window, and to "
      "gather what it shows. Both should stay well below a millisecond.");
  ImGui::PushStyleColor(ImGuiCol_ChildBg, p.panel);
  if (ImGui::BeginChild("##logtext", ImVec2(0, 0),
                        ImGuiChildFlags_AlwaysUseWindowPadding)) {
    pushFont(fonts().mono);
    for (const LogLine& l : m.log) {
      if (st.logFilter[0] && !icontains(l.msg, st.logFilter)) continue;
      const ImU32 c = l.level == LogLevel::Error  ? p.bad
                      : l.level == LogLevel::Warn ? p.warn
                                                  : 0;
      ImGui::TextDisabled("%s", l.time.c_str());
      ImGui::SameLine();
      ImGui::TextColored(vec(c ? c : p.dim), "%-5s", logLevelName(l.level));
      ImGui::SameLine();
      if (c) ImGui::PushStyleColor(ImGuiCol_Text, c);
      if (l.count > 1)
        ImGui::TextWrapped("%s  (x%d)", l.msg.c_str(), l.count);
      else
        ImGui::TextWrapped("%s", l.msg.c_str());
      if (c) ImGui::PopStyleColor();
    }
    ImGui::PopFont();
    if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
      ImGui::SetScrollHereY(1.0f);
  }
  ImGui::EndChild();
  ImGui::PopStyleColor();
}

}  // namespace xn::ui
