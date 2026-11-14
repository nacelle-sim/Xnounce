# In-sim acceptance

The owner runs these checks in X-Plane. Agents never mark them passed. Record the date, the X-Plane and ToLiss
versions and the result of each row.

## Setup
1. Copy `build/release/Xnounce` (or the `Xnounce` folder of a CI artifact) to `X-Plane 12/Resources/plugins/`.
2. Put the packs into `X-Plane 12/Resources/plugins/AirlinePacks/` (folders named `DLH`, `EJU`, `UAL`, …; the
   `DLH OLD PA SYSTEM` wrapper can stay as it is).
3. Optional: `[general] recorder = yes` in `Output/preferences/Xnounce.ini`, or tick Settings → Advanced →
   "Record flight trace". The trace goes to `Output/Xnounce/`.
4. For the SimBrief checks: simbrief_hub installed with your Pilot ID, and an OFP generated for the flight.

## Checks
| # | Check | Expected |
|---|---|---|
| A-01 | Load the C172 | Plugins → Xnounce → Show/hide window opens the window (Mute and Overwater greyed out): Status shows the UNSUPPORTED AIRCRAFT card with the C172's name and the chip says UNSUPPORTED; Packs and Settings work. `Log.txt` says dormant; frame time unchanged with the window closed |
| A-02 | Load the ToLiss A321 with the BA livery | Chip ACTIVE; the Status Aircraft card shows the A321, the livery and the pack; `Xnounce.log` gives the reason for the pack |
| A-03 | Titlebar: the White/Dark switch, then the Timeline checkbox | The theme changes at once and is kept after an X-Plane restart; unticking Timeline hides its tab, ticking it brings it back |
| A-04 | At the gate with power, press FWD (or AFT/ALL) on the overhead CALLS panel | `Xnounce.log` shows "cabin call button"; boarding music plays; BoardingWelcome every 2–6 min |
| A-04b | Settings → Announcements → "Boarding starts" = Doors open, doors open | Boarding starts 15 s after a door opens |
| A-05 | Boarding time up (Status, Cabin card: the boarding bar's "complete in …") or beacon on | BoardingComplete, then ArmDoors on beacon/push/engine start |
| A-06 | Beacon on and doors armed, wait at the gate, then push back | Nothing while parked; PreSafetyBriefing 20–40 s after the push starts (at least 60 s after ArmDoors), then SafetyBriefing; music stopped |
| A-07 | After the briefing and the cabin check (taxiing) | The cockpit call buzzer; pulling CAB on the ACP plays CabinSecured. Stage "Cabin secured, waiting for line-up"; CrewTakeSeats not yet |
| A-08 | Takeoff thrust while a call plays | The call stops; nothing more until after takeoff |
| A-09 | Acceleration altitude | AfterTakeoff 20–60 s later; seatbelt sign off → SeatbeltSignOff |
| A-10 | Top of climb, descent | CabinService in cruise; DescentStarted after the descent starts |
| A-11 | Descent below 10,000 ft (or APPR phase), then gear down | The buzzer and CabinSecured (landing) once the cabin check is done; CrewTakeSeats (landing), and CabinDim at night, when the gear lever goes down |
| A-12 | Landing, spoilers stowed, landing lights off | AfterLanding as soon as the lights go off below 40 kt (not 2 min after touchdown) |
| A-13 | At the stand, beacon off | DisarmDoors, then music and DeboardingStarted when the door opens, DeboardingComplete later |
| A-14 | Pause the sim during an announcement | Audio stops; resumes from the same point on unpause |
| A-15 | Cockpit door open/closed, ACP PA knob in/out, camera in the cabin | PA level follows (never below the floor) |
| A-16 | Packs tab during an announcement | The Now playing card shows the announcement, the file and its progress; Skip and Mute work; with Settings → Advanced → Show log on, "What would play now, and why" explains the choice |
| A-17 | Skip (Packs), Play now (Timeline) and the commands | Skip stops the current call; Play now fires the next event |
| A-18 | Rescan with a pack added | The new pack appears; the active pack stays locked after ArmDoors |
| A-19 | Plugins → Admin → disable and re-enable Xnounce | No crash; state rebuilt |
| A-21 | Press a cabin call during the safety briefing, before taxiing | The briefing plays to the end; the buzzer and CabinSecured come about 15 s after it; CrewTakeSeats still waits for the line-up |
| A-20 | Frame time with the window closed vs plugin removed | ± 0.1 ms |
| A-22 | SimBrief OFP for this airport, loaded more than 40 min before its ETD | The Status Flight card shows the OFP, the ETD, the boarding time and "Boarding in …". Silent even with the AUTO door open; boarding starts at that time without a button |
| A-23 | Same, but load inside the lead time (e.g. 20 min before ETD) | Status says the boarding time has passed; boarding starts 15 s after a door opens |
| A-24 | OFP for another airport | The Flight card says "Not used here: origin … NM away"; doors rule; the pack doesn't come from the OFP |
| A-25 | Short flight without initializing the FMGS | Log says the phase is estimated; AfterTakeoff, CabinService (if the cruise lasts) and DescentStarted still play |
| A-26 | A321ceo full flight (any livery) | Same sequence as on the A320; the trace shows the A321 door indices and ACP indices |
| A-27 | ISCS "Ovrd XP Int/Ext Vol" on, then move the internal slider | Once `[toliss] volume_override_ref` is set (`docs/aircraft/toliss.md` § 7), the PA level follows the ToLiss slider |
| A-28 | Load the A339 | Status shows UNSUPPORTED AIRCRAFT with ": a ToLiss model this version doesn't support yet"; `Log.txt` says the model is not supported yet |
| A-29 | Settings → Advanced → Show log on, through a flight | The Log tab appears; a line that repeats takes one row at the bottom with `(xN)`; Live inputs shows the sim values; `Xnounce.log` still has every line |
| A-30 | Instant fuel load in the ISCS at the gate | No "refuelling detected" in the log; an IACP refuel is detected |
| A-31 | Boarding running, ISCS doors on AUTO: wait until ToLiss closes L1 on its own (~13 min after loading) | Boarding continues: music and BoardingWelcome keep going; BoardingComplete only when the Status countdown ends |
| A-32 | During boarding, set L1 from AUTO to OPEN in the ISCS, then close and reopen it | No change to the stage or the countdown |
| A-33 | After BoardingComplete, reopen L1, then beacon on | Stage stays "Boarding complete"; ArmDoors waits for the door to close (up to 120 s) |
| A-34 | During boarding press FWD on the CALLS panel | BoardingComplete about 60 s later |
| A-35 | SimBrief OFP with the OBT left at its default (ETD about 30 min after dispatch), load straight away | Status warns "Boarding shortened to … min"; BoardingComplete about 10 min before the ETD, never sooner than 5 min after boarding starts |
| A-36 | SimBrief OFP, X-Plane clock set 6 h away from the flight's time | Status: "SimBrief boarding time is … h away: is the X-Plane clock set to the flight's time?"; the doors rule starts boarding; the pack still follows the OFP. Setting the clock to the flight's time brings back the schedule |
| A-37 | Doors rule (no OFP), cold and dark: let ToLiss close L1 before switching on power | Boarding starts about 15 s after AC power comes on |
| A-38 | ACP: pull PA, turn its volume knob; pull CAB during a CabinSecured call, turn its knob | The PA level follows the PA knob; the interphone follows the CAB knob (never below 0.15). CAB alone doesn't bring in PA announcements |
| A-39 | Cockpit door closed, PA knob in, camera in the seat | Announcements are quiet and muffled; open the door or walk into the cabin and they become clear; Settings → Sound → Muffle through the door off brings back the old unfiltered sound |
| A-40 | Line up with the landing lights ON (not just extended), or the strobe ON (not AUTO) | CrewTakeSeats (takeoff), then CabinDim at night, right after the signal |
| A-41 | While taxiing (above 5 kt) press FWD on the CALLS panel, after CabinSecured | CrewTakeSeats (takeoff) straight away. Pressed before the taxi it only makes the cabin check quick |
| A-42 | Landing lights ON while the safety briefing still plays | The briefing plays to the end; the cabin check takes 15 s; CabinSecured, then CrewTakeSeats |
| A-43 | Take off with no line-up signal (lights and strobe off, no call) | No CrewTakeSeats; the Timeline shows it expired at takeoff thrust |
| A-44 | Climbing, before AfterTakeoff: press FWD, or switch NO SMOKING off and on again within 10 s | AfterTakeoff plays at once |
| A-45 | Descent: switch NO SMOKING off and on again after CabinSecured (landing), gear still up | CrewTakeSeats (landing) at once |
| A-46 | Cruise with a SimBrief OFP accepted at the gate | Status: "<callsign> <orig>-<dest>: en route to <dest>, … NM"; Xnounce.log has no "pack locked" lines after takeoff |
| A-47 | Turn back toward the departure airport for more than 10 NM | Status: "returning to <orig>, … NM" |
| A-48 | Land at the destination | Status: "arrived at <dest>" |
| A-49 | Settings → Announcements → Turnaround off (default): deboard completely | Stage "Flight complete"; nothing more plays with the doors open. Generate the next leg's OFP in SimBrief: a new flight starts and boards on its schedule |
| A-50 | Turnaround on: deboard completely | As before: stage "Turnaround", next boarding after the gap |
| A-51 | Load at a gate with X-Plane's "start with engines running" | Status note "Started with engines running: boarding skipped…"; a cabin call doesn't board. Shut both engines down (parking brake set): boarding works as from cold and dark |
| A-52 | A pack with `BoardingStarted.ogg` or `Turbulence.ogg` | Packs tab: "not a Fenix announcement type, not played (rename to BoardingWelcome / SeatbeltSignOn to use it)"; Xnounce.log: "pack …: n sounds, 2 not played (not Fenix types: …)" |
| A-53 | Engines shut down at the stand after landing | No "FMGS not initialized" line in Xnounce.log |
| A-54 | Settings → Sound: drag Cabin PA volume during an announcement, then restart X-Plane | The level follows while dragging; the value is kept after the restart (there is no Apply button) |
| A-55 | Settings → Reset to defaults | A confirmation first; then defaults everywhere, but the window position, theme and text size are kept |
| A-56 | Timeline during boarding and in flight | The Coming up card shows the next announcement with a countdown or what it waits for; Play now fires it. History shows Zulu times (local time in 24 h and 12 h on hover) and plain names (the Fenix name on hover) |
| A-57 | Settings → Announcements → This flight is overwater, with a pack that has an `[Overwater]` file | The overwater file becomes eligible; the menu item and the command show the same state |
| A-58 | Status at the gate with a SimBrief OFP, then narrow the window | Cabin and Flight cards side by side, stacked when narrow; the Flight card shows the airline name, callsign, route bar, ETD and boarding time; Recent and Coming up fill down to the Aircraft card (up to 8 rows) as the window grows; long text wraps inside the cards |
| A-59 | Both themes with the cockpit at night and in daylight | Text readable everywhere; the cards are flat, square panels in the theme's tint (from 1.0.0; black with a coloured top line before); no clipped text at 520 px wide or at text size 0.6 and 2.0 |
| A-60 | Titlebar and Plugin Admin (0.3.5) | The titlebar reads "Xnounce 0.3.5 by Nacelle"; Plugin Admin lists the signature `nacelle.xnounce` and the description ends "By Nacelle." |
| A-61 | (0.3.5 only; from 1.0.0 see A-75) Load the Zibo 737-800, then each LevelUp 737 (-600 to -900ER) (0.3.5) | No announcements; Status shows UNSUPPORTED AIRCRAFT with the aircraft's name and ": 737 support is in development"; `Log.txt` says "dormant: Zibo 737-800 (b738.acf) recognised; 737 support is in development" (LevelUp: "LevelUp 737-…"); Settings shows every aircraft's settings under their names |
| A-62 | Load Laminar's default 737-800 (0.3.5) | Not recognised as a Zibo: the log says it is not a supported aircraft |
| A-63 | A full ToLiss flight on 0.3.5 with the recorder on, an FXAnnounce.ini from 0.3.1 in place | The same announcements at the same moments as on 0.3.1; the settings are carried over; Settings → ToLiss shows the ToLiss settings; the trace replays with `xn_replay` |

## 1.0.0-rc.1
Install the `Xnounce-1.0.0-rc.1.zip` prerelease from GitHub. Turn the recorder on for the flights (Setup 3).

| # | Check | Expected |
|---|---|---|
| A-70 | Titlebar and Plugin Admin | The titlebar reads "Xnounce 1.0.0-rc.1 by Nacelle" |
| A-71 | During a flight: Mute, Overwater, Skip and Play now; watch a countdown and the Now Playing bar | Each click shows on the next frame (no visible delay); countdowns tick every second and the bars move smoothly. Log tab (Show log on): "window" and "model" stay well below 1 ms |
| A-72 | Both themes, every tab | The panels are flat and square in the theme's tint, with no black cards; B612, the airline colours and the tab names are as before |
| A-73 | Settings → Sound → PA system: Worn, then Old, then Random per airframe, with an announcement after each change | Worn: thinner, with hiss and hum; Old: tinny, with crackle, short dropouts and a burst of feedback at the start; Random: the same choice every time on the same tail number. The interphone (CabinSecured) and the call buzzer stay clean. Modern sounds as before |
| A-74 | A livery with `pa_effects = off` in its `airlinepack.cfg`, PA system Old | That livery's PA stays clean |
| A-75 | Load the LevelUp 737-800 (then the Zibo 737-800) at a gate | Chip ACTIVE; Status shows "LevelUp 737-800" (or "Zibo 737-800"), the livery and the pack; `Xnounce.log` "active on 737_80NG.acf"; Settings has a "Zibo / LevelUp" page and the 737 rows under Boarding |
| A-76 | A full 737 flight with the FMC set up (doors open at the gate, beacon, push, taxi, line-up, takeoff, cruise, descent, landing, stand) | The same calls as on the ToLiss, in order: welcomes, BoardingComplete, ArmDoors on the beacon or push, the briefings, the cabin's call (X-Plane's seatbelt chime) answered by selecting the cabin interphone microphone (record whether that worked), CrewTakeSeats at line-up (landing lights or strobes), AfterTakeoff, CabinService, DescentStarted, CrewTakeSeats with the gear down, AfterLanding, DisarmDoors at the stand, deboarding. The overhead ATTEND button counts as a cabin call |
| A-77 | A 737 flight with the FMC left empty | `Xnounce.log` notes the estimated phase; the flight calls still come |
| A-78 | On the ground: ground power to APU, then APU to the engine generators, during boarding music or an announcement (737 and ToLiss), Cut out on power transfer on | The PA and the music drop out for 1 to 2 s and come back; the music playlist goes on. With the setting off nothing drops out |
| A-79 | The LevelUp 737-600 and 737-900ER | Status shows 737-600 / 737-900ER; boarding takes less / more time than on the -800 |
| A-80 | Send the 737 traces (`Output/Xnounce/`) | They replay with `xn_replay` (the owner's data for rc.2) |

## 1.0.0-rc.2
Install the `Xnounce-1.0.0-rc.2.zip` prerelease from GitHub. Turn the recorder on (Setup 3) and Show log on, so the
Log tab has the Aircraft check. A-81 to A-87 on the LevelUp 737-800 and the Zibo 737-800, A-88 on the ToLiss.

| # | Check | Expected |
|---|---|---|
| A-81 | Load a 737 with every Cabin crew setting off (the default), fly a short flight | Nothing in the aircraft changes: no door, light or option moves; `Xnounce.log` has "cabin actions: doors available, cabin lights available, aircraft chime …" and no "cabin crew:" lines |
| A-82 | Log tab → Aircraft check at the gate: Try each door, the airstairs, each cabin light setting, the cabin chime | Each door opens or closes within about 7 s, the airstairs in about 32 s, the light setting changes at once; the result column shows "before …, after …". Zibo: the chime sounds. LevelUp: the chime row changes nothing audible. Record any row whose "after" isn't what was tried |
| A-83 | Settings → LevelUp (or Zibo): Cabin crew: doors on. At the gate with the airstairs out, L1 and the forward cargo door open; board until the beacon | At BoardingComplete the airstairs stow first, then the cargo door and L1 close; ArmDoors follows. After landing and DisarmDoors, L1 opens 20–40 s later and deboarding starts with it |
| A-84 | Doors on, with the EFB's Airside Services running the turnaround | Xnounce moves no door; `Xnounce.log`: "the doors are left to the aircraft's Airside Services" |
| A-85 | Cabin crew: cabin lights on; a night flight (sun below the horizon), Night cruise lights Night | White bright at the gate and taxiing; takeoff/landing at the line-up; night after AfterTakeoff; white medium from CabinService to DescentStarted; takeoff/landing from CrewTakeSeats (landing); white bright after AfterLanding. By day: white bright throughout. Change the lights yourself in cruise: Xnounce leaves them until the next stage |
| A-86 | Any cabin crew action on, on the Zibo, then on the LevelUp | Status shows "Cabin crew: …" with the doors' and lights' state; its tooltip says which aircraft announcements are off. Zibo: none of the aircraft's own crew or captain announcements play. LevelUp: no captain welcome/cruise/descent messages and no takeoff announcement; record which of its own announcements still play. Switch the actions off (or load another aircraft): the aircraft's FMC audio options are as before |
| A-87 | Zibo, Cabin crew: aircraft's chime on; the cabin's call before takeoff | The Zibo's own chime sounds instead of X-Plane's. On the LevelUp the setting shows "unavailable" in the Status tooltip and X-Plane's chime plays |
| A-88 | A full ToLiss flight | As on rc.1: nothing changes in the aircraft, Settings → ToLiss has no cabin crew settings |
| A-89 | A 737 flight with the cabin crew off and the aircraft's captain messages on | Xnounce doesn't start an announcement while the captain's message plays ("waiting for the aircraft's own announcement" in the trace) |
| A-90 | Send the 737 traces (`Output/Xnounce/`) | Every action and its result is in the trace ("crew …" lines) |

## 1.0.0-rc.3
Install the `Xnounce-1.0.0-rc.3.zip` prerelease from GitHub. Turn the recorder on (Setup 3) and Show log on. A-91 to
A-100 on the LevelUp 737-800 (A-95 on the Zibo as well), A-101 on the ToLiss. Fly with X-Camera as in Run1(XN).

| # | Check | Expected |
|---|---|---|
| A-91 | Log tab → Live inputs "Camera" while switching X-Camera views: pilot, the wing seats, the cabin camera, an outside view | Pilot: "cockpit, about 0 m aft"; wing seats and cabin camera: "in the cabin, 8…26 m aft"; outside: "external". The trace's `cam_aft_m` column follows |
| A-92 | An announcement heard from the cabin views, then from the cockpit with the door shut, then with the door open | Cabin: clear and full, a little louder under a row of ceiling speakers, quieter towards the galleys at the ends; cockpit with the door shut: quiet and dull (as before); door open: clearer |
| A-93 | Select PA (the PA microphone button) on the captain's audio panel with the cockpit door shut | The PA is clear in the cockpit at Xnounce's PA volume; deselect it: back through the door |
| A-94 | RYR pack: DescentStarted (DescentSeatbelts) and BoardingWelcome, Settings → Sound → Even loudness on (default), then off | On: both equally loud. Off: DescentSeatbelts clearly quieter, as in Run1(XN) |
| A-95 | PA system Worn, then Old, through a long announcement; skip one halfway; pause the sim mid-sentence and resume | No pops or clicks: the crackle is a soft, rare tick; nothing clicks when the sound starts, stops, is skipped, pauses or resumes |
| A-96 | Cabin acoustics on (default), then off, with the boarding welcome | On: fuller, wider (stereo), a little room, "a cabin's ceiling speakers"; off: as recorded. Record whether it now has enough weight |
| A-97 | At the gate during boarding music: GPU → APU → GPU (as in Run1(XN)) | The music drops out for 1–2 s on each transfer and goes on. Then switch the AC power off for good mid-announcement: silent at once; after 3 s the announcement is dropped and isn't repeated when the power returns; the music comes back |
| A-98 | A flight with the default "Silence the aircraft's cabin announcements" on | None of the LevelUp's own crew announcements are heard (safety demo, seatbelts, cabin secure, 10,000 ft, taxi-in, arrival cross-check); the captain's messages (welcome, after takeoff, cruise, descent) do play and Xnounce waits for them. `Xnounce.log`: "aircraft sounds: 12 of 12 cabin crew announcements found". Switch the setting off mid-flight: the LevelUp's crew is heard again. Record any crew announcement that still plays with it on |
| A-99 | A departure with the EFB's Airside Services, as in Run1(XN) | Pressing ATTEND before the tablet asks doesn't start boarding ("the aircraft's turnaround service runs the boarding" in the log). When the tablet asks and you set NO SMOKING OFF then ON/AUTO, the welcome follows within 45 s and repeats every 2–6 min; BoardingComplete plays when the tablet reports the aircraft loaded, not on a timer. Status shows the boarding progress by passengers |
| A-100 | The arrival with Airside Services | DisarmDoors when parked with the beacon off; DeboardingStarted when the tablet deboards (or L1 opens); DeboardingComplete when the tablet is done |
| A-101 | ToLiss A321: load at the airport of a SimBrief plan, and a full flight | The plan is accepted (no "not in X-Plane's airport data"); the cabin is recognised with X-Camera too; everything else as before |
| A-102 | X-Plane replay after a flight, scrubbing back and forth | No "flight reset" lines during the replay; the flight goes on where it was when the replay ends. The takeoff roll logs one "en route" and no "back at" |
| A-103 | Send the traces (`Output/Xnounce/`) and `Xnounce.log` | The log has no per-passenger "boarding plan" lines, and lists "pack RYR has no: …" once |
