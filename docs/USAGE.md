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

When Jimmy receives MIDI notes, it will automatically identify the chord (major, minor, 7th, dim, aug, sus, etc.) and display it. Chords detected during playback are recorded at their bar positions and persist when you stop.

### 2b. Import chords from MIDI (recommended)

Instead of relying only on live playback capture, you can import all chords at once by dragging a MIDI file onto Jimmy's window:

1. In Cubase, go to **Project → Chord Track → Chords to MIDI** to convert your chord track into a MIDI part
2. **Drag the MIDI part** from the Cubase arrangement directly onto Jimmy's plugin window
3. Alternatively, export a `.mid` file and drag that file onto the window

Jimmy will parse all the MIDI notes, identify the chords, and place them at their correct bar positions on the timeline. A toast message will confirm how many chords were imported.

**Tip:** Use the **Clear MIDI Chords** button in Edit mode (Lyrics tab) to remove imported/captured chords before re-importing.

### 2c. Import full song data from MIDI

If the MIDI file was exported from Jimmy (contains the `JIMMY:v1` marker), **all song data** is restored — lyrics, sections, chords, timing, and per-line overrides. This is the recommended way to transfer songs between Cubase projects.

1. Export from Jimmy using the **"Drag to Jimmy track ↗"** zone (see below)
2. Drag the exported MIDI part or file back onto Jimmy's plugin window
3. Jimmy detects the marker and imports the full song state

If you already have clips loaded, the new data is added as a **new clip** after the last one.

**Note:** For the primary workflow (syncing Jimmy with your DAW timeline), drag the exported file to Jimmy's **instrument track** instead. See section 2d.

### 2d. Exporting to MIDI

When lyrics are loaded in Edit Mode, a **"Drag to Jimmy track ↗"** zone appears at the bottom of the editor. This lets you export all Jimmy data as a MIDI file that syncs with your DAW:

1. Click and drag the export zone onto your **Jimmy instrument track** in Cubase
2. Cubase creates a MIDI clip containing your lyrics, chords, sections, and timing
3. The clip is self-contained — moving it in the arrangement keeps everything in sync
4. When Cubase plays the clip, Jimmy automatically displays the correct song data

The exported file uses:
- **SysEx messages** containing the full song data (gzip-compressed XML) — this is what the DAW sends to Jimmy during playback, keeping lyrics and chords synced to the timeline
- **Lyric Meta Events** (RP-017) for lyrics text (backward compatibility)
- **Text Meta Events** for chords, sections, and metadata (backward compatibility)
- **Tempo and time signature** matching your current project settings

Song data SysEx is repeated every 16 bars in the MIDI file, so if you start playback mid-song, Jimmy will pick up the song data within ~16 bars.

**Tip:** You can also drop the exported MIDI file directly onto Jimmy's plugin window to re-import the full song (useful for copying between projects).

### 2e. Live Source Toggle

The toolbar features a **LIVE INPUT / FROM EDITOR** toggle button:

- **LIVE INPUT** (default) — chords come from real-time MIDI input, detected by Jimmy's chord parser. The transport position drives lyric scrolling.
- **FROM EDITOR** — chords and lyrics come entirely from the pre-loaded data in the editor. The plugin reads only the DAW transport bar position to determine which lyric line and chord to display. No MIDI chord parsing occurs, reducing audio thread load.

Use **FROM EDITOR** mode when you've imported or entered all your chord and lyric data and want a fully pre-programmed display.

### 2f. Multi-Song Setlist

You can set up a full live show with multiple songs on a single Jimmy instrument track:

1. Enter lyrics and chords for Song 1 → export using "Drag to Jimmy track"
2. Clear the editor, enter Song 2 → export to the same track, placed after Song 1
3. Repeat for all songs in your setlist

Each exported MIDI clip is **self-contained** with its own lyrics, chords, sections, and timing. Jimmy automatically:

- Detects which song is currently playing when Cubase sends the clip's SysEx data
- Displays the correct lyrics and chords with bar positions relative to each song
- Transitions seamlessly between songs — the next song loads instantly when its clip begins
- Shows a blank display during gaps between clips

**Moving clips** in the Cubase arrangement automatically moves the songs in Jimmy — no need to re-export or re-import.

**Starting mid-song:** If you start playback in the middle of a song clip, Jimmy will pick up the song data within ~16 bars (song data is repeated at regular intervals in the MIDI clip).

### 2g. Chord display in Live Mode

In Live Mode, **all chords** are shown above each lyric line at their proportional position within the line's bar range. This means:

- If a lyric line spans bars 5–9 and has chord changes at bars 5, 7, and 8, all three chords appear above the line at roughly 0%, 50%, and 75% of the line width
- The **currently active chord** (the one playing right now) is displayed brighter than upcoming chords
- A carry-over chord from the previous line is shown at the start of each line

### 3. Using sections (optional)

Sections represent song structure (Verse, Chorus, Bridge, etc.). There are two ways to define them:

**Option A: Inline in lyrics text (recommended)**

Include section markers directly in your lyrics using `[Section Name]` syntax:

```
[Verse 1]
First line of verse
Second line of verse

[Chorus]
Chorus line one
Chorus line two
```

When you click **Apply Lyrics**, sections are automatically created from the markers.

**Option B: Manual section editor**

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

#### Timeline directives

You can control bar timing directly in the lyrics text instead of editing the mapping table:

- **`[break: N]`** — inserts a gap of N bars before the next lyric line
- **`[length: N]`** at the end of a line — sets that line's duration to N bars

Example:
```
[Verse 1]
[break: 2]
First line of verse [length: 4]
Second line [length: 3]

[Chorus]
Chorus line one [length: 4]
Chorus line two [length: 4]
```

Lines without a `[length:]` directive default to 2 bars each.

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

## In-Plugin Help

Click the **?** button in the toolbar to view a quick-reference guide directly inside the plugin. This covers the lyrics format, section markers, and timeline directives.
