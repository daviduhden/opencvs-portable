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
use Encode         qw(decode FB_CROAK LEAVE_SRC);
use File::Basename qw(basename);
use File::Path     qw(make_path);
use File::Spec;
use Getopt::Long qw(GetOptions);

my ( $mandoc, $output_dir, $version, $lint_only );
my $usage =
"usage: $0 --mandoc PATH (--lint-only | --output-dir DIR --version VERSION) MANUAL ...\n";
GetOptions(
    'mandoc=s'     => \$mandoc,
    'output-dir=s' => \$output_dir,
    'version=s'    => \$version,
    'lint-only'    => \$lint_only
  )
  && defined $mandoc
  && @ARGV
  or die $usage;
die $usage
  unless $lint_only
  || ( defined $output_dir
    && defined $version
    && $version =~ /\A[0-9][0-9A-Za-z.+_-]*\z/ );

# References to manuals from the host system or other projects remain text.
# New unbundled references require a deliberate decision rather than silently
# becoming missing local topics.
my %unbundled = map { $_ => 1 } qw(
  diff.1 ex.1 gzip.1 patch.1 ping.8 rcs.1 rcsdiff.1 sh.1 ssh.1 sshd.8
  traceroute.8 vi.1
);
my %topic;
for my $file (@ARGV) {
    my $name = lc basename($file);
    my ($section) = $name =~ /\A[a-z0-9][a-z0-9_.+-]*\.([1-9][a-z0-9]*)\z/;
    die "Not a manual filename: $file\n" unless defined $section && -f $file;
    my $html  = "manual-$name.html";
    my $label = $name;
    $label =~ s/\.\Q$section\E\z/($section)/;
    die "Duplicate manual topic: $label\n" if exists $topic{$html};
    $topic{$html} = {
        label   => $label,
        source  => File::Spec->rel2abs($file),
        section => $section,
    };
}

sub escape_html {
    my ($value) = @_;
    $value =~ s/&/&amp;/g;
    $value =~ s/</&lt;/g;
    $value =~ s/>/&gt;/g;
    $value =~ s/"/&quot;/g;
    $value =~ s/'/&#39;/g;
    $value =~ s/([^\x00-\x7f])/sprintf('&#x%X;', ord $1)/eg;
    return $value;
}

sub inspect_manual {
    my ($source) = @_;
    open my $manual, '<:raw', $source or die "Cannot read $source: $!\n";
    my ( $notice, $in_preamble ) = ( '', 1 );
    while ( my $line = <$manual> ) {
        $line =~ s/\A\xef\xbb\xbf// if $. == 1;
        $line =~ s/\r?\n\z//;

        # Manuals are standalone. Reject file inclusion and changes to the
        # control characters before allowing mandoc to open the source.
        if ( $line =~ /\A(?:[.']|\\\.)[ \t]*(so|mso|cc|c2)(?=[ \t\\]|\z)/ ) {
            die "$source:$.: roff request $1 is disabled for standalone help\n";
        }
        next unless $in_preamble;
        next if !length $line;
        if ( $line =~ s/\A[.']\\"[ \t]?// ) {
            $notice .= "$line\n";
        }
        else {
            $in_preamble = 0;
        }
    }
    close $manual or die "Cannot close $source: $!\n";
    die "$source: invalid control byte in the manual preamble\n"
      if $notice =~ /[\x00-\x08\x0b-\x1f\x7f]/;
    return
      eval { decode( 'UTF-8', $notice, FB_CROAK | LEAVE_SRC ) }
      // decode( 'ISO-8859-1', $notice );
}

sub render {
    my ($source) = @_;
    open my $pipe, '-|', $mandoc, '-W', 'warning,stop', '-I', 'os=OpenBSD',
      '-T', 'html', '-O', 'fragment,man=manual-%N.%S.html,toc', $source
      or die "Cannot start mandoc: $!\n";
    binmode $pipe or die "Cannot set binary mode for mandoc: $!\n";
    my $html = do { local $/; <$pipe> };
    close $pipe or die "mandoc failed for $source (status $?)\n";
    die "mandoc returned no content for $source\n"
      unless defined $html && length $html;

    # Mandoc emits numeric HTML entities for non-ASCII manual content. Keeping
    # the files ASCII avoids dependence on the compiler's system code page.
    die "Unexpected characters in mandoc output for $source\n"
      if $html =~ /[^\x09\x0a\x0d\x20-\x7e]/;
    $html =~ s/\r\n/\n/g;
    return $html;
}

my %element = map { $_ => 1 } qw(
  a b blockquote br caption code col colgroup dd del dfn div dl dt em
  h1 h2 h3 h4 h5 h6 hr i kbd li ol p pre s samp section small span strong
  sub sup table tbody td tfoot th thead tr tt u ul var wbr
);
my %void = map { $_ => 1 } qw(br col hr wbr);
my $fragment =
qr/(?:[-A-Za-z0-9_.:\x24~]|&(?:amp|quot|apos|lt|gt|#x[0-9A-Fa-f]+|#[0-9]+);)+/;

sub help_id {
    my ($id) = @_;

    # Some CHM compilers fold case when comparing anchors. Encode the bytes
    # so mdoc anchors for distinct options such as -Q and -q stay distinct.
    return 'm-' . unpack( 'H*', $id );
}

# Tokenise the converter's fragment and reconstruct a restricted subset.
# Attributes are never copied through. Only local links can survive.
sub safe_fragment {
    my ( $html,   $manual ) = @_;
    my ( $result, $anchor, $external ) = ( '', '', '' );
    my ( %ids,    @links );
    pos($html) = 0;
    while ( pos($html) < length($html) ) {
        if ( $html =~ /\G([^<]+)/gc ) {
            $result .= $1;
            next;
        }
        $html =~ /\G<(\/?)([a-z][a-z0-9]*)\b([^<>]*)>/gc
          or die "$manual: unrecognised mandoc markup\n";
        my ( $end, $tag, $attrs ) = ( $1, $2, $3 );
        die "$manual: unsupported HTML element $tag\n" unless $element{$tag};
        $tag = 'div' if $tag eq 'section';
        if ($end) {
            die "$manual: invalid end tag $tag\n"
              if $attrs =~ /\S/ || $void{$tag};
            if ( $tag eq 'a' ) {
                die "$manual: unexpected end of link\n" unless length $anchor;
                $tag    = $anchor;
                $anchor = '';
                $result .= "</$tag>";
                $result .= " ($external)" if length $external;
                $external = '';
                next;
            }
            $result .= "</$tag>";
            next;
        }

        my %attrs;
        pos($attrs) = 0;
        while ( $attrs =~ /\G\s+([a-z]+)="([^"]*)"/gc ) {
            my ( $attr, $value ) = ( $1, $2 );
            die "$manual: duplicate HTML attribute $attr\n"
              if exists $attrs{$attr};
            $attrs{$attr} = $value;
        }
        die "$manual: unrecognised HTML attributes on $tag\n"
          unless $attrs =~ /\G\s*\/?\s*\z/gc;
        for my $attr ( keys %attrs ) {
            my $value = $attrs{$attr};
            $value =~ s/\s+/ /g if $attr eq 'class';
            die "$manual: unsafe attribute $attr on $tag ($value)\n"
              unless ( $attr eq 'class' && $value =~ /\A[- A-Za-z0-9_]+\z/ )
              || ( $attr eq 'id'   && $value =~ /\A$fragment\z/ )
              || ( $attr eq 'href' && $tag eq 'a' )
              || ( $attr =~ /\A(?:colspan|rowspan)\z/
                && $tag   =~ /\A(?:td|th)\z/
                && $value =~ /\A[1-9][0-9]*\z/ );
            $attrs{$attr} = $value;
        }

        if ( $tag eq 'a' ) {
            die "$manual: nested HTML links\n" if length $anchor;
            my $href = delete $attrs{href};
            if ( !defined $href ) {
                $tag = 'span';
            }
            elsif ( $href =~ /\A#($fragment)\z/ ) {
                my $id = $1;
                push @links, $id;
                $attrs{href} = '#' . help_id($id);
            }
            elsif ( $href =~ /\Amanual-([a-z0-9_.+-]+)\.html\z/i
                && ( $attrs{class} // '' ) eq 'Xr' )
            {
                my $name = lc $1;
                if ( exists $topic{"manual-$name.html"} ) {
                    $attrs{href} = "manual-$name.html";
                }
                elsif ( $unbundled{$name} ) {
                    $tag = 'span';
                }
                else {
                    die "$manual: unknown manual reference $name\n";
                }
            }
            elsif ( ( $attrs{class} // '' ) =~ /\A(?:Lk|Mt)\z/
                && $href =~ /\A(?:https?:\/\/|mailto:)/i )
            {
                $tag = 'span';

                # The token contains converter-escaped attribute text and
                # cannot include raw angle brackets. Keep it only as text.
                $external = $href;
            }
            else {
                die "$manual: unsafe or broken link $href\n";
            }
            $anchor = $tag;
        }
        if ( exists $attrs{id} ) {
            my $id = $attrs{id};
            die "$manual: duplicate HTML fragment $id\n" if $ids{$id}++;
            $attrs{id} = help_id($id);
        }
        $result .= "<$tag";
        for my $attr ( sort keys %attrs ) {
            $result .= ' ' . $attr . '="' . escape_html( $attrs{$attr} ) . '"';
        }
        $result .= '>';
    }
    die "$manual: unterminated HTML link\n" if length $anchor;
    for my $id (@links) {
        die "$manual: unknown fragment #$id\n" unless $ids{$id};
    }
    return $result;
}

my %section_name = (
    1 => 'General Commands Manual',
    2 => 'System Calls Manual',
    3 => 'Library Functions Manual',
    4 => 'Device Drivers Manual',
    5 => 'File Formats Manual',
    6 => 'Games Manual',
    7 => 'Miscellaneous Information Manual',
    8 => 'System Manager Manual',
    9 => 'Kernel Developer Manual',
);
my ( %sections, %section_volume );
my @pages =
  sort { $topic{$a}{section} cmp $topic{$b}{section} || $a cmp $b } keys %topic;
for my $page (@pages) {
    my $entry = $topic{$page};
    $entry->{notice} = inspect_manual( $entry->{source} );
    $entry->{body} =
      safe_fragment( render( $entry->{source} ), $entry->{label} );
    my ($volume) =
      $entry->{body} =~ m{<td class="head-vol">([- A-Za-z.'0-9]+)</td>};
    $section_volume{ $entry->{section} } //= $volume if defined $volume;
    push @{ $sections{ $entry->{section} } }, $page;
}
if ($lint_only) {
    print 'Validated manuals: ',
      join( ', ', map { $topic{$_}{label} } @pages ), "\n";
    exit 0;
}

my $title = "Portable OpenCVS $version - Manual";
my $meta =
  '<meta http-equiv="Content-Type" content="text/html; charset=windows-1252">';
my %files;

sub document {
    my ( $heading, $body ) = @_;
    return
        "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n$meta\n<title>"
      . escape_html($heading)
      . "</title>\n<link rel=\"stylesheet\" type=\"text/css\" href=\"manual.css\">\n"
      . "</head>\n<body>\n$body\n</body>\n</html>\n";
}

# The sitemap objects are input records for the help compiler. Their names
# and local targets come from the same validated topic and anchor inventory.
sub sitemap_entry {
    my ( $label, $local, $children ) = @_;
    my $item =
        '<li><object type="text/sitemap">' . "\n"
      . '  <param name="Name" value="'
      . escape_html($label) . '">' . "\n";
    if ( defined $local ) {
        $item .=
          '  <param name="Local" value="' . escape_html($local) . '">' . "\n";
    }
    $item .= "</object>\n";
    $item .= "<ul>\n$children</ul>\n" if defined $children;
    return $item . "</li>\n";
}

sub sitemap {
    my ($items) = @_;
    return
        "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n$meta\n"
      . "<title>Portable OpenCVS</title>\n</head>\n<body>\n"
      . "<ul>\n$items</ul>\n</body>\n</html>\n";
}

sub index_terms {
    my ($body) = @_;
    my %term;
    while (
        $body =~ m{<(h[1-6]|dt)\b[^>]*\bid="($fragment)"[^>]*>(.*?)</\1>}gs )
    {
        my ( $id, $text ) = ( $2, $3 );
        $text =~ s/<[^>]+>//g;
        $text =~ s/\s+/ /g;
        $text =~ s/\A\s+|\s+\z//g;

        # Terms with non-ASCII entities stay searchable as topic text. ASCII
        # labels also avoid variation in native binary index code page handling.
        next unless $text =~ /\A[- A-Za-z0-9_.:\x24+\/]{2,80}\z/;
        $term{$text} //= $id;
    }
    return %term;
}

my $toc = sitemap_entry( 'Manual home', 'index.html', undef );
my %index;
my $home = '<h1>'
  . escape_html($title)
  . "</h1>\n"
  . "<p>Reference manuals for the Portable OpenCVS version control system.</p>\n"
  . "<p>The Windows executable is named <code>opencvs.exe</code>. Topic names follow the original manual pages.</p>\n"
  . "<p>Use Contents to browse sections, Index to find a term, or Search to find text across all manuals.</p>\n";

for my $section ( sort keys %sections ) {
    my $volume = $section_volume{$section}
      // $section_name{ substr( $section, 0, 1 ) };
    my $label = "Section $section - $volume";
    $home .=
      "<h2 id=\"section-$section\">" . escape_html($label) . "</h2>\n<ul>\n";
    my $children = '';
    for my $page ( @{ $sections{$section} } ) {
        my $entry      = $topic{$page};
        my $page_label = $entry->{label};
        $home .=
          "<li><a href=\"$page\">" . escape_html($page_label) . "</a></li>\n";
        $children .= sitemap_entry( $page_label, $page, undef );
        $index{$page_label}{$page} = $page;
        my %term = index_terms( $entry->{body} );
        for my $name ( keys %term ) {
            $index{$name}{$page} = "$page#$term{$name}";
        }
        my $nav =
            "<p class=\"navigation\"><a href=\"index.html\">Manual home</a>"
          . " / <a href=\"index.html#section-$section\">Section $section</a></p>\n";
        my $notice = $entry->{notice};
        my $body   = $nav . $entry->{body};
        if ( $notice =~ /\S/ ) {
            $body .=
                '<h2>Source notices</h2><pre>'
              . escape_html($notice)
              . "</pre>\n";
        }
        $files{$page} = document( "$page_label - $title", $body );
    }
    $home .= "</ul>\n";
    $toc  .= sitemap_entry( $label, "index.html#section-$section", $children );
}

my @keywords;
for my $term ( keys %index ) {
    my @targets = sort keys %{ $index{$term} };
    for my $page (@targets) {
        my $label = @targets > 1 ? "$term - $topic{$page}{label}" : $term;
        push @keywords, [ $label, $index{$term}{$page} ];
    }
}
my $keywords = join '', map { sitemap_entry( @$_, undef ) }
  sort {
    lc $a->[0] cmp lc $b->[0] || $a->[0] cmp $b->[0] || $a->[1] cmp $b->[1]
  } @keywords;
$files{'index.html'}  = document( $title, $home );
$files{'opencvs.hhc'} = sitemap($toc);
$files{'opencvs.hhk'} = sitemap($keywords);
$files{'manual.css'}  = <<'CSS';
body {
  background: #ffffff;
  color: #202020;
  font: 100% Georgia, "Times New Roman", serif;
  line-height: 1.45;
  margin: 1.5em 2em;
  max-width: 58em;
}
a { color: #17447c; }
h1, h2, h3 { font-family: Arial, sans-serif; line-height: 1.2; }
h1 { font-size: 1.35em; margin: 1.3em 0 0.45em; }
h2 { font-size: 1.12em; margin: 1.25em 0 0.4em; }
h3 { font-size: 1em; }
code, pre, samp, kbd, .Li { font-family: Consolas, "Courier New", monospace; }
pre { background: #f4f4f4; border: 1px solid #dddddd; padding: 0.6em; overflow: auto; }
dt { margin: 0.6em 0 0.1em; }
dd { margin-left: 2em; }
.Bd-indent { margin-left: 2em; }
.Pa, .Ad { font-style: italic; }
.Nm, .Fl, .Cm, .Ic, .In, .Fd, .Fn, .Cd { font-weight: bold; }
.Bl-dash { list-style-type: square; }
.Bl-compact li, .Bl-compact dt, .Bl-compact dd { margin-top: 0; }
table { border-collapse: collapse; margin: 0.5em 0; }
th, td { padding: 0.15em 0.5em; text-align: left; vertical-align: top; }
table.head, table.foot { width: 100%; border: 1px solid #cccccc; }
.head-vol, .foot-date { text-align: center; }
.head-rtitle, .foot-os { text-align: right; }
.navigation { border-bottom: 1px solid #cccccc; padding-bottom: 0.5em; }
a.permalink { color: inherit; text-decoration: none; }
a.permalink:hover { text-decoration: underline; }
CSS

# 0x420 enables the tri-pane viewer (0x20) and Search tab (0x400).
# 0x304e selects Contents toggle, Back, Forward, Home, Options, and Print.
$files{'opencvs.hhp'} = <<"HHP" . join( '', map { "$_\n" } @pages );
[OPTIONS]
Compatibility=1.1 or later
Compiled file=opencvs.chm
Contents file=opencvs.hhc
Index file=opencvs.hhk
Default topic=index.html
Default Window=Manual
Full-text search=Yes
Binary Index=Yes
Language=0x0409
Title=$title

[WINDOWS]
Manual="$title","opencvs.hhc","opencvs.hhk","index.html","index.html","","","","",0x420,260,0x304e

[FILES]
index.html
manual.css
HHP

make_path($output_dir);
for my $name ( sort keys %files ) {
    my $target = File::Spec->catfile( $output_dir, $name );
    die "Expected a regular output file: $target\n"
      if -l $target || ( -e $target && !-f $target );
    open my $out, '>:raw', $target or die "Cannot write $target: $!\n";
    my $data = $files{$name};
    $data =~ s/\n/\r\n/g;
    print {$out} $data or die "Cannot write $target: $!\n";
    close $out         or die "Cannot close $target: $!\n";
}
print 'Generated HTML Help project for ', scalar @pages, " manuals\n";
