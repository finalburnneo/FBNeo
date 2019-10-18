# FinalBurn Neo for Raspberry Pi

## Compiling
To compile the emulator, run `make pi`. You will need SDL libraries for sound
and input.

## Compatibility
Presently, the emulator will run on Raspberry Pi 1, 2 and 3. It will most
likely compile on 4, but will not run on it.

## Input
To customize joystick input, place a text file in a directory named `joyconfig`.
The file should have extension `.jc`, and named as follows:

```
<name of romset>.jc
<name of parent romset>.jc
capcom6.jc
default.jc
```

To determine mapping for a particular set, emulator will look for the first
applicable file, in that order. `capcom6.jc` will only be checked if the set
is determined to be a 6-button Capcom ROMset, such as SFII, III, etc. 
`default` is a fallback file.

For instance, for set `sfau`, emulator will look for `sfau`, `sfa`, `capcom6`,
then `default`, in that order. For `mkr4` (Mortal Kombat rev 4.0), `mkr4`,
`mk`, `default` are applicable.

### Joyconfig file format
The format of the file is as follows:

```
<INPUT NAME>=<hardware joystick button number>
...
```

Input name can be one of: `TEST` (usually `F2` on keyboard), `SERVICE` (`9` on
keyboard) , `RESET` (`F3`), `QUIT`, `START`, `COIN`, or `BUTTONx` (where `x` is
a numeric digit between 1 and 9).

### Example
A generic `default` file could look as follows:

```
BUTTON1=1
BUTTON2=2
BUTTON3=3
BUTTON4=4
BUTTON5=6
BUTTON6=7
BUTTON7=8
COIN=9
START=10
```

In this case, buttons 1-8 on the hardware joystick map to emulated buttons 1-8,
while joystick buttons 9 and 10 map to "Coin" and "Start", respectively.

## Command-line switches
`-f`: Enables free play in games that include a Free Play dipswitch. This
switch isn't guaranteed to always work, since not every game has a free play
dipswitch, and because different sets may have the switch labeled differently.
`-F`: Enables free play hack mode. In hack mode, pressing Start also generates
a Coin button press.
