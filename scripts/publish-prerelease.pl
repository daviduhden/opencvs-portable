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
use Digest::SHA;
use File::Basename qw(dirname basename);
use File::Spec;
use Cwd qw(abs_path);
use JSON::PP;
use IPC::Open3;
use Symbol qw(gensym);
use IO::Select;

my ( $tag, $repo, $expected, $expected_tag, $assets, $publish, $help );
GetOptions(
    'tag=s'                 => \$tag,
    'repo=s'                => \$repo,
    'expected-sha=s'        => \$expected,
    'expected-tag-object=s' => \$expected_tag,
    'assets=s'              => \$assets,
    'publish'               => \$publish,
    'help'                  => \$help
) or die "invalid arguments\n";

if ($help) {
    print
"usage: $0 --tag vX.Y.Z-alpha.N --repo OWNER/REPO --expected-sha SHA --assets DIRECTORY [--expected-tag-object SHA] [--publish]\nDefault: validate and prepare checksums/notes only. Publishing requires the owner-gated manual workflow.\n";
    exit 0;
}
die "valid tag, expected SHA and existing assets directory required\n"
  unless defined($tag)
  && $tag =~ /\Av\d+\.\d+\.\d+-(?:alpha|beta|rc)\.\d+\z/
  && defined($expected)
  && $expected =~ /\A[0-9a-f]{40}\z/
  && defined($assets)
  && -d $assets;
$repo //= $ENV{GITHUB_REPOSITORY};
die "invalid repository\n"
  unless defined($repo) && $repo =~ m{\A[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+\z};

sub capture {
    my (@args) = @_;
    my $err    = gensym;
    my $pid    = open3( undef, my $out, $err, @args );
    my $sel    = IO::Select->new( $out, $err );
    my $fd     = fileno($out);
    my ( $stdout, $stderr ) = ( '', '' );
    while ( my @ready = $sel->can_read ) {
        for my $fh (@ready) {
            my $n = sysread( $fh, my $buffer, 65536 );
            die "process read failed: $!\n" unless defined $n;
            if   ( !$n )                { $sel->remove($fh); close $fh; next; }
            if   ( fileno($fh) == $fd ) { $stdout .= $buffer; }
            else                        { $stderr .= $buffer; }
        }
    }
    waitpid( $pid, 0 ) == $pid or die "cannot wait for $args[0]: $!\n";
    my $status = $?;
    my $signal = $status & 127;
    return ( $signal ? 128 + $signal : $status >> 8, $stdout, $stderr );
}
my $root     = abs_path( File::Spec->catdir( dirname(__FILE__), '..' ) );
my $metadata = File::Spec->catfile( $assets, 'release-metadata.json' );

sub read_json {
    my ($path) = @_;
    open my $in, '<:raw', $path or die "cannot read $path: $!\n";
    my $json = do { local $/; <$in> };
    close $in or die "cannot close $path: $!\n";
    return JSON::PP->new->utf8->decode($json);
}

sub verify_release_identity {
    my @identity = (
        '--tag',    $tag, '--repo', $repo, '--expected-sha', $expected,
        '--output', $metadata
    );
    push @identity, '--expected-tag-object', $expected_tag
      if defined $expected_tag;
    my ( $code, $stdout, $stderr ) =
      capture( $^X, "$root/scripts/release-check.pl", @identity );
    die "release identity check failed: $stdout $stderr\n" if $code;
    print $stdout;
    $expected_tag = read_json($metadata)->{tag_object};
}
verify_release_identity();
opendir my $dir, $assets or die "cannot read assets: $!\n";
my @names =
  sort grep { /\.(?:tar\.gz|zip)\z/ && -f File::Spec->catfile( $assets, $_ ) }
  readdir $dir;
closedir $dir;
my %targets = (
    'linux-x86_64-gcc'        => [ 'glibc',        'GNU' ],
    'linux-x86_64-clang'      => [ 'glibc',        'Clang' ],
    'linux-x86_64-musl'       => [ 'musl',         'GNU' ],
    'windows-x86_64-msvc'     => [ 'windows-nt',   'MSVC' ],
    'windows-x86_64-clang-cl' => [ 'windows-nt',   'Clang' ],
    'windows-x86_64-mingw'    => [ 'windows-ucrt', 'GNU' ],
    'source'                  => [ 'glibc',        'GNU' ]
);
my @archives;
my %digests;

for my $target ( sort keys %targets ) {
    my $extension = $target =~ /^windows/ ? 'zip' : 'tar.gz';
    my $name      = "opencvs-portable-$tag-$target.$extension";
    die "missing expected tested artifact: $name\n"
      unless grep { $_ eq $name } @names;
    my $path = File::Spec->catfile( $assets, $name );
    die "empty artifact $name\n" unless -s $path;
    my $manifest = read_json("$path.manifest.json");
    my ( $abi, $compiler ) = @{ $targets{$target} };
    die "invalid package producer metadata for $name\n"
      unless ref($manifest) eq 'HASH'
      && ( $manifest->{archive}         // '' ) eq $name
      && ( $manifest->{source_revision} // '' ) eq $expected
      && ( $manifest->{abi}             // '' ) eq $abi
      && ( $manifest->{compiler}        // '' ) =~ /\A\Q$compiler\E \S+\z/;
    open my $archive, '<:raw', $path or die "cannot hash $path: $!\n";
    my $sha = Digest::SHA->new(256)->addfile($archive)->hexdigest;
    close $archive or die "cannot close archive: $!\n";
    die "package digest differs from the qualified artifact for $name\n"
      unless $sha eq ( $manifest->{sha256} // '' );
    $digests{$path} = $sha;
    my @list_command =
      $extension eq 'zip' ? ( 'unzip', '-Z1', $path ) : ( 'tar', '-tf', $path );
    my ( $status, $listing, $errors ) = capture(@list_command);
    die "cannot inspect $name: $errors\n" if $status;
    my @entries = split /\r?\n/, $listing;

    for my $entry (@entries) {
        die "unsafe archive member in $name\n"
          if $entry =~ m{\A[/\\]|:|(?:\A|/)\.\.(?:/|\z)};
    }
    if ( $target eq 'source' ) {
        die "source archive omits generated/date.c\n"
          unless grep { m{(?:\A|/)generated/date\.c\z} } @entries;
        die "source archive omits generated/GENERATOR.txt\n"
          unless grep { m{(?:\A|/)generated/GENERATOR\.txt\z} } @entries;
        die "source archive omits parser provenance\n"
          unless grep { m{(?:\A|/)provenance/portability-changes\.json\z} }
          @entries;
        die "source archive includes Git metadata\n"
          if grep { m{(?:\A|/)\.git(?:/|\z)} } @entries;
    }
    else {
        my $binary = $extension eq 'zip' ? 'opencvs.exe' : 'opencvs';
        die "binary absent in $name\n"
          unless grep { m{(?:\A|/)bin/\Q$binary\E\z} } @entries;
        die "upstream license notices absent in $name\n"
          unless grep { m{(?:\A|/)licenses/Upstream-notices\.txt\z} } @entries;
        die "upstream version record absent in $name\n"
          unless grep { m{(?:\A|/)UPSTREAM\z} } @entries;
        die "build metadata absent in $name\n"
          unless grep { m{(?:\A|/)build-info\.txt\z} } @entries;
    }
    push @archives, $path;
}
die "unexpected archive names present; review the artifact set\n"
  unless @names == keys %targets;
my $checksums = File::Spec->catfile( $assets, 'SHA256SUMS' );
open my $sums, '>:raw', $checksums or die "cannot create checksums: $!\n";
for my $path (@archives) {
    print {$sums} "$digests{$path}  ", basename($path), "\n";
}
close $sums or die "cannot close checksums: $!\n";
my $notes = File::Spec->catfile( $assets, 'RELEASE_NOTES.md' );
my $run =
  defined( $ENV{GITHUB_RUN_ID} )
  ? "https://github.com/$repo/actions/runs/$ENV{GITHUB_RUN_ID}"
  : 'No workflow run supplied; this preparation does not establish test qualification.';
open my $nf, '>:raw', $notes or die "cannot create notes: $!\n";
print {$nf}
"# Portable OpenCVS $tag\n\nDevelopment prerelease, not a stable production guarantee.\n\nSource commit: `$expected`.\n\nQualification run: $run\n\nThe workflow gates publication on native Ubuntu GCC/Clang and sanitizer tests, musl tests, and native Windows MSVC/clang-cl/MinGW tests plus PE import inspection.\n\nLinux glibc and musl packages are separate ABI targets. Windows ZIPs are native x86-64; no MSYS/Cygwin runtime is permitted. ARM64, DEB/RPM and untested operating systems are not certified by these artifacts.\n\nSee bundled UPSTREAM, licenses, documentation and the exact job logs for command-level limits, skipped peers, and runtime requirements. SHA256SUMS covers all seven archives.\n\nThis product includes software developed or owned by Caldera International, Inc.\n";
close $nf or die "cannot close notes: $!\n";
print
"Prepared seven inspected archives, SHA256SUMS, metadata and prerelease notes.\n";
exit 0 unless $publish;
my ($owner) = split '/', $repo;
die "publication is restricted to an explicitly dispatched owner workflow\n"
  unless ( $ENV{GITHUB_ACTIONS} // '' ) eq 'true'
  && ( $ENV{GITHUB_EVENT_NAME} // '' ) eq 'workflow_dispatch'
  && ( $ENV{GITHUB_ACTOR}      // '' ) eq $owner
  && ( $ENV{GITHUB_TRIGGERING_ACTOR} // $ENV{GITHUB_ACTOR} // '' ) eq $owner;
my ( $existing, $body, $error ) =
  capture( 'gh', 'api', "repos/$repo/releases/tags/$tag" );
die "release already exists; no assets will be overwritten\n" if $existing == 0;
die "cannot establish release absence: $error\n" unless $error =~ /HTTP 404/;
verify_release_identity();
my @command = (
    'gh',           'release',
    'create',       $tag,
    '--repo',       $repo,
    '--verify-tag', '--prerelease',
    '--title',      "Portable OpenCVS $tag",
    '--notes-file', $notes,
    @archives,      $checksums,
    $notes,         $metadata
);
system @command;
die "release publication failed; inspect partial remote state before retrying\n"
  if $?;
print "Published prerelease $tag without replacing an existing release.\n";
