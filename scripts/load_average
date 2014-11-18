#!/bin/sh
# Copyright (C) 2014 Julien Bonjean <julien@bonjean.info>

# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

load="$(cut -d ' ' -f1 /proc/loadavg)"
cpus="$(nproc)"

# full text
echo "$load"

# short text
echo "$load"

# color if load is too high
awk -v cpus=$cpus -v cpuload=$load '
    BEGIN {
        if (cpus <= cpuload) {
            print "#FF0000";
            exit 33;
        }
    }
'
