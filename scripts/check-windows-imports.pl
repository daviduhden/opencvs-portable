#!/usr/bin/env perl
# Portable OpenCVS.
#
# Copyright (c) 2026, David Uhden Collado <david@uhden.dev>
#
# Permission to use, copy, modify, and distribute this software for any
# purpose with or without fee is hereby granted, provided that the above
# copyright notice and this permission notice appear in all copies.
#
# THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
# WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
# MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
# ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
# WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
# ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
# OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.

use strict;
use warnings;
use Getopt::Long qw(GetOptions);
use File::Basename qw(basename);

my ($tool, $exe, $help);
GetOptions('tool=s' => \$tool, 'exe=s' => \$exe, 'help' => \$help)
    or die "usage: $0 --tool llvm-readobj-or-objdump --exe opencvs.exe\n";
if ($help) { print "usage: $0 --tool llvm-readobj-or-objdump --exe opencvs.exe\n"; exit 0; }
die "--tool and an existing --exe are required\n" unless defined($tool) && defined($exe) && -f $exe;
my @args = basename($tool) =~ /llvm-readobj/i ? ('--coff-imports', $exe) : ('-p', $exe);
open my $fh, '-|', $tool, @args or die "cannot execute dependency inspector: $!\n";
my $output = do { local $/; <$fh> };
close $fh or die "dependency inspector failed: $?\n";
my @imports = $output =~ /(?:DLL Name:|Name:)\s*([^\s]+\.dll)/ig;
die "inspector returned no DLL imports; cannot certify a native executable\n" unless @imports;
for my $dll (@imports) {
    die "forbidden non-system runtime dependency: $dll\n"
        if $dll =~ /^(?:cygwin1|msys-.*|libgcc_s.*|libwinpthread.*|libstdc\+\+.*|libssp.*)\.dll$/i;
}
print "PASS: native Windows imports; no MSYS/Cygwin or external compiler runtime DLLs\n";
print "$_\n" for sort @imports;
