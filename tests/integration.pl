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
#
# Black-box tests use private temporary repositories only. Perl is test
# infrastructure, not an opencvs runtime dependency. The local transport shim
# exercises CVS framing, not SSH encryption or authentication.
#
use strict;
use warnings;
use utf8;
use Cwd qw(abs_path getcwd);
use Digest::SHA qw(sha256_hex);
use Encode qw(encode decode FB_DEFAULT LEAVE_SRC);
use File::Basename qw(dirname);
use File::Find qw(find);
use File::Path qw(make_path);
use File::Spec;
use File::Temp qw(tempfile);
use Getopt::Long qw(GetOptions);
use IPC::Open3 qw(open3);
use JSON::PP;
use POSIX qw(WNOHANG);
use Test::More;
use Time::HiRes qw(time sleep);

# Diagnostics can contain native Unicode filenames on every runner.
binmode STDOUT, ':encoding(UTF-8)';
binmode STDERR, ':encoding(UTF-8)';

my ($opencvs, $gnu_cvs, $ssh_shim, $fs_probe, $report, $require_gnu);
my $suite = 'all';
my $timeout = 30;
my @selected;
GetOptions('opencvs=s' => \$opencvs, 'gnu-cvs=s' => \$gnu_cvs,
    'ssh-shim=s' => \$ssh_shim, 'fs-probe=s' => \$fs_probe,
    'suite=s' => \$suite, 'case=s@' => \@selected, 'timeout=i' => \$timeout,
    'report=s' => \$report, 'require-gnu' => \$require_gnu)
    or die "Invalid options\n";
die "--opencvs is required\n" unless defined $opencvs;
die "Unknown suite $suite\n" unless $suite =~ /\A(?:all|core|security|interop)\z/;
die "--timeout must be positive\n" unless $timeout > 0;
die "--require-gnu requires --gnu-cvs\n" if $require_gnu && !defined $gnu_cvs;
for my $executable (\$opencvs, \$gnu_cvs, \$ssh_shim, \$fs_probe) {
    next unless defined $$executable;
    $$executable = abs_path($$executable)
        // die "Executable does not exist: $$executable\n";
}
my @events;
my @failures;
my @skips;
my $current_test;
my $TEXT = "alpha\nbase\nomega\n";
my $BINARY = pack('C*', 0 .. 255) x 3 . "\r\n\0\$Id\$\xff\n";

sub path { return File::Spec->catfile(@_); }
sub bytes_path { return $^O eq 'MSWin32' ? $_[0] : encode('UTF-8', $_[0]); }
sub check { die $_[1] . "\n" unless $_[0]; }
sub equal { check($_[0] eq $_[1], $_[2] // 'Byte strings differ'); }
sub skip_case { die bless({reason => $_[0]}, 'PortableOpenCVS::Skip'); }
sub write_bytes {
    my ($filename, $data) = @_;
    make_path(dirname(bytes_path($filename))) unless -d dirname(bytes_path($filename));
    open my $fh, '>:raw', bytes_path($filename) or die "open $filename: $!\n";
    print {$fh} $data or die "write $filename: $!\n";
    close $fh or die "close $filename: $!\n";
}
# These paths belong to the case sandbox. The product creates archives and
# administrative files read-only, so fixture setup must opt in to rewriting.
sub replace_fixture {
    my ($filename, $data) = @_;
    my $native = bytes_path($filename);
    if (-e $native) {
        chmod(0600, $native) == 1 or die "chmod $filename: $!\n";
    }
    write_bytes($filename, $data);
}
sub read_bytes {
    my ($filename) = @_;
    open my $fh, '<:raw', bytes_path($filename) or die "open $filename: $!\n";
    local $/;
    my $data = <$fh>;
    close $fh or die "close $filename: $!\n";
    return defined $data ? $data : '';
}
sub sandbox {
    my $native_cleanup = $^O eq 'MSWin32' && defined $fs_probe;
    my $object = File::Temp->newdir('portable-opencvs-XXXXXXXX', TMPDIR => 1,
        CLEANUP => $native_cleanup ? 0 : 1);
    my $base = "$object";
    my $env = {%ENV};
    delete @{$env}{qw(CVSROOT CVS_RSH CVS_SERVER CVSREAD CVSREADONLYFS CVSIGNORE CVSWRAPPERS)};
    @{$env}{qw(LC_ALL TZ CVSEDITOR)} = ('C', 'UTC', 'false');
    my $s = {object => $object, base => $base, repo => path($base, 'repository'),
        seed => path($base, 'seed'), tmp => path($base, 'temporary'), env => $env,
        native_cleanup => $native_cleanup};
    make_path($s->{seed}, $s->{tmp});
    @{$env}{qw(TMPDIR TEMP TMP)} = ($s->{tmp}) x 3;
    return $s;
}
sub cleanup_sandbox {
    my ($s) = @_;
    return unless $s->{native_cleanup};
    # Capture files cannot be placed in the tree while deleting it: Windows
    # correctly refuses deletion of still-open non-share-delete handles.
    my ($control_fh, $control) = tempfile('opencvs-cleanup-XXXXXXXX',
        TMPDIR => 1, UNLINK => 1);
    binmode $control_fh;
    print {$control_fh} encode('UTF-8', $s->{base}) . "\n"
        or die "write native cleanup control: $!\n";
    close $control_fh or die "close cleanup control: $!\n";
    my @command = ($fs_probe, 'cleanup', $control);
    my $status = system {$fs_probe} @command;
    my $native_status = ${^CHILD_ERROR_NATIVE};
    push @events, {test => $current_test, argv => \@command,
        purpose => 'private native sandbox cleanup', raw_status => $status,
        native_status => $native_status};
    check($status == 0, "Native cleanup failed; retained test data: $s->{base}");
}
sub invoke {
    my ($s, $exe, $args, %options) = @_;
    my @command = ($exe, @$args);
    my ($in_fh, $in_name) = tempfile('stdin-XXXXXXXX', DIR => $s->{base}, UNLINK => 1);
    binmode $in_fh;
    print {$in_fh} ($options{input} // '') or die "write subprocess input: $!\n";
    close $in_fh or die "close subprocess input: $!\n";
    my ($out_fh, $out_name) = tempfile('stdout-XXXXXXXX', DIR => $s->{base}, UNLINK => 1);
    my ($err_fh, $err_name) = tempfile('stderr-XXXXXXXX', DIR => $s->{base}, UNLINK => 1);
    close $out_fh;
    close $err_fh;
    my $working = $options{cwd} // $s->{base};
    my $previous = getcwd();
    my ($pid, $spawn_error);
    {
        local %ENV = %{$options{env} // $s->{env}};
        local *CHILD_IN;
        local *CHILD_OUT;
        local *CHILD_ERR;
        open CHILD_IN, '<:raw', $in_name or die "open input: $!\n";
        open CHILD_OUT, '>:raw', $out_name or die "open output: $!\n";
        open CHILD_ERR, '>:raw', $err_name or die "open error: $!\n";
        chdir bytes_path($working) or die "chdir $working: $!\n";
        eval { $pid = open3('<&main::CHILD_IN', '>&main::CHILD_OUT',
            '>&main::CHILD_ERR', @command); 1 } or $spawn_error = $@;
        chdir $previous or die "restore working directory: $!\n";
    }
    die "Cannot start @command: $spawn_error\n" if defined $spawn_error;
    my $deadline = time() + $timeout;
    my ($status, $native_status, $timed_out);
    while (1) {
        my $waited = waitpid($pid, WNOHANG);
        if ($waited == $pid) {
            $status = $?;
            $native_status = ${^CHILD_ERROR_NATIVE};
            last;
        }
        die "waitpid $pid: $!\n" if $waited == -1;
        if (time() >= $deadline) {
            $timed_out = 1;
            if ($^O eq 'MSWin32') {
                # Native system tool, argument-list invocation; no cmd.exe.
                my $taskkill = path($ENV{SystemRoot} // 'C:\\Windows', 'System32', 'taskkill.exe');
                system {$taskkill} $taskkill, '/PID', "$pid", '/T', '/F';
            }
            kill 'KILL', $pid;
            waitpid($pid, 0);
            $status = $?;
            $native_status = ${^CHILD_ERROR_NATIVE};
            last;
        }
        sleep 0.02;
    }
    my $stdout_raw = read_bytes($out_name);
    my $stderr_raw = read_bytes($err_name);
    my $stdout = decode('UTF-8', $stdout_raw, FB_DEFAULT | LEAVE_SRC);
    my $stderr = decode('UTF-8', $stderr_raw, FB_DEFAULT | LEAVE_SRC);
    # Perl's POSIX-shaped status can lose Windows exception bits. Preserve
    # the native status immediately after waitpid, before any other child.
    my $exit = $status >> 8;
    push @events, {test => $current_test, argv => \@command, cwd => $working,
        returncode => $exit, raw_status => $status, native_status => $native_status,
        stdout => $stdout, stderr => $stderr,
        timed_out => $timed_out ? JSON::PP::true : JSON::PP::false};
    die "Command timed out: @command\n$stdout\n$stderr\n" if $timed_out;
    # NTSTATUS exceptions use severity 0xC. A normal C return(-1) is
    # 0xffffffff on Windows and must remain an ordinary nonzero error.
    check(($^O eq 'MSWin32' ? ($native_status & 0xf0000000) != 0xc0000000 : ($status & 127) == 0),
        "Process crashed: @command\n$stdout\n$stderr");
    check(($stdout . $stderr) !~ /(?:assertion[^\r\n]*failed|debug assertion failed)/i,
        "Process assertion failure: @command\n$stdout\n$stderr");
    check(($stdout . $stderr) !~ /(?:ERROR: (?:AddressSanitizer|LeakSanitizer)|UndefinedBehaviorSanitizer|runtime error:)/,
        "Sanitizer finding: @command\n$stdout\n$stderr");
    my $allowed = exists $options{allowed} ? $options{allowed} : [0];
    check(!defined $allowed || scalar(grep {$_ == $exit} @$allowed),
        "Unexpected exit $exit: @command\n$stdout\n$stderr");
    return {code => $exit, stdout => $stdout, stderr => $stderr,
        stdout_raw => $stdout_raw, stderr_raw => $stderr_raw, text => $stdout . $stderr};
}
sub cvs {
    my ($s, $args, %options) = @_;
    my $exe = delete($options{executable}) // $opencvs;
    my $root = delete($options{root}) // $s->{repo};
    return invoke($s, $exe, ['-f', '-T', $s->{tmp}, '-d', $root, @$args], %options);
}
sub initialize {
    my ($s, $exe) = @_;
    cvs($s, ['init'], defined $exe ? (executable => $exe) : ());
    check(-d path($s->{repo}, 'CVSROOT'), 'init did not create CVSROOT');
}
sub import_tree {
    my ($s, $files, %options) = @_;
    $files //= {'sample.txt' => $TEXT};
    for my $name (sort keys %$files) { write_bytes(path($s->{seed}, split m{/}, $name), $files->{$name}); }
    my @args = ('import');
    push @args, '-kb' if $options{binary};
    push @args, '-m', 'initial test import', 'module', 'TEST_VENDOR', 'TEST_START';
    cvs($s, \@args, cwd => $s->{seed}, defined $options{executable} ? (executable => $options{executable}) : ());
}
sub checkout {
    my ($s, $name, %options) = @_;
    $name //= 'work';
    cvs($s, ['checkout', '-d', $name, 'module'], %options);
    my $work = path($s->{base}, $name);
    check(-d $work, 'checkout did not create working directory');
    return $work;
}
sub populated {
    my ($s, $files, %options) = @_;
    initialize($s, $options{executable});
    import_tree($s, $files, %options);
    return checkout($s, 'work', defined $options{executable} ? (executable => $options{executable}) : ());
}
sub changed {
    my ($filename, $content) = @_;
    my $stamp = (stat(bytes_path($filename)))[9] + 5;
    write_bytes($filename, $content);
    utime $stamp, $stamp, bytes_path($filename) or die "utime $filename: $!\n";
}
sub slash_path { my $p = $_[0]; $p =~ s{\\}{/}g; return $p; }
sub native_file {
    my ($s, $operation, $target, $content) = @_;
    if (!defined $fs_probe) {
        skip_case('Native filesystem helper required on Windows for Unicode/long paths') if $^O eq 'MSWin32';
        if ($operation eq 'write') { write_bytes($target, $content); return; }
        return read_bytes($target);
    }
    my ($fh, $control) = tempfile('path-XXXXXXXX', DIR => $s->{base}, UNLINK => 1);
    binmode $fh;
    print {$fh} encode('UTF-8', $target) . "\n" or die "write UTF-8 control path: $!\n";
    close $fh;
    my ($data_fh, $data) = tempfile('data-XXXXXXXX', DIR => $s->{base}, UNLINK => 1);
    binmode $data_fh;
    print {$data_fh} $content if $operation eq 'write';
    close $data_fh;
    invoke($s, $fs_probe, [$operation, $control, $data]);
    return $operation eq 'read' ? read_bytes($data) : undef;
}
sub server {
    my ($s, $payload) = @_;
    return invoke($s, $opencvs, ['-f', '-T', $s->{tmp}, 'server'], input => $payload, allowed => undef);
}
sub protocol_prefix {
    my ($s) = @_;
    my $root = encode('UTF-8', slash_path($s->{repo}));
    return "Root $root\nValid-responses ok error Valid-requests E M Checked-in Updated Merged Removed\nDirectory .\n$root\n";
}
sub remote_environment {
    my ($s, $server) = @_;
    skip_case('Native local transport shim not supplied; CVS wire interoperability not run') unless defined $ssh_shim;
    return {%{$s->{env}}, CVS_RSH => $ssh_shim, CVS_SERVER => $server};
}
my @cases;
sub test_case { push @cases, [@_]; }

test_case core => version_and_invalid_command => sub {
    my ($s) = @_;
    my $version = invoke($s, $opencvs, ['-f', '-v']);
    check($version->{text} =~ /cvs/i, 'Version did not identify CVS');
    check(cvs($s, ['not-a-cvs-command'], allowed => undef)->{code} != 0, 'Unknown command succeeded');
};
test_case core => init_absent_root_and_repeat => sub {
    my ($s) = @_;
    check(!-e $s->{repo}, 'Sandbox repository existed before init');
    initialize($s);
    my $config = path($s->{repo}, 'CVSROOT', 'config,v');
    check(-f $config, 'init did not create versioned config');
    my $before = read_bytes($config);
    my $checkoutlist = path($s->{repo}, 'CVSROOT', 'checkoutlist');
    my $existing = read_bytes($checkoutlist);
    cvs($s, ['init']);
    equal(read_bytes($config), $before, 'Repeated init changed existing metadata');
    equal(read_bytes($checkoutlist), $existing,
        'Repeated init changed the existing administrative file');
};
test_case core => full_lifecycle => sub {
    my ($s) = @_;
    my $work = populated($s);
    equal(read_bytes(path($work, 'sample.txt')), $TEXT);
    for my $name (qw(Entries Root Repository)) { check(-f path($work, 'CVS', $name), "Missing CVS/$name"); }
    check(read_bytes(path($work, 'CVS', 'Entries')) =~ m{/sample\.txt/}, 'Missing sample entry');
    check(read_bytes(path($work, 'CVS', 'Root')) =~ /repository/, 'Incorrect Root metadata');
    cvs($s, ['diff', '-u', 'sample.txt'], cwd => $work, allowed => [0]);
    my $updated = "alpha\nchanged by commit\nomega\n";
    changed(path($work, 'sample.txt'), $updated);
    my $diff = cvs($s, ['diff', '-u', 'sample.txt'], cwd => $work, allowed => [1]);
    check($diff->{text} =~ /-base/ && $diff->{text} =~ /\+changed by commit/, 'Missing actual unified diff changes');
    cvs($s, ['commit', '-m', 'change test content', 'sample.txt'], cwd => $work);
    check(cvs($s, ['status', 'sample.txt'], cwd => $work)->{text} =~ /Up-to-date/, 'Committed file not up-to-date');
    my $log = cvs($s, ['log', 'sample.txt'], cwd => $work)->{text};
    check($log =~ /change test content/, 'Commit log message missing');
    my @revisions = $log =~ /revision [0-9]/g;
    check(@revisions >= 2, 'Historical revisions were lost');
    check(cvs($s, ['annotate', 'sample.txt'], cwd => $work)->{text} =~ /changed by commit/, 'Annotate content missing');
    cvs($s, ['tag', 'TEST_SNAPSHOT'], cwd => $work);
    cvs($s, ['checkout', '-r', 'TEST_SNAPSHOT', '-d', 'snapshot-control', 'module']);
    equal(read_bytes(path($s->{base}, 'snapshot-control', 'sample.txt')), $updated,
        'Export tag resolves to the expected committed content');
    cvs($s, ['export', '-r', 'TEST_SNAPSHOT', '-d', 'exported', 'module']);
    my $export = path($s->{base}, 'exported');
    equal(read_bytes(path($export, 'sample.txt')), $updated);
    my @administrative;
    find({wanted => sub { push @administrative, $File::Find::name if $_ eq 'CVS'; }, no_chdir => 0}, $export);
    check(!@administrative, 'Export retained CVS administrative directories');
    write_bytes(path($work, 'added.txt'), "added and removed\n");
    cvs($s, ['add', 'added.txt'], cwd => $work);
    cvs($s, ['commit', '-m', 'add test file', 'added.txt'], cwd => $work);
    equal(read_bytes(path(checkout($s, 'after-add'), 'added.txt')), "added and removed\n");
    unlink path($work, 'added.txt') or die "unlink test file: $!\n";
    cvs($s, ['remove', 'added.txt'], cwd => $work);
    cvs($s, ['commit', '-m', 'remove test file', 'added.txt'], cwd => $work);
    my $fresh = checkout($s, 'after-remove');
    check(!-e path($fresh, 'added.txt'), 'Removed file reappeared');
    equal(read_bytes(path($fresh, 'sample.txt')), $updated);
};
test_case core => export_nested_module_to_new_name => sub {
    my ($s) = @_;
    initialize($s);
    import_tree($s, {'sub/leaf.txt' => "nested export\n"});
    cvs($s, ['export', '-r', 'TEST_START', '-d', 'renamed-export', 'module']);
    my $export = path($s->{base}, 'renamed-export');
    equal(read_bytes(path($export, 'sub', 'leaf.txt')), "nested export\n");
    check(!-e path($export, 'CVS') && !-e path($export, 'sub', 'CVS'),
        'Export created working-copy metadata');
};
test_case core => diff_preserves_separated_hunks => sub {
    my ($s) = @_;
    my @lines = map { "line $_\n" } 1 .. 30;
    my $work = populated($s, {'sample.txt' => join('', @lines)});
    my $file = path($work, 'sample.txt');
    cvs($s, ['diff', '-u', 'sample.txt'], cwd => $work, allowed => [0]);
    $lines[3] = "first change\n";
    $lines[25] = "last change\n";
    changed($file, join('', @lines));
    my $diff = cvs($s, ['diff', '-u', 'sample.txt'], cwd => $work,
        allowed => [1])->{text};
    my @hunks = $diff =~ /^@@ /mg;
    check(@hunks == 2, 'Separated changes did not produce two hunks');
    check($diff =~ /^\+first change$/m && $diff =~ /^\+last change$/m,
        'Unified diff omitted one of the changes');
};
test_case core => binary_roundtrip_and_modification => sub {
    my ($s) = @_;
    my $work = populated($s, {'binary.dat' => $BINARY}, binary => 1);
    equal(read_bytes(path($work, 'binary.dat')), $BINARY, 'Binary checkout changed bytes');
    my $replacement = reverse($BINARY) . "\0new binary\r\n";
    changed(path($work, 'binary.dat'), $replacement);
    cvs($s, ['commit', '-m', 'binary test update', 'binary.dat'], cwd => $work);
    equal(read_bytes(path(checkout($s, 'binary-fresh'), 'binary.dat')), $replacement, 'Binary commit/checkout changed bytes');
};
test_case core => unicode_and_space_paths => sub {
    my ($s) = @_;
    initialize($s);
    my $relative = path("donn\x{e9}es", "caf\x{e9}-\x{96ea}.txt");
    native_file($s, 'write', path($s->{seed}, $relative), "UTF-8 file name\n");
    cvs($s, ['import', '-m', 'Unicode test import', 'module', 'TEST_VENDOR', 'TEST_START'], cwd => $s->{seed});
    my $work = checkout($s);
    equal(native_file($s, 'read', path($work, $relative)), "UTF-8 file name\n", 'Unicode checkout bytes differ');
    my $entries = native_file($s, 'read', path($work, "donn\x{e9}es", 'CVS', 'Entries'));
    check(index($entries, encode('UTF-8', "caf\x{e9}-\x{96ea}.txt")) >= 0, 'Entries filename is not UTF-8');
    equal(native_file($s, 'read', path(checkout($s, 'working tree with spaces'), $relative)), "UTF-8 file name\n");
};
test_case core => long_nested_paths => sub {
    my ($s) = @_;
    my $relative = path(map { sprintf('segment-%02d-', $_) . ('x' x 24) } 0 .. 8);
    $relative = path($relative, 'long.txt');
    check(length(path($s->{seed}, $relative)) > 300, 'Long path fixture unexpectedly short');
    initialize($s);
    native_file($s, 'write', path($s->{seed}, $relative), "long path data\n");
    cvs($s, ['import', '-m', 'long path test import', 'module', 'TEST_VENDOR', 'TEST_START'], cwd => $s->{seed});
    equal(native_file($s, 'read', path(checkout($s), $relative)), "long path data\n");
};
test_case core => conflict_and_recovery => sub {
    my ($s) = @_;
    my $first = populated($s);
    my $second = checkout($s, 'second-work');
    changed(path($first, 'sample.txt'), "alpha\nleft change\nomega\n");
    cvs($s, ['commit', '-m', 'left revision', 'sample.txt'], cwd => $first);
    changed(path($second, 'sample.txt'), "alpha\nright change\nomega\n");
    cvs($s, ['update', 'sample.txt'], cwd => $second, allowed => [0, 1]);
    my $conflict = read_bytes(path($second, 'sample.txt'));
    for my $marker ('<<<<<<<', '=======', '>>>>>>>', 'left change', 'right change') { check(index($conflict, $marker) >= 0, "Missing conflict marker/content $marker"); }
    my $archive = path($s->{repo}, 'module', 'sample.txt,v');
    my $archive_before = sha256_hex(read_bytes($archive));
    check(cvs($s, ['commit', '-m', 'unresolved conflict', 'sample.txt'], cwd => $second, allowed => undef)->{code} != 0, 'Unresolved conflict was committed');
    equal(sha256_hex(read_bytes($archive)), $archive_before,
        'Rejected conflict changed the archive');
    equal(read_bytes(path($second, 'sample.txt')), $conflict,
        'Rejected conflict changed the working file');
    my $resolved = "alpha\nresolved change\nomega\n";
    changed(path($second, 'sample.txt'), $resolved);
    cvs($s, ['commit', '-m', 'resolved conflict', 'sample.txt'], cwd => $second);
    cvs($s, ['update', 'sample.txt'], cwd => $first);
    equal(read_bytes(path($first, 'sample.txt')), $resolved);
};
test_case core => sticky_tag_reset_and_date => sub {
    my ($s) = @_;
    my $work = populated($s);
    cvs($s, ['tag', 'TEST_OLD'], cwd => $work);
    my $newer = "alpha\nnew revision\nomega\n";
    changed(path($work, 'sample.txt'), $newer);
    cvs($s, ['commit', '-m', 'new revision', 'sample.txt'], cwd => $work);
    cvs($s, ['update', '-r', 'TEST_OLD'], cwd => $work);
    equal(read_bytes(path($work, 'sample.txt')), $TEXT);
    cvs($s, ['update', '-A'], cwd => $work);
    equal(read_bytes(path($work, 'sample.txt')), $newer);
    my $future = (gmtime)[5] + 1901;
    cvs($s, ['checkout', '-D', "$future-01-01 00:00:00 UTC", '-d', 'date-work', 'module']);
    equal(read_bytes(path($s->{base}, 'date-work', 'sample.txt')), $newer);
    cvs($s, ['checkout', '-D', '1 day', '-d', 'relative-work', 'module']);
    equal(read_bytes(path($s->{base}, 'relative-work', 'sample.txt')), $newer,
        'Relative day selects the expected revision');
};
test_case core => missing_repository_is_explicit_error => sub {
    my ($s) = @_;
    check(cvs($s, ['checkout', 'module'], allowed => undef)->{code} != 0, 'Missing repository succeeded');
    check(!-e path($s->{base}, 'module'), 'Failed checkout created unexpected module');
};
test_case core => unsupported_pserver_is_explicit => sub {
    my ($s) = @_;
    initialize($s);
    my $result = cvs($s, ['checkout', 'module'], root => ':pserver:test.invalid:' . slash_path($s->{repo}), allowed => undef);
    check($result->{code} != 0 && $result->{text} =~ /(?:not supported|unsupported)/i, 'pserver was not rejected explicitly');
};

test_case security => unknown_server_request_is_error => sub {
    my ($s) = @_;
    my $result = server($s, "Not-a-real-CVS-request\n");
    check($result->{code} != 0 && $result->{text} =~ /(?:not supported|unsupported|unknown)/i, 'Unknown server request did not fail clearly');
};
test_case security => untrusted_server_responses_preserve_working_copy => sub {
    my ($s) = @_;
    skip_case('Native fake-server and transport helpers not supplied')
        unless defined $fs_probe && defined $ssh_shim;
    my $work = populated($s);
    my $root = slash_path($s->{repo});
    my $remote_root = ':ext:test.invalid:' . $root;
    my $fixture = path($s->{base}, 'server-response.bin');
    my $env = {%{remote_environment($s, $fs_probe)}, PC_TEST_SERVER_RESPONSE => $fixture};
    my $file = path($work, 'sample.txt');
    my $entries = path($work, 'CVS', 'Entries');
    my $canary = path($s->{base}, 'canary.txt');
    write_bytes($canary, 'outside working copy');
    my $replacement = "alpha\nserver baseline\nomega\n";
    my $frame = sub {
        my ($kind, $repository, $entry_name, $length, $body) = @_;
        return "$kind ./\n$repository\n/$entry_name/1.2///\nu=rw,g=r,o=r\n$length\n$body";
    };
    # Prove that negotiation and the positive fixture exercise Updated itself.
    write_bytes($fixture, $frame->('Updated', "$root/module/sample.txt", 'sample.txt',
        length($replacement), $replacement) . "ok\n");
    cvs($s, ['update', 'sample.txt'], cwd => $work, root => $remote_root, env => $env);
    equal(read_bytes($file), $replacement, 'Positive fake server did not update working file');
    my $file_before = sha256_hex(read_bytes($file));
    my $entries_before = sha256_hex(read_bytes($entries));
    my @bad = (
        $frame->('Updated', '/x', 'sample.txt', 5, 'PWNED'),
        $frame->('Updated', "$root-collision/module/sample.txt", 'sample.txt', 5, 'PWNED'),
        $frame->('Updated', "$root/../outside/sample.txt", 'sample.txt', 5, 'PWNED'),
        $frame->('Merged', "$root/module/../../canary.txt", 'canary.txt', 5, 'PWNED'),
        $frame->('Updated', "$root/module/sample.txt", 'different.txt', 5, 'PWNED'),
        $frame->('Updated', "$root/module/sample.txt", 'sample.txt', 100, 'xx'),
        $frame->('Merged', "$root/module/sample.txt", 'sample.txt', 100, 'xx')
    );
    for my $bad (@bad) {
        write_bytes($fixture, $bad);
        my $result = cvs($s, ['update', 'sample.txt'], cwd => $work,
            root => $remote_root, env => $env, allowed => undef);
        check($result->{code} != 0, 'Malicious/truncated server response succeeded');
        equal(sha256_hex(read_bytes($file)), $file_before, 'Rejected response changed working file');
        equal(sha256_hex(read_bytes($entries)), $entries_before, 'Rejected response changed Entries');
        equal(read_bytes($canary), 'outside working copy', 'Rejected response changed canary');
    }
};
test_case security => invalid_protocol_argument_state_is_rejected => sub {
    my ($s) = @_;
    for my $request ("ArgumentX\n", "Argument one\nArgumentX\n", "rlog\n", "rtag\n") {
        my $result = server($s, $request);
        check($result->{code} != 0, 'Malformed protocol state succeeded');
        check($result->{text} =~ /(?:Root|Argument|argument|Protocol)/,
            'Malformed state did not produce a protocol diagnostic');
    }
};
test_case security => modified_path_traversal_preserves_canary => sub {
    my ($s) = @_;
    initialize($s);
    my $sentinel = path($s->{tmp}, 'sentinel.txt');
    write_bytes($sentinel, 'intact');
    for my $name ('../sentinel.txt', '..\\sentinel.txt') {
        my $before = sha256_hex(read_bytes($sentinel));
        my $result = server($s, protocol_prefix($s) . "Modified $name\nu=rw,g=r,o=r\n5\nPWNED");
        check($result->{code} != 0, 'Traversal request succeeded');
        equal(sha256_hex(read_bytes($sentinel)), $before, 'Traversal changed outside canary');
    }
};
test_case security => directory_traversal_preserves_canary => sub {
    my ($s) = @_;
    initialize($s);
    my $sentinel = path($s->{base}, 'outside', 'sentinel.txt');
    write_bytes($sentinel, 'intact');
    my $root = encode('UTF-8', slash_path($s->{repo}));
    check(server($s, "Root $root\nDirectory .\n$root/../../outside\n")->{code} != 0, 'Directory traversal succeeded');
    equal(read_bytes($sentinel), 'intact');
};
test_case security => invalid_lengths_and_truncated_body => sub {
    my ($s) = @_;
    initialize($s);
    for my $frame (['-1', ''], ['2147483648', ''], ['12', 'xx']) {
        my ($length, $body) = @$frame;
        my $result = server($s, protocol_prefix($s) . "Modified frame.txt\nu=rw,g=r,o=r\n$length\n$body");
        check($result->{code} != 0, 'Invalid/truncated frame succeeded');
    }
};
test_case security => malformed_rcs_rejected_without_crash => sub {
    my ($s) = @_;
    populated($s);
    replace_fixture(path($s->{repo}, 'module', 'sample.txt,v'), "head not-a-revision;\naccess;\nsymbols;\nlocks; strict;\n");
    check(cvs($s, ['checkout', '-d', 'malformed', 'module'], allowed => undef)->{code} != 0, 'Malformed RCS archive succeeded');
};
test_case security => invalid_sticky_date_preserves_working_copy => sub {
    my ($s) = @_;
    my $work = populated($s);
    my $tag = path($work, 'CVS', 'Tag');
    my $entries = path($work, 'CVS', 'Entries');
    my $file = path($work, 'sample.txt');
    my $before_entries = read_bytes($entries);
    my $invalid_date = "D2026.02.30.00.00.00\n";
    write_bytes($tag, $invalid_date);
    my $result = cvs($s, ['update'], cwd => $work, allowed => undef);
    check($result->{code} != 0, 'Invalid calendar date accepted');
    equal(read_bytes($entries), $before_entries, 'Rejected date changed Entries');
    equal(read_bytes($file), $TEXT, 'Rejected date changed working file');
    equal(read_bytes($tag), $invalid_date, 'Rejected date changed Tag');
};
test_case security => invalid_tag_does_not_change_repository => sub {
    my ($s) = @_;
    my $work = populated($s);
    my $archive = path($s->{repo}, 'module', 'sample.txt,v');
    my $before = sha256_hex(read_bytes($archive));
    check(cvs($s, ['tag', 'invalid.tag'], cwd => $work, allowed => undef)->{code} != 0, 'Invalid tag succeeded');
    equal(sha256_hex(read_bytes($archive)), $before, 'Invalid tag changed archive');
};
test_case security => malformed_rcs_delta_commands_are_rejected => sub {
    my ($s) = @_;
    populated($s);
    my $archive = path($s->{repo}, 'module', 'sample.txt,v');
    my $prefix = <<'RCS';
head 1.2;
access;
symbols;
locks; strict;
comment @# @;

1.2
date 2026.01.02.00.00.00; author fixture; state Exp;
branches;
next 1.1;

1.1
date 2026.01.01.00.00.00; author fixture; state Exp;
branches;
next;

desc
@Controlled delta parser regression archive@
1.2
log
@new revision@
text
@new
@
1.1
log
@old revision@
text
@
RCS
    # The opening quote is on its own source line, but the first delta
    # command must start immediately after it in the archive.
    chomp $prefix;
    # Establish that the fixture is valid before testing malformed commands.
    replace_fixture($archive, $prefix . "d1 1\na1 1\nold\n\@\n");
    cvs($s, ['checkout', '-r', '1.1', '-d', 'delta-valid', 'module']);
    equal(read_bytes(path($s->{base}, 'delta-valid', 'sample.txt')), "old\n");
    my $index = 0;
    for my $delta ("a1\n", "a2147483648 1\nx\n", "a0 2147483648\n",
        "d0 1\n", "d1 0\n", "d1 1junk\n", "d1 2\n", "x1 1\n",
        "a0 1\n", "a0 2\none\n", "a0 1\0\nbody\n", "d1\0 1\n",
        "d1 1", "a0 -1\n", "a0 999999999999999999999999999999\n") {
        replace_fixture($archive, $prefix . $delta . "\@\n");
        my $before = sha256_hex(read_bytes($archive));
        my $result = cvs($s, ['checkout', '-r', '1.1', '-d', 'delta-bad-' . ++$index,
            'module'], allowed => undef);
        check($result->{code} != 0, 'Malformed RCS delta command succeeded');
        equal(sha256_hex(read_bytes($archive)), $before, 'Rejected delta changed archive');
    }
};

test_case security => invalid_rcs_numbers_and_dates_are_rejected => sub {
    my ($s) = @_;
    populated($s);
    my $archive = path($s->{repo}, 'module', 'sample.txt,v');
    my $original = read_bytes($archive);
    my @headers;
    for my $revision ('1..1', '.1', '1.', '65536.1') {
        my $bad = $original;
        check($bad =~ s/\Ahead\s+[^;]+;/head $revision;/,
            'RCS header fixture has no head');
        push @headers, $bad;
    }
    for my $date ('2026.13.01.00.00.00', '2026.01.00.00.00.00',
        '2026.01.01.24.00.00', '2026.01.01.00.60.00',
        '2025.02.29.00.00.00', '2026.04.31.00.00.00') {
        my $bad = $original;
        check($bad =~ s/date\s+[^;]+;/date $date;/,
            'RCS date fixture has no date');
        push @headers, $bad;
    }
    my $index = 0;
    for my $bad (@headers) {
        replace_fixture($archive, $bad);
        my $before = sha256_hex(read_bytes($archive));
        my $result = cvs($s, ['checkout', '-d', 'bad-header-' . ++$index, 'module'],
            allowed => undef);
        check($result->{code} != 0, 'Invalid RCS number/date succeeded');
        equal(sha256_hex(read_bytes($archive)), $before,
            'Rejected RCS header changed archive');
    }
};

test_case security => template_editor_failure_preserves_archive => sub {
    my ($s) = @_;
    skip_case('Native editor regression helper not supplied') unless defined $fs_probe;
    my $work = populated($s);
    my $archive = path($s->{repo}, 'module', 'sample.txt,v');
    my $before = sha256_hex(read_bytes($archive));
    my $template = path($s->{base}, 'commit-template.txt');
    my $marker = path($s->{base}, 'editor-template-verified.txt');
    write_bytes($template, "Portable OpenCVS template sentinel\n");
    replace_fixture(path($s->{repo}, 'CVSROOT', 'rcsinfo'), "ALL " . slash_path($template) . "\n");
    changed(path($work, 'sample.txt'), "alpha\nfailed editor update\nomega\n");
    my $editor = '"' . slash_path($fs_probe) . '" editor-check-template "'
        . slash_path($marker) . '"';
    my $env = {%{$s->{env}}, CVSEDITOR => $editor};
    my $result = cvs($s, ['commit', 'sample.txt'], cwd => $work, env => $env,
        allowed => undef);
    check($result->{code} != 0, 'Failed editor allowed a commit');
    equal(read_bytes($marker), "template verified\n", 'Editor did not receive configured template');
    equal(sha256_hex(read_bytes($archive)), $before, 'Failed editor changed archive');
};

test_case security => archive_write_failure_preserves_original => sub {
    my ($s) = @_;
    skip_case('POSIX RLIMIT_FSIZE injection requires native helper')
        if $^O eq 'MSWin32' || !defined $fs_probe;
    my $work = populated($s);
    my $archive = path($s->{repo}, 'module', 'sample.txt,v');
    my $before = sha256_hex(read_bytes($archive));
    changed(path($work, 'sample.txt'), "alpha\nprivate failed commit\nomega\n");
    my $result = invoke($s, $fs_probe, ['limit-exec', '4096', $opencvs,
        '-T', $s->{tmp}, '-d', $s->{repo}, 'commit', '-m', 'X' x 16384,
        'sample.txt'], cwd => $work, allowed => undef);
    check($result->{code} != 0, 'File-size-limited commit succeeded');
    equal(sha256_hex(read_bytes($archive)), $before,
        'Write failure replaced the original RCS archive');
    check($result->{text} =~ /(?:write|close|large|limit|space)/i,
        'Commit did not report a write-related failure');
};

test_case core => native_remote_rlog_reads_repository => sub {
    my ($s) = @_;
    skip_case('Native local transport shim not supplied') unless defined $ssh_shim;
    populated($s);
    my $root = ':ext:test.invalid:' . slash_path($s->{repo});
    my $env = remote_environment($s, $opencvs);
    my $result = cvs($s, ['rlog', 'module'], root => $root, env => $env);
    check($result->{text} =~ /sample\.txt/ && $result->{text} =~ /(?:revision|head:)/,
        'Remote rlog did not report archive history');
};

test_case interop => gnu_repository_read_write_by_opencvs => sub {
    my ($s) = @_;
    initialize($s, $gnu_cvs);
    import_tree($s, undef, executable => $gnu_cvs);
    my $work = checkout($s);
    equal(read_bytes(path($work, 'sample.txt')), $TEXT);
    my $updated = "alpha\nOpenCVS writes GNU repository\nomega\n";
    changed(path($work, 'sample.txt'), $updated);
    cvs($s, ['commit', '-m', 'OpenCVS interoperability commit', 'sample.txt'], cwd => $work);
    equal(read_bytes(path(checkout($s, 'gnu-reader', executable => $gnu_cvs), 'sample.txt')), $updated);
};
test_case interop => opencvs_repository_read_write_by_gnu => sub {
    my ($s) = @_;
    my $work = populated($s);
    my $gnu_work = checkout($s, 'gnu-writer', executable => $gnu_cvs);
    my $updated = "alpha\nGNU writes OpenCVS repository\nomega\n";
    changed(path($gnu_work, 'sample.txt'), $updated);
    cvs($s, ['commit', '-m', 'GNU interoperability commit', 'sample.txt'], cwd => $gnu_work, executable => $gnu_cvs);
    cvs($s, ['update', 'sample.txt'], cwd => $work);
    equal(read_bytes(path($work, 'sample.txt')), $updated);
};
test_case interop => binary_repository_interchange => sub {
    my ($s) = @_;
    initialize($s, $gnu_cvs);
    import_tree($s, {'binary.dat' => $BINARY}, executable => $gnu_cvs, binary => 1);
    my $work = checkout($s);
    equal(read_bytes(path($work, 'binary.dat')), $BINARY);
    my $updated = $BINARY . "\0OpenCVS-to-GNU\r\n";
    changed(path($work, 'binary.dat'), $updated);
    cvs($s, ['commit', '-m', 'binary interchange', 'binary.dat'], cwd => $work);
    equal(read_bytes(path(checkout($s, 'gnu-binary', executable => $gnu_cvs), 'binary.dat')), $updated);
};
test_case interop => opencvs_client_gnu_server_wire => sub {
    my ($s) = @_;
    initialize($s, $gnu_cvs);
    import_tree($s, undef, executable => $gnu_cvs);
    my $root = ':ext:test.invalid:' . slash_path($s->{repo});
    my $env = remote_environment($s, $gnu_cvs);
    # Synthetic repository traffic only: identify the metadata response at the
    # point of failure without dumping file contents or relaxing validation.
    my $wire_log = path($s->{base}, 'gnu-server-wire');
    $env->{CVS_CLIENT_LOG} = $wire_log;
    my $work;
    my $checkout_ok = eval {
        $work = checkout($s, 'wire-opencvs-client', root => $root, env => $env);
        1;
    };
    if (!$checkout_ok) {
        my $error = $@;
        # cvs_remote_input records server output in the .out log.
        my $incoming = $wire_log . '-0.out';
        if (-f $incoming) {
            my $traffic = read_bytes($incoming);
            my @lines = split /\n/, $traffic;
            my $last = @lines ? pop @lines : '';
            my $context = @lines ? pop @lines : '';
            my $diagnostic = join "\n", map {
                my $line = substr($_, 0, 512);
                $line =~ s/([^\x20-\x7e])/sprintf('\\x%02x', ord($1))/ge;
                $line;
            } ($context, $last);
            push @events, {test => $current_test,
                purpose => 'last synthetic GNU server metadata context',
                protocol_context => $diagnostic};
            $error .= "GNU response context (last two lines, 512 bytes each):\n$diagnostic\n";
        }
        die $error;
    }
    equal(read_bytes(path($work, 'sample.txt')), $TEXT);
    my $updated = "alpha\nOpenCVS client wire commit\nomega\n";
    changed(path($work, 'sample.txt'), $updated);
    cvs($s, ['commit', '-m', 'wire commit', 'sample.txt'], cwd => $work, root => $root, env => $env);
    equal(read_bytes(path(checkout($s, 'wire-gnu-reader', executable => $gnu_cvs), 'sample.txt')), $updated);
};
test_case interop => gnu_client_opencvs_server_wire => sub {
    my ($s) = @_;
    initialize($s);
    import_tree($s);
    my $root = ':ext:test.invalid:' . slash_path($s->{repo});
    my $env = remote_environment($s, $opencvs);
    my $work = checkout($s, 'wire-gnu-client', root => $root, env => $env, executable => $gnu_cvs);
    equal(read_bytes(path($work, 'sample.txt')), $TEXT);
    my $updated = "alpha\nGNU client wire commit\nomega\n";
    changed(path($work, 'sample.txt'), $updated);
    cvs($s, ['commit', '-m', 'reverse wire commit', 'sample.txt'], cwd => $work, executable => $gnu_cvs, root => $root, env => $env);
    equal(read_bytes(path(checkout($s, 'wire-opencvs-reader'), 'sample.txt')), $updated);
};

my $run = 0;
for my $case (@cases) {
    my ($group, $name, $code) = @$case;
    next if $suite ne 'all' && $suite ne $group;
    next if @selected && !grep {index($name, $_) >= 0} @selected;
    $run++;
    $current_test = "$group.$name";
    my $s = sandbox();
    my $ok = eval {
        skip_case('GNU CVS oracle not supplied; interoperability not run') if $group eq 'interop' && !defined $gnu_cvs;
        $code->($s);
        1;
    };
    my $error = $@;
    my $cleanup_ok = eval { cleanup_sandbox($s); 1; };
    my $cleanup_error = $@;
    if (!$cleanup_ok) {
        $error = ($ok || ref($error) ? '' : "$error") . "$cleanup_error";
        $ok = 0;
    }
    if (!$ok && ref($error) eq 'PortableOpenCVS::Skip') {
        push @skips, {test => $current_test, reason => $error->{reason}};
        SKIP: { skip $error->{reason}, 1; }
    } elsif (!$ok) {
        push @failures, {test => $current_test, detail => "$error"};
        fail($current_test);
        diag("$error");
    } else {
        pass($current_test);
    }
}
die "No tests selected\n" unless $run;
done_testing();
if (defined $report) {
    make_path(dirname($report));
    open my $out, '>:raw', $report or die "open report $report: $!\n";
    print {$out} JSON::PP->new->utf8->canonical->pretty->encode({
        executable => $opencvs, gnu_cvs => $gnu_cvs, suite => $suite,
        tests_run => $run, failures => \@failures, errors => [], skipped => \@skips,
        success => @failures ? JSON::PP::false : JSON::PP::true, commands => \@events});
    close $out or die "close report: $!\n";
}
exit(@failures ? 1 : 0);
