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
use JSON::PP;

my ( $tag, $repo, $output, $expected, $expected_tag, $help );
GetOptions(
    'tag=s'                 => \$tag,
    'repo=s'                => \$repo,
    'output=s'              => \$output,
    'expected-sha=s'        => \$expected,
    'expected-tag-object=s' => \$expected_tag,
    'help'                  => \$help
  )
  or die
"usage: $0 --tag vX.Y.Z-alpha.N --repo OWNER/REPO [--output FILE] [--expected-sha SHA] [--expected-tag-object SHA]\n";
if ($help) {
    print
"Validate an existing signed prerelease tag and its signed target commit through GitHub. No mutations.\n";
    exit 0;
}
die "an explicit prerelease tag is required\n"
  unless defined($tag) && $tag =~ /\Av\d+\.\d+\.\d+-(?:alpha|beta|rc)\.\d+\z/;
$repo //= $ENV{GITHUB_REPOSITORY};
die "repository must be OWNER/REPO\n"
  unless defined($repo) && $repo =~ m{\A[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+\z};

sub api {
    my ($endpoint) = @_;
    open my $fh, '-|', 'gh', 'api', $endpoint or die "cannot start gh: $!\n";
    my $data = do { local $/; <$fh> };
    close $fh or die "GitHub API request failed\n";
    return JSON::PP->new->decode($data);
}

# Resolve the ref through its annotated tag object to the commit. Carry both
# identities forward so a replaced tag object can be detected even if its
# target commit has stayed the same.
my $metadata = api("repos/$repo");
die "repository identity differs\n"
  unless lc( $metadata->{full_name} ) eq lc($repo);
my $ref = api("repos/$repo/git/ref/tags/$tag");
die "release requires an existing annotated signed tag, not a lightweight ref\n"
  unless $ref->{object}{type} eq 'tag';
my $object = api("repos/$repo/git/tags/$ref->{object}{sha}");
die "tag object changed since qualification\n"
  if defined($expected_tag) && $expected_tag ne $object->{sha};
die "tag signature is not verified by GitHub\n"
  unless $object->{verification}{verified}
  && $object->{verification}{reason} eq 'valid';
my $signature = $object->{verification}{signature} // '';
die "prerelease tag must have an OpenPGP signature\n"
  unless $signature =~ /\A-----BEGIN PGP SIGNATURE-----\r?\n/
  && $signature =~ /\n-----END PGP SIGNATURE-----\r?\n?\z/;
die "tag must point directly to a commit\n"
  unless $object->{object}{type} eq 'commit';
my $sha = $object->{object}{sha};
die "invalid target SHA\n" unless $sha =~ /\A[0-9a-f]{40}\z/;
die "tag moved since the tested build\n"
  if defined($expected) && $expected ne $sha;

# GitHub reports signature verification independently for the tag and commit.
my $commit = api("repos/$repo/commits/$sha");
die "target commit signature is not verified\n"
  unless $commit->{commit}{verification}{verified}
  && $commit->{commit}{verification}{reason} eq 'valid';
my $result = {
    repository      => $repo,
    tag             => $tag,
    tag_object      => $object->{sha},
    commit          => $sha,
    tag_verified    => JSON::PP::true,
    commit_verified => JSON::PP::true,
    prerelease      => JSON::PP::true
};

if ( defined $output ) {
    open my $fh, '>:raw', $output or die "cannot create metadata: $!\n";
    print {$fh} JSON::PP->new->canonical->pretty->encode($result);
    close $fh or die "cannot write metadata: $!\n";
}
if ( defined $ENV{GITHUB_OUTPUT} ) {
    open my $fh, '>>:raw', $ENV{GITHUB_OUTPUT}
      or die "cannot open GitHub output: $!\n";
    print {$fh} "sha=$sha\n";
    print {$fh} "tag_object=$object->{sha}\n";
    close $fh or die "cannot write GitHub output: $!\n";
}
print "PASS: $repo $tag -> $sha; annotated tag and commit are Verified\n";
