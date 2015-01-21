#!/usr/bin/env python3

# Copyright (C) 2014 Jason Pleau <jason@jpleau.ca>

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


from subprocess import Popen, PIPE
from re import findall
from os import environ
from sys import exit, stderr
from argparse import ArgumentParser

class MpdBlock:
    def __init__(self, options):
        default_labels = {
            "symbols": {
                    "playing": "▶",
                    "paused": "▮▮ ",
                    "stopped": "■ stopped",
            },
            "text": {
                    "playing": "playing: ",
                    "paused": "paused: ",
                    "stopped": "stopped",
            }
        }

        self.format = options.format
        self.color = options.color
        self.use_symbols = options.use_symbols
        self.text = default_labels["symbols"] if options.use_symbols else default_labels["text"]

        if options.label_playing:
            self.text["playing"] = options.label_playing

        if options.label_paused:
            self.text["paused"] = options.label_paused

        if options.label_stopped:
            self.text["stopped"] = options.label_stopped

    def get_mpc_status(self):
        mpc_options = ["mpc", "status"]
        if self.format:
            mpc_options.extend(["--format", self.format])

        proc_data = Popen(mpc_options, stdout=PIPE, stderr=PIPE).communicate()

        # exit if mpc can't connect to the server (ie: it's down)
        if len(proc_data[1]) > 0:
            print(proc_data[1].decode("utf-8"), end="")
            exit()

        return proc_data[0].decode("utf-8").split("\n")

    def run(self):
        mpc_status = self.get_mpc_status()

        button_pressed = int(environ["BLOCK_BUTTON"]) if len(environ["BLOCK_BUTTON"]) > 0 else 0

        changed = False
        if button_pressed > 0:
            if button_pressed == 1:
                changed = True
                Popen(["mpc", "toggle"], stdout=PIPE)
            elif button_pressed == 4:
                changed = True
                Popen(["mpc", "prev"], stdout=PIPE)
            elif button_pressed == 5:
                changed = True
                Popen(["mpc", "next"], stdout=PIPE)

        if changed:
            mpc_status = self.get_mpc_status()

        if len(mpc_status) == 2:
            print(self.text["stopped"], end=" ")
        else:
            status_test = findall("(playing|pause)", mpc_status[1])
            if status_test:
                status = status_test[0]
                print_status = self.text["playing"] if status == "playing" else self.text["paused"]

                # output long format
                print("{} {}".format(print_status, mpc_status[0]))

                # output short format
                print(print_status)

                # output color
                print(self.color)
            else:
                print("error")


if __name__ == "__main__":
    parser = ArgumentParser()

    parser.add_argument("-f", "--format", dest="format", default="%artist% - %title%", help="Same formats that 'mpc' uses.")
    parser.add_argument("-c", "--color", dest="color", default="#FFFFFF", help="Color of the returned text. Default: #FFFFFF")

    parser.add_argument("--symbols", dest="use_symbols", default=True, action="store_true", help="Use unicode symbols to display mpd status")
    parser.add_argument("--text", dest="use_symbols", default=False, action="store_false", help="Use text to display mpd status")

    parser.add_argument("--label-playing", dest="label_playing", default="", help="Text / unicode symbol to show when a song is playing")
    parser.add_argument("--label-paused", dest="label_paused", default="", help="Text / unicode symbol to show when a song is paused")
    parser.add_argument("--label-stopped", dest="label_stopped", default="", help="Text / unicode symbol to show when the player is stopped")

    options = parser.parse_args()

    mpd = MpdBlock(options)
    mpd.run()
