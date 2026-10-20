# DIRECT JUMP

Direct Jump pattern changes for the **Elektron Digitone mk1**, as an [elekloader](https://github.com/irpina/elekloader)
mod (id `directjump`). With **SETTINGS > DIRECT JUMP** ticked, a pattern you pick while the sequencer plays takes
over on the **next step**, not at the end of the current pattern, and carries on from the same place in the bar.
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

Add `elemods/directjump-1.2-os1.44.elemod` in the elekloader app together with core-dn1, build, and send the
firmware to the Digitone. From the command line:

```
python -m elekloader.patch --stock Digitone_and_Digitone_Keys_OS1.44.syx \
  --mod core-4.1.1-os1.44.elemod --mod directjump-1.2-os1.44.elemod --out Digitone_directjump.syx
```

It combines with DIGISNAKE, PUMP and the rest: checked linked with core-dn1 4.1.1, seq-events 0.1,
DIGISNAKE 1.0, PUMP 2.9 and LØST LVLS 1.0. Tested on a unit: 1.0 never jumped (it took a pick's entry flag, 0xFF, for a stop); fixed in 1.1. 1.1 started the new pattern a step late, so the beat slipped a 16th at each jump; fixed in 1.2.

**Use core-dn1 4.1.1** whenever the build has two or more mod rows in SETTINGS (DIRECT JUMP and PUMP ANIMATION,
say). With 4.1 and earlier, each mod row takes firmware heap at boot, and two of them can stall a *first* boot
(the "FACTORY PROJECT >> +DRIVE" screen). 4.1.1 keeps the rows off the heap. It is in elekloader's source
(`mods/core-dn1`), not yet a release: build it with the SDK (`python -m elekloader.sdk.build mods/core-dn1 --stock ...`).

## Using it

**SETTINGS > DIRECT JUMP**: YES (or RIGHT) ticks it, YES (or LEFT) unticks it.

Ticked, with the sequencer playing:

- **Pick a pattern** (PATTERN + a trig key, or a program change in): it starts on the next step, at the step where
  the old pattern was, wrapped to the new pattern's length. Bar 2, step 5 of a 64-step pattern, into a
  16-step one: the new pattern plays on from its own step 5.
- **Per-track scale mode**: every track lands where it would be had the new pattern been playing all along,
  in its own speed, wrapped to its own length.
- **Chains** play on as stock (pattern to pattern at each end). Picking a new pattern or chain while one plays
  jumps to its first pattern.
- **Song mode** is left as stock.
- **Picking the pattern already playing** changes nothing (it does not restart it).

The jump lands at most one step and a bit after the press: the sequencer decides two ticks into each step,
for the step after it.

The setting is saved with the project, and on OS 1.44 it survives a power cycle. Loading a project saved
without it, or making a new one, keeps whatever the unit had.

## How it works

The Digitone's sequencer changes pattern in its 24 PPQN tick interrupt. Two ticks into every master step it
looks at the next entry of its pattern source (the chain), and only on the master length's last step it arms
the change; at the next step boundary it switches, starting the new pattern at step 0.

Picking a pattern gives the source a new chain and sets its index to -1 until the switch. DIRECT JUMP reads
that: on the step after a pick, it arms the change itself, as the stock does, with the master counter (wrapped
to the new length) as the start step (site `0x40095fdc`). The stock switch then plays it. The switch's per-track
arithmetic assumes step 0 (it reads the old pattern's track speeds and never wraps), so after a jump DIRECT JUMP
works each track's step out again from the new pattern (site `0x4009643a`).

The SETTINGS row is a core-dn1 `ev_settings` row; the setting is core-dn1 project data (`DJMP`), and on 1.44
a change asks the OS to refresh its power-up copy of the project, as DIGISNAKE and PUMP do.

| | 1.44 |
|---|---|
| site A, after the next pattern is peeked | `0x40095fdc`, over `mvs.w 0x4138f2d8,d1` |
| site B, the switch's per-track steps | `0x4009643a`, over `move.w d4,0x4138f276` |
| the stock arm it reproduces | `0x40096054` (goes on at `0x40096464`) |
| the sequencer's state | `0x4138f000` + offsets in `directjump.c` |

## Checked

- Builds, lints and links with core-dn1 4.1.1, seq-events 0.1, DIGISNAKE 1.0, PUMP 2.9 and LØST LVLS 1.0;
  elekloader verified the patched image (every other section byte for byte, the main OS unpacks in place).
- The compiled ColdFire code read back against the stock routines it stands in for.
- The logic run natively against a model of the sequencer's state: every condition that leaves a change to the
  stock, the arm's fields, the start step, and each track's step and offset in per-track mode at mixed speeds
  and lengths.
- **Not yet run on a unit or in an emulator.** First things to try on the Digitone: a jump mid-pattern in normal
  mode, a jump into a pattern with a different length, a chain, and a per-track pattern.

## Building

With elekloader and an m68k cross GCC on the path:

```
python -m elekloader.sdk.build --stock Digitone_and_Digitone_Keys_OS1.44.syx .
```

## Credits and third-party code

DIRECT JUMP is Robbie's own work, written with Claude (Anthropic). The SETTINGS row's four callbacks are adapted
from **digihealth**'s SYSTEM INFO row (irpina/digihealth, `sysinfo.s`, GPL-2.0-or-later), as PUMP's are; the
power-up copy request follows DIGISNAKE's. It builds against irpina's elekloader SDK and runs on core-dn1
(GPL-2.0-or-later); none of them is included here. The firmware addresses were found by studying the Digitone's
own OS so the mod can work with it; no Elektron code is included.

## Trademarks

Elektron and Digitone are trademarks of Elektron Music Machines. They are named only to say what this works
with. This project is not affiliated with or endorsed by Elektron.

## Licence

GPL-2.0-or-later, see [LICENSE](LICENSE).
