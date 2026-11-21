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
use Cwd            qw(abs_path);
use Digest::SHA    qw(sha256_hex);
use File::Basename qw(dirname basename);
use File::Find     qw(find);
use File::Spec;
use Getopt::Long qw(GetOptions);
use JSON::PP;

my ( $update, $help );
GetOptions( 'update-inventory' => \$update, 'help' => \$help )
  or die "usage: $0 [--update-inventory]\n";
if ($help) {
    print
"Check full ISC notices and provenance inventories. --update-inventory records reviewed current source paths and hashes; no source file is changed.\n";
    exit 0;
}
my $root = abs_path( File::Spec->catdir( dirname(__FILE__), '..' ) );
my @errors;

sub read_file {
    my ($relative) = @_;
    open my $fh, '<:raw', "$root/$relative"
      or die "cannot read $relative: $!\n";
    local $/;
    return <$fh> // '';
}
sub json { return JSON::PP->new->decode( read_file( $_[0] ) ); }

sub safe_path {
    my ($path) = @_;
    die "invalid provenance path\n"
      if !defined($path) || $path =~ m{\A/|\\|(?:\A|/)\.\.(?:/|\z)|\0};
    return $path;
}

sub header {
    my ( $text, $path ) = @_;
    $text =~ s/\A\xEF\xBB\xBF//;
    $text =~ s/\r\n/\n/g;
    $text =~ s/\A%\{[^\S\n]*\n// if $path =~ /\.y\z/;
    my $prefix = '';
    if ( $path =~ /\.manifest\z/ ) {
        $text =~ s/\A\s*<\?xml[^>]*\?>\s*//;
        while ( $text =~ s/\A\s*(<!--.*?-->)\s*//s ) { $prefix .= "$1\n"; }
    }
    elsif ( $path =~ /\.(?:c|h|y|rc)(?:\.in)?\z/ ) {
        while ( $text =~ s/\A\s*(\/\*.*?\*\/|\/\/[^\n]*)(?:\n)?//s ) {
            $prefix .= "$1\n";
        }
    }
    else {
        $text =~ s/\A#![^\n]*\n//;
        while ( $text =~ s/\A[ \t]*(#[^\n]*|\n)(?:\n)?// ) {
            $prefix .= "$1\n";
        }
    }
    return $prefix;
}

sub uncomment {
    my ($text) = @_;
    $text =~ s/\r\n/\n/g;
    my @lines;
    for my $line ( split /\n/, $text ) {
        $line =~ s/^\s*(?:\/\*+|\*\/|<!--|-->)\s*//;
        $line =~ s/^\s*(?:\*|\/\/|#) ?//;
        $line =~ s/[ \t]+\z//;
        push @lines, $line;
    }
    return join( "\n", @lines );
}
my $notice = read_file('LICENSES/ISC.txt');
$notice =~ s/\r\n/\n/g;
$notice =~ s/\n\z//;
my %imports;
my $baseline;
for my $manifest (qw(provenance/manifest.json provenance/compat-manifest.json))
{
    my $data = json($manifest);
    $baseline //= $data->{revision};
    die "manifest revisions differ\n" unless $data->{revision} eq $baseline;
    for my $file ( @{ $data->{files} } ) {
        my $path = safe_path( $file->{local_path} );
        die "duplicate imported path $path\n" if exists $imports{$path};
        die "invalid baseline hash for $path\n"
          unless $file->{sha256} =~ /\A[0-9a-f]{64}\z/;
        $imports{$path} = $file;
    }
}
my $inventory_path = 'provenance/portability-changes.json';
my $inventory      = json($inventory_path);
my @modified;
for my $path ( sort keys %imports ) {
    if ( !-f "$root/$path" ) {
        push @errors, "missing imported file: $path";
        next;
    }
    push @modified, $path
      if sha256_hex( read_file($path) ) ne $imports{$path}{sha256};
}
my %derived = map { safe_path( $_->{path} ) => $_ }
  @{ $inventory->{derived_code_files} // [] };
my $parser_record = read_file('generated/GENERATOR.txt');
my $grammar_sha   = sha256_hex( read_file('src/date.y') );
push @errors,
  "generated parser record is stale: regenerate from the current date grammar"
  unless $parser_record =~ /^Grammar SHA256: \Q$grammar_sha\E\r?$/m;
if ( $update && -f "$root/generated/date.c" ) {
    my $generated = read_file('generated/date.c');
    die "generated date parser lacks its public-domain provenance\n"
      unless $generated =~ /#define YYBYACC 1/
      && $generated =~ /This code is in the public domain and has no copyright/;
    $derived{'generated/date.c'} = {
        path             => 'generated/date.c',
        sha256           => sha256_hex($generated),
        source           => 'src/date.y',
        generator_record => 'generated/GENERATOR.txt',
        license          => 'public-domain skeleton; grammar notices retained',
        reason           =>
          'Unmodified byacc output retains the notices copied from the grammar.'
    };
}
my @new;
find(
    {
        no_chdir => 1,
        wanted   => sub {
            my $file = $File::Find::name;
            if (
                -d $file
                && ( basename($file) =~
/\A(?:\.git|build[^\/]*|dist[^\/]*|out|artifacts|__pycache__)\z/
                )
              )
            {
                $File::Find::prune = 1;
                return;
            }
            return unless -f $file;
            my $relative = File::Spec->abs2rel( $file, $root );
            $relative =~ s{\\}{/}g;
            return if exists $imports{$relative};
            my $code =
              $relative =~
/(?:\.(?:c|h|y|rc|pl|sh|bash|ksh|ps1|cmake|manifest)(?:\.in)?|(?:\A|\/)CMakeLists\.txt|(?:\A|\/)Makefile)\z/
              || $relative =~ m{\A\.github/workflows/[^/]+\.ya?ml\z};
            return unless $code;
            if ( exists $derived{$relative} ) {
                push @errors,
"derived file changed without reviewed inventory update: $relative"
                  unless sha256_hex( read_file($relative) ) eq
                  $derived{$relative}{sha256};
                return;
            }
            push @new, $relative;
        }
    },
    $root
);
@new = sort @new;
for my $path ( @modified, @new ) {
    my $preamble = uncomment( header( read_file($path), $path ) );
    push @errors, "missing or partial exact full ISC notice: $path"
      if index( $preamble, $notice ) < 0;
}

# Original import notices remain present even when portability code is added.
# First import and auxiliary import commits are local provenance evidence;
# hashes of their original comment blocks are persisted for shallow CI clones.
my %preserved = %{ $inventory->{original_notice_sha256} // {} };
if ($update) {
    for my $path ( sort keys %imports ) {
        my $commit = $path =~ m{\Asrc/} ? '16ca96e' : '6253704';
        open my $fh, '-|', 'git', '-C', $root, 'show', "$commit:$path"
          or die "cannot read original Git blob for $path: $!\n";
        local $/;
        my $original = <$fh> // '';
        close $fh
          or die
"original import object unavailable for $path; update with full Git history\n";
        my $preamble = header( $original, $path );
        my @blocks   = $preamble =~ /(\/\*.*?\*\/)/sg;
        $preserved{$path} = [ map { sha256_hex($_) } @blocks ];
    }
}
for my $path ( sort keys %imports ) {
    next unless -f "$root/$path";
    if ( !exists $preserved{$path} ) {
        push @errors,
"original notice baseline missing: $path (run --update-inventory with full history)";
        next;
    }
    my $preamble = header( read_file($path), $path );
    my %blocks = map { sha256_hex($_) => 1 } ( $preamble =~ /(\/\*.*?\*\/)/sg );
    push @errors,
      "original imported notice or OpenBSD marker removed/changed: $path"
      if grep { !$blocks{$_} } @{ $preserved{$path} };
}
my $expected = {
    baseline_revision      => $baseline,
    modified_imports       => \@modified,
    new_code_files         => \@new,
    derived_code_files     => [ map { $derived{$_} } sort keys %derived ],
    new_code_license       => 'ISC',
    full_notice            => 'LICENSES/ISC.txt',
    original_notice_sha256 => \%preserved,
    policy                 =>
'Original notices retained; David Uhden Collado copyright covers only new portability changes.'
};
if ($update) {
    open my $out, '>:raw', "$root/$inventory_path"
      or die "cannot update inventory: $!\n";
    print {$out}
      JSON::PP->new->canonical->indent->indent_length(2)
      ->space_after->encode($expected);
    close $out or die "cannot finish inventory: $!\n";
}
else {
    for my $key (qw(modified_imports new_code_files)) {
        push @errors,
"$key inventory is stale: run --update-inventory after reviewing changes"
          unless join( "\0", @{ $inventory->{$key} // [] } ) eq
          join( "\0", @{ $expected->{$key} } );
    }
}
if (@errors) { print STDERR "$_\n" for @errors; exit 1; }
printf
"PASS: exact full ISC notices in %d new code files and %d modified imports; %d original notice baselines preserved; %d generated exemptions\n",
  scalar(@new), scalar(@modified), scalar( keys %imports ),
  scalar( keys %derived );
