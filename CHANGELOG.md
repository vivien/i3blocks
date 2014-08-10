# CHANGELOG

## master

  * Now check for the followingfiles before ~/.i3blocks.conf (GH #32):
     ~/.config/i3blocks/config (or  $XDG_CONFIG_HOME/i3blocks/config if set) or
     ~/.config/i3/i3blocks (or $XDG_CONFIG_HOME/i3/i3blocks if set)
  * Add a cpu usage script (GH #11).
  * Fix click checking.

## 1.2

  * Always define env variables related to clicks. Thus, set them to an empty 
  string when no click happened (GH #9).

## 1.1

  * Change return code for urgency to 33 (GH #8).
  * Do not setup stdin if it refers to a tty (GH #7).

## 1.0

Initial release
