#!/usr/bin/env perl
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

use strict;
use warnings;
use Cwd qw(abs_path getcwd);
use Digest::SHA;
use File::Basename qw(basename dirname);
use File::Find;
use File::Spec;
use File::Temp   qw(tempdir);
use Getopt::Long qw(GetOptions);
use JSON::PP;

my ( $archive, $binary, $source, $manifest );
GetOptions(
    'archive=s'  => \$archive,
    'binary=s'   => \$binary,
    'source'     => \$source,
    'manifest=s' => \$manifest
) or die "invalid arguments\n";
die
"usage: $0 --archive FILE (--binary TESTED_EXECUTABLE | --source) [--manifest FILE]\n"
  unless defined($archive) && ( defined($binary) xor $source );
die "missing archive: $archive\n" unless -s $archive;
$archive  = abs_path($archive);
$manifest = File::Spec->rel2abs($manifest) if defined $manifest;

if ( defined $binary ) {
    die "missing tested executable: $binary\n" unless -s $binary;
    $binary = abs_path($binary);
}

sub run {
    my ( $exe, @args ) = @_;
    system {$exe} $exe, @args;
    die "$exe failed (process status $?)\n" unless $? == 0;
}

sub digest {
    my ($path) = @_;
    open my $in, '<:raw', $path or die "cannot read $path: $!\n";
    my $sha = Digest::SHA->new(256)->addfile($in)->hexdigest;
    close $in or die "cannot close $path: $!\n";
    return $sha;
}

# Require an unambiguous match while allowing CPack's archive root to vary.
sub find_member {
    my ( $dir, $name ) = @_;
    my @matches;
    find(
        {
            no_chdir => 1,
            wanted   => sub {
                push @matches, $File::Find::name
                  if -f $_ && basename($_) eq $name;
            }
        },
        $dir
    );
    die "expected one $name in $archive, found " . scalar(@matches) . "\n"
      unless @matches == 1;
    return $matches[0];
}

# Extract the runner-produced archive into an isolated directory. The source
# path builds from that copy and the binary path verifies the tested bytes.
my $cwd = getcwd();
my $dir = tempdir( 'opencvs-package-XXXXXXXX', TMPDIR => 1, CLEANUP => 1 );
my $verified_info;
chdir $dir or die "cannot enter extraction directory: $!\n";
my $ok = eval {
    run( 'cmake', '-E', 'tar', 'xf', $archive );
    if ($source) {
        my $root  = dirname( find_member( $dir, 'CMakeLists.txt' ) );
        my $build = File::Spec->catdir( $dir, 'source-build' );
        run( $^X,
            File::Spec->catfile( $root, 'scripts', 'check-license-notices.pl' )
        );
        run( 'cmake', '-S', $root, '-B', $build, '-G', 'Ninja',
            '-DCMAKE_BUILD_TYPE=Release', '-DPC_BUILD_TESTS=OFF',
            '-DPC_STRICT_WARNINGS=ON' );
        run( 'cmake', '--build', $build, '--parallel', '2' );
        run( File::Spec->catfile( $build, 'opencvs' ), '-v' );
        $verified_info = File::Spec->catfile( $build, 'build-info.txt' );
    }
    else {
        my $packaged = find_member( $dir, basename($binary) );
        find_member( $dir, 'Upstream-notices.txt' );
        find_member( $dir, 'UPSTREAM' );
        my $build_info = find_member( $dir, 'build-info.txt' );
        my $tested_info =
          File::Spec->catfile( dirname($binary), 'build-info.txt' );
        die "packaged executable differs from the tested executable\n"
          unless digest($packaged) eq digest($binary);
        die "packaged build metadata differs from the tested build metadata\n"
          unless digest($build_info) eq digest($tested_info);
        run( $packaged, '-v' );
        $verified_info = $build_info;
    }
    1;
};

# Leave the extraction directory before propagating failure or letting File::Temp
# remove it at process exit. The optional manifest still needs its metadata.
my $error = $@;
chdir $cwd or die "cannot restore working directory: $!\n";
die $error unless $ok;

# Bind the actual archive digest to the verified build and expected CI target.
# The release job checks this manifest again after downloading the archive.
if ( defined $manifest ) {
    open my $info, '<:raw', $verified_info
      or die "cannot read verified build metadata: $!\n";
    my %info;
    while ( my $line = <$info> ) {
        $info{$1} = $2 if $line =~ /\A([^:]+): ([^\r\n]*)\r?\n\z/;
    }
    close $info or die "cannot close build metadata: $!\n";
    my $sha      = $info{'Source revision'} // '';
    my $abi      = $info{'Target ABI'}      // '';
    my $compiler = $info{'Compiler'}        // '';
    die "missing or inconsistent package build identity\n"
      unless $sha =~ /\A[0-9a-f]{40}\z/
      && $sha eq ( $ENV{PC_SOURCE_REVISION} // '' )
      && $abi ne ''
      && $abi eq ( $ENV{PC_BUILD_ABI} // '' )
      && $compiler ne '';
    open my $out, '>:raw', $manifest or die "cannot create $manifest: $!\n";
    print {$out} JSON::PP->new->canonical->pretty->encode(
        {
            archive         => basename($archive),
            sha256          => digest($archive),
            source_revision => $sha,
            abi             => $abi,
            compiler        => $compiler
        }
    );
    close $out or die "cannot write $manifest: $!\n";
}
print "PASS: inspected and ran the extracted package ", basename($archive),
  "\n";
