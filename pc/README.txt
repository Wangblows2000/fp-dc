Freedom Planet DC - PC version (Linux x86_64 / Windows 32-bit)
=================================================================

This is the Dreamcast port's game code running on a PC through SDL2. It needs the converted game data
(the "converted2" folder made from your own copy of Freedom Planet; see ASSET_PIPELINE.md in the
source tree). No game data is included.

Data
  Put the converted data in a folder named "data" next to the program, or pass its path:
      fpdc /path/to/converted2            (Windows: fpdc.exe C:\path\to\converted2)

Controls (keyboard)
  Arrow keys         move
  Z                  jump / confirm        (Dreamcast A)
  X                  attack / back         (Dreamcast B)
  C                  special               (Dreamcast X; shown as "X" in the game)
  V                  guard                 (Dreamcast Y, unused)
  Enter or Esc       Start / pause
  A / S              L / R triggers
  Alt+Enter or F11   fullscreen on / off
  F10                scaling: aspect fit (sharp) / integer / stretch
  F9                 smooth (bilinear) filter on / off
  F3                 show the VMU screen
  F12                screenshot (BMP, in the save folder)

Game controllers (any SDL2-supported pad: Xbox, PlayStation, Switch Pro, ...)
  south button (Xbox A) jump, east (B) attack, west (X) special, north (Y) guard, Start, LT / RT
  (or LB / RB) the triggers, d-pad or left stick to move.

Saves
  The Dreamcast VMU save is written to fpdc.sav in the user folder:
      Linux:   ~/.local/share/fpdc/FreedomPlanetDC/
      Windows: %APPDATA%\fpdc\FreedomPlanetDC\
  If a file named fpdc.sav exists next to the program, that one is used instead (portable install).
  fpdc.log (the program's log) is written to the same user folder.

Options
  --fullscreen  --window N (window size = N x 426x240)  --scale aspect|integer|stretch  --smooth
  --no-vsync  --no-audio  --save <file>
  Testing: --level <dir> [--char n]  --script <host input script>  --frames N  --fast  --shot-dir <dir> [--shot-every N]

Options > SCREEN SIZE in the game switches between the 16:9 picture and the Dreamcast's 4:3 fill.
