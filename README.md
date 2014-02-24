# i3blocks

**i3blocks** is a status line for the [i3](http://i3wm.org) window manager.

The content of each **block** (e.g. time, battery status, network state, ...) 
is the output of a **command** provided by the user. Blocks are updated at 
a given **interval** of time or on a given **signal**, also specified by the 
user.

Here is an example of status line, showing the time (updated every 5 seconds) 
and the volume, updated only when **i3blocks** receives a SIGUSR1:

    [volume]
    command=echo -n 'Volume: '; amixer get Master | grep -E -o '[0-9][0-9]?%'
    signal=10
    # use 'killall -USR1 i3blocks' after changing the volume

    [time]
    command=date '+%D %T'
    interval=5

*For more information about how it works, please see the 
[manpage](http://vivien.github.io/i3blocks).*
