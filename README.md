# DIRECT JUMP

Direct Jump and Start on Bar for the **Elektron Digitone mk1**, as an [elekloader](https://github.com/irpina/elekloader)
mod (id `directjump`). Two settings decide what picking a pattern does while the sequencer plays:

- **DIRECT JUMP**: the pattern takes over on the **next step**, legato, carrying on from the same place in the bar.
- **START ON BAR**: the pattern starts from its **first step on the next bar line**, perfectly in time. Press the
  key of the pattern that's playing to relaunch it from its first step on the **next beat**, as an Erica Synths
  Perkons does.

Unticked, the Digitone behaves as stock.

By Robbie (designed for a Digitone mk1, OS 1.44). GPL-2.0-or-later.

> Unofficial and unsupported. Flashing modified firmware is at your own risk; the stock OS always
> recovers the unit (hold FUNC while powering on, TRIG 4 OS UPGRADE, send the stock .syx).

## What you need

| | |
|---|---|
| Unit | Digitone mk1 or Digitone Keys |
| OS | **1.44** |
| Mods | elekloader's **core-dn1** (3.3 or later; **4.1.1** recommended, see below) |

## Install

Add `elemods/directjump-1.5-os1.44.elemod` in the elekloader app together with core-dn1, build, and send the
firmware to the Digitone. From the command line:

```
python -m elekloader.patch --stock Digitone_and_Digitone_Keys_OS1.44.syx \
  --mod core-4.1.1-os1.44.elemod --mod directjump-1.5-os1.44.elemod --out Digitone_directjump.syx
```

It combines with DIGISNAKE, PUMP and the rest: checked linked with core-dn1 4.1.1, seq-events 0.1,
DIGISNAKE 1.1, PUMP 2.9 and LØST LVLS 1.0. Tested on a unit: 1.0 never jumped (it took a pick's entry flag, 0xFF, for a stop); fixed in 1.1. 1.1 started the new pattern a step late, so the beat slipped a 16th at each jump; fixed in 1.2. 1.3 added RESTART PATTERN, which restarted on the next 16th and drifted off the beat; 1.4 replaced it with START ON BAR, whose relaunch waited for the bar line, where a 16-step pattern is at step 1 anyway; 1.5 relaunches on the beat.

**Use core-dn1 4.1.1** whenever the build has two or more mod rows in SETTINGS (DIRECT JUMP and PUMP ANIMATION,
say). With 4.1 and earlier, each mod row takes firmware heap at boot, and two of them can stall a *first* boot
(the "FACTORY PROJECT >> +DRIVE" screen). 4.1.1 keeps the rows off the heap. It is in elekloader's source
(`mods/core-dn1`), not yet a release: build it with the SDK (`python -m elekloader.sdk.build mods/core-dn1 --stock ...`).

## Using it

**SETTINGS > DIRECT JUMP** and **SETTINGS > START ON BAR**: YES (or RIGHT) ticks a row, YES (or LEFT) unticks it.

| DIRECT JUMP | START ON BAR | pick another pattern | press the pattern playing |
|---|---|---|---|
| off | off | at the pattern's end (stock) | nothing (stock) |
| **on** | off | **next step, legato** | nothing |
| off | **on** | **step 1, next bar line** | **relaunch: step 1, next beat** |
| **on** | **on** | **next step, legato** | **relaunch: step 1, next beat** |

Both on is the performance setup: jump between patterns without losing the groove, and relaunch the one
playing on the beat. Tap it on every beat to stutter its first beat, or once a bar to keep replaying its first
bar. A relaunch lands on a beat rather than a bar line because a 16-step pattern is on its first step at every
bar line anyway: relaunched there, it could not be heard.

- **Legato jumps**: the new pattern plays on from the step the old one would have played, wrapped to its
  length. Playing step 38 of a 64-step pattern, a jump into a 16-step one goes on from its own step 7. In
  per-track scale mode every track lands where it would be had the new pattern been playing all along.
- **The bar** is the Digitone's own song position: the 16ths since PLAY, or the MIDI song position it is sent.
  So starts stay locked to the tempo and to other gear, whatever the patterns' lengths. A pick waits for the
  next bar line; picking again before then changes what starts. Into a pattern with another master speed,
  step 1 lands as close after the bar line as that speed allows (within a step).
- **DIGISNAKE** (1.1): a start relaunches every snake from its path's start with the pattern, so the whole
  groove restarts together. A legato jump leaves the snakes walking.
- **Chains** play on as stock. **Song mode** is left as stock.

The settings are saved with the project, and on OS 1.44 they survive a power cycle. Loading a project saved
without them, or making a new one, keeps whatever the unit had.

## How it works

The Digitone's sequencer changes pattern in its 24 PPQN tick interrupt. Two ticks into every master step it
looks at the next entry of its pattern source (the chain), and only on the master length's last step it arms
the change; at the next step boundary it switches, starting the new pattern at step 0.

Picking a pattern gives the source a new chain and sets its index to -1 until the switch. DIRECT JUMP reads
that (site `0x40095fdc`) and arms the change itself, as the stock does:

- a **jump** on the very next step, with the step after the one playing as the start step;
- a **start** with step 0 as the start, but only on the step that puts the new step 1 on a bar line (a
  relaunch: a beat) of the song position (16ths × 6 + ticks; a new pattern's first step is heard one of its steps after the switch).
  Until then it holds the stock arm off too.

The switch's per-track arithmetic assumes step 0 (it reads the old pattern's track speeds and never wraps),
so after a jump DIRECT JUMP works each track's step out again from the new pattern (site `0x4009643a`). It
counts each start in `dj_starts`, which DIGISNAKE 1.1 reads (a weak import) to relaunch its paths.

The SETTINGS rows are core-dn1 `ev_settings` rows; the settings are core-dn1 project data (`DJMP`), and on 1.44
a change asks the OS to refresh its power-up copy of the project, as DIGISNAKE and PUMP do.

| | 1.44 |
|---|---|
| site A, after the next pattern is peeked | `0x40095fdc`, over `mvs.w 0x4138f2d8,d1` |
| site B, the switch's per-track steps | `0x4009643a`, over `move.w d4,0x4138f276` |
| the stock arm it reproduces | `0x40096054` (goes on at `0x40096464`) |
| the song position | `0x4138f244` (16ths), `0x4138f248` (ticks) |
| the sequencer's state | `0x4138f000` + offsets in `directjump.c` |

## Checked

- Builds, lints and links with core-dn1 4.1.1, seq-events 0.1, DIGISNAKE 1.0, PUMP 2.9 and LØST LVLS 1.0;
  elekloader verified the patched image (every other section byte for byte, the main OS unpacks in place).
- The compiled ColdFire code read back against the stock routines it stands in for.
- The logic run natively against a model of the sequencer's state: every condition that leaves a change to the
  stock, the arm's fields, the start step, and each track's step and offset in per-track mode at mixed speeds
  and lengths; and a tick-by-tick timeline of 14,112 picks at step lengths 3-48: every start's first step lands
  on a bar line (at equal speeds exactly), within a bar of the pick; and 2,016 relaunches, each on a beat, within
  a beat of the press.
- On a unit (Robbie's Digitone mk1): 1.2's legato jumps stay in time. START ON BAR (1.4) in time for other patterns; 1.5's relaunch on the beat not yet.

## Building

With elekloader and an m68k cross GCC on the path:

```
python -m elekloader.sdk.build --stock Digitone_and_Digitone_Keys_OS1.44.syx .
```

## Credits and third-party code

DIRECT JUMP is Robbie's own work, written with Claude (Anthropic). The SETTINGS rows' callbacks are adapted
from **digihealth**'s SYSTEM INFO row (irpina/digihealth, `sysinfo.s`, GPL-2.0-or-later), as PUMP's are; the
power-up copy request follows DIGISNAKE's. It builds against irpina's elekloader SDK and runs on core-dn1
(GPL-2.0-or-later); none of them is included here. The firmware addresses were found by studying the Digitone's
own OS so the mod can work with it; no Elektron code is included.

## Trademarks

Elektron and Digitone are trademarks of Elektron Music Machines. They are named only to say what this works
with. This project is not affiliated with or endorsed by Elektron.

## Licence

GPL-2.0-or-later, see [LICENSE](LICENSE).
