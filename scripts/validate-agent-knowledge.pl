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
use File::Basename qw(dirname basename);
use File::Find qw(find);
use File::Spec;
use Cwd qw(abs_path);

my $root = abs_path(File::Spec->catdir(dirname(__FILE__), '..'));
my @errors;
sub read_file {
    my ($p) = @_;
    open my $fh, '<:raw', $p or do { push @errors, "cannot read $p: $!"; return ''; };
    return do { local $/; <$fh> };
}
for my $dir ("$root/tests", "$root/scripts", "$root/.agents/skills") {
    next unless -d $dir;
    find({ wanted => sub {
        return unless -f $_ && /\.py\z/;
        push @errors, "Python project script conflicts with the Perl-only test/maintenance policy: $File::Find::name";
    }, no_chdir => 1 }, $dir);
}
my @skills = qw(upstream-sync linux-portability windows-portability compatibility-layer cvs-protocol security-audit build-and-test release-management);
my @agents = qw(upstream-analysis linux-portability windows-portability architecture security testing ci-cd documentation);
my @docs = qw(README.md CONTRIBUTING.md SECURITY.md LICENSE UPSTREAM AGENTS.md docs/architecture.md docs/building.md docs/portability.md docs/windows.md docs/linux.md docs/testing.md docs/upstream-sync.md docs/compatibility.md docs/security.md docs/releasing.md docs/initial-audit.md);
my @instruction_files = ("$root/AGENTS.md");
for my $name (@skills) {
    my $path = "$root/.agents/skills/$name/SKILL.md";
    push @instruction_files, $path;
    my $text = read_file($path);
    my ($front) = $text =~ /\A---\r?\n(.*?)\r?\n---(?:\r?\n|\z)/s;
    if (!defined $front) { push @errors, "missing YAML frontmatter: $path"; next; }
    my ($declared) = $front =~ /^name:\s*([a-z0-9-]+)\s*$/m;
    my ($description) = $front =~ /^description:\s*(\S[^\r\n]*)/m;
    push @errors, "skill name differs from directory: $path" unless defined($declared) && $declared eq $name;
    push @errors, "skill description missing: $path" unless defined($description) && length($description) >= 20;
    push @errors, "unfinished skill scaffold: $path" if $text =~ /\b(?:TODO|TBD|PLACEHOLDER)\b/;
}
for my $name (@agents) {
    my $path = "$root/.github/agents/$name.agent.md";
    push @instruction_files, $path;
    my $text = read_file($path);
    push @errors, "agent frontmatter missing: $path" unless $text =~ /\A---\r?\n.*?^description:\s*\S.*?\r?\n---/ms;
    push @errors, "agent lacks concrete scope: $path" unless length($text) > 250;
}
for my $relative (@docs) { push @errors, "required project document missing: $relative" unless -s "$root/$relative"; }
push @instruction_files, map { "$root/$_" } @docs;
my %seen;
for my $path (grep { !$seen{$_}++ && -f $_ } @instruction_files) {
    my $text = read_file($path);
    while ($text =~ /\[[^\]\n]+\]\(([^)\n]+)\)/g) {
        my $target = $1;
        next if $target =~ /\A(?:https?:|mailto:|#)/;
        $target =~ s/\A<|>\z//g;
        $target =~ s/[#?].*\z//;
        $target =~ s/%([0-9A-Fa-f]{2})/chr(hex($1))/ge;
        my $resolved = abs_path(File::Spec->catfile(dirname($path), $target));
        push @errors, "broken local link $target in $path" unless defined($resolved) && -e $resolved;
        push @errors, "local link escapes project: $target in $path" if defined($resolved) && index($resolved, $root . '/') != 0 && $resolved ne $root;
    }
}
if (@errors) { print STDERR "$_\n" for @errors; exit 1; }
print "PASS: eight skill schemas, eight concrete agent scopes, required documents, and repository-local links\n";
print "Human review remains required for semantic accuracy and current CI evidence.\n";
