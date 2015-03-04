#!/usr/bin/perl
#
# Copyright 2014 Marcelo Cerri <mhcerri at gmail dot com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.

use strict;
use warnings;
use utf8;
use Getopt::Long;
use File::Basename;

# Default values
my $indicator = $ENV{BLOCK_INSTANCE} || "CAPS";
my $color_on  = "#00FF00";
my $color_off = "#222222";

sub help {
    my $program = basename($0);
    printf "Usage: %s [-c <color on>] [-C <color off>]\n", $program;
    printf "  -c <color on>: hex color to use when indicator is on\n";
    printf "  -C <color off>: hex color to use when indicator is off\n";
    printf "\n";
    printf "Note: environment variable \$BLOCK_INSTANCE should be one of:\n";
    printf "  CAPS, NUM (default is CAPS).\n";
    exit 0;
}

Getopt::Long::config qw(no_ignore_case);
GetOptions("help|h" => \&help,
           "c=s"    => \$color_on,
           "C=s"    => \$color_off) or exit 1;

# Key mapping
my %indicators = (
    CAPS => 0x00000001,
    NUM  => 0x00000002,
);

# Retrieve key flags
my $mask = 0;
open(XSET, "xset -q |") or die;
while (<XSET>) {
    if (/LED mask:\s*([0-9]+)/) {
        $mask = $1;
        last;
    }
}
close(XSET);

# Output
printf "%s\n", $indicator;
printf "%s\n", $indicator;
if (($indicators{$indicator} || 0) & $mask) {
    printf "%s\n", $color_on;
} else {
    printf "%s\n", $color_off;
}
exit 0
