#!/bin/sh
#
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

set -eu

# Generate in an isolated directory with the same relative input/output paths.
# Parity checks both the C output and the recipe, package, and grammar record.
case ${1:-} in
generate)
	output=${2:?parser output directory required}
	mkdir -p "$output/src" "$output/generated"
	cp src/date.y "$output/src/date.y"
	cd "$output"
	byacc -l -o generated/date.c src/date.y
	{
		byacc -V 2>&1
		printf '\nCommand: byacc -l -o generated/date.c src/date.y\n'
		printf 'Package: byacc=%s\n' "$(dpkg-query -W -f='${Version}' byacc)"
		printf 'Grammar SHA256: %s\n' "$(sha256sum src/date.y | cut -d' ' -f1)"
	} >generated/GENERATOR.txt
	;;
verify)
	output=${2:?parser output directory required}
	cmp generated/date.c "$output/generated/date.c"
	diff -u generated/GENERATOR.txt "$output/generated/GENERATOR.txt"
	;;
*)
	printf 'usage: %s {generate|verify} RUNNER_OUTPUT_DIRECTORY\n' "$0" >&2
	exit 1
	;;
esac
