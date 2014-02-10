i3blocks(1) -- simple i3 status line
====================================

## SYNOPSIS

`i3blocks` [-c <configfile>] [-h] [-v]

## DESCRIPTION

**i3blocks** generates a status line for the [i3](http://i3wm.org) window 
manager. Here are the available options:

  * `-c` <configfile>:
    Specifies an alternate configuration file path. By default, i3blocks looks 
    for configuration files in the following order:

        1. ~/.i3blocks.conf
        2. /etc/i3blocks.conf or /usr/local/etc/i3blocks.conf

  * `-v`:
    Print the version and exit.

  * `-h`:
    Print the help message and exit.

The blocks are defined in a simple ini configuration file. Each section define 
a new block. The properties and values are the keys describing a block, 
according to the [i3bar protocol](http://i3wm.org/docs/i3bar-protocol.html).
Two additional properties are added to a block definition: `command` which 
defines the shell command to execute to get the block text, and the `interval` 
the block should be updated.

A block command may return 3 lines. If so, they will overwrite (in this order) 
the following block property:

  1. full_text
  2. short_text
  3. color

For example, a script setting a full_text in blue but no short_text would look 
like:

    echo "Here's my label"
    echo
    echo \#0000FF

## EXAMPLES

Here is an example configuration:

    [volume]
    command=echo -n 'Volume: '; amixer get Master | grep -E -o '[0-9][0-9]?%'

    [label]
    full_text=A simple text

    [time5]
    command=echo -n 'Time: '; date +%T
    interval=5
    color=#00FF00

    [ethernet]
    command=/usr/lib/i3blocks/weather
    instance=montreal
    color=#00ff00
    min_width=
    align=right
    urgent=false
    separator=false
    separator_block_width=9
    interval=3600

## SEE ALSO

The development of i3blocks takes place at https://github.com/vivien/i3blocks

i3(1), i3bar(1), i3status(1)

## BUGS

Currently the output is not JSON-escaped. This means writing chars such as '"' 
will break the status line.

## AUTHOR

Written by Vivien Didelot <vivien.didelot@savoirfairelinux.com>.

## COPYRIGHT

Copyright (C) 2014 Vivien Didelot <vivien.didelot@savoirfairelinux.com>
License GPLv3+: GNU GPL version 3 or later <http://gnu.org/licenses/gpl.html>.

This is free software: you are free to change and redistribute it. There is NO 
WARRANTY, to the extent permitted by law.
