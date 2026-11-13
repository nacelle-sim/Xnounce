# Zibo and LevelUp Boeing 737

**Scope:** the Zibo 737-800 and the LevelUp 737NG series: how Xnounce recognises them, and the datarefs and commands
it reads. From 1.0.0 the purser runs on them; 0.3.5 only recognised them. The code is in `src/aircraft/zibo737/`:
`zibo737_signals` decodes the raw values (shared with the scripted 737 flights of the tests), `zibo737_types` holds
the models and the cockpit wording, `zibo737_options` the settings (Settings → Zibo / LevelUp, and the 737 rows of
Boarding).

**Sources:** the aircraft's own files: `B738_Datarefs.txt`, `B738_Commands.txt`, the xlua scripts and the strings
of `zibomod.xpl`, for ZiboMod 4.05.35 and LevelUp 737NG 2.S1.51C. Both aircraft run the same systems: the same
`zibomod.xpl` plugin and 14 of their 16 xlua scripts byte for byte (the FMS and the tablet scripts differ in a few
dozen lines; LevelUp adds animation, sine and sound scripts).

**Checked live** on the LevelUp 737-800 (v2.S1.50, X-Plane 12.4.4) on 2026-11-11, during a flight EDDB–EGSS, through
X-Plane's local web API: every dataref named below exists with this exact spelling, and the rows marked *(seen)*
showed the values described. The others are still **assumed** until an rc flight with the recorder on confirms them
(`toliss.md` § 5 describes the method). Dataref names are case-sensitive: the FMOD set is spelled
`laminar/b738/fmodpack/...` with a lower-case b.

## 1. Models and recognition
| .acf | ICAO | `acf/_author` | `acf/_studio` | Model shown |
|---|---|---|---|---|
| `B737-800X/b738.acf`, `b738_4k.acf` | B738 | Alex Unruh | Laminar Research modified by Zibo and Twkster | Zibo 737-800 |
| `737NG Series/737_60NG.acf` | B736 | LevelUp | LevelUp, Laminar Research, ZiboMod | LevelUp 737-600 |
| `737NG Series/737_70NG.acf` | B737 | LevelUp | (same) | LevelUp 737-700 |
| `737NG Series/737_80NG.acf` | B738 | LevelUp | (same) | LevelUp 737-800 |
| `737NG Series/737_90NG.acf` | B739 | LevelUp | (same) | LevelUp 737-900 |
| `737NG Series/737_9ENG.acf` | B739 | LevelUp | (same) | LevelUp 737-900ER |

A loaded aircraft is one of these when:
- the dataref `laminar/B738/FMS/flight_phase` exists (created by the Zibo FMS script; Laminar's default 737-800
  uses X-Plane's own FMS and has no such dataref), and
- the maker is known: "LevelUp" in the author or studio, or "Zibo" in the studio or the path (`B737-800X`), and
- the ICAO designator is B736, B737, B738 or B739 (B739 with "9E" in the file name is the -900ER).

Laminar's own 737-800 has the same ICAO designator and author, so the studio and the dataref both matter.

## 2. Signals
How each signal of the snapshot (VP_SPEC § 4.2) comes from these aircraft. "Use" says what Xnounce does with it.

| Signal | Dataref | Encoding |
|---|---|---|
| Flight phase | `laminar/B738/FMS/flight_phase` | 0 takeoff (also before the flight and after the rollout), 1 climb, 2 cruise, 3 cruise climb (step), 4 cruise descent (step), 5 descent, 6 approach, 7 go-around armed, 8 go-around *(seen: 2 and 3 in cruise, 5 in the descent, then 7 on the approach
without 6)*. Use: 1 Climb, 2–4 Cruise, 5 Descent, 6–7 Approach, 8 GoAround. 0 on the ground is PreFlight before the flight (Takeoff once rolling above 40 kt with the engines running) and Done after it, until the engines stop at the stand. 0 in the air is no phase: the purser estimates it until the FMC moves on, which also covers an FMC that was never set up (D-20) |
| Takeoff power cue | `laminar/B738/autopilot/ap_takeoff` (TO/GA engaged), `laminar/B738/FMS/N1_mode` (the N1 limit selected in the FMC: TO, TO 1, TO 2, D-TO…) | Use: TO/GA engaged on the ground. `N1_mode` is only the selected limit, not the thrust: trace only. The thrust levers (`sim/cockpit2/engine/actuators/throttle_ratio`) at or above 0.85 *(tunable)* are the levers cue |
| Lift-off, touchdown cues | `laminar/B738/air_ground_sensor` (0 in the air, 1 on the ground) *(seen: 0 in cruise)*; `laminar/b738/fmodpack/flightphase_landed` (1 from touchdown through the rollout) | Use: lift-off when the sensor says air while the FMC phase is still 0; touchdown from `flightphase_landed`, or from the sensor after a flight |
| Seatbelt sign | `laminar/B738/toggle_switch/seatbelt_sign_pos` (0 OFF, 1 AUTO, 2 ON) *(seen: 2, then 0 in cruise)*; lit: `laminar/b738/fmodpack/seatbelt_on_light` *(seen: 0 with the switch OFF)* | Use: the light; without it, ON, or AUTO with the gear lever down |
| NO SMOKING | `laminar/B738/toggle_switch/no_smoking_pos` (0 OFF, 1 AUTO, 2 ON) | the lit state is not published. Use: ON, or AUTO with the gear lever down (as the aircraft lights it). `laminar/B738/fms/no_smoking` is an aircraft option (signs, chimes only, off) |
| Passenger doors | `737u/doors/L1` (forward left entry), `R1`, `L2`, `R2`, `L_mid`, `R_mid` (-900/-900ER), `emerg1`–`emerg4`; cargo `Fwd_Cargo`, `aft_Cargo` | 0 closed … 1 open. `laminar/B738/doors/status` is a status array (0 open, 1 moving, 2 closed) |
| Slides | none | the 737NG's girt bars aren't modeled: no slide signal (`slidesKnown` false) |
| Passengers | `laminar/B738/tab/zone1_payload` … `zone5_payload` (men, women, children per zone), `laminar/B738/std_pax_num` (seats per zone of the layout) | *(seen: zones 25+33+28+33+33 = 152, the SimBrief count; seats 27+42+36+42+42 = 189)*. Use: the sum of the zones; the seats are the type's seats for the boarding model. Boarding speed setting `laminar/B738/FMS/pax_time` (0 real, 1 long, 2 short) |
| AC power | `laminar/B738/electric/ac_tnsbus1_status`, `ac_tnsbus2_status` (transfer buses, 1 powered); `laminar/B738/electric/ife_pass_bus1`, `ife_pass_bus2` (IFE and passenger buses) | Use: powered when either transfer bus is |
| AC source (the PA's cut-out, VP_SPEC § 4.6) | per bus: `laminar/B738/annunciator/gen_off_bus1/2` (unlit: the engine generator feeds it), `laminar/B738/electrical/apu_power_bus1/2` (the APU feeds it) | Use: 3 engine generator, 2 APU, 1 ground power (powered by neither), 0 unpowered; the id is bus 1 × 10 + bus 2. A change while powered is a transfer |
| Parking brake, chocks | `laminar/B738/parking_brake_pos` (1 set); `laminar/B738/fms/chock_status` (1 placed) | |
| Landing lights ON | `laminar/B738/switch/land_lights_ret_left_pos`, `_right_pos` (0 retract, 1 extend, 2 on); `laminar/B738/switch/land_lights_left_pos`, `_right_pos` (wing root, 0/1) | ON is 2 on a retractable light or 1 on a fixed one |
| Strobe ON | `laminar/B738/toggle_switch/position_light_pos` (−1 STEADY, 0 OFF, 1 STROBE & STEADY) | 1 is the line-up position |
| Beacon | `sim/cockpit2/switches/beacon_on` | X-Plane standard (already in the shared reader) |
| Gear lever | `laminar/B738/controls/gear_handle_down` (0 up, 0.5 off, 1 down) | |
| Cockpit door | `laminar/B738/door/flt_dk_door_ratio` (0 closed … 1 open) | |
| Cruise altitude | `laminar/B738/autopilot/fmc_cruise_alt` (ft) | |
| Flight number | none | the FMC keeps it in script variables; the airline must come from SimBrief, the livery or `liveries.ini` |
| Masks, cabin altitude | `laminar/B738/annunciator/pax_oxy` (PASS OXY ON), `sim/operation/failures/rel_pass_o2_on` (6 deployed), `laminar/B738/one_way_switch/pax_oxy_pos`; `sim/cockpit2/pressurization/indicators/cabin_altitude_ft` | Use: the annunciator or the failure |
| Interphone taken | `laminar/B738/audio/capt/mic_button5`, `fo/mic_button5` | Use: either selected answers the cabin's call (assumed; see § 3) |
| Cabin chime | X-Plane's `Resources/sounds/alert/seatbelt.wav` (16-bit stereo PCM, 22.05 kHz, 2.1 s) | Use: the cockpit call chime. If it can't be read, a generated single high chime plays |

## 3. Cabin calls and the interphone
- **Cockpit to cabin:** the overhead ATTEND button, command `laminar/B738/push_button/attend` *(seen: a command, not
  a dataref)*, native (position `laminar/B738/push_button/attend_pos`, read-only; it sounds the attendant call for
  2.5 s). It is the 737's cabin call button for VP_SPEC § 4.3a. GRD CALL
  (`laminar/B738/push_button/grd_call`) calls the ground crew and would only be logged.
- **Cabin to cockpit:** the CALL light, `laminar/B738/annunciator/cabin_call_annun`.
- **Taking the call and listening to the PA:** the audio control panels' microphone selectors,
  `laminar/B738/audio/{capt,fo}/mic_button5` (cabin interphone) and `mic_button6` (PA), with the commands
  `laminar/B738/audio/{capt,fo,obs}/mic_push5`/`mic_push6`. Whether a receiver volume is published is still open.

## 4. The aircraft's own cabin announcements
The 737 plays crew and captain announcements of its own (the "fmodpack" scripts and sound banks). Read from the
scripts (`B738.a_AXP_3_FMOD_commands`, `B738.a_AXP_4_FMOD_logic`), the sound files (`fmod/*.snd`) and the sound banks
of both aircraft, 2026-11-12. All `laminar/b738/fmodpack/...`.

| Dataref | What it does | Writable |
|---|---|---|
| `fmod_vol_crew` (0–10) | **Zibo:** the crew voices' volume, a parameter of its voice bank (`V_1.bank`). 0 silences the crew and captain announcements. **LevelUp:** its own banks have no such parameter: no effect | yes |
| `msg_airside` (0 off, 1 automatic; default 1) | the captain's welcome, cruise, descent and pre-landing messages (`A4` `play_*_msg`) | yes |
| `TO_trigger` (0 off, 1 STROBE & STEADY, 2 ATTEND; default 0) | the crew's and captain's takeoff announcement (`play_TO`), once taxiing with the doors closed after the safety demo. With 0 the ATTEND button only chimes | yes |
| `msg_playing` | seconds left of the captain's message playing (130 after takeoff, 35 cruise, 65 descent, 90 pre-landing, 55 welcome, 30 turbulence); counts down to 0. A new message starts only at 0 | yes |
| `fmod_crew_on` | **nothing**: the scripts call it "obsolete, kept for Zibo's FMC menu"; no script, sound file or bank reads it | yes |
| `call_chime` | set to 1 for a moment, it sounds `TO_chime` on the **Zibo** (`b738.snd`); LevelUp's sound files don't use it. The script puts it back to 0 a frame later | yes |
| `cabin_secured` | 1 for a moment: the flight attendant's "cabin secure" (both aircraft) | (native writes it) |

What LevelUp plays and nothing can switch off: the seatbelt announcement each time the seatbelt switch goes to ON,
the gate departure, "strapped in" and safety demo (timed from the doors closing, `axp_FA_dep_timer` 7, 25 and 88 s),
the 10,000 ft call, "cabin secure", the taxi-in and arrival cross-check. The Zibo's are silenced by `fmod_vol_crew`.

**With cabin actions on** (§ 6), Xnounce sets `msg_airside` and `TO_trigger` to 0, and on the Zibo `fmod_vol_crew` to
0. It remembers the values and puts them back afterwards (`docs/ARCHITECTURE.md` § 4 rule 4). The aircraft saves
these options with its own configuration: saving the configuration from the FMC while Xnounce holds them would
store the zeros. Whether actions are on or off, Xnounce starts no announcement while `msg_playing` counts.

## 5. Airside Services (the EFB's turnaround)
`laminar/B738/tab/auto_flight` (read-only), from `B738.tablet.lua`: 0 idle or done, 1 started, 2 loading (doors,
jetway or stairs, cargo, boarding), 3 loaded, **which stays 3 through the whole flight** *(seen)*, 4 parked after the
landing (all wheels down, engines off, parking brake or chocks, beacon off), 5 cargo doors open, 6 L1 open (and L2
with stairs), 7 deboarding (the zone counts fall to 0), then 0. Xnounce records it from 1.0.0; following it is the
rc.3 work. It never touches the cabin lights, CABIN READY or the announcement options; the doors it opens and
closes are listed in the tablet script (`B738_auto_flight`): cargo and L1 for loading, R1 and R2 for catering, L1
(and L2 with stairs) for boarding, all closed again at state 3; cargo and L1 (L2) opened in states 4–5.

## 6. Cabin actions (VP_SPEC § 4.7)
Read from the fresh Zibo 4.05.35 and LevelUp 2.S1.51C installs, 2026-11-12: the tablet script `B738.tablet.lua` and
the native `zibomod.xpl` (the same file in both). Settings → Zibo / LevelUp (the page carries the loaded aircraft's
name), every action off by default.

**Doors.** The tablet script replaces X-Plane's own door commands with explicit opens and closes:

| Door | Position (0 closed … 1 open) | Open | Close | Takes |
|---|---|---|---|---|
| Forward left (L1) | `737u/doors/L1` | `sim/flight_controls/door_open_1` | `sim/flight_controls/door_close_1` | 7 s |
| Forward right (R1) | `737u/doors/R1` | `door_open_5` | `door_close_5` | 7 s |
| Aft left (L2) | `737u/doors/L2` | `door_open_4` | `door_close_4` | 7 s |
| Aft right (R2) | `737u/doors/R2` | `door_open_8` | `door_close_8` | 7 s |
| Forward cargo | `737u/doors/Fwd_Cargo` | `door_open_9` | `door_close_9` | 7 s |
| Aft cargo | `737u/doors/aft_Cargo` | `door_open_10` | `door_close_10` | 7 s |
| Mid exits (-900/-900ER) | `737u/doors/L_mid`, `R_mid` | `laminar/B738/door/L_mid_exit_open`, `R_mid_exit_open` | `…_close` | 2 s |

- The `laminar/B738/door/*_toggle` commands exist as well; Xnounce uses the explicit ones so a late reading can't
  turn a close into an open. The positions are read-only.
- The aircraft won't **open** a main door while an engine burns fuel, in the air or with more than 0.2 psi of cabin
  pressure (`door_locked`). Closing always works, except L1 while the built-in airstairs are out.
- **Airstairs:** `laminar/B738/airstairs_toggle` stows or deploys them; `laminar/B738/airstairs_hide` is 1 when
  stowed (or not fitted), `laminar/B738/door/airstair_ratio` 0 stowed … 1 out. Stowing takes about 32 s (55 s on the
  battery alone). Xnounce stows them before closing L1, and only toggles while `airstairs_hide` is 0.
- **The aircraft moves doors itself** while its Airside Services runs (`laminar/B738/tab/auto_flight` ≠ 0, § 5), and
  while its cargo-loading sound runs (`laminar/b738/fmodpack/fmod_play_cargo` = 1, with Airside Services off) it
  reopens the cargo doors every frame. Xnounce leaves all doors alone in the first case and the cargo doors in the
  second.

**Cabin lights.** Native commands, read back from `737x/cabin/set_pax_lighting` (read-only): 0 white bright
(`laminar/B738/cabin/pax_white_bright`), 1 takeoff and landing (`pax_to_landing`), 2 cruise (`pax_cruise`), 3 white
medium (`pax_white_medium`), 4 night and 5 sleep (`pax_night_sleep`, which alternates 4 and 5; on the -900 it always
gives 4). The aircraft sets white bright when a flight starts and never changes the lights by itself. The night
cruise preset Sleep falls back to Night where 5 can't be reached.

**Cabin chime from the aircraft:** the Zibo only (`call_chime` above). On LevelUp the setting is shown unavailable and
X-Plane's chime plays.

**CABIN READY** (`laminar/B738/system/cabin_ready`): the native plugin sets it to 1 by itself on the taxi out (a
5 s timer once taxiing; its other conditions, around the aircraft's own safety demo, were only partly read from the
machine code) and again on the approach. With the CABIN READY light option off (`laminar/B738/panel_cab_ready` 0) it
then chimes, and on the taxi out also plays "cabin secure". Only `laminar/B738/push_button/
rst_cabin_ready` and a new flight reset it. A plugin's write is accepted and discarded (its setter is empty), so
Xnounce leaves it alone and only records it.

**Aircraft check** (Log tab, with *Show log* on): each door, the airstairs, each light setting, the chime and the
announcement options with their current reading and a Try button.

## 7. Open points
- The cabin's call to the cockpit for CabinSecured: the audio panels publish only the microphone selectors
  (`mic_button1`–`6`, `mic_indicator1`–`6`), no receiver volumes. Xnounce takes `mic_button5` as answering the
  interphone; an rc flight must confirm it.
- The rc.2 flight confirms in the Aircraft check: the door commands and their timing, the airstairs, each light
  setting, the Zibo's chime, and which announcements `msg_airside`, `TO_trigger` and `fmod_vol_crew` silence.
- Verify the rows not marked *(seen)* in a flight with the recorder.
