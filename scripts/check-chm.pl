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
use Cwd            qw(abs_path);
use File::Basename qw(dirname);
use File::Spec;
use Getopt::Long qw(GetOptions);

my ( $chm, $project, $seven_zip );
GetOptions(
    'chm=s'     => \$chm,
    'project=s' => \$project,
    '7z=s'      => \$seven_zip
) or die "invalid arguments\n";
die "usage: $0 --chm FILE --project FILE [--7z FILE]\n"
  unless defined($chm) && defined($project);
die "missing compiled help: $chm\n"    unless -s $chm;
die "missing help project: $project\n" unless -s $project;
$chm = abs_path($chm);

if ( !defined $seven_zip ) {
    for my $dir ( File::Spec->path() ) {
        for my $name (qw(7z 7zz)) {
            $name .= '.exe' if $^O eq 'MSWin32';
            my $candidate = File::Spec->catfile( $dir, $name );
            if ( -f $candidate && -x _ ) {
                $seven_zip = $candidate;
                last;
            }
        }
        last if defined $seven_zip;
    }
}
die "7-Zip is required to inspect compiled HTML Help\n"
  unless defined $seven_zip;

sub read_bytes {
    my ($path) = @_;
    open my $in, '<:raw', $path or die "cannot read $path: $!\n";
    my $data = do { local $/; <$in> // '' };
    close $in or die "cannot close $path: $!\n";
    return $data;
}

sub capture {
    open my $out, '-|', $seven_zip, @_ or die "cannot run 7-Zip: $!\n";
    binmode $out or die "cannot set 7-Zip output to binary: $!\n";
    my $data = do { local $/; <$out> // '' };
    close $out or die "7-Zip command $_[0] failed (process status $?)\n";
    return $data;
}

# The generated project is a flat, local collection. Sitemaps appear under
# OPTIONS rather than FILES because the compiler adds them itself.
my ( %options, %expected, $section, $window_record );
for my $line ( split /\r?\n/, read_bytes($project) ) {
    next if $line eq '';
    if ( $line =~ /\A\[(OPTIONS|WINDOWS|FILES)\]\z/ ) {
        $section = $1;
    }
    elsif ( ( $section // '' ) eq 'OPTIONS' && $line =~ /\A([^=]+)=(.+)\z/ ) {
        my ( $key, $value ) = ( lc($1), $2 );
        die "duplicate CHM project option: $key\n"
          if exists $options{$key};
        $options{$key} = $value;
    }
    elsif ( ( $section // '' ) eq 'WINDOWS' && !defined $window_record ) {
        $window_record = $line;
    }
    elsif ( ( $section // '' ) eq 'FILES'
        && $line =~ /\A[a-zA-Z0-9][a-zA-Z0-9_+.-]*\.(?:html|css)\z/ )
    {
        die "duplicate CHM project member: $line\n"
          if exists $expected{$line};
        $expected{$line} = 1;
    }
    else {
        die "invalid CHM project entry: $line\n";
    }
}
my $contents = $options{'contents file'}  // '';
my $index    = $options{'index file'}     // '';
my $default  = $options{'default topic'}  // '';
my $title    = $options{'title'}          // '';
my $window   = $options{'default window'} // '';
die "missing or invalid CHM navigation options\n"
  unless $contents =~ /\A[a-zA-Z0-9][a-zA-Z0-9_.-]*\.hhc\z/
  && $index        =~ /\A[a-zA-Z0-9][a-zA-Z0-9_.-]*\.hhk\z/
  && $default      =~ /\.html\z/
  && exists $expected{$default}
  && $title ne ''
  && $window =~ /\A[A-Za-z0-9]+\z/
  && lc( $options{'full-text search'} // '' ) eq 'yes';
my @window_fields =
  ( $title, $contents, $index, $default, $default, '', '', '', '' );
my $window_prefix = "$window=" . join( ',', map { qq{"$_"} } @window_fields );
die "invalid CHM viewer window definition\n"
  unless ( $window_record // '' ) =~
  /\A\Q$window_prefix\E,0x([a-fA-F0-9]+),[0-9]+,0x[a-fA-F0-9]+\z/
  && ( hex($1) & 0x420 ) == 0x420;
my $html_count = grep { /\.html\z/ } keys %expected;
die "the CHM project contains no manual topics\n" unless $html_count > 1;
$expected{$contents} = 1;
$expected{$index}    = 1;

open my $signature, '<:raw', $chm or die "cannot read $chm: $!\n";
my $magic;
die "invalid HTML Help file signature\n"
  unless ( read( $signature, $magic, 4 ) // 0 ) == 4 && $magic eq 'ITSF';
close $signature or die "cannot close $chm: $!\n";
capture( 't', '-tchm', '-bso0', '-bsp0', '--', $chm );

# 7-Zip decodes the LZX payload and reports each logical member. Accept
# only the planned files and well-known compiler-owned metadata streams.
my $compiler_notice = '_#_README_#_';
my %metadata        = map { $_ => 1 } (
    $compiler_notice,
    (
        map { "#$_" }
          qw(IDXHDR ITBITS IVB STRINGS SYSTEM TOCIDX TOPICS URLSTR URLTBL WINDOWS)
    ),
    qw(
      $FIftiMain $OBJINST $WWAssociativeLinks $WWAssociativeLinks/Property
      $WWKeywordLinks $WWKeywordLinks/BTree $WWKeywordLinks/Data
      $WWKeywordLinks/Map $WWKeywordLinks/Property
    )
);
my %members;

for my $record ( split /\r?\n\r?\n/,
    capture( 'l', '-ba', '-slt', '-tchm', '--', $chm ) )
{
    my ($name) = $record =~ /^Path = ([^\r\n]+)\r?$/m;
    my ($size) = $record =~ /^Size = ([0-9]+)\r?$/m;
    die "unrecognized 7-Zip CHM listing record\n"
      unless defined($name) && defined($size);
    $name =~ tr{\\}{/};
    die "duplicate compiled help member: $name\n" if exists $members{$name};
    die "unexpected compiled help member: $name\n"
      unless $expected{$name} || $metadata{$name};
    $members{$name} = $size;
}

sub extract_member {
    my ($name) = @_;
    die "missing or empty CHM member: $name\n" unless $members{$name};
    my $data =
      capture( 'x', '-so', '-tchm', '-spd', '-bso0', '-bsp0', '--', $chm,
        $name );
    die "invalid decompressed size for CHM member: $name\n"
      unless length($data) == $members{$name};
    return $data;
}

# chmcmd emits a plain-text compiler notice outside the HHP file list.
# Accept its stock text, rather than treating the name as a content exemption.
if ( exists $members{$compiler_notice} ) {
    my $notice = extract_member($compiler_notice);
    my $prefix =
"This archive was not made by the MS HTML Help Workshop(r)(tm) program, but by Free Pascal's chm package ";
    die "unexpected Free Pascal compiler notice\n"
      unless $notice =~
/\A\Q$prefix\E[0-9]+(?:\.[0-9]+)+\.\r\n\x00?Compiled by CHMCmd [0-9]+(?:\.[0-9]+)+\z/;
}

for my $name ( sort keys %expected ) {
    my $rendered =
      read_bytes( File::Spec->catfile( dirname($project), $name ) );
    die "compiled help member differs from rendered source: $name\n"
      unless extract_member($name) eq $rendered;
}

# #SYSTEM carries the default topic, navigation files, title and search
# capability flag consumed by HTML Help. Read its length-prefixed records.
my $system = extract_member('#SYSTEM');
die "invalid CHM system stream\n"
  unless length($system) >= 4 && unpack( 'V', $system ) == 3;
my %settings;
my $offset = 4;
while ( $offset < length($system) ) {
    die "truncated CHM system record\n" if length($system) - $offset < 4;
    my ( $code, $length ) = unpack( 'v2', substr( $system, $offset, 4 ) );
    $offset += 4;
    die "truncated CHM system payload\n"
      if length($system) - $offset < $length;
    die "duplicate CHM system record: $code\n" if exists $settings{$code};
    $settings{$code} = substr( $system, $offset, $length );
    $offset += $length;
}
my @settings = ( $contents, $index, $default, $title );
for my $code ( 0 .. $#settings ) {
    die "CHM system setting $code differs from the project\n"
      unless ( $settings{$code} // '' ) eq "$settings[$code]\0";
}
die "CHM default window differs from the project\n"
  unless ( $settings{5} // '' ) eq "$window\0";
my $search_options = $settings{4} // '';
die "CHM does not advertise full-text search\n"
  unless length($search_options) >= 12
  && unpack( 'V', substr( $search_options, 8, 4 ) );
if ( lc( $options{'binary index'} // '' ) eq 'yes' ) {
    my $keywords = extract_member('$WWKeywordLinks/BTree');
    die "CHM does not expose its binary keyword index\n"
      unless length($search_options) >= 16
      && unpack( 'V', substr( $search_options, 12, 4 ) )
      && length($keywords) >= 4;
}
my $windows = extract_member('#WINDOWS');
die "invalid CHM viewer window stream\n" unless length($windows) >= 8;
my ( $window_count, $window_size ) = unpack( 'V2', $windows );
die "CHM viewer window does not expose the Search tab\n"
  unless $window_count == 1
  && $window_size >= 20
  && length($windows) == 8 + $window_size
  && ( unpack( 'V', substr( $windows, 24, 4 ) ) & 0x420 ) == 0x420;

# The search header records the number of indexed HTML files and the
# location of the search tree. A stream alone does not imply usable search.
my $search = extract_member('$FIftiMain');
die "invalid CHM full-text index header\n"
  unless length($search) >= 1024;
my ( $marker, $indexed, $root ) = unpack( 'V3', $search );
my $depth     = unpack( 'v', substr( $search, 24, 2 ) );
my $node_size = unpack( 'V', substr( $search, 46, 4 ) );
die "invalid or incomplete CHM full-text index\n"
  unless $marker == 0x00280000
  && $indexed == $html_count
  && $depth > 0
  && $node_size > 0
  && $root >= 1024
  && $root <= length($search) - $node_size;

print
"PASS: compiled help preserves all $html_count HTML topics and navigation files and contains a full-text index\n";
