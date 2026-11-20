#!/usr/bin/env python3
"""Turns what a plugin's native interface drew (NULL_UI_LAYOUT, native/common/NullUI.hpp) into
the layout file of its 3D hardware render: pixels of the 2x screenshot, origin top left.

Kinds: screen (the display card: curves, meters, spectrum, grids; printed or lit on the panel),
panel (the other cards), logo, button (the header's dice, undo, redo, A, B, A>B), tab (v = 1 on
the open tab), select (arrow boxes, the preset box), knob (body x, y, w, h; cx, cy; ring_radius
for the value arc; v = 0..1 position), slider (the track; v = 0..1), keyboard.

    layout.py <raw.json> <out.json> <plugin>
"""
import json
import sys

S = 2.0


def main(raw, out, plugin):
    d = json.load(open(raw, encoding="utf-8"))
    items, cards = [], 0
    for it in d["items"]:
        kind = it["kind"]
        if kind == "card":
            kind = "screen" if cards == 0 else "panel"
            cards += 1
        o = {"kind": kind, "x": round(it["x"] * S, 1), "y": round(it["y"] * S, 1),
             "w": round(it["w"] * S, 1), "h": round(it["h"] * S, 1)}
        if it["label"]:
            o["label"] = it["label"]
        if it["v"] >= 0:
            o["v"] = it["v"]
        if kind == "knob":
            o["cx"], o["cy"] = round(o["x"] + o["w"] / 2, 1), round(o["y"] + o["h"] / 2, 1)
            o["ring_radius"] = round(o["w"] / 2 + 10 * S, 1)
        items.append(o)
    json.dump({"plugin": plugin, "image_size": [d["width"] * S, d["height"] * S], "scale": S, "controls": items},
              open(out, "w", encoding="utf-8"), indent=1)


if __name__ == "__main__":
    main(*sys.argv[1:4])
