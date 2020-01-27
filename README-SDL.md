# FinalBurn Neo for SDL1.2/2 

## Differences between SDL1.2 and SDL2 versions

The SDL2 port is the recommended version as it has the following features over the SDL1.2 port:

* A better renderer
* OSD  - shows when fast-forwarding and when FPS display is enabled via F12
* Game selection menu which also replicates the rom scanning functionality of the Windows version of FBNeo. Handy for cabinets!
* A lot more testing

## Compiling
### SDL1.2

Assuming you have a working GCC (Mingw and GCC under ubuntu 18.04 have been tested) you can just install libsdl 1.2 and type:

`make sdl`

And out will pop an fbneo executable.

### SDL2

Assuming you have a working GCC (Mingw and GCC under ubuntu 18.04 have been tested) you can just install libsdl 2 and libsdl2-image and type:

'make sdl2'

and out will pop an fbneo executable.

## Running

### SDL1.2

* First run

run the fbneo executable and an fbneo.ini will be created. You will need to edit this to point to your rom directories. You can also edit any of the other otions in the ini file as needed

* subsiquent runs

type 

'fbneo <romname>' 

where <romname> is the name of a supported rom. For example

'fbneo sf2'

will run sf2. 

### SDL2

'-cd' used when running a neocd game. You'll have to look in the code to work out where the the CD images should be placed

'-joy' enable joystick

'-menu' load the menu

'-novsync' disable vsync

'-integerscale' only scale to the closest integer value. This will cause a border around the games unless you are running at a resolution that is a whole multiple of the games original resolution

'-fullscreen' enable fullscreen mode

'-dat' generate dat files

'-autosave' autosave/autoload a save state when starting or exiting a game

'-nearest' enbable nearest neighbour filtering (e.g. just scale the pixels)

'-linear' enable linear filter (or is it a bilinear filter) to smooth out pixels

'-best' enable sdl2 'best' filtering, which actually makes the games look the worst
 

recommend command line options:

'fbneo -menu -integerscale -fullscreen -joy'

The above will give you a nicely scalend game screen and the menu for launching games. 

## In-game controls

'tab' brings up the in game menu
'f12' quit game. This will return you to the game select menu if run with '-menu'. Press 'f12' again to quit 
'f1' fast forward game. Also rescans roms when in the menu
'f11' show FPS counter

