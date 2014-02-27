# i3blocks

i3blocks is a highly flexible *status line* for the [i3](http://i3wm.org) 
window manager. It handles *clicks*, *signals* and *language-agnostic* user 
*scripts*. It aims to respect the
[i3bar protocol](http://i3wm.org/docs/i3bar-protocol.html).

The content of each *block* (e.g. time, battery status, network state, ...) is 
the output of a *command* provided by the user. Blocks are updated on *click*, 
at a given *interval* of time or on a given *signal*, also specified by the 
user.

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

## Documentation

For more information about how it works, please refer to the 
[**manpage**](http://vivien.github.io/i3blocks).

You can also take a look at the
[i3bar protocol](http://i3wm.org/docs/i3bar-protocol.html) to see what 
possibilities it offers you.

## Installation

To install i3blocks, run `make install` within the source directory.

## Copying

i3blocks is Copyright (C) 2014 Vivien Didelot
See the file COPYING for information of licensing and distribution.
