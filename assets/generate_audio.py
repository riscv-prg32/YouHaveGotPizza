#!/usr/bin/env python3
"""Generate the You Have Got Pizza SID-like audio score (assets/audio.json).

PRG32's audio mixer is a real 8-voice, stereo-panned, ADSR synth engine (four
waveforms -- triangle/saw/pulse/noise -- with a filter cutoff/resonance baked
into the sample id, exactly like the classic three-oscillator SID chip scaled
up to eight). This script writes a JSON score consumed by PRG32's own
`tools/prg32audio_pack.py`, which is the same pipeline the PRG32 `bachdemo`
cartridge uses. Nothing here is bespoke to this repository's build: it is the
documented, supported way to ship tracker music on a PRG32 cartridge.

Channel plan (channel index == instrument index for tracker-driven notes,
enforced by the runtime tracker -- see PRG32 components/prg32_audio):

    0  triangle bass       walking ostinato under the chord progression
    1  pulse pad A         chord tone, panned left of center
    2  pulse pad B         chord tone, panned right of center
    3  saw arpeggio lead   the "pizza" hook, bright and busy
    4  filtered noise      off-beat hi-hat tick

Channels 5, 6, and 7 are intentionally left silent in the looping track: the
cartridge reserves them for one-shot sound effects (collect, climb, collide,
pizza-ready, game-over) triggered directly from C via prg32_audio_note*(), so
gameplay stingers never fight the music for a voice. Instruments 8..10 are
extra one-shot timbres for those effects; they are legal because direct note
calls decouple channel from instrument (only the tracker enforces
channel == instrument for NOTE_ON events).
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

WAVE_TRIANGLE = 0
WAVE_SAW = 1
WAVE_PULSE = 2
WAVE_NOISE = 3


def synth_id(wave: int, pulse: int, cutoff: int, resonance: int) -> int:
    assert 0 <= wave <= 3
    assert 0 <= pulse <= 15
    assert 0 <= cutoff <= 15
    assert 0 <= resonance <= 3
    return 0x8000 | (resonance << 10) | (cutoff << 6) | (pulse << 2) | wave


# Chord plan, one entry per bar: (root pitch class, is_minor). C major -> A
# minor -> F major -> G major is the cheerful, instantly-familiar "campus
# quad" progression, played twice per loop (8 bars).
PROGRESSION = [(0, False), (9, True), (5, False), (7, False)] * 2

ARP_PATTERN = (0, 1, 2, 1, 0, 1, 2, 3)  # indexes into each bar's arp notes


def chord_tones(root_pc: int, minor: bool) -> tuple[int, int, int]:
    third = root_pc + (3 if minor else 4)
    fifth = root_pc + 7
    return root_pc, third, fifth


def note(pitch_class: int, octave: int) -> int:
    """MIDI note number for a pitch class (0=C) placed in the given octave."""
    return 12 * (octave + 1) + pitch_class


def event(delta: int, command: str, arg0: int | None = None,
          arg1: int | None = None) -> dict[str, int | str]:
    result: dict[str, int | str] = {"delta": delta, "command": command}
    if arg0 is not None:
        result["arg0"] = arg0
    if arg1 is not None:
        result["arg1"] = arg1
    return result


def build_instruments() -> list[dict]:
    return [
        # 0: triangle bass -- warm, centered, slow attack so it feels rooted.
        {"sample_id": synth_id(WAVE_TRIANGLE, 8, 9, 1), "default_volume": 200,
         "default_pan": 0, "attack": 2, "decay": 20, "sustain": 200, "release": 30},
        # 1: pulse pad A -- panned left, medium filter for a soft edge.
        {"sample_id": synth_id(WAVE_PULSE, 6, 8, 1), "default_volume": 120,
         "default_pan": -36, "attack": 10, "decay": 30, "sustain": 150, "release": 40},
        # 2: pulse pad B -- panned right, mirrors instrument 1.
        {"sample_id": synth_id(WAVE_PULSE, 6, 8, 1), "default_volume": 120,
         "default_pan": 36, "attack": 10, "decay": 30, "sustain": 150, "release": 40},
        # 3: saw arpeggio lead -- bright, resonant, the tune's hook.
        {"sample_id": synth_id(WAVE_SAW, 8, 13, 2), "default_volume": 175,
         "default_pan": -8, "attack": 1, "decay": 12, "sustain": 130, "release": 20},
        # 4: filtered noise hi-hat -- short, dry, ticks the off-beat.
        {"sample_id": synth_id(WAVE_NOISE, 8, 4, 1), "default_volume": 80,
         "default_pan": 0, "attack": 0, "decay": 4, "sustain": 0, "release": 6},
        # 5: collect blip -- bright narrow pulse, pitch varies per ingredient.
        {"sample_id": synth_id(WAVE_PULSE, 3, 14, 1), "default_volume": 210,
         "default_pan": 0, "attack": 0, "decay": 8, "sustain": 60, "release": 20},
        # 6: chime -- soft triangle bell, used for climbing and pizza-ready.
        {"sample_id": synth_id(WAVE_TRIANGLE, 8, 11, 0), "default_volume": 190,
         "default_pan": 0, "attack": 1, "decay": 14, "sustain": 90, "release": 40},
        # 7: thud -- low filtered noise for collisions and game-over.
        {"sample_id": synth_id(WAVE_NOISE, 8, 3, 2), "default_volume": 210,
         "default_pan": 0, "attack": 0, "decay": 30, "sustain": 40, "release": 80},
    ]


def build_track_events() -> list[dict]:
    events: list[dict] = [event(0, "SET_TEMPO", 148)]
    ticks_per_bar = 16  # sixteenth-note ticks; SET_TEMPO ticks are quarter/4.

    for bar, (root_pc, minor) in enumerate(PROGRESSION):
        root, third, fifth = chord_tones(root_pc, minor)
        bass = note(root_pc, 2)
        pad_a = note(third % 12, 4)
        pad_b = note(fifth % 12, 4)
        arp_notes = [note(root_pc, 5), note(third % 12, 5),
                     note(fifth % 12, 5), note((root_pc + 12) % 12, 6)]

        if bar:
            events.append(event(0, "NOTE_OFF", 0))
            events.append(event(0, "NOTE_OFF", 1))
            events.append(event(0, "NOTE_OFF", 2))

        events.append(event(0, "NOTE_ON", 0, bass))
        events.append(event(0, "NOTE_ON", 1, pad_a))
        events.append(event(0, "NOTE_ON", 2, pad_b))

        for step, arp_index in enumerate(ARP_PATTERN):
            delta = 0 if step == 0 else ticks_per_bar // len(ARP_PATTERN)
            if step:
                events.append(event(delta, "NOTE_OFF", 3))
                delta = 0
            events.append(event(delta, "NOTE_ON", 3, arp_notes[arp_index]))

            if step & 1:
                events.append(event(0, "NOTE_OFF", 4))
                pan = -40 if (bar + step) & 2 else 40
                events.append(event(0, "SET_PAN", 4, pan & 0xff))
                events.append(event(0, "NOTE_ON", 4, note(9, 3)))  # noise: pitch is timbre-only

    # Let the last arp note and pads ring briefly, then loop the whole score.
    for channel in range(5):
        events.append(event(2 if channel == 0 else 0, "NOTE_OFF", channel))
    events.append(event(24, "JUMP", 0, 0))
    return events


def build_score() -> dict:
    return {
        "instruments": build_instruments(),
        "tracks": [{"events": build_track_events()}],
    }


def validate(score: dict) -> None:
    instruments = score["instruments"]
    events = score["tracks"][0]["events"]
    assert len(instruments) == 8
    assert all(item["sample_id"] & 0x8000 for item in instruments)
    assert all(-64 <= item["default_pan"] <= 63 for item in instruments)
    assert events[0]["command"] == "SET_TEMPO"
    assert events[-1]["command"] == "JUMP"
    music_channels = {0, 1, 2, 3, 4}
    used_channels = {ev.get("arg0") for ev in events if ev["command"] in ("NOTE_ON", "NOTE_OFF")}
    assert used_channels <= music_channels, "looping track must leave channels 5-7 free for SFX"


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", type=Path, default=Path(__file__).resolve().parent / "audio.json")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()

    score = build_score()
    validate(score)
    rendered = json.dumps(score, indent=2) + "\n"
    if args.check and args.out.exists() and args.out.read_text(encoding="utf-8") != rendered:
        raise SystemExit(f"{args.out} is stale; run generate_audio.py to regenerate it")
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(rendered, encoding="utf-8")
    print(f"wrote {args.out}")


if __name__ == "__main__":
    main()
