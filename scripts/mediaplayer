#!/usr/bin/perl
# Copyright (C) 2014 Tony Crisci <tony@dubstepdish.com>

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

# Requires playerctl binary to be in your path (except cmus)
# See: https://github.com/acrisci/playerctl

# Set instance=NAME in the i3blocks configuration to specify a music player
# (playerctl will attempt to connect to org.mpris.MediaPlayer2.[NAME] on your
# DBus session).

use Env qw(BLOCK_INSTANCE);

my @metadata = ();
my $player_arg = "";

if ($BLOCK_INSTANCE) {
    $player_arg = "--player='$BLOCK_INSTANCE'";
}

if ($ENV{'BLOCK_BUTTON'} == 1) {
    system("playerctl $player_arg previous");
} elsif ($ENV{'BLOCK_BUTTON'} == 2) {
    system("playerctl $player_arg play-pause");
} elsif ($ENV{'BLOCK_BUTTON'} == 3) {
    system("playerctl $player_arg next");
}

if ($player_arg eq '' or $player_arg =~ /cmus$/) {
    # try cmus first
    my @cmus = split /^/, qx(cmus-remote -Q);
    if ($? == 0) {
        foreach my $line (@cmus) {
            my @data = split /\s/, $line;
            if (shift @data eq 'tag') {
                my $key = shift @data;
                my $value = join ' ', @data;

                @metadata[0] = $value if $key eq 'artist';
                @metadata[1] = $value if $key eq 'title';
            }
        }

        if (@metadata) {
            # metadata found so we are done
            print(join ' - ', @metadata);
            exit 0;
        }
    }

    # if cmus was given, we are done
    exit 0 unless $player_arg eq '';
}

my $artist = qx(playerctl $player_arg metadata artist);
# exit status will be nonzero when playerctl cannot find your player
exit(0) if $?;
push(@metadata, $artist) if $artist;

my $title = qx(playerctl $player_arg metadata title);
exit(0) if $?;
push(@metadata, $title) if $title;

print(join(" - ", @metadata)) if @metadata;
