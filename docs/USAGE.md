# Using Jimmy in Cubase

## Overview

Jimmy is a VST3 instrument plugin that acts as a **live teleprompter** — displaying lyrics and chords synced to your Cubase timeline. It has two modes:

- **Edit Mode** — enter lyrics, define song sections, and map them to bar positions
- **Live Mode** — auto-scrolling teleprompter display optimized for performing or recording

## Setup

### 1. Add Jimmy to your project

1. Create a new **Instrument Track** in Cubase
2. Select **Jimmy** as the instrument (under the "Instrument" category)
3. Open the plugin GUI

### 2. Route your chord track

Jimmy receives chords via MIDI. To use it with Cubase's chord track:

1. In the **chord track**, make sure you have chords defined on the timeline
2. Go to **chord track inspector** and set it to send MIDI to the Jimmy instrument track
3. Alternatively, use the **Chord Pad** or **Chord Events → MIDI** export to send MIDI notes to Jimmy's track

When Jimmy receives MIDI notes, it will automatically identify the chord (major, minor, 7th, dim, aug, sus, etc.) and display it.

### 3. Using sections (optional)

Sections represent song structure (Verse, Chorus, Bridge, etc.):

1. Switch to **Edit Mode** (default when you open the plugin)
2. Click the **Sections** tab
3. Click **+ Add Section** and set the name, start bar, and end bar
4. Double-click any cell to edit it

**Tip:** You can also trigger sections via MIDI program changes on the same track.

### 4. Enter lyrics

1. In **Edit Mode**, click the **Lyrics** tab
2. Type your lyrics in the text area (one phrase per line)
3. Click **Apply Lyrics** — each line will be mapped to a 2-bar range by default
4. Use **Auto-distribute** to evenly spread lyrics across your defined sections
5. Double-click the **Start Bar** or **End Bar** cells in the mapping table to fine-tune positions

### 5. Switch to Live Mode

Click the **LIVE MODE** button in the toolbar. The teleprompter will show:

- **Section headers** with color coding
- **Chord names** above each lyric line
- **Lyrics** with the current line highlighted
- Auto-scrolling that follows the DAW timeline

Use the **+** and **-** buttons to zoom in/out for readability.

## Hebrew / RTL Support

Jimmy automatically detects Hebrew (and Arabic) text and right-aligns those lines. Chord names above RTL lines are also right-aligned. Chords themselves are always displayed in standard Western notation (LTR).

## Tips for Live Performance

- **Zoom to comfortable size** — use + / - until you can read from your performing position
- **Keep the plugin window visible** — resize and position it on your screen before the show
- **All data is saved with the project** — lyrics, sections, chords, zoom level all persist when you save the Cubase project
- **Low CPU impact** — Jimmy produces no audio output, so it adds virtually zero latency or CPU load
