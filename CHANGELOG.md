# CHANGELOG

## master

  * Add support for persistent blocks (GH #85).
  * Add support for JSON output format (GH #85).

## 1.3

  * Fix click checking (GH #34).
  * Use an alarm (if needed), which is more accurate.
  * Use real-time signals (SIGRTMIN+1 to SIGRTMAX) for blocks, deprecate 
  SIGUSR1 and SIGUSR2.
  * Implement asynchronous block updates (GH #23).
  * Now check for config file ~/.config/i3blocks/config (or
  $XDG_CONFIG_HOME/i3blocks/config if set) before ~/.i3blocks.conf (GH #32), 
  and similar with $XDG_CONFIG_DIRS.
  * Add a cpu usage script (GH #11).

## 1.2

  * Always define env variables related to clicks. Thus, set them to an empty 
  string when no click happened (GH #9).

## 1.1

  * Change return code for urgency to 33 (GH #8).
  * Do not setup stdin if it refers to a tty (GH #7).

## 1.0

Initial release
