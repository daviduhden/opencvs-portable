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
use Digest::SHA    qw(sha256_hex);
use File::Basename qw(dirname);
use File::Path     qw(make_path);
use File::Spec;
use File::Temp qw(tempdir);
use Cwd        qw(abs_path);
use IPC::Open3;
use IO::Select;
use Symbol qw(gensym);

my ( $revision, $output, $help );
GetOptions(
    'revision=s' => \$revision,
    'output=s'   => \$output,
    'help'       => \$help
) or die "usage: $0 [--revision FULL_SHA] [--output NEW_DIRECTORY]\n";
if ($help) {
    print
"usage: $0 [--revision FULL_SHA] [--output NEW_DIRECTORY]\nStages upstream and baseline copies outside the working tree; never applies changes.\n";
    exit 0;
}
my $root = abs_path( File::Spec->catdir( dirname(__FILE__), '..' ) );

sub read_file {
    my ($path) = @_;
    open my $fh, '<:raw', $path or die "cannot read $path: $!\n";
    return do { local $/; <$fh> };
}

sub run {
    my (@args) = @_;
    my $error  = gensym;
    my $pid    = open3( undef, my $out, $error, @args );
    my $sel    = IO::Select->new( $out, $error );
    my ( $stdout, $stderr ) = ( '', '' );
    my $outfd = fileno($out);
    while ( my @ready = $sel->can_read ) {
        for my $fh (@ready) {
            my $n = sysread( $fh, my $buf, 65536 );
            die "subprocess read failed: $!\n" unless defined $n;
            if ( !$n )                   { $sel->remove($fh); close $fh; next; }
            if ( fileno($fh) == $outfd ) { $stdout .= $buf; }
            else                         { $stderr .= $buf; }
        }
    }
    waitpid( $pid, 0 );
    my $exit = $?;
    die "command failed (@args): $stderr\n" if $exit;
    return $stdout;
}
use constant PRIVATE_DIRECTORY_MODE => oct('700');

sub write_file {
    my ( $path, $data ) = @_;
    make_path( dirname($path), { mode => PRIVATE_DIRECTORY_MODE } );
    open my $fh, '>:raw', $path or die "cannot create $path: $!\n";
    print {$fh} $data or die "cannot write $path: $!\n";
    close $fh         or die "cannot close $path: $!\n";
}

sub safe_relative {
    my ($path) = @_;
    die "unsafe manifest path: $path\n"
      if !defined($path)
      || $path =~ m{\A[/\\]|[:\x00]}
      || grep { $_ eq '..' || $_ eq '' } split m{/}, $path;
    return $path;
}
my $json     = JSON::PP->new->canonical->pretty;
my $original = $json->decode( read_file("$root/provenance/manifest.json") );
my $extra = $json->decode( read_file("$root/provenance/compat-manifest.json") );
my $previous = $original->{revision};
$revision //= $previous;
die "revision must be a complete 40-character hexadecimal commit SHA\n"
  unless $revision =~ /\A[0-9a-f]{40}\z/i;
$revision = lc $revision;
my $commit =
  $json->decode( run( 'gh', 'api', "repos/openbsd/src/commits/$revision" ) );
die "GitHub resolved a different revision\n" unless $commit->{sha} eq $revision;

if ( defined $output ) {
    $output = File::Spec->rel2abs($output);
    die "output must not already exist\n" if -e $output || -l $output;
    my $parent = abs_path( dirname($output) )
      // die "output parent must exist\n";
    my $candidate =
      File::Spec->catfile( $parent, ( File::Spec->splitpath($output) )[2] );
    my ( $normalized_candidate, $normalized_root ) = ( $candidate, $root );
    s{\\}{/}g for ( $normalized_candidate, $normalized_root );
    die "staging inside the project is not permitted\n"
      if index( lc($normalized_candidate), lc( $normalized_root . '/' ) ) == 0
      || lc($normalized_candidate) eq lc($normalized_root);
    mkdir $output, PRIVATE_DIRECTORY_MODE or die "cannot create output: $!\n";
}
else {
    $output = tempdir( 'opencvs-upstream-XXXXXXXX', TMPDIR => 1, CLEANUP => 0 );
}
my @old     = ( @{ $original->{files} }, @{ $extra->{files} } );
my %old     = map { $_->{local_path} => $_ } @old;
my $listing = $json->decode(
    run( 'gh', 'api', "repos/openbsd/src/contents/usr.bin/cvs?ref=$revision" )
);
my @selected;
for my $file (@$listing) {
    die "unexpected upstream entry type\n" unless $file->{type} eq 'file';
    die "unsafe upstream filename\n"
      unless $file->{name} =~ /\A[A-Za-z0-9_.-]+\z/ && $file->{name} ne '..';
    push @selected,
      { upstream_path => $file->{path}, local_path => "src/$file->{name}" };
}
push @selected, @{ $extra->{files} };
my ( @files, @conflicts, @removed );
my %seen;
for my $item (@selected) {
    my $local = safe_relative( $item->{local_path} );
    my $up    = safe_relative( $item->{upstream_path} );
    die "duplicate local destination: $local\n" if $seen{$local}++;
    my $fresh = run(
        'gh',                                           'api',
        "repos/openbsd/src/contents/$up?ref=$revision", '-H',
        'Accept: application/vnd.github.raw+json'
    );
    write_file( "$output/candidate/$local", $fresh );
    my $base;
    if ( exists $old{$local} ) {
        $base = $revision eq $previous ? $fresh : run(
            'gh',
            'api',
"repos/openbsd/src/contents/$old{$local}{upstream_path}?ref=$previous",
            '-H',
            'Accept: application/vnd.github.raw+json'
        );
        die "baseline provenance hash does not match: $local\n"
          unless sha256_hex($base) eq $old{$local}{sha256};
        write_file( "$output/baseline/$local", $base );
    }
    my $working =
      -f "$root/$local" ? sha256_hex( read_file("$root/$local") ) : undef;
    my $upstream_changed =
      !defined($base) || sha256_hex($fresh) ne sha256_hex($base);
    my $local_modified = exists( $old{$local} )
      && ( !defined($working) || $working ne $old{$local}{sha256} );
    my $row = {
        local_path       => $local,
        upstream_path    => $up,
        candidate_sha256 => sha256_hex($fresh),
        baseline_sha256  => defined($base) ? sha256_hex($base) : undef,
        working_sha256   => $working,
        upstream_changed => $upstream_changed
        ? JSON::PP::true
        : JSON::PP::false,
        local_modified => $local_modified ? JSON::PP::true : JSON::PP::false
    };
    push @files,     $row;
    push @conflicts, $local if $upstream_changed && $local_modified;
}
@removed = grep { !$seen{$_} } sort keys %old;
my $report = {
    repository                       => 'https://github.com/openbsd/src',
    previous_revision                => $previous,
    selected_revision                => $revision,
    stage_directory                  => $output,
    source_tree_modified             => JSON::PP::false,
    files                            => \@files,
    upstream_removed                 => \@removed,
    manual_three_way_review_required => \@conflicts
};
write_file( "$output/report.json", $json->encode($report) );
print "Staged ", scalar(@files), " files at $output\n";
print
"Local source files were not overwritten. Review report.json, baseline/, and candidate/.\n";
print "Manual conflict candidates: ", scalar(@conflicts), "; removals: ",
  scalar(@removed), "\n";
