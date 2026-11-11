// xn_ui_bench: how long the window takes to build a frame with a lot of data,
// how many frames a resize takes to settle, and what the model's per-refresh
// copies cost. It uses ImGui's null backend and the real B612 fonts, so the
// numbers are the CPU side of a frame only (no OpenGL). Build it with the
// release preset for meaningful times:
//   cmake --build --preset release --target xn_ui_bench &&
//   build/release/xn_ui_bench
#include <imgui.h>
#include <imgui_impl_null.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "aircraft/options.h"
#include "core/pack_library.h"
#include "ui/panel.h"
#include "ui/theme.h"

namespace {

using Clock = std::chrono::steady_clock;

/// A hash of every vertex position of the frame, to see when the layout stops
/// moving.
std::uint64_t layoutHash() {
  std::uint64_t h = 1469598103934665603ull;
  for (const ImDrawList* dl : ImGui::GetDrawData()->CmdLists)
    for (const ImDrawVert& v : dl->VtxBuffer) {
      const auto x = static_cast<std::int64_t>(v.pos.x * 4);
      const auto y = static_cast<std::int64_t>(v.pos.y * 4);
      h = (h ^ static_cast<std::uint64_t>(x)) * 1099511628211ull;
      h = (h ^ static_cast<std::uint64_t>(y)) * 1099511628211ull;
    }
  return h;
}

/// Builds and renders one frame; returns the time it took in microseconds.
double frame(xn::ui::UiModel& m, xn::ui::UiState& s, float w, float h) {
  const auto t0 = Clock::now();
  ImGui_ImplNullPlatform_NewFrame();
  ImGui_ImplNullRender_NewFrame();
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(w, h);
  io.DeltaTime = 1.0f / 60.0f;
  const xn::Settings* set = m.settings;
  xn::ui::setTheme(
      set && set->theme == 1 ? xn::ui::Theme::White : xn::ui::Theme::Dark,
      set && set->colorBlind);
  xn::ui::applyStyle(set ? set->fontScale : 1.0f);
  ImGui::NewFrame();
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(io.DisplaySize);
  ImGui::Begin("Xnounce", nullptr, ImGuiWindowFlags_NoDecoration);
  xn::ui::drawPanel(m, s);
  ImGui::End();
  ImGui::Render();
  ImGui_ImplNullRender_RenderDrawData(ImGui::GetDrawData());
  return std::chrono::duration<double, std::micro>(Clock::now() - t0).count();
}

struct Stats {
  double median, p95, max;
};

Stats stats(std::vector<double> v) {
  std::sort(v.begin(), v.end());
  return {v[v.size() / 2], v[v.size() * 95 / 100], v.back()};
}

xn::PackLibrary bigLibrary() {
  static const char* kNames[] = {"BoardingWelcome[Refueling][Evening].ogg",
                                 "CrewSeatsLanding[1].ogg",
                                 "MyCustom~Duty free.ogg",
                                 "SafetyBriefing[A21N][WiFi].ogg",
                                 "BoardingStarted.ogg",
                                 "CallCabinSecureTakeoff.ogg",
                                 "AfterLanding[Morning].ogg",
                                 "DescentSeatbelts.ogg"};
  xn::PackLibrary lib;
  for (int k = 0; k < 4; ++k) {
    xn::Pack p;
    p.name = k == 0 ? "DLH" : "P" + std::to_string(k);
    p.root = "/packs/" + p.name;
    const int count = k == 0 ? 1000 : 50;
    for (int i = 0; i < count; ++i) {
      const std::string f = std::string(kNames[i % 8]);
      auto sf = xn::parseSoundFileName(f);
      if (!sf) continue;
      sf->relPath = "sub" + std::to_string(i / 8) + "/" + f;
      sf->path = p.root + "/" + sf->relPath;
      p.files.push_back(*sf);
    }
    lib.packs.push_back(p);
  }
  return lib;
}

void fill(xn::ui::UiModel& m) {
  m.active = true;
  m.aircraftGroup = "toliss";
  m.aircraftName = "ToLiss";
  m.stage = "Cruise";
  m.playing = "Cabin crew safety check";
  m.now.file = "DLH/SafetyBriefing[A21N][WiFi].ogg";
  m.now.pos = 62;
  m.now.dur = 288;
  m.flight.state = 1;
  m.flight.callsign = "DLH400";
  m.flight.origin = "EDDM";
  m.flight.destination = "EGLL";
  m.flight.routeFrac = 0.4f;
  for (int k = 0; k < 12; ++k)
    m.pending.push_back({"Descent", "DescentStarted", 600.0 + k, "", ""});
  for (int k = 0; k < 200; ++k)
    m.events.push_back(
        {13 * 3600.0 + k * 10, 15 * 3600.0 + k * 10, "Boarding welcome",
         "BoardingWelcome", k % 3 ? "played" : "no sound in pack",
         k % 3 ? "DLH/BoardingWelcome[Evening].ogg" : "pack DLH has no file",
         k % 3});
  for (int k = 0; k < 400; ++k) {
    xn::LogLine l;
    l.time = "12:00:00";
    l.level = k % 7 ? xn::LogLevel::Info : xn::LogLevel::Warn;
    l.msg = k % 7 ? "purser: stage Cruise, next DescentStarted in 12:30 after "
                    "the seatbelt sign goes on"
                  : "no file for WifiAnnouncement in pack DLH";
    l.count = 1;
    m.log.push_back(l);
  }
  for (int k = 0; k < 30; ++k)
    m.liveInputs.push_back({"Input " + std::to_string(k), "value"});
}

}  // namespace

int main() {
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  ImGui_ImplNullPlatform_Init();
  ImGui_ImplNullRender_Init();
  io.BackendFlags &= ~ImGuiBackendFlags_RendererHasVtxOffset;
  xn::ui::addFonts(io.Fonts);

  xn::Settings settings;
  settings.showLog = true;
  settings.showTimeline = true;
  xn::AircraftOptions aircraftOptions;
  std::vector<xn::SettingDef> defs = xn::coreSettingDefs(settings);
  for (xn::SettingDef& d : xn::aircraftSettingDefs(aircraftOptions))
    defs.push_back(d);
  xn::PackLibrary lib = bigLibrary();
  xn::ui::UiModel m;
  m.version = "bench";
  m.settings = &settings;
  m.settingDefs = &defs;
  m.library = &lib;
  m.explain = [](int, int) { return std::vector<std::string>{"x"}; };
  fill(m);

  std::printf(
      "frame build time, 900 x 720, 1000-file pack, 400 log lines, 200 "
      "timeline rows (microseconds)\n");
  std::printf("%-10s %8s %8s %8s %10s\n", "tab", "median", "p95", "max",
              "vertices");
  for (int tab : xn::ui::kTabIds) {
    xn::ui::UiState s;
    s.tab = tab;
    for (int f = 0; f < 10; ++f)
      frame(m, s, 900, 720);  // warm up: fonts, layout
    std::vector<double> t;
    for (int f = 0; f < 200; ++f) t.push_back(frame(m, s, 900, 720));
    int vtx = 0;
    for (const ImDrawList* dl : ImGui::GetDrawData()->CmdLists)
      vtx += dl->VtxBuffer.Size;
    const Stats st = stats(t);
    std::printf("%-10s %8.0f %8.0f %8.0f %10d\n", xn::ui::tabName(tab),
                st.median, st.p95, st.max, vtx);
  }

  std::printf(
      "\nframes until the layout stops moving after a resize from 760 to 1100 "
      "wide\n");
  for (int tab : xn::ui::kTabIds) {
    xn::ui::UiState s;
    s.tab = tab;
    for (int f = 0; f < 10; ++f) frame(m, s, 760, 720);
    std::uint64_t last = 0;
    int settled = -1;
    for (int f = 0; f < 12; ++f) {
      frame(m, s, 1100, 720);
      const std::uint64_t h = layoutHash();
      if (f > 0 && h == last) {
        settled = f;  // frame f drew the same as frame f - 1
        break;
      }
      last = h;
    }
    std::printf("%-10s %s%d\n", xn::ui::tabName(tab), settled < 0 ? "> " : "",
                settled < 0 ? 12 : settled);
  }

  {
    std::vector<double> t;
    for (int k = 0; k < 200; ++k) {
      const auto t0 = Clock::now();
      std::deque<xn::LogLine> copy = m.log;
      t.push_back(
          std::chrono::duration<double, std::micro>(Clock::now() - t0).count());
      if (copy.empty()) return 1;
    }
    const Stats st = stats(t);
    std::printf(
        "\ncopying the 400-line log into the model: median %.0f us, p95 %.0f "
        "us\n",
        st.median, st.p95);
  }

  ImGui_ImplNullRender_Shutdown();
  ImGui_ImplNullPlatform_Shutdown();
  ImGui::DestroyContext();
  return 0;
}
