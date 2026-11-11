// The headless UI test: every tab and every Settings page, with the real B612
// fonts, from narrow to wide, in both themes, with the colour-blind colours,
// the Timeline and the Log on and off, in every plugin state, with ImGui's
// assertions on. It also fails when a draw list reaches the 16-bit index
// range, because ImgWindow's OpenGL path ignores the vertex offset.
// Run it with `ctest --preset dev` (CTest name `ui`).
#include <imgui.h>
#include <imgui_impl_null.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "aircraft/options.h"
#include "core/pack_library.h"
#include "ui/panel.h"
#include "ui/theme.h"

#ifdef NDEBUG
#error \
    "xn_ui_tests needs ImGui's assertions: build it with -UNDEBUG (see native/CMakeLists.txt)"
#endif

static int gFail = 0;
#define CHECK(cond, ...)                               \
  do {                                                 \
    if (!(cond)) {                                     \
      ++gFail;                                         \
      std::printf("FAIL %s:%d  ", __FILE__, __LINE__); \
      std::printf(__VA_ARGS__);                        \
      std::printf("\n");                               \
    }                                                  \
  } while (0)

namespace {

int gFrames = 0;
int gMaxVtx = 0;  // busiest frame proves the tabs draw

void frame(xn::ui::UiModel& m, xn::ui::UiState& s, float w, float h) {
  ImGui_ImplNullPlatform_NewFrame();
  ImGui_ImplNullRender_NewFrame();
  ImGuiIO& io = ImGui::GetIO();
  io.DisplaySize = ImVec2(w, h);
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
  int total = 0;
  for (const ImDrawList* dl : ImGui::GetDrawData()->CmdLists) {
    CHECK(dl->VtxBuffer.Size < 65536, "draw list '%s' has %d vertices",
          dl->_OwnerName, dl->VtxBuffer.Size);
    total += dl->VtxBuffer.Size;
  }
  gMaxVtx = std::max(gMaxVtx, total);
  ++gFrames;
}

struct State {
  const char* name;
  bool active, muted, safe, calling, playing;
};

void fillActive(xn::ui::UiModel& m, const State& st, int variant) {
  m.active = st.active;
  m.muted = st.muted;
  m.safeMode = st.safe;
  m.lastFault = st.safe ? "tick: test fault" : "";
  m.calling = st.calling;
  m.callAttempt = 2;
  m.answerHint = "Pull CAB on the ACP to answer";
  m.aircraftGroup = st.active ? "toliss" : "";
  m.aircraftName = st.active ? "ToLiss" : "";
  m.loadedAircraft =
      st.active ? "" : (variant % 2 ? "Laminar Research Cessna 172 SP" : "");
  m.oldPlugin = !st.active && variant == 2;  // fxannounce still installed
  m.stage = st.active ? "Cabin secured, waiting for line-up" : "";
  m.playing = st.playing ? "Cabin crew safety check" : "";
  m.playingFenix = st.playing ? "SafetyBriefing" : "";
  m.now = {};
  if (st.playing) {
    m.now.file = "DLH/SafetyBriefing[A21N][WiFi].ogg";
    m.now.pos = 62;
    m.now.dur = 288;
    m.now.decoding = variant == 2;
    m.now.paused = variant == 1;
  }
  m.music = variant % 2 ? "DLH/BoardingMusic.ogg (paused)" : "";
  m.boardingNote = variant == 1 ? "Boarding shortened to 15 min: the SimBrief "
                                  "ETD is only 24 min away. Set the OBT later "
                                  "in SimBrief for a full boarding."
                                : "";
  m.flight = {};
  m.flight.state = st.active ? variant % 3 : 0;
  m.flight.callsign = "DLH400";
  m.flight.origin = "EDDM";
  m.flight.destination = "EGLL";
  m.flight.etd = "13:15Z";
  m.flight.boarding = "12:39Z";
  m.flight.line = "boarding in 12:30";
  m.flight.airline = variant ? "American Airlines" : "";
  m.flight.routeFrac =
      variant == 2 ? -1.0f : 0.37f * static_cast<float>(variant);
  m.flight.toGoNm = 312;
  m.boardingFrac = st.active && variant != 2 ? 0.62f : -1.0f;
  m.boardingLeftSec = 580;
  m.boardingPax = variant ? 121 : 0;
  m.flight.reason = m.flight.state == 2
                        ? "OFP ignored: origin EGLL is 512 NM away; the doors "
                          "rule starts boarding"
                        : "simbrief_hub not installed: boarding starts when a "
                          "door has been open at the gate";
  m.aircraft = st.active ? "ToLiss A321neo" : "";
  m.livery = st.active
                 ? "Lufthansa (D-AIEA) 2018 livery with a very long folder name"
                 : "";
  m.pack = st.active ? "DLH" : "";
  m.customTags = variant ? "WiFi, USB" : "";
  m.pending.clear();
  if (st.active) {
    m.pending = {{"Crew take seats (takeoff)", "CrewTakeSeats (takeoff)", -1,
                  "on line-up: landing lights ON, strobe ON, or a cabin call "
                  "while taxiing"},
                 {"After takeoff", "AfterTakeoff", 540, ""},
                 {"Boarding", "", -1,
                  "starts 15 s after a door is open at the gate with power, or "
                  "on a cabin call"},
                 {"Next boarding", "", 0, ""}};
    if (variant == 2) m.pending.front().inSec = 80;
  }
  m.events.clear();
  if (st.active)
    for (int k = 0; k < 220; ++k)
      m.events.push_back(
          {13 * 3600.0 + k * 10, k % 4 ? 15 * 3600.0 + k * 10 : -1,
           "Boarding welcome", "BoardingWelcome",
           k % 3 ? "played" : "no sound in pack",
           k % 3 ? "DLH/BoardingWelcome[Evening].ogg" : "pack DLH has no file",
           k % 3});
  m.liveInputs.clear();
  if (st.active)
    m.liveInputs = {{"FMGS phase", "Cruise (3)"},
                    {"Passenger doors", "most open 0%, L1 0%"},
                    {"Cabin PA heard", "clear 0%, through the door 10%"}};
  m.recorderPath =
      variant ? "Output/Xnounce/2026-11-03_1342_A321.xntrace.csv" : "";
}

}  // namespace

int main() {
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  ImGui_ImplNullPlatform_Init();
  ImGui_ImplNullRender_Init();
  io.BackendFlags &=
      ~ImGuiBackendFlags_RendererHasVtxOffset;  // like imgwindows gl path
  xn::ui::addFonts(io.Fonts);                   // same fonts as the plugin
  CHECK(xn::ui::fonts().regular && xn::ui::fonts().bold && xn::ui::fonts().mono,
        "B612 fonts did not load");

  xn::Settings settings;
  xn::AircraftOptions aircraftOptions;
  std::vector<xn::SettingDef> defs = xn::coreSettingDefs(settings);
  for (xn::SettingDef& d : xn::aircraftSettingDefs(aircraftOptions))
    defs.push_back(d);
  xn::ui::UiModel m;
  m.version = "0.3.5";
  m.settings = &settings;
  m.settingDefs = &defs;
  m.supportedText = "Xnounce works with the ToLiss A319, A320 and A321.";
  m.loadHint = "Load a ToLiss A319, A320 or A321.";
  int applied = 0, actions = 0;
  m.onSettingsChanged = [&] { ++applied; };
  m.onSkip = m.onAdvance = m.onToggleMute = m.onToggleOverwater = m.onRescan =
      m.onReenable = [&] { ++actions; };
  for (int k = 0; k < 500; ++k) {
    xn::LogLine l;
    l.time = "12:00:00";
    l.level = k % 7 ? xn::LogLevel::Info : xn::LogLevel::Warn;
    l.msg = k % 7 ? "purser: stage Cruise, next DescentStarted"
                  : "no file for WifiAnnouncement in pack DLH";
    l.count = k % 5 + 1;
    m.log.push_back(l);
  }
  xn::PackLibrary lib;
  lib.warnings = {"pack folder not found: /x"};
  for (const char* name : {"DLH", "Default", "UALES"}) {
    xn::Pack p;
    p.name = name;
    p.root = std::string("/packs/") + name;
    p.mappingOnly = std::string(name) == "UALES";
    p.wrapper = std::string(name) == "DLH" ? "DLH OLD PA SYSTEM" : "";
    for (const char* f :
         {"BoardingWelcome[Refueling][Evening].ogg", "CrewSeatsLanding[1].ogg",
          "MyCustom~Duty free.ogg", "SafetyBriefing[A21N][WiFi].ogg",
          "BoardingStarted.ogg", "CallCabinSecureTakeoff.ogg"}) {
      auto sf = xn::parseSoundFileName(f);
      if (!sf) continue;
      sf->relPath = f;
      sf->path = p.root + "/" + f;
      p.files.push_back(*sf);
    }
    p.maxVariant = 1;
    p.warnings = {"x.mp4: not an .ogg file, skipped"};
    lib.packs.push_back(p);
  }
  m.library = &lib;
  m.packsDir = "/packs/";
  m.explain = [](int, int) {
    return std::vector<std::string>{"Pack DLH", "-> plays x.ogg",
                                    "  ok   x.ogg: no tags"};
  };

  const State states[] = {
      {"dormant", false, false, false, false, false},
      {"active", true, false, false, false, true},
      {"muted", true, true, false, false, false},
      {"calling", true, false, false, true, true},
      {"safe", true, false, true, false, false},
  };
  const float widths[] = {520.0f, 760.0f, 2000.0f};
  struct Vis {
    bool timeline, log;
  };
  const Vis visibility[] = {{true, true}, {false, false}};

  for (int theme = 0; theme < 2; ++theme)
    for (const Vis& vis : visibility)
      for (const State& st : states) {
        const int variant = static_cast<int>(&st - states) % 3;
        settings.theme = theme;
        settings.colorBlind = variant == 1;
        settings.showTimeline = vis.timeline;
        settings.showLog = vis.log;
        fillActive(m, st, variant);
        for (float w : widths)
          for (int tab : xn::ui::kTabIds) {
            xn::ui::UiState s;
            s.tab = tab;
            s.explainLines = {"Pack DLH", "-> plays x.ogg"};
            for (int f = 0; f < 6; ++f) frame(m, s, w, w * 0.8f);
            const int expect =
                xn::ui::tabVisible(tab, &settings) ? tab : xn::ui::TAB_STATUS;
            CHECK(s.tab == expect,
                  "%s, theme %d: tab %d restored as %d (expected %d)", st.name,
                  theme, tab, s.tab, expect);
          }
      }

  // every Settings page, with an aircraft loaded and while dormant (then the
  // aircraft page lists every aircraft's settings under its name)
  for (int active = 1; active >= 0; --active)
    for (int theme = 0; theme < 2; ++theme)
      for (float w : {520.0f, 1200.0f})
        for (int page = 0; page < xn::kSettingPageCount; ++page) {
          fillActive(m, states[active], 1);
          settings.theme = theme;
          xn::ui::UiState s;
          s.tab = xn::ui::TAB_SETTINGS;
          s.settingsPage = page;
          for (int f = 0; f < 4; ++f) frame(m, s, w, w * 0.8f);
          CHECK(s.settingsPage == page, "settings page %d became %d", page,
                s.settingsPage);
        }

  // text size extremes small window
  for (float scale : {0.6f, 2.0f}) {
    settings.fontScale = scale;
    for (int tab : xn::ui::kTabIds) {
      xn::ui::UiState s;
      s.tab = tab;
      for (int f = 0; f < 4; ++f) frame(m, s, 520, 380);
    }
  }

  // the timers count on between the plugin's refreshes
  CHECK(xn::ui::countdown(10, 0.25) == 9.75, "countdown");
  CHECK(xn::ui::countdown(0.1, 0.25) == 0, "countdown stops at now");
  CHECK(xn::ui::countdown(-1, 0.25) == -1, "a waiting row stays waiting");
  {
    xn::ui::UiModel::Now now;
    now.pos = 10;
    now.dur = 10.2;
    CHECK(xn::ui::nowPosition(now, 0.5) == 10.2, "position stops at the end");
    now.paused = true;
    CHECK(xn::ui::nowPosition(now, 0.5) == 10, "a paused file stays put");
  }
  CHECK(xn::ui::boardingProgress(0.5f, 100, 0) == 0.5f, "boarding at age 0");
  CHECK(std::abs(xn::ui::boardingProgress(0.5f, 100, 50) - 0.75f) < 1e-4f,
        "boarding counts on");
  CHECK(xn::ui::boardingProgress(-1, 100, 50) == -1, "not boarding");

  // drawing alone never changes settings or fires actions
  CHECK(applied == 0, "onSettingsChanged fired %d times without input",
        applied);
  CHECK(actions == 0, "actions fired %d times without input", actions);

  ImGui_ImplNullRender_Shutdown();
  ImGui_ImplNullPlatform_Shutdown();
  ImGui::DestroyContext();
  CHECK(gMaxVtx > 8000,
        "the busiest frame drew only %d vertices: are the tabs drawing?",
        gMaxVtx);
  std::printf("xn_ui_tests: %d frames (busiest %d vertices), %d failures\n",
              gFrames, gMaxVtx, gFail);
  return gFail == 0 ? 0 : 1;
}
