# Zibo and LevelUp Boeing 737

**Scope:** the Zibo 737-800 and the LevelUp 737NG series: how Xnounce recognises them, and the datarefs and commands
the 737 support will read. In 0.3.5 the module only recognises these aircraft and stays dormant on them; the purser
support follows in the next versions. The code is in `src/aircraft/zibo737/`.

**Sources:** the aircraft's own files: `B738_Datarefs.txt`, `B738_Commands.txt`, the xlua scripts and the strings
of `zibomod.xpl`, for ZiboMod 4.05.35 and LevelUp 737NG 2.S1.51C. Both aircraft run the same systems: the same
`zibomod.xpl` plugin and 14 of their 16 xlua scripts byte for byte (the FMS and the tablet scripts differ in a few
dozen lines; LevelUp adds animation, sine and sound scripts). Every row below is **assumed** until a flight with the
recorder on confirms it (`toliss.md` § 5 describes the method).

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
How each signal of the snapshot (VP_SPEC § 4.2) could come from these aircraft.

| Signal | Dataref | Encoding |
|---|---|---|
| Flight phase | `laminar/B738/FMS/flight_phase` | 0 takeoff (also before the flight and after the rollout), 1 climb, 2 cruise, 3 cruise climb (step), 4 cruise descent (step), 5 descent, 6 approach, 7 go-around armed, 8 go-around. Not the order of Xnounce's phases: it needs a mapping, and there is no PreFlight or Done |
| Takeoff power cue | `laminar/B738/autopilot/ap_takeoff` (TO/GA engaged), `laminar/B738/FMS/N1_mode` (1–7 takeoff modes: TO, TO 1, TO 2, D-TO…) | to be decided which one follows the thrust |
| Lift-off, touchdown cues | `laminar/B738/air_ground_sensor` (0 in the air, 1 on the ground); `laminar/b738/fmodpack/flightphase_landed` (1 from touchdown through the rollout) | |
| Seatbelt sign | `laminar/B738/toggle_switch/seatbelt_sign_pos` (0 OFF, 1 AUTO, 2 ON); lit: `laminar/b738/fmodpack/seatbelt_on_light` | AUTO lights the sign by the aircraft's own logic |
| NO SMOKING | `laminar/B738/toggle_switch/no_smoking_pos` (0 OFF, 1 AUTO, 2 ON) | the lit state is not published; the switch position must do. `laminar/B738/fms/no_smoking` is an aircraft option (signs, chimes only, off) |
| Passenger doors | `737u/doors/L1` (forward left entry), `R1`, `L2`, `R2`, `L_mid`, `R_mid` (-900/-900ER), `emerg1`–`emerg4`; cargo `Fwd_Cargo`, `aft_Cargo` | 0 closed … 1 open. `laminar/B738/doors/status` is a status array (0 open, 1 moving, 2 closed) |
| Slides | none | the 737NG's girt bars aren't modeled: no slide signal (`slidesKnown` false) |
| Passengers | `laminar/B738/tab/zone1_payload` … `zone5_payload` (men, women, children per zone), `laminar/B738/std_pax_num` (seats per zone of the layout) | the sum of the zones; boarding speed setting `laminar/B738/FMS/pax_time` (0 real, 1 long, 2 short) |
| AC power | `laminar/B738/electric/ac_tnsbus1_status`, `ac_tnsbus2_status` (transfer buses, 1 powered); `laminar/B738/electric/ife_pass_bus1`, `ife_pass_bus2` (IFE and passenger buses) | |
| Parking brake, chocks | `laminar/B738/parking_brake_pos` (1 set); `laminar/B738/fms/chock_status` (1 placed) | |
| Landing lights ON | `laminar/B738/switch/land_lights_ret_left_pos`, `_right_pos` (0 retract, 1 extend, 2 on); `laminar/B738/switch/land_lights_left_pos`, `_right_pos` (wing root, 0/1) | ON is 2 on a retractable light or 1 on a fixed one |
| Strobe ON | `laminar/B738/toggle_switch/position_light_pos` (−1 STEADY, 0 OFF, 1 STROBE & STEADY) | 1 is the line-up position |
| Beacon | `sim/cockpit2/switches/beacon_on` | X-Plane standard (already in the shared reader) |
| Gear lever | `laminar/B738/controls/gear_handle_down` (0 up, 0.5 off, 1 down) | |
| Cockpit door | `laminar/B738/door/flt_dk_door_ratio` (0 closed … 1 open) | |
| Cruise altitude | `laminar/B738/autopilot/fmc_cruise_alt` (ft) | |
| Flight number | none | the FMC keeps it in script variables; the airline must come from SimBrief, the livery or `liveries.ini` |
| Masks, cabin altitude | `sim/operation/failures/rel_pass_o2_on` (6 deployed), `laminar/B738/one_way_switch/pax_oxy_pos`; `sim/cockpit2/pressurization/indicators/cabin_altitude_ft` | |

## 3. Cabin calls and the interphone
- **Cockpit to cabin:** the overhead ATTEND button, command `laminar/B738/push_button/attend` (position
  `laminar/B738/push_button/attend_pos`). It is the 737's cabin call button for VP_SPEC § 4.3a. GRD CALL
  (`laminar/B738/push_button/grd_call`) calls the ground crew and would only be logged.
- **Cabin to cockpit:** the CALL light, `laminar/B738/annunciator/cabin_call_annun`.
- **Taking the call and listening to the PA:** the audio control panels' microphone selectors,
  `laminar/B738/audio/{capt,fo}/mic_button5` (cabin interphone) and `mic_button6` (PA), with the commands
  `laminar/B738/audio/{capt,fo,obs}/mic_push5`/`mic_push6`. Whether a receiver volume is published is still open.

## 4. The aircraft's own cabin announcements
The 737 plays announcements of its own (the "fmodpack"). Xnounce must not play on top of them.

| What | Dataref | Notes |
|---|---|---|
| Captain's messages on or off | `laminar/b738/fmodpack/msg_airside` | 0 off, 1 automatic (default 1); EFB tablet, audio features page |
| Crew announcements on or off | `laminar/b738/fmodpack/fmod_crew_on` | also a command of the same name that toggles it |
| An announcement is playing | `laminar/b738/fmodpack/msg_playing` | seconds left; 0 when silent |
| Takeoff announcement trigger | `laminar/b738/fmodpack/TO_trigger` | 0 off, 1 STROBE & STEADY, 2 ATTEND button |
| What plays | `play_welcome_msg`, `play_TO`, `play_dta` (doors to automatic), `play_after_takeoff`, `play_cruise_msg`, `play_descent_msg`, `play_preland_msg`, `play_turbulence_msg`, `play_seatbelt`, `play_landed`, `play_taxi_to_gate` (all `laminar/b738/fmodpack/...`) | triggers set by the aircraft's FMOD logic script |
| Voice set, volumes | `fmod_announcement_set` (1–4), `fmod_vol_crew`, `fmod_vol_int_pax` (0–10) | |

## 5. Open points for the 737 support
- How to map the flight phase: 0 serves before the flight, the takeoff and after the rollout; Done must come from the
  ground state after a flight.
- Which takeoff-power signal follows the thrust levers best (`ap_takeoff`, `N1_mode` or the throttle ratio).
- Whether to switch off the aircraft's own announcements (Xnounce never writes datarefs), wait for `msg_playing`, or
  ask the user to turn the captain's messages off in the EFB.
- The cabin's call to the cockpit for CabinSecured: whether the 737 has a receiver knob like the Airbus ACP, or the
  call is answered by a microphone selector.
- Verify every row in a flight with the recorder.
