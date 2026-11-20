# BBD research, fact-checked

Fact-check of a raw Gemini report (search snippets only; not published) against sources that were actually opened on 2026-11-15. Section numbers follow the raw report.

Status values: **verified** (seen in the source), **corrected** (raw report was wrong, old value given), **unverified** (no source reached, kept only if plausible), **removed** (fabricated or unsupported).

## Sources opened

| Key | Document | URL |
|---|---|---|
| DS3005 | Panasonic MN3005 datasheet (MN3000 Series, pp. 24-25) | https://www.experimentalistsanonymous.com/diy/Datasheets/MN3005.pdf |
| DS3205 | Panasonic MN3205 datasheet (pp. 70-71) | https://www.experimentalistsanonymous.com/diy/Datasheets/MN3205.pdf |
| DS3007 | Panasonic MN3007 datasheet (pp. 34-35) | https://www.experimentalistsanonymous.com/diy/Datasheets/MN3007.pdf |
| DS3207 | Panasonic MN3207 datasheet (pp. 80-81) | https://www.experimentalistsanonymous.com/diy/Datasheets/MN3207.pdf |
| DS3208 | Panasonic MN3208 datasheet (p. 86), opened because MN3008 was unreachable | https://www.experimentalistsanonymous.com/diy/Datasheets/MN3208.pdf |
| NE570 | ON Semiconductor NE570/D, Rev. 4, May 2006 | https://www.onsemi.com/pdf/datasheet/ne570-d.pdf |
| AND8159 | ON Semiconductor AND8159/D "Applications for Compandors NE570/571/SA571", Rev. 1, Feb 2005. Same title as Philips AN174; the Philips original was not reachable | https://www.onsemi.com/pub/Collateral/AND8159-D.PDF |
| RS10 | Raffel & Smith, DAFx-10, full text | https://colinraffel.com/publications/dafx2010practical.pdf |
| HP18 | Holters & Parker, DAFx-18, full text | https://dafx.de/paper-archive/2018/papers/DAFx2018_paper_12.pdf |
| RE20 | Boss RE-20 owner's manual, pp. 18-19 (Roland's own description of the RE-201 selector) | https://media.sweetwater.com/store/media/re-20_om.pdf |
| KIM | Boss DM-2 parts list (blog, secondary) | https://kimsmil.blogspot.com/2011/04/boss-dm-2-delay-pedal.html |
| MBTR | madbean "Total Recall" build document, a Deluxe Memory Man clone (secondary); quotes the EH-7850 calibration procedure | https://madbeanpedals.com/projects/_folders/Delay/pdf/TotalRecall24.pdf |
| PPCB | PedalPCB forum, Carbon Copy repair thread (forum) | https://forum.pedalpcb.com/posts/388278/ |
| DUNLOP | Dunlop product page, Carbon Copy | https://www.jimdunlop.com/mxr-carbon-copy-analog-delay |
| ECHO | Raffel's `echo.cpp` and its project page | https://colinraffel.com/software/bbdmodeling/echo.cpp |
| JPC | jpcima/bbd-delay-experimental (GitHub API, README, sources) | https://github.com/jpcima/bbd-delay-experimental |
| CHOW | jatinchowdhury18/BBDDelay (GitHub API, README, sources) | https://github.com/jatinchowdhury18/BBDDelay |
| GIST | jpcima `bbd~.cc` gist | https://gist.github.com/jpcima/7b02edb894ecfbd4009d284793c3d5ed |

Not reachable: RE-201 owner's manual and service notes, Boss DM-2 and DM-3 service notes or schematics, any Carbon Copy schematic, any EHX factory schematic, MN3008 datasheet, Philips AN174 original, Signetics 1985 data book.

---

## 1. Roland RE-201 mode selector

| Position | Head 1 | Head 2 | Head 3 | Reverb | Source | Status |
|:---:|:---:|:---:|:---:|:---:|---|---|
| 1 | on | | | | RE20 p. 18 | verified |
| 2 | | on | | | RE20 p. 18 | verified |
| 3 | | | on | | RE20 p. 18 | verified |
| 4 | | on | on | | RE20 p. 18 | verified |
| 5 | on | | | on | RE20 p. 18 | verified |
| 6 | | on | | on | RE20 p. 18 | verified |
| 7 | | | on | on | RE20 p. 18 | verified |
| 8 | on | on | | on | RE20 p. 18 | verified |
| 9 | | on | on | on | RE20 p. 18 | verified |
| 10 | on | | on | on | RE20 p. 18 | verified |
| 11 | on | on | on | on | RE20 p. 18 | verified |
| REVERB ONLY | | | | on | RE20 p. 18 | verified |

The table is verified against Roland's description of the RE-201 selector in the RE-20 manual, not against the RE-201 manual itself.

| Claim | Value | Source | Status |
|---|---|---|---|
| Groups | REPEAT 1-4, REVERB ECHO 5-11, REVERB ONLY | RE20 p. 18 | verified |
| Reverb-only position | Exists. It is labelled REVERB ONLY, after position 11. Raw report called it "Mode 12" | RE20 p. 18 | corrected |
| Head spacing ratio | Three playback heads "at equal intervals"; head 2 = 2x, head 3 = 3x head 1 | RE20 pp. 18-19 | verified |
| Reverb routing | Spring reverb path is in parallel with the tape echo | RE20 p. 19 | verified |
| Head delay ranges (69-177, 138-354, 207-531 ms) | No source reached. Out of scope: measure on the Volante | none | unverified |
| Tape speed 20-40 cm/s | No source reached | none | unverified |
| "4-spring reverb tank" | No source reached. Do not rely on the spring count | none | unverified |
| Reverb-only mutes the "OP-14B Echo Board" outputs | Board names and wiring could not be checked | none | unverified |
| Citation "Owner's Manual pp. 4-5", link to roland.com | The link is Roland's home page, not a document. Roland's support site lists only the software-effect manual | none | removed |
| Citation "Service Notes, Publication No. 149-014B, OP-13 SW2" | Not reachable, publication number not confirmed anywhere | none | removed |
| "Confidence 100 %, confirmed against schematic contact closures" | The raw report's author did not open a schematic | none | removed |

---

## 2. BBD IC specifications

### 2.1 Datasheet values

Delay formula, verified in RS10 section 2.1: `t_d = N / (2 f_cp)`. All delay ranges below were recomputed and match.

| IC | Stages | f_cp | Delay | V_DD min / typ / max (abs. max) | V_GG | THD typ (max) at V_i, f_cp 40 kHz, 1 kHz | S/N typ | Source | Status |
|---|:---:|---|---|---|---|---|:---:|---|---|
| MN3005 | 4096 | 10-100 kHz | 20.48-204.8 ms | -14 / -15 / -16 V (-18 V) | V_DD + 1 V | 1.0 % (2.5 %) at **0.78 Vrms**. Raw report said 0.5 Vrms | 75 dB | DS3005 | corrected |
| MN3205 | 4096 | 10-100 kHz | 20.48-204.8 ms | +4 / +5 / +9 V (+11 V) | 14/15 V_DD | 0.8 % (2.5 %) at 0.25 Vrms | 67 dB | DS3205 | verified |
| MN3007 | 1024 | 10-100 kHz | 5.12-51.2 ms | -14 / -15 / -16 V (-18 V) | V_DD + 1 V | 0.5 % (2.5 %) at 0.78 Vrms | 80 dB | DS3007 | verified |
| MN3008 | 2048 | 10-100 kHz | 10.24-102.4 ms | -15 V nominal | V_DD + 1 V | 0.5 % at 0.78 Vrms | 78 dB | datasheet not reachable | unverified |
| MN3207 | 1024 | 10-200 kHz | 2.56-51.2 ms | +4 / +5 / +10 V (+11 V) | 14/15 V_DD (4.67 V) | 0.4 % (2.5 %) at 0.25 Vrms | 73 dB | DS3207 | verified |

MN3008: stage count and delay range are consistent with the formula and with the MN3208 (2048 stages, 10.24-102.4 ms, 10-100 kHz, THD 0.5 %, S/N 71 dB, DS3208). The 78 dB and 0.5 % figures for the MN3008 itself were not seen.

Further datasheet values the raw report left out:

| Item | MN3005 | MN3205 | MN3007 | MN3207 | Status |
|---|---|---|---|---|---|
| Input swing at THD 2.5 % | 1.0 Vrms | 0.36 Vrms | 1.5 Vrms | 0.36 Vrms | verified |
| Insertion loss min / typ / max | -4 / 0 / +4 dB | same | same | same | verified |
| Noise, A-weighted, f_cp 100 kHz (max) | 0.4 mVrms | 0.35 mVrms | 0.3 mVrms | 0.25 mVrms | verified |
| Input frequency for 3 dB loss at f_cp 40 kHz (min) | 10 kHz | 6 kHz (as read from the scan) | 12 kHz | 10 kHz | verified |

| Claim | Finding | Status |
|---|---|---|
| Catalogue numbers "BBD-005", "BBD-3205", "BBD-007", "BBD-008", "BBD-3207" | Not on any datasheet page. The pages are from the Panasonic MN3000 / MN3200 Series data book | removed |
| S/N of the MN3205 is "A-weighted" | The noise voltage is measured "weighted by A curve" for all five parts, not only the MN3205 | verified |
| Coolaudio / Xvive clones have "identical" characteristics | Not checked. A search listing of the Coolaudio V3208 datasheet showed THD 0.8 % typ, against 0.5 % for the MN3208, so "identical" is doubtful | unverified |

### 2.2 Supply-dependent limits

| Claim | Value | Source | Status |
|---|---|---|---|
| P-MOS input headroom | 1.0 Vrms (MN3005) and 1.5 Vrms (MN3007) at THD 2.5 %. Raw report said 0.78-1.0 Vrms; 0.78 Vrms is the THD test level, not the limit | DS3005, DS3007 | corrected |
| N-MOS input headroom | 0.36 Vrms at THD 2.5 %. Raw report said 0.25-0.35 Vrms; 0.25 Vrms is the THD test level | DS3205, DS3207 | corrected |
| S/N penalty of the 5 V parts | 8 dB (75 vs 67, 4096 stages) and 7 dB (80 vs 73, 1024 stages). Raw report said 8-10 dB | DS3005/3205/3007/3207 | corrected |
| MN32xx supply ceiling | MN3205 operating max is +9 V; MN3207 and MN3208 +10 V; absolute max +11 V for all. Raw report said "+9 V or +10 V" for the whole series | DS3205, DS3207, DS3208 | corrected |
| P-MOS clock levels | High level 0 to -1 V, low level = V_DD | DS3005, DS3007 | verified |
| N-MOS clock levels | High level = V_DD, low level 0 to +0.5 V (MN3205) or 0 to +1 V (MN3207) | DS3205, DS3207 | verified |
| Headroom "strictly constrained by V_DD - 2 V_th" | Not in any source opened | none | removed |
| Higher supply "accelerates thermal leakage" | Not in any source opened | none | removed |
| Poor clock swing raises charge transfer inefficiency | Plausible, not in any source opened | none | unverified |

### 2.3 Maximum usable delay

Not published as a number. The evidence that exists:

| Claim | Value | Source | Status |
|---|---|---|---|
| Datasheet floor | f_cp min 10 kHz, so 204.8 ms for 4096 stages | DS3005, DS3205 | verified |
| Common practice | "One common configuration" is 4096 stages at 300 ms, which needs 6826.7 Hz and an input band-limited to 3413.3 Hz | RS10 section 2.1 | verified |
| Usable bandwidth | Filter cutoff is "typically" between 1/3 and 1/2 of the BBD sampling frequency; "in some BBD-based echo circuits, the filter cutoff can be as low as 1.5 kHz". Raw report said "practically 3-3.5 kHz" at a 10 kHz clock | RS10 section 2.2 | corrected |
| System S/N | About 60 dB near maximum level, before companding | RS10 section 3.1 | verified |
| Leakage currents (10-100 pA, doubling every 8-10 degrees C) | Not in RS10 and not in the datasheets, although the raw report cites both | none | removed |
| "Total collapse" below 4 kHz, whistle at 2 kHz, filter below 1.5 kHz | No source. RS10 only gives the 1.5 kHz lower cutoff quoted above | none | removed |
| Units trimmed to 6.0-6.8 kHz (300-340 ms) | 6.83 kHz follows from the RS10 300 ms example. The 6.0 kHz end has no source | none | unverified |
| "Confidence 100 %, physics documented in Raffel & Smith and Matsushita application notes" | RS10 contains no leakage physics. No Matsushita application note was opened by anyone | none | removed |

For the Matriarch range in `bbd-matriarch.md` (780 ms): 4096 stages would need 2626 Hz, 8192 stages 5251 Hz. Both are far below the datasheet floor, which fits the manual's "noisy or lo-fi" remark.

---

## 3. Anti-alias and reconstruction filters

### 3.1 What a primary source says (generic, not per pedal)

| Claim | Value | Source | Status |
|---|---|---|---|
| Typical topology | Third-order Sallen-Key anti-alias filter; reconstruction is a third-order filter followed by a second-order "corner correction" filter. Eighth order in total | RS10 sections 2.2, 2.3 | verified |
| Active device | "The vast majority of BBD circuits use transistor-based Sallen-Key circuits". Raw report said op-amp based | RS10 footnote 2 | corrected |
| Typical echo values | R = 10 kOhm throughout. Anti-alias: 6.8 nF, 82 nF, 330 pF. Second-order reconstruction: 39 nF, 330 pF. Third-order reconstruction: 2.2 nF, 33 nF, 1 nF | RS10 section 2.3 | verified |
| Cutoff of that typical chain | About 3.2-3.3 kHz at -3 dB with a resonant peak near 2 kHz before the roll-off. My own computation from the printed transfer functions; treat as +-10 % (see Disagreements). Recomputed with the corrected formula: -3 dB at 3.24 kHz, peak +11.6 dB at 1.97 kHz (6 item 4) | RS10 formulas, Fig. 6 | verified |

### 3.2 Per pedal

Arithmetic was recomputed: 4096 / (2 x 0.300) = 6827 Hz, 8192 / (2 x 0.450) = 9102 Hz, 8192 / (2 x 0.550) = 7447 Hz, 8192 / (2 x 0.600) = 6827 Hz. "Underclock" here means below the 10 kHz datasheet minimum: 31.7 %, 9.0 %, 25.5 %, 31.7 %. The arithmetic is right; most inputs to it are not sourced.

| Pedal | Claim | Finding | Source | Status |
|---|---|---|---|---|
| Boss DM-2 | BBD and clock | MN3005 or MN3205 (4096 stages), MN3101 or MN3102 | KIM | verified |
| Boss DM-2 | Compander | NE570N, IC2 | KIM | verified |
| Boss DM-2 | Delay range | 20-300 ms, so 6.83 kHz at maximum | KIM | verified |
| Boss DM-2 | Filters: third-order in, fifth-order out, 3.0-3.3 kHz | Matches the generic RS10 topology, but RS10 does not name the DM-2. No DM-2 schematic was opened | RS10 (generic) | unverified |
| Boss DM-2 | "Factory trims reach about 330 ms", VR2, -65 dBm noise spec, 6.0-6.2 kHz | No source | none | removed |
| Boss DM-2 | Citation "Raffel & Smith" for DM-2 values | The paper names no product | RS10 | removed |
| Boss DM-3 | BBD MN3205, 20-300 ms | Search listings only, no page opened | none | unverified |
| Boss DM-3 | Compander "uPC1571C" | No source reached. Plausible (NEC 571 type), but not confirmed | none | unverified |
| Boss DM-3 | Filter orders and 3.3-3.5 kHz cutoffs | No source reached | none | unverified |
| Boss DM-3 | Citation "Schematic 104-453" | Not found anywhere | none | removed |
| EHX Deluxe Memory Man | BBD | 2x MN3005 or 4x MN3008, 8192 stages | MBTR | verified |
| EHX Deluxe Memory Man | Compander and clock | NE570; CD4047 clock | MBTR | verified |
| EHX Deluxe Memory Man | Maximum delay | About 550 ms (clone document). Measured clock range on that clone: about 75 kHz to 8.4 kHz, which is 54.6 ms to 487.6 ms | MBTR | verified |
| EHX Deluxe Memory Man | "Calibrated; trimmable to 550 ms" | The document says the DMM has no clock calibration; that arrived only with later reissues | MBTR | corrected |
| EHX Deluxe Memory Man | Passband | EH-7850 calibration text: flat to about 900 Hz, about +3 dB peak near 2.5 kHz, -3 dB at about 3.2 kHz, sharp roll-off above 3.5 kHz. Supports the raw report's 3.0-3.5 kHz | MBTR | verified |
| EHX Deluxe Memory Man | Filter orders, "passive 6 dB/oct pre-emphasis", de-emphasis | No source reached | none | unverified |
| EHX Deluxe Memory Man | Citations "EH-7550, EC-2000, Howard Davis design notes" | Only EH-7850 is confirmed to exist (linked from MBTR) | none | unverified |
| MXR Carbon Copy | Delay range | "Up to 600 ms", bucket brigade | DUNLOP | verified |
| MXR Carbon Copy | BBD type | V3208 on the REV-F board (forum post). Chip count not stated | PPCB | verified |
| MXR Carbon Copy | Compander present | A compander at U7 is mentioned (forum post). Part number not stated on that page | PPCB | verified |
| MXR Carbon Copy | "4x BL3208 or 2x V3205, 8192 stages, 6.83 kHz" | Chip count not confirmed by any source opened. If the board carries two 2048-stage chips the clock at 600 ms is 3413 Hz, not 6827 Hz. Measure or open a unit | none | unverified |
| MXR Carbon Copy | Filter cutoffs 2.0-2.5 kHz, "confirmed via hardware traces" | No trace was opened by the raw report's author or by me | none | removed |

### 3.3 Loop darkening (raw section 3.2)

| Claim | Finding | Source | Status |
|---|---|---|---|
| Repeats pass the whole filter chain on every pass | True for the RS10 reference model: `echo.cpp` sums the fed-back BBD output into the compressor input, ahead of the anti-alias filter. Not confirmed per pedal. The raw report contradicts itself in one sentence ("do not re-enter the anti-aliasing filter ... routed back right before the anti-aliasing filter") | ECHO | verified |
| "8th-to-9th-order" per pass | Eighth order in the RS10 typical chain | RS10 | corrected |
| "-36 to -60 dB per repeat above 2 kHz" | No source. The RS10 typical chain has a resonant peak near 2 kHz and only starts cutting above about 3 kHz | RS10 Fig. 6 | removed |

---

## 4. NE570 / NE571 compander

### 4.1 Architecture

| Claim | Value | Source | Status |
|---|---|---|---|
| Blocks per channel | Full-wave rectifier, variable gain cell, op amp, bias system. Two channels | NE570 | verified |
| Internal resistors | R1 = 10 kOhm (rectifier input), R2 = 20 kOhm (gain cell input), R3 = 20 kOhm, R4 = 30 kOhm. Raw report described one "10 kOhm reference/summing resistor" | NE570 Fig. 3 | corrected |
| Resistor tolerance | +-5 % typ, +-15 % max. Raw report said +-20 % | NE570 | corrected |
| Pins (16-pin) | C_RECT 1, 16; rectifier in 2, 15; gain cell in 3, 14; inverter in 5, 12; R3 6, 11; output 7, 10; THD trim 8, 9; GND 4; V_CC 13 | NE570 Fig. 3 | verified |
| Gain law | Gain proportional to the average of the rectified input, so 2:1 compression in the feedback connection and 1:2 expansion feedforward | NE570, RS10 section 3.3 | verified |
| "uPC1571C" as a second source | Not checked | none | unverified |
| ASCII block diagram | Not from a source and wired incorrectly (it shows one 10 k resistor feeding the op amp). Use NE570 Fig. 3 | NE570 | removed |

### 4.2 Time constants

| Claim | Value | Source | Status |
|---|---|---|---|
| Time constant | tau = 10 kOhm x C_RECT. Gain follows one exponential: `G(t) = (G_initial - G_final) e^(-t/tau) + G_final` | NE570 circuit description | verified |
| Attack / release definitions | CCITT, for a compressor. Attack: after a +12 dB input step the output jumps from 1 to 4 units and falls toward 2; attack time is when it reaches 3 units, at t = 0.15 tau. Release: after a -12 dB step the output drops from 4 to 1 and rises toward 2; release time is when it reaches 1.5 units, at t = 0.675 tau. Raw report said "time to settle within 10 % of final gain" | AND8159 | corrected |
| Cause of the asymmetry | There is one RC time constant. Attack looks faster than release because of the compressor's gain law and the CCITT thresholds. Raw report claimed a low-impedance charge path and a 10 kOhm discharge path, which is not what the documents say | NE570, AND8159 | corrected |
| C_RECT 2.0 uF | tau 20 ms, attack 3.0 ms, release 13.5 ms. This is the CCITT recommendation (3.0 +- 2.0 ms and 13.5 +- 9.0 ms) | AND8159 | verified |
| "2.0 uF is the datasheet test standard" | The datasheet test circuit shows 2.2 uF on pins 1, 16 | NE570 Fig. 3 | corrected |
| C_RECT 1.0 uF | tau 10 ms, attack 1.5 ms, release 6.75 ms (0.15 tau, 0.675 tau). Raw report gave 1.5-2.0 and 6.5-7.0 ms | AND8159, recomputed | verified |
| C_RECT 0.47 uF | tau 4.7 ms, attack 0.705 ms, release 3.17 ms | AND8159, recomputed | verified |
| Range used in BBD circuits | 0.22 uF to 1 uF | RS10 section 3.3 | verified |
| C_RECT = 1.0 uF in the DM-2, DM-3 and Memory Man | No schematic opened. Inside the RS10 range, so plausible | none | unverified |
| Designators C8/C15, C7/C16, C10/C19 | No source | none | removed |

Modelling consequence: use one one-pole averager with tau = 10 kOhm x C_RECT on the rectified signal, with the compressor in feedback form and the expander in feedforward form. The attack/release asymmetry then appears by itself. Separate attack and release constants, as the raw report recommends in its section 6.5, are not supported by the sources.

### 4.3 Distortion and noise

| Claim | Value | Source | Status |
|---|---|---|---|
| Ripple distortion | Ripple on the gain control modulates the signal and gives third harmonic distortion | NE570 | verified |
| Rule of thumb | C_RECT = 1.0 uF gives 0.2 % at 1 kHz; "inversely proportional to both frequency and capacitance". So `THD = 0.2 % x (1 uF / C_RECT) x (1 kHz / f)`. The note states it in words, not as a formula | AND8159 | verified |
| Extrapolation | 1 uF at 100 Hz: 2.0 %. At 50 Hz: 4.0 %. Arithmetic is right, but the note gives no validity range | recomputed | verified |
| Missing caveat | "The low frequency distortion generated by a compressor would be cancelled (or undistorted) by an expandor, providing that they have the same value of C_RECT." So the ripple distortion is present inside the BBD path but largely cancels at the output. The raw report presents it as an audible signature | AND8159 | corrected |
| Second harmonic (gain cell) | Untrimmed 0.3 % typ, 1.0 % max; trimmed 0.05 % typ. Caused by offset: 1 mV gives 0.34 % at 0 dBm. Raw report said 0.5-1.0 % | NE570 | corrected |
| THD trim pins | Pins 8 and 9. Raw report said 5 and 12, which are the inverter inputs | NE570 Fig. 3 | corrected |
| "Guitar pedals leave the trim pins open" | Not checked on any schematic | none | unverified |
| Attack overshoot | On a +12 dB step the compressor output momentarily rises to twice its final level before the gain settles | AND8159 Fig. 10 | verified |
| Noise breathing, "25-30 dB" drop | The mechanism follows from the expander law. The number has no source. The datasheet's worked example quotes up to 45 dB channel-noise reduction | NE570 | unverified |
| Source "Signetics Linear LSI Data and Applications Manual 1985, pp. 6-39 to 6-47" | Not reachable, page range not confirmed | none | unverified |
| Source "AN174 (1988)" | Date not confirmed. A search listing dates a Philips copy December 1991; RS10 cites a 1997 edition | none | unverified |
| Source "ON Semiconductor NE571 Datasheet, Rev. 6, NE571/D" | The document is NE570/D. The copy opened is Rev. 4, May 2006 | NE570 | corrected |
| "Formulas verbatim from primary application notes, confidence 100 %" | Several were not verbatim and two were wrong (see above) | NE570, AND8159 | removed |

---

## 5. Papers

### 5.1 Raffel & Smith, DAFx-10

Citation: C. Raffel and J. O. Smith, "Practical Modeling of Bucket-Brigade Device Circuits", Proc. DAFx-10, Graz, 6-10 September 2010, pp. 50-56 (page range from the reference list in HP18). Verified.

| Item | What the paper says | Raw report said | Status |
|---|---|---|---|
| Signal chain (Fig. 2) | Input, compression, anti-alias filter, BBD, reconstruction filter, expansion, output; feedback from the output side to the input | Pre-emphasis and de-emphasis blocks around the compander. The paper has none | corrected |
| Delay | `Delay = N / (2 f_cp)` | Same | verified |
| Filters | Second-order Sallen-Key, equal R: `H(s) = (1/(R^2 C1 C2)) / (s^2 + (2/(R C1)) s + 1/(R^2 C1 C2))`. Third-order: `H(s) = b0 / (a3 s^3 + a2 s^2 + a1 s + a0)` with `b0 = a0 = 1/(R^3 C1 C2 C3)`, `a1 = 1/(R^2 C1 C2) + 1/(R^2 C2 C3)`, `a2 = 2/(R C1) + 2/(R C2)`, `a3 = 1` | A generic product of biquads with omega_0 and Q | corrected |
| Digitisation | Fit the combined analogue response with `invfreqz` (equation-error method); an eighth-order IIR is shown. Main error is high-frequency phase | Same method | verified |
| "Instead of the bilinear transform, which warps near Nyquist" | The paper does not mention the bilinear transform | | removed |
| Averager | One-pole RC: `y(n) = x(n) T/(RC + T) + y(n-1) RC/(RC + T)`, input is abs(x), RC = 10000 x C_rect | `e[n] = (1 - a) e[n-1] + a abs(x[n])` with `a = 1 - exp(-1/(f_s tau))`. Close in behaviour, not the paper's equation | corrected |
| Compander gain | Expander (feedforward): `f(x) = avg(abs(x)) x`. Compressor (feedback): `f(x) = x / avg(abs(f(x)))` | Feedforward `1/sqrt(e/V_ref)` and `sqrt(e/V_ref)` with a V_ref and epsilon. Not in the paper. The compressor in the paper divides by the average of its own output | corrected |
| BBD THD estimate | `THD = 1.01^(N/1024) - 1`, about 1 % per 1024 stages: 1.0 %, 2.0 %, 4.1 %, 8.3 % for 1024, 2048, 4096, 8192 stages. It "does not vary greatly depending on the signal level" | "Roughly 1 % per 1024 stages" | verified |
| Nonlinearity | `f(x) = x - a x^2 - b x^3 + a` for -1 < x < 1; `1 - a - b` for x > 1; `-1 - a + b` for x < -1. Fitted with a = 1/8, b = 1/18. It underestimates harmonics at low level | `f(v) = v - a3 v^3`: no even term, no clip branches, no coefficients | corrected |
| BBD line | Most accurate: a fixed-length delay line of N stages clocked at an artificial clock rate, with interpolation between clock and audio rate. Simpler: an interpolating delay line | Not described | verified |
| Other effects | Insertion gain 0 to 2 dB at low frequency, falling to -4 to -6 dB at the BBD Nyquist limit. Noise at least 60 dB below signal; add it only if self-oscillation without input is wanted | Not described | verified |

### 5.2 Holters & Parker, DAFx-18

Citation: M. Holters and J. D. Parker, "A Combined Model for a Bucket Brigade Device and its Input and Output Filters", Proc. DAFx-18, Aveiro, 4-8 September 2018, **pp. 11-18** (footers DAFx-11 to DAFx-18). Raw report said pp. 79-85. Paper number 12 and the URL are right.

| Item | What the paper says | Raw report said | Status |
|---|---|---|---|
| Page range | 11-18 | 79-85 | corrected |
| Formulation | Each filter is expanded into partial fractions (pole-residue), one first-order recursive section per pole, complex poles paired into real second-order sections. A "modified impulse-invariant transform" | A state-space system with matrix exponentials and piecewise-linear input. None of that is in the paper | corrected |
| BBD | A queue of fixed length N running at the BBD clock. Input sampled on even clock edges, output switched on odd edges. Output is held: `y_BBD(t) = u_BBD(t_n)` for `t_(n+N-1) <= t < t_(n+N+1)`, n even (Eq. 1) | Sample, delay by N/2 clock cycles, zero-order hold. Same idea | verified |
| Input filter | `H_in(s) = sum_m r_m / (s - p_m)` (Eq. 2). Per audio sample k: `x_m(k) = pbar_m x_m(k-1) + u(k)` with `pbar_m = exp(p_m T_s)` (Eq. 11). At a BBD tick `t_n = (l_n + d_n) T_s`: `u_BBD(t_n) = sum_m g_in,m(d_n) x_m(l_n)` with `g_in,m(d) = T_s r_m pbar_m^d` (Eqs. 6, 10). The audio input is treated as a train of weighted Dirac impulses | `v_k = y_i(t_k)` from a state-space model | corrected |
| Output filter | BBD output is a sum of steps `delta(n) = y_BBD(t_n) - y_BBD(t_(n-1))`, n odd. `g_out,m(d) = (r_m / p_m) pbar_m^(1-d)` (Eq. 21). `x_m(k) = pbar_m x_m(k-1) + sum g_out,m(d_n) delta(n)` over the ticks in the last audio interval (Eq. 23). `y(k) = H_0 y_BBD(k T_s) + sum_m x_m(k)` (Eq. 25), with H_0 the DC gain, `H_0 = -sum_m r_m / p_m` (sign reconstructed by me from Eqs. 15-16; the PDF text layer drops minus signs) | Continuous output filter driven by a zero-order hold, integrated with matrix exponentials | corrected |
| Loop | Algorithm 1: for each audio sample, run all BBD ticks that fall in the interval (enqueue on even ticks, dequeue and accumulate on odd ticks), then update the input-filter states with the new sample and emit `y(k)` | Not given | verified |
| Constant-clock check | The BBD then equals `H_BBD(i w) = exp(-i w N / (2 f_BBD)) sinc(w / (2 pi f_BBD))`: a delay of N/(2 f_BBD) plus a sinc droop from the held output (Eq. 34) | Not given | verified |
| Cost | Per second-order section and per BBD tick: one exponential and two cosines, or a polynomial / look-up table in d | Not given | verified |
| Scope | Models sampling and delay only. Noise, nonlinearity and gain errors are explicitly left out. Poles are assumed simple. No compander | Not stated | verified |
| Test case | Juno-60 chorus: fifth-order low-pass in and out (plus a first-order high-pass handled separately with the bilinear transform), N = 256, clock about 26 kHz and up, f_s 44.1 and 88.2 kHz | Not stated | verified |

### 5.3 Does the Holters & Parker model handle a time-varying clock without artefacts?

Raw report: "Yes, it completely eliminates artificial digital interpolation artefacts". Status: **corrected**. What the paper supports:

| Claim | Finding | Source | Status |
|---|---|---|---|
| Built for a varying clock | Yes. Tick times are arbitrary and the fractional offset d_n is evaluated per tick. No separate interpolation filter is used | HP18 sections 1, 3 | verified |
| Step change in clock | A step from 50 kHz to 25 kHz gives "a smooth change in the frequency of the output", where a plain digital delay line would give a discontinuity | HP18 Fig. 8 | verified |
| Modulated clock | Triangular LFO, compared with a recorded Juno-60. Objective difference grades -0.39 to -0.70 (0 = imperceptible, -1 = perceptible but not annoying) | HP18 Table 2 | verified |
| "Completely eliminates" artefacts | No. The model adds "extra aliasing" of its own at 44.1 kHz, from the impulse-train input assumption and from BBD image spectra folding around the audio Nyquist frequency. It disappears at 88.2 kHz. The filters must have enough attenuation at f_s/2; otherwise oversample | HP18 section 4, conclusion | corrected |
| Authentic BBD aliasing is kept | Yes, the paper says aliasing of the analogue system is reproduced | HP18 section 2, conclusion | verified |
| "Clock-whine breakthrough emerges exactly as in the circuit" | Not in the paper. Clock feedthrough is not modelled | HP18 | removed |
| "Lagrange, allpass, sinc interpolators introduce spectral notches, passband ripple, zipper noise" | The paper only says interpolation adds filtering and "distortion produced by the constant variation of the interpolation filter" | HP18 section 1 | corrected |
| Echo-range clocks | Not tested. The lowest clock in the paper is about 26 kHz with N = 256. Behaviour at 3-7 kHz with 4096 or 8192 stages is not covered. With a clock far below f_s there are fewer than one tick per audio sample, which the algorithm permits but the paper does not evaluate | HP18 section 4 | unverified |

### 5.4 Open-source implementations

| Project | Finding | Source | Status |
|---|---|---|---|
| jpcima/bbd-delay-experimental | Exists. C++. BSL-1.0. README cites Holters & Parker 2018. Tabulates the g coefficients over d and interpolates instead of calling `pow` per tick. Clock input is f_clk / f_s per sample. Last push 2019 | JPC | verified |
| "Foundation for ensemble-chorus and string-machine" | Not checked. CHOW's README lists both repositories as references | CHOW | unverified |
| "Chowdhury-DSP/BBDDelay" | That URL returns 404. The repository is https://github.com/jatinchowdhury18/BBDDelay. C++ (JUCE), BSD-3-Clause, "very rough and raw" by its own README. Last push 2021 | CHOW | corrected |
| BBDDelay model | Delay line follows Holters & Parker Algorithm 1. The compander averager cites Huovilainen, DAFx-05, Fig. 11, and uses abs(x)^1.25 in code; the exponent is BBDDelay's own, Huovilainen's paper uses abs(x) (see section 8). README cites both papers | CHOW `src/BBDDelayLine.h`, `src/BBDCompander.h` | corrected |
| Maintained ChowDSP version (not in raw report) | `chowdsp_BBDDelayLine.h` in Chowdhury-DSP/chowdsp_utils, module `chowdsp_dsp_utils`, whose header declares GPLv3. https://github.com/Chowdhury-DSP/chowdsp_utils | module header | verified |
| Raffel `echo.cpp` | Exists. C++ on the Synthesis ToolKit, 2010, Raffel & Smith model. No licence is stated in the file or on the project page. Raw report said "Academic / STK, permissive BSD-style" | ECHO | corrected |
| `echo.cpp` detail | The clip branches read `1 - 1/8 - 1/18` and `-1 - 1/8 + 1/18` with integer literals, which evaluate to 1 and -1 in C++. Do not copy them | ECHO | verified |
| jpcima `bbd~.cc` gist | Exists. Pure Data external, C++, MIT text in the header, copyright Cimalando 2017-2018 and Raffel 2010. Port of `echo.cpp`. Uses the bilinear transform and a fifth-order Butterworth anti-alias filter instead of `invfreqz` | GIST | verified |

---

## 6. Disagreements between sources

1. **NE570 ripple distortion at 2 uF.** AND8159 says 0.1 % at 1 kHz "and 0.33 % at 800 Hz", and in the same paragraph that distortion is inversely proportional to frequency. Inverse proportionality gives 0.125 % at 800 Hz; 0.33 % fits 300 Hz. Probably a typo in the ON Semiconductor reissue. Use the rule, not the 800 Hz figure.
2. **One time constant, two times.** RS10 calls tau = 10000 x C_rect the "decay time". AND8159 measures attack at 0.15 tau and release at 0.675 tau with CCITT definitions. Same circuit, different definitions.
3. **Averager law.** RS10 and Huovilainen (DAFx-05 eqs. 26-27) both use avg(abs(x)); only BBDDelay's code uses abs(x)^1.25. The Signetics datasheet supports avg(abs(x)) (section 8).
4. **RS10 formulas against RS10 Fig. 5.** Evaluating the printed third-order formula with the printed capacitor values gives peaks of 5.8 dB (anti-alias) and 11.8 dB (third-order reconstruction); Fig. 5 shows about 4.5 dB and 6 dB. The second-order filter matches (14.7 dB near 4.4 kHz). Either the printed formula or the printed values are approximate. Fit to a measured response if the exact shape matters. **Resolved 2026-11-16:** the printed formula has a typo. For the unity-gain circuit (C1 to ground, C2 feedback to the output, C3 to ground at the op-amp input), nodal analysis gives a1 = (C1 + 3 C3)/(R^2 C1 C2 C3), not (C1 + C3)/(...). With the corrected a1 the peaks are 4.4 dB and 6.1 dB, which matches Fig. 5. Implemented in `dsp/BbdFilter.h` and checked in `tests/bbd_tests.cpp`. The whole chain (anti-alias x reconstruction) still peaks at +11.6 dB near 2 kHz, re 200 Hz, with -3 dB at 3.24 kHz. That is implausible for a real DM-2, which probably has emphasis or other R values.
5. **Memory Man maximum delay.** MBTR states about 550 ms, and reports a measured minimum clock of 8.4 kHz on the same build, which is 488 ms for 8192 stages.
6. **Carbon Copy BBD complement.** Raw report: 4x 3208 or 2x 3205 (8192 stages). PPCB: V3208 on REV-F, count not given. Nothing opened settles it, and the clock at 600 ms differs by a factor of two between the two readings.
7. **RE-201 last selector position.** Raw report numbers it "Mode 12"; Roland labels it REVERB ONLY and numbers only 1-11.
8. **MN3005 maximum clock.** Raw report says some sources list 200 kHz. DS3005 says 100 kHz. No source with 200 kHz for the MN3005 was found; 200 kHz is the MN3207 figure.
9. **Clone parity.** MN3208 THD 0.5 % typ (DS3208) against 0.8 % typ in a search listing of the Coolaudio V3208 datasheet (not opened).
10. **AN174 date.** 1988 (raw report), December 1991 (search listing of a Philips copy), 1997 (RS10 reference list).
11. **Raw report's "Modes 8 and 9 are swapped in some sources".** No such source was found; the one secondary article seen agrees with the table above.

## What the raw report got wrong

- Holters & Parker is a partial-fraction, modified impulse-invariant model, not a state-space model with matrix exponentials. Its page range is 11-18, not 79-85.
- The claim that the Holters & Parker model is free of artefacts under a varying clock. The paper reports extra model aliasing at 44.1 kHz and recommends oversampling, and it never tests echo-range clock rates.
- Every Raffel & Smith equation except the delay formula: the averager, the compander gains, the filter transfer functions and the nonlinearity (which has an even term, clip branches and a = 1/8, b = 1/18). The pre-emphasis and de-emphasis blocks are invented.
- NE570: asymmetric charge and discharge paths (there is one time constant), the attack and release definitions, THD trim pins (8 and 9, not 5 and 12), untrimmed distortion (0.3 % typ, not 0.5-1.0 %), resistor tolerance, the document number and revision, and the missing statement that compressor ripple distortion is cancelled by a matched expander.
- MN3005 THD test level (0.78 Vrms, not 0.5 Vrms), input headroom figures for both families, and catalogue numbers that do not exist.
- Leakage currents, "V_DD - 2 V_th", the 4 kHz collapse threshold and the per-repeat attenuation figures have no source, although Raffel & Smith and "Matsushita application notes" are cited for them.
- Pedal details presented as service-manual facts (trimmer names, capacitor designators, schematic and publication numbers, a -65 dBm factory spec, "confirmed via hardware traces") come from no document that could be found. Raffel & Smith names no pedal.
- The Memory Man has no clock trimmer in the original design.
- "Chowdhury-DSP/BBDDelay" is a dead URL, its compander is not Raffel & Smith's, and `echo.cpp` carries no licence.
- "Confidence 100 %" appears on five sections whose sources were never opened.

---

## 7. Additions from Gemini Deep Research (checked 2026-11-15)

Claims from a Gemini Deep Research report, each opened and checked by a Sonnet agent. The PDFs are scans; values were read from rendered pages or OCR.

| Claim | Finding | Source | Status |
|---|---|---|---|
| RE-201 manual, mode count | "a total of 12 different echo variations can be obtained (4 ECHO ONLY, 7 ECHO/REVERB, 1 REVERB ONLY)" | RE201 p. 1 | verified |
| RE-201 per-position heads | The manual's dot table matches section 1 exactly (1-4 REPEAT, 5-11 REVERB ECHO, REVERB ONLY unnumbered). Section 1 is now verified against the RE-201 manual itself | RE201 p. 1 | verified |
| RE-201 numbers | No delay times, tape speed or frequency response. It does say: 3-spring reverb unit; keep Intensity under about 6-7 to avoid oscillation; tape life about 300 h (blurry scan). Corrects the raw report's "4-spring" | RE201 | verified |
| MN3008 datasheet | 2048 stages, 10-100 kHz, 10.24-102.4 ms, V_DD -14/-15/-16 V (abs. -18 V), V_GG = V_DD + 1 V, **max input 1.2 Vrms** at THD 2.5 %, THD 0.5 % typ (2.5 % max) at 0.78 Vrms, **S/N 78 dB**, noise 0.4 mVrms max (A, f_cp 100 kHz), -3 dB input frequency >= 10 kHz at f_cp 40 kHz, insertion loss -4/0/+4 dB. Closes the MN3008 gap in section 2.1 | DS3008 pp. 38-41 | verified |
| RS10 "typical echo circuit" = Boss DM-2 | The DM-2 clone's capacitor values equal RS10's typical values exactly: input C12 6.8 nF, C13 82 nF, C14 330 pF; output third-order C19 2.2 nF, C20 33 nF, C21 1 nF; second-order C22 39 nF, C23 330 pF; R = 10 kOhm. RS10 itself names no product, so "RS10 modelled a DM-2" is an inference from the match | AION parts list, RS10 section 2.3 | verified (values); inference (attribution) |
| DM-2 filter devices | Q1-Q4 2N5088 NPN (MPSA18 alternative). Transistor Sallen-Key fits RS10 footnote 2; the document does not say "Sallen-Key" | AION | verified (parts); inference (topology) |
| DM-2 compander | IC2 is NE571 in the parts list and schematic (the text says NE570). No part is labelled C_RECT; "C_RECT = 1 uF" is not supported | AION | corrected |
| DM-2 clock calibration | "Adjust the CLOCK trimmer until you read a frequency of 6.8kHz" (146 us period) at longest delay, giving "around 300ms" with the MN3005 | AION | verified |
| DM-2 8.2 V zener | Z1 8.2 V is fitted only for the MN3205 build, to protect the chip from overvoltage; omitted for the MN3005 | AION | corrected |
| DM-2 behaviour and trims | Self-oscillation at about 12 o'clock on Intensity; each repeat loses treble through the filters; BIAS trim set with a 200 Hz 0 dBm sine for symmetric clipping; CANCEL trim (clock feedthrough); controls Repeat Rate = 1 MB pot, Intensity = 50 kA, Echo = 50 kB. MN3005 build can run on 12 or 15 V | AION | verified |
| "BBDs clip above about 10 % of supply" (also in the Argon run) | Not in RS10. It says only that the input "cannot exceed a small fraction of the supply voltage level" for low THD | RS10 section 3.1 | removed |
| "1.5 kHz cutoff means a 3.0-4.5 kHz clock, 455-682 ms" | The report's own extrapolation of the 1/3-1/2 rule; RS10 does not connect them | none | unverified |

Sources: RE201 = Roland RE-201 owner's manual, https://cdn.prod.website-files.com/6020bb169c964f974852ad3d/61ca079903e8b98857ce62e9_Roland%20Space%20Echo%20ReE-201%20Manual.pdf ; DS3008 = https://www.synfo.nl/datasheets/MN3008.pdf ; AION = Aion FX Amethyst (DM-2 clone) documentation v1.0, https://aionfx.com/app/files/docs/amethyst_documentation_v1.0.pdf

Consequence for d21: the DM-2 is the first non-Matriarch profile with complete, sourced filter values (RS10 transfer functions + AION values), a clock target (6.8 kHz, 300 ms), compander type and a self-oscillation point to test against.

Still open: Carbon Copy chip count, DM-3 filters and compander, where DM-2 repeats re-enter (not traced on the schematic).

---

## 8. Round 2: Gemini Deep Research on the open gaps (checked 2026-11-15)

Raw answer: a Gemini report (not published; Gemini later withdrew several claims itself after pushback). Every row below was opened by a Sonnet agent. Moog manual read via manualsdir.com (https://www.manualsdir.com/manuals/279714/moog-music-500-series-analog-delay.html?page=N, printed page numbers); zzounds and Medium via the Wayback Machine.

### 8.1 Moog 500 Series Analog Delay = Matriarch's delay

| Claim | Finding | Source | Status |
|---|---|---|---|
| Provenance | "MODULE PROVENANCE: Matriarch's Stereo Delay module is based on the Moog 500 Series Analog Delay module." Two BBD lines, 35-780 ms. Measured on one unit: TIME 41.65-404.18 ms (see 8.1b) | Matriarch manual p. 32 | verified (range measured different) |
| Stages | "The BBDs in the Delay Line contains 8192 'buckets'"; Fig. 1 box "8192 STAGE / BBD CHAIN"; title "employing 4 NOS bucket brigate devices, 4 six order low pass filters" | Moog 500 manual pp. 15-16 | verified |
| Chips | "Four NOS Panasonic Low Noise High Voltage BBDs" (retailer list). 4 chips x 2048 = 8192 fits the MN3008 (high-voltage, 2048 stages); 4 x MN3005 would be 16384. Part number not printed anywhere | zzounds (Wayback); inference | inference |
| Signal chain | Sum node (input + feedback) -> COMPRESSOR WITH LIMITING -> INPUT FILTER (SHORT/LONG) -> BBD -> OUTPUT FILTER (SHORT/LONG) -> EXPANDER. Feedback taps the expander output and returns before the compressor. "MIX" in the diagram is the dry/wet pot at the output. Gemini placed feedback after the compressor: wrong | Moog 500 manual Fig. 1 p. 16 | verified / corrected |
| Filters | Sixth-order low-pass, input and output ("2 Dual High quality Moog 6th order filters for anti alias and clock removal") | Moog 500 manual Fig. 1 title; zzounds | verified |
| Wet bandwidth | "0.5x 20Hz - 2.6Khz, 1.0x 20Hz - 1.65KHz nominal" (the 500). Matriarch measured: -3 dB at 2.31-2.35 kHz Bright (2.09 kHz at 404 ms), 1.41 kHz Dark (8.1b) | Moog 500 manual p. 21 | verified (500); measured (Matriarch) |
| 0.5x / 1.0x switch | Halves or doubles delay (35-400 ms / 70-800 ms) and "also selects between Bright (0.5x) and Dark (1.0x) filter settings. Since the Delay's internal anti-alias filter must change with the Delay Time..."; an editor/MIDI override (CC89) sets Dark/Bright separately | Moog 500 manual pp. 5-6, 8, 12 | verified |
| Matriarch Dark/Bright | Manual says only "can be set to either a Bright tone or a Dark tone" (default Bright, CC52). That it is the same 2.6 / 1.65 kHz filter switch is an inference from the provenance line. Measured: a switch of the delay filter, corner down 0.74 oct (the 500's figures: 0.66 oct), same slope; Bright barely moves with TIME, Dark measured at one TIME (8.1b) | Matriarch manual; 8.1b | measured |
| Limiting LED | "RED indicates clipping ... ORANGE flashes indicate the start of limiting" | Moog 500 manual p. 5 | verified |
| Clock range | Not published. Computed for 8192 stages: 800 ms = 5.12 kHz, 780 ms = 5.25 kHz, 400 ms = 10.2 kHz, 35 ms = 117 kHz (above the 100 kHz MN3008/MN3005 datasheet maximum; open). With the measured TIME range: 98.3 kHz at 41.65 ms, 10.13 kHz at 404.18 ms, inside the 10-100 kHz datasheet range (8.1b) | computed | inference (clock not measured) |
| Compander part, clock driver | Not published | - | not found |

### 8.1b Measured on one Matriarch unit (2026-11-17)

Source: `matriarch-measurement.md`, Results 2026-11-17 (Model 24, USB MIDI notes and CC; settings and takes listed there). "Measured" = read from recordings of this one unit.

| Item | Manual or earlier inference | Measured | Status |
|---|---|---|---|
| TIME range | 35-780 ms (Matriarch p. 32) | 41.65-404.18 ms; linear in CC12 MSB, 42.04 + 2.847 ms per step | measured |
| 780 ms | TIME maximum | Not reached by TIME. TIME max + SPACING max is 768.7 ms over MIDI (measured 2026-11-19; earlier inferred as about 764); tap tempo or CV not checked | measured (MIDI); tap tempo/CV open |
| Stages and clock | 8192 stages (500 manual); clock not published | Not measured (needs a recording above 24 kHz). With 8192 stages the measured range means 10.13-98.3 kHz, which fits the 10-100 kHz datasheet range of MN3005/MN3008; this supports, but does not prove, 8192 stages | inference |
| Delay filter, Bright | 500: 2.6 kHz "nominal" wet bandwidth | -3 dB at 2.31 kHz (43.6 ms), 2.35 kHz (202.9 ms), 2.09 kHz (403.6 ms); -6 dB at 2.56-2.58 kHz; then about -50 dB/oct. Barely moves with TIME over this range | measured |
| Delay filter, Dark | 500: 1.65 kHz | -3 dB 1.41 kHz, -6 dB 1.58 kHz, same slope, at TIME 203 ms only | measured (one TIME) |
| Filter in the feedback loop | 500 Fig. 1: feedback returns before the input filter | Each repeat passes the same filter again (pass 2 vs 1: -5.2 dB at 250 Hz, -16.4 dB at 3 kHz) | measured (consistent) |
| Feedback | Self-oscillation above about 2 o'clock (p. 33) | Loss per pass: 9 o'clock -14.5 dB, 12 -5.5 dB, 1 -3.2 dB, just below 2 -0.65 dB; past that it self-oscillates | measured (knob by eye) |
| Polarity | - | The delay path does not invert | measured |
| SPACING | Bipolar, CW lengthens Delay 1 (p. 32) | Adds milliseconds to Delay 1: -157.9 to +360.2 ms at TIME 203 ms (clamped at 45.0 ms), the same curve minus 4.4 ms at TIME min; not a ratio, not linear in CC. At TIME max (2026-11-19) -355 to +363 ms, clamped at 50.9 ms; Delay 2 stays put | measured |
| TIME change | - | Settles within 0.45 s. What is in the loop is re-pitched by t_old/t_new and keeps that pitch round the loop (clocked-buffer behaviour; does not tell BBD from tape or digital) | measured |
| Ping-pong | Delay 1 out -> Delay 2 in -> Delay 1 (p. 33) | Echoes alternate L, R, L ... with one input fed | measured |
| Compander, limiting, THD, hold | Compander and limiter in the 500 (Fig. 1) | Not measured | open |

### 8.2 MXR Carbon Copy (closes the open item)

| Claim | Finding | Source | Status |
|---|---|---|---|
| Chips | Traced M169 schematic: 4x BL3208A (U1-U4), CD4047BM clock (U6), 2x SA571D compander (U7A/B). 8192 stages, so 600 ms = 6.83 kHz | kanengomibako.github.io/pages/00324.html (schematic image) | verified |
| Teardown | "four BL3208 chips, blue LEDs" (post #28, not #29; plain "Carbon Copy", model not stated) | tdpri.com thread page 2 (Wayback) | verified |
| Filters | The author notes pre-/de-emphasis and the pre/post-BBD low-pass filters are "全く同じ" (exactly the same) as the DM-2's | kanengomibako | verified (author's statement) |

Consequence: Carbon Copy profile = DM-2 filter values (section 7) + 8192 stages + SA571 compander.

### 8.3 Boss DM-2

| Claim | Finding | Source | Status |
|---|---|---|---|
| Feedback re-entry | "a portion of the delayed signal is now split off and 'fed back' into the compressor stage", in a generic block diagram of a BBD delay in an article about the DM-2W, whose engineers analysed "the original DM-2". Not stated for the DM-2 schematic itself | rolandindonesia.com/boss-talk-bbd/ | verified (generic) |
| Compander timing caps | C9 (IC2A pin 1) and C25 (IC2B pin 16), both 220 nF to ground, so tau = 10 kOhm x 220 nF = 2.2 ms for compressor and expander. Corrects the "1 uF" in section 4.2 for the DM-2 | Aion Amethyst v1.2.0 BOM and schematic p. 10; pedalpcb forum post #6 | verified (clone docs) |

### 8.4 Low clock rates: DAFx-25 BLEP paper

| Claim | Finding | Source | Status |
|---|---|---|---|
| Paper | Gabrielli, D'Angelo, Squartini, "Antialiasing in BBD Chips Using BLEP", DAFx-25, Ancona, pp. 71-75 | https://dafx.de/paper-archive/2025/DAFx25_paper_29.pdf | verified |
| On Holters & Parker | "The approach is not immune to aliasing even if in many cases it is not practically an issue. The authors proposed oversampling as a solution" | DAFx-25 | verified |
| Method | Input linearly interpolated into an N/2-sample delay line; a polyBLEP residual (third-order Lagrange) added around each clock-tick step at the output, 3 steps each side (eq. 1). Leaves the real BBD clock aliasing at k f_clk +- f0 in place | DAFx-25 | verified |
| Test | MN3005 4096 stages, fs 44.1 kHz, static clocks 10 / 50 / 90 kHz, plus 1 Hz +-20 % clock modulation | DAFx-25 | verified |
| Result | SNR +36.2 dB at 50 kHz, +27.8 dB at 90 kHz. At 10 kHz 16.9 -> 19.1 dB, or 23.2 -> 48.0 dB with the intended clock aliasing excluded. 4-point Lagrange input interpolation adds up to +15.4 dB | DAFx-25 | verified |
| CPU | No numbers; cost scales with the number of BLEP steps | DAFx-25 | verified |
| Code | Octave code and audio at https://dangelo.audio/dafx2025-bbd.html | DAFx-25 | not opened |

Still untested by anyone: 8192 stages at 5 kHz (the Matriarch's long end).

### 8.5 Interpolation alternatives

| Claim | Finding | Source | Status |
|---|---|---|---|
| Holters & Parker speed | "measures 10-40x faster than libsamplerate depending on the sample rate conversion factor" (SRC_SINC_FASTEST, Intel i7, 4th-order Butterworth) | Chowdhury, "Faster Non-Integer Sample Rate Conversion", 2021 (Medium, Wayback) | verified |
| Lanczos | "Update: ... resampling with Lanczos interpolation that performs comparably to the Holters-Parker resampler, but with better accuracy." No numbers | same | verified |

### 8.6 NE570/571 averager law

| Claim | Finding | Source | Status |
|---|---|---|---|
| Law | Rectifier current proportional to the average of abs(V_in - V_ref) | Signetics SA571 datasheet | verified |
| Error | "The alpha's of .995 and .975 will produce errors of .5% on negative swings and 2.5% on positive swings ... a mere .13dB gain error" (datasheet, not AN174 as Gemini said) | SA571 datasheet pp. 5-6 | verified |
| Range | Bias current 50-100 nA limits the rectifier to about 60 dB; a 1 uA average gives a 10 % (1 dB) gain error, "42dB below the maximum input level"; a trim network adds about 20 dB | AN174 (Philips, Dec 1991) pp. 770-771, Fig. 7-8 | verified |
| Noise / range | Output noise 20 uV typ (60 uV max NE/SA571, 45 uV NE570), 15 Hz-20 kHz; "Greater than 110dB dynamic range"; 90 dB S/N at high gain | SA571 datasheet | verified |
| Huovilainen | Expander `y = avg(abs(x)) x` (eq. 26), compressor `y = x / avg(abs(y))` (eq. 27), averager a first-order low-pass. Exponent 1. The abs(x)^1.25 attributed to him in section 5.4 is BBDDelay's own (corrected there) | Huovilainen DAFx-05 pp. 159-160, Fig. 11 | corrected |

Modelling consequence: plain average of abs(x) through a one-pole low-pass, as in Raffel & Smith. Agreed by two papers and the datasheet.

### 8.7 Still not found

DM-3 compander and filter values; measured BBD noise and clock feedthrough at low clocks; Matriarch chip part number, clock driver and compander; how the Moog limiter works. Measure the Matriarch for the last group: protocol in `matriarch-measurement.md`.

### 8.8 Round 3: agy (Gemini 4 Argon, high) on the 8.7 gaps (2026-11-18), checked

Run: a Gemini 4 Argon research run with web search only; its report was complete. Every row below was opened by a Sonnet agent (WebSearch/WebFetch; the datasheets were downloaded and read from Wayback copies of synthdiy.com). Reddit could not be opened.

| Claim (Gemini) | What the opened source says | Source | Status |
|---|---|---|---|
| Matriarch TIME alone stops short of 780 ms; SPACING, tap or sync give more | Owners (2019) found TIME alone well short of 780 ms; with TIME fully cw, turning SPACING cw lengthens the delay, and one owner thinks that is intended. No Moog staff answer. The manual page says 35-780 ms and does not say SPACING is needed. Agrees with our bench (TIME max 404 ms, SPACING +360 ms, 8.1b) | Moog forum "Matriarch delay time" (forum.moogmusic.com/t/matriarch-delay-time/20614); Matriarch manual p. 32 | verified (forum reports) |
| Matriarch uses 4 x MN3008 (Reddit) | Not found. Moog's own design article: Matriarch uses the MF-104M / 500 Series circuit "with a few tweaks to allow for the use of a modern and readily available BBD", so probably not NOS MN3008. No source gives the stages per Matriarch line | musiciansfriend.com/thehub/designing-moog-matriarch-synthesizer | refuted as stated; chip unknown |
| MF-104M range and filter | "TIME - Adjusts delay times from 40ms to 400ms in Short mode and 80ms to 800ms in Long mode." "The Short setting is brighter while the Long setting is darker." "The BBDs in the Delay Line contain 8192 'buckets'". The Matriarch's measured 41.65-404 ms matches the Short range, and its default is Bright | MF-104M manual (manualslib 3755545, manualsnet) | verified |
| MF-104: 4 x MN3008 or 2 x MN3005 | "one has 3005s and the other has 3008s"; "you need only 2 3005's or 4 3008's" (8192 buckets either way). MF-104, not Matriarch | Moog forum "MF-104M empty MN3008 PCB spots" (12033) | verified (forum) |
| MN3005 Vno-fcp curve: -62 / -73 / -81 dBm at 10 / 40 / 100 kHz | The MN3005 datasheet has no Vno-fcp curve. The numbers have no source | MN3005 datasheet (synthdiy.com copy) | refuted (invented) |
| MN3205 Vno-fcp curve: -69 dBm at 10 kHz, -85 dBm at 100 kHz | The curve exists (VDD 5 V, 25 C, log axis 1-1000 kHz, data from about 5 kHz). Read from a 400 dpi render, about 1 dB: -59 dBm at 10 kHz, -64 at 40 kHz, -68 at 100 kHz, -84 at about 400 kHz. So output noise rises about 9 dB from 100 to 10 kHz clock | MN3205 datasheet p. 72 (synthdiy.com copy) | corrected (Gemini's values wrong) |
| MN3005 / MN3205 printed noise | MN3005: Vno max 0.4 mVrms at fcp 100 kHz, A-weighted, S/N 75 dB typ. MN3205: max 0.35 mVrms, same conditions, S/N 67 dB typ (VDD 5 V) | MN3005, MN3205 datasheets | verified |
| DM-3 parts | uPC1571C compander, MN3205 BBD with its clock generator, NJM4558DD input op-amp; "maximum delay of approximately 350ms" on the redrawn unit, spec 20-300 ms. Gemini's IC1-IC4 numbering not confirmed (schematic not opened) | toshi.life.coocan.jp DIY redraw of the DM-3 service schematic (secondary); craveguitars.co.uk DM-3 feature (20-300 ms, MN3205) | partly verified (secondary) |
| DM-3 filters and emphasis | Pre-BBD 3rd-order low-pass, about -18 dB/oct above 2.5 kHz; post-BBD 3rd-order 1.9 kHz plus 2nd-order 4.4 kHz; a shelf boost above 1 kHz of up to 14 dB before the delay with the matching cut after it. No compander capacitor value found | toshi.life.coocan.jp (secondary) | inference (secondary, not the schematic) |

What it changes: the 780 ms on the Matriarch is now supported as "TIME max + SPACING" by owners as well as by our bench. MN3205 noise against clock has a datasheet curve (about +9 dB from 100 to 10 kHz), the first source for "noise at low clocks". The Matriarch's chip is probably not the 500's NOS part, so `stages = 8192` for m8192 rests only on the 500 lineage: the clock measurement settles it. Gemini invented one whole curve (MN3005) and misread the other by 10-17 dB; as in rounds 1-2, nothing from search snippets is usable unchecked.

Still not found: the Matriarch's chip, clock driver and compander; measured clock feedthrough against clock; the DM-3 compander capacitors (need the schematic itself).
