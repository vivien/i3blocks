# i3blocks

[![Build Status](https://travis-ci.org/vivien/i3blocks.svg?branch=master)](https://travis-ci.org/vivien/i3blocks)

i3blocks is a highly flexible **status line** for the [i3](http://i3wm.org) 
window manager. It handles *clicks*, *signals* and *language-agnostic* user 
*scripts*.

The content of each *block* (e.g. time, battery status, network state, ...) is 
the output of a *command* provided by the user. Blocks are updated on *click*, 
at a given *interval* of time or on a given *signal*, also specified by the 
user.

It aims to respect the
[i3bar protocol](http://i3wm.org/docs/i3bar-protocol.html), providing 
customization such as text alignment, urgency, color, and more.

- - -

Here is an example of status line, showing the time updated every 5 seconds, 
the volume updated only when i3blocks receives a SIGUSR1, and click events.

```` ini
[volume]
command=echo -n 'Volume: '; amixer get Master | grep -E -o '[0-9][0-9]?%'
signal=10
# use 'killall -USR1 i3blocks' after changing the volume

[time]
command=date '+%D %T'
interval=5

[clickme]
command=echo button=$BLOCK_BUTTON x=$BLOCK_X y=$BLOCK_Y
min_width=button=1 x=1366 y=768
align=left
````

You can use your own scripts, or the 
[ones](https://github.com/vivien/i3blocks/tree/master/scripts) provided with 
i3blocks. Feel free to contribute and improve them!

The default config will look like this:

![](http://i.imgur.com/p3d6MeK.png)

The scripts provided by default may use external tools:

  * `mpstat` (often provided by the *sysstat* package) used by `cpu_usage`.
  * `acpi` (often provided by a package of the same name) used by `battery`.
  * `playerctl` (available [here](https://github.com/acrisci/playerctl)) used by `mediaplayer`.

## Documentation

For more information about how it works, please refer to the 
[**manpage**](http://vivien.github.io/i3blocks).

You can also take a look at the
[i3bar protocol](http://i3wm.org/docs/i3bar-protocol.html) to see what 
possibilities it offers you.

Take a look at the [wiki](https://github.com/vivien/i3blocks/wiki) for examples 
of blocks and screenshots. If you want to share your ideas and status line, 
feel free to edit it!

## Installation

  * Download i3blocks and run `make install` within the source directory
    * *Note that there's a [AUR](https://aur.archlinux.org/packages/i3blocks/) 
    package for Archlinux.*
  * set your `status_command` in a bar block of your ~/.i3/config file:

        bar {
          status_command i3blocks
        }

  * For customization, copy the default i3blocks.conf into ~/.i3blocks.conf
  * Restart i3 (e.g. `i3-msg restart`)

## Copying

i3blocks is Copyright (C) 2014 Vivien Didelot<br />
See the file COPYING for information of licensing and distribution.
