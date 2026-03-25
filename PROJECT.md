# Jimmy -  VST for playing live over Cubase project with chords and lyrics

## Background
When I'm creating a cubase project for my live shows, I start with the following tracks:
- **marker track**  - for marking the song parts (verse, pre-chorus, chorse)
- **chord track** - for writing the chords on the timeline

The missing part in my workflow is a place to add the lyrics of the song and display both lyrics and chords in a live mode telepromter style - when the view sync with the current place of the timeline and moved automatically while playing or recording in the project.

## Suggested solution
Create a new "VST instrument" that will be used as a teleprompter for displaying the chords and lyrics in a live mode. The VST will pull the data from the chord track and marker track, will have a place to set the lyrics and chords, and display it in a nice design that will be easy to read while performing live or recording.

## considerations
- If you cant access directly to chord track - I can pull the data as midi to the channel the vst will work on or output the data in any other way that you can access it in the vst
- Think of a solution to sync lyrics with the timeline - maybe in the vst plugin UI there will be a place to set the lyrics and mark the length in bars of each line in the lyrics with the timeline.
- When performing live, UI in live mode should be as simple as possible and easy to read - dark mode, zoom in and zoom out options, different colors for chords and lyrics, etc.

## Guideline
- should support cubase 12+
- will be use a lot in live or recording mode so consider build with nice design but without affecting much the overall latancy of the project
- Some songs will be in Hebrew so the VST should support RTL text and Hebrew characters

## Repo content
- source code of the VST plugin with the best practices for VST development
- documentation on how to build the plugin and how to install it in cubase
- documentation on how to use the plugin in cubase and how to set it up with the chord track and marker track
