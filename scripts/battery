#!/usr/bin/perl
#
# Copyright 2014 Pierre Mavro <deimos@deimos.fr>
# Copyright 2014 Vivien Didelot <vivien@didelot.org>
#
# Licensed under the terms of the GNU GPL v3, or any later version.
#
# This script is meant to use with i3blocks. It parses the output of the "acpi"
# command (often provided by a package of the same name) to read the status of
# the battery, and eventually its remaining time (to full charge or discharge).
#
# The color will gradually change for a percentage below 85%, and the urgency
# (exit code 33) is set if there is less that 5% remaining.

use strict;
use warnings;
use utf8;

my $acpi;
my $status;
my $percent;
my $ac_adapt;
my $full_text;
my $short_text;
my $bat_number = $ENV{BLOCK_INSTANCE} || 0;

# read the first line of the "acpi" command output
open (ACPI, "acpi -b | grep 'Battery $bat_number' |") or die;
$acpi = <ACPI>;
close(ACPI);

# fail on unexpected output
if ($acpi !~ /: (\w+), (\d+)%/) {
	die "$acpi\n";
}

$status = $1;
$percent = $2;
$full_text = "$percent%";

if ($status eq 'Discharging') {
	$full_text .= ' DIS';
} elsif ($status eq 'Charging') {
	$full_text .= ' CHR';
} elsif ($status eq 'Unknown') {
	open (AC_ADAPTER, "acpi -a |") or die;
	$ac_adapt = <AC_ADAPTER>;
	close(AC_ADAPTER);

	if ($ac_adapt =~ /: ([\w-]+)/) {
		$ac_adapt = $1;

		if ($ac_adapt eq 'on-line') {
			$full_text .= ' CHR';
		} elsif ($ac_adapt eq 'off-line') {
			$full_text .= ' DIS';
		}
	}
}

$short_text = $full_text;

if ($acpi =~ /(\d\d:\d\d):/) {
	$full_text .= " ($1)";
}

# print text
print "$full_text\n";
print "$short_text\n";

# consider color and urgent flag only on discharge
if ($status eq 'Discharging') {

	if ($percent < 20) {
		print "#FF0000\n";
	} elsif ($percent < 40) {
		print "#FFAE00\n";
	} elsif ($percent < 60) {
		print "#FFF600\n";
	} elsif ($percent < 85) {
		print "#A8FF00\n";
	}

	if ($percent < 5) {
		exit(33);
	}
}

exit(0);
