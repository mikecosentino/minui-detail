# minui-detail

A detail screen for [MinUI](https://github.com/shauninman/MinUI) and
[NextUI](https://github.com/LoveRetro/NextUI) paks: a cover image, a title, a
column of facts and a description that scrolls, over a row of button hints.

minui-list only shows rows of single-line text, so a pak's "about this game" screen
turns into `Developer: …` rows with the summary chopped into more of them. This
draws the same facts as a page, and exits with the button that was pressed.

```sh
minui-detail --file /tmp/detail.json \
    --confirm-text DOWNLOAD --action-button X --action-text STAR
case $? in
    0) download ;;   # A
    2) back ;;       # B
    4) star ;;       # X
esac
```

It shows the page and nothing else. What the buttons do is up to the script, so a
download can go on to [minui-progress](https://github.com/mikecosentino/minui-progress)
the same way it does from any other screen.

## The file

```json
{
  "title":       "Final Fantasy VII",
  "subtitle":    "Square · 1997",
  "image":       "/mnt/SDCARD/.../cover.png",
  "fields":      [ {"label": "Genre", "value": "Role-playing (RPG)"},
                   {"label": "Size",  "value": "1.40 GB (Free: 22.1 GB)"} ],
  "note":        "Already downloaded",
  "description": "The world has fallen under the control of …"
}
```

Every key is optional.

| key | |
|---|---|
| `title` | large, wraps to two lines, then cut short with `…` |
| `subtitle` | one dimmed line under the title |
| `image` | an absolute path to a PNG or JPEG. It's scaled to fit on the left with a box filter, so a big cover shrinks cleanly. If it's missing or unreadable, the text uses the full width. |
| `fields` | label/value rows with the labels lined up in a column. A value wraps to three lines. A field with an empty or missing value is left out, so you can pass through whatever you have. Values can be strings or numbers. |
| `note` | one line in the accent color, e.g. `Already downloaded` |
| `description` | wraps to fit. A blank line between paragraphs is kept. |

Only the cover stays put. The text column scrolls as a whole: every MinUI screen is
about 240 units tall once scaled, which leaves no room to give the description a
fixed box of its own. A scrollbar shows when the column doesn't fit.

## Controls

| button | |
|---|---|
| up / down | scroll a line |
| left / right, L1 / R1 | scroll a page |
| confirm (`A`) | exit 0 |
| cancel (`B`) | exit 2 |
| action (`X`) | exit 4, only when `--action-text` is given |

## Options

| flag | default | |
|---|---|---|
| `--file <path>` | *(required)* | the JSON above |
| `--confirm-button A\|B\|X\|Y` | `A` | |
| `--confirm-text <text>` | `SELECT` | |
| `--cancel-button A\|B\|X\|Y` | `B` | |
| `--cancel-text <text>` | `BACK` | |
| `--action-button A\|B\|X\|Y` | `X` | |
| `--action-text <text>` | none | the action button is hidden without it |
| `--show-hardware-group` | off | battery/wifi in the corner, as in the menu |

## Exit codes

These are the same numbers minui-list uses.

| code | |
|---|---|
| `0` | confirm |
| `1` | error: bad arguments, or the file is missing or isn't valid JSON (the reason goes to stderr) |
| `2` | cancel |
| `4` | action |
| `130` / `143` | SIGINT / SIGTERM |

## Building

```sh
make PLATFORM=macos setup-resources   # once: fonts and assets into /tmp/FAKESD
make PLATFORM=macos                   # ./minui-detail-macos
make test                             # the SDL-free parts, no toolchain needed
```

Device builds come from CI (`.github/workflows/ci.yaml`), one artifact per platform.
NextUI on the TrimUI Brick / Smart Pro is `minui-detail-tg5040-nextui`.

To check a layout without a device, `MINUI_DETAIL_SNAPSHOT=/tmp/shot.bmp` saves
every frame drawn, and `MINUI_DETAIL_SCROLL=<px>` starts the view scrolled that far
down. `sample/` has a page to try it on.

## Credits

Built on [minui-presenter](https://github.com/josegonzalez/minui-presenter) (MIT, Jose
Diaz-Gonzalez) by way of minui-progress. JSON parsing is
[cJSON](https://github.com/DaveGamble/cJSON) (MIT, Dave Gamble), vendored as
`cJSON.c` / `cJSON.h`.
