#!/usr/bin/env perl
use strict;
use warnings;
use File::Basename qw(dirname basename);
use File::Path qw(make_path);

sub usage {
	die "usage: " . basename($0) . " -o output.c header... -- source...\n";
}

my $out = "";
my @headers;
my @sources;
my $mode = "headers";

while (@ARGV) {
	my $arg = shift @ARGV;
	if ($arg eq "-o") {
		usage() unless @ARGV;
		$out = shift @ARGV;
	} elsif ($arg eq "--") {
		$mode = "sources";
	} elsif ($arg =~ /^-/) {
		usage();
	} elsif ($mode eq "headers") {
		push @headers, $arg;
	} else {
		push @sources, $arg;
	}
}

usage() unless $out && @headers && @sources;

my @all_files = (@headers, @sources);
my ($type_header) = grep { basename($_) eq "type.h" } @headers;
$type_header //= $headers[0];

sub read_lines {
	my ($file) = @_;
	open my $fh, "<", $file or die "$file: $!\n";
	my @lines = <$fh>;
	close $fh;
	chomp @lines;
	return @lines;
}

sub strip_file {
	my ($file) = @_;
	my @in = read_lines($file);
	my @out;
	my $in_license = 0;
	my $guard_seen = 0;
	my $skip_define_guard = 0;

	for my $i (0 .. $#in) {
		my $line = $in[$i];
		if ($i == 0 && $line =~ m{^\s*/\*}) {
			$in_license = 1;
			next;
		}
		if ($in_license) {
			$in_license = 0 if $line =~ m{\*/};
			next;
		}
		if ($line =~ /^\s*#ifndef\s+ZWM_[A-Z0-9_]+_H\s*$/) {
			$guard_seen = 1;
			$skip_define_guard = 1;
			next;
		}
		if ($skip_define_guard && $line =~ /^\s*#define\s+ZWM_[A-Z0-9_]+_H\s*$/) {
			$skip_define_guard = 0;
			next;
		}
		push @out, $line;
	}

	if ($guard_seen) {
		for (my $i = $#out; $i >= 0; --$i) {
			next if $out[$i] =~ /^\s*$/;
			$out[$i] = "" if $out[$i] =~ m{^\s*#endif(?:\s*/\*.*\*/)?\s*$};
			last;
		}
	}

	return @out;
}

sub write_section {
	my ($fh, $name) = @_;
	print {$fh} "\n/* $name */\n\n";
}

sub emit_license {
	my ($fh) = @_;
	my $license = "LICENSE";
	return unless -f $license;

	print {$fh} "/*\n";
	for my $line (read_lines($license)) {
		print {$fh} " *";
		print {$fh} " $line" if $line ne "";
		print {$fh} "\n";
	}
	print {$fh} " */\n\n";
}

sub update_depth {
	my ($depth, $line) = @_;
	for my $c (split //, $line) {
		++$depth if $c eq "{";
		--$depth if $c eq "}";
	}
	return $depth;
}

sub ltrim {
	my ($s) = @_;
	$s =~ s/^\s+//;
	return $s;
}

sub is_preprocessor {
	my ($line) = @_;
	return $line =~ /^\s*#/;
}

sub is_one_line_proto {
	my ($line) = @_;
	return $line =~ /^\s*[A-Za-z_]/ && $line =~ /\([^;]*\)\s*;/ && $line !~ /=/;
}

sub static_decl {
	my ($line) = @_;
	return $line if $line =~ /^\s*static\s+/;
	return $line if $line =~ /^\s*main\s*\(/;
	$line =~ s/^\s*/static /;
	return $line;
}

sub is_return_line {
	my ($line) = @_;
	return $line =~ /^\s*(?:static\s+)?(?:const\s+)?[A-Za-z_][A-Za-z0-9_\s\*]*$/ &&
	       $line !~ /^\s*(if|for|while|switch|return|sizeof)\s/ &&
	       $line !~ /[=;]/;
}

sub starts_array {
	my ($line) = @_;
	return $line =~ /^\s*(?:static\s+)?(?:const\s+)?[A-Za-z_][A-Za-z0-9_\s\*]*\s+[A-Za-z_][A-Za-z0-9_]*\s*\[[^\]]*\]\s*(?:=|;)/;
}

sub is_array {
	my ($line) = @_;
	return $line =~ /^\s*(?:extern\s+)?(?:static\s+)?(?:const\s+)?[A-Za-z_][A-Za-z0-9_\s\*]*\s+[A-Za-z_][A-Za-z0-9_]*\s*\[[^\]]*\]\s*(?:=|;)/;
}

sub is_global {
	my ($line) = @_;
	return $line =~ /^\s*(?:(?:const|volatile|static)\s+)*[A-Za-z_]/ &&
	       $line =~ /=/ &&
	       $line !~ /\(/;
}

sub count_brace_delta {
	my ($line) = @_;
	my $delta = 0;
	for my $c (split //, $line) {
		++$delta if $c eq "{";
		--$delta if $c eq "}";
	}
	return $delta;
}

sub emit_includes {
	my ($fh) = @_;
	my %seen;
	for my $file (@all_files) {
		for my $line (read_lines($file)) {
			next unless $line =~ /^\s*#include\s*</;
			next if $seen{$line}++;
			print {$fh} "$line\n";
		}
	}
}

sub emit_macros {
	my ($fh) = @_;
	for my $file (@headers) {
		my @lines = strip_file($file);
		my $in_comment = 0;
		my $in_macro = 0;
		for my $line (@lines) {
			if ($in_comment) {
				$in_comment = 0 if $line =~ m{\*/};
				next;
			}
			if ($in_macro) {
				print {$fh} "$line\n";
				$in_macro = 0 unless $line =~ /\\\s*$/;
				next;
			}
			if ($line =~ m{^\s*/\*}) {
				$in_comment = 1 unless $line =~ m{\*/};
				next;
			}
			next if $line =~ /^\s*#include\s/;
			if (is_preprocessor($line)) {
				print {$fh} "$line\n";
				$in_macro = 1 if $line =~ /\\\s*$/;
			}
		}
	}
}

sub emit_types {
	my ($fh) = @_;
	my $depth = 0;
	my $pp_cont = 0;
	for my $line (strip_file($type_header)) {
		if ($pp_cont) {
			$pp_cont = 0 unless $line =~ /\\\s*$/;
			next;
		}
		next if $line =~ /^\s*#include\s/;
		if (is_preprocessor($line)) {
			$pp_cont = 1 if $line =~ /\\\s*$/;
			next;
		}
		next if $line =~ /^\s*extern\s/;
		next if $depth == 0 && is_one_line_proto($line);
		print {$fh} "$line\n";
		$depth = update_depth($depth, $line);
	}
}

sub emit_declarations {
	my ($fh) = @_;
	for my $file (@headers, @sources) {
		my $depth = 0;
		my $pp_cont = 0;
		my $pending = "";
		my $collect = 0;
		my $decl = "";

		for my $line (strip_file($file)) {
			if ($collect) {
				if ($line =~ /\{/) {
					$collect = 0;
					$decl = "";
					$depth = update_depth($depth, $line);
					next;
				}
				$decl .= " " . ltrim($line);
				if ($line =~ /;/) {
					print {$fh} "$decl\n";
					$collect = 0;
					$decl = "";
				}
				next;
			}
			if ($pp_cont) {
				$pp_cont = 0 unless $line =~ /\\\s*$/;
				next;
			}
			next if $line =~ /^\s*#include\s/;
			if (is_preprocessor($line)) {
				$pp_cont = 1 if $line =~ /\\\s*$/;
				next;
			}
			next if $line =~ /^\s*extern\s/;
			next if $line =~ /^\s*typedef\s/;

			if ($pending ne "") {
				if ($depth == 0 && $line =~ /^\s*[A-Za-z_][A-Za-z0-9_]*\s*\(/ && $line !~ /\{/) {
					$decl = $pending . " " . ltrim($line);
					$pending = "";
					if ($line =~ /;/) {
						print {$fh} "$decl\n";
						$decl = "";
					} else {
						$collect = 1;
					}
					next;
				}
				$pending = "";
			}

			if ($depth == 0 && $line =~ /^\s*static\s+[A-Za-z_][A-Za-z0-9_\s\*]*$/) {
				$pending = $line;
				next;
			}
			if ($depth == 0 && $line =~ /^\s*static\s+/ && is_one_line_proto($line)) {
				print {$fh} "$line\n";
				next;
			}
			if ($depth == 0 && is_one_line_proto($line)) {
				print {$fh} static_decl($line) . "\n";
				next;
			}

			$depth = update_depth($depth, $line);
		}
	}
}

sub emit_arrays {
	my ($fh) = @_;
	for my $file (@all_files) {
		my $depth = 0;
		my $pp_cont = 0;
		my $in_array = 0;
		my $array_depth = 0;

		for my $line (strip_file($file)) {
			if ($pp_cont) {
				$pp_cont = 0 unless $line =~ /\\\s*$/;
				next;
			}
			next if $line =~ /^\s*#include\s/;
			if (is_preprocessor($line)) {
				$pp_cont = 1 if $line =~ /\\\s*$/;
				next;
			}
			if ($in_array) {
				print {$fh} "$line\n";
				$array_depth += count_brace_delta($line);
				if ($array_depth <= 0 && $line =~ /;/) {
					$in_array = 0;
				}
				next;
			}
			next if $depth == 0 && $line =~ /^\s*extern\s+.*\[[^\]]*\]\s*;/;
			if ($depth == 0 && starts_array($line)) {
				$line =~ s/^\s*/static / unless $line =~ /^\s*static\s/;
				print {$fh} "$line\n";
				if ($line =~ /=/) {
					$in_array = 1;
					$array_depth = count_brace_delta($line);
					if ($array_depth <= 0 && $line =~ /;/) {
						$in_array = 0;
					}
				}
				next;
			}
			$depth = update_depth($depth, $line);
		}
	}
}

sub emit_globals {
	my ($fh) = @_;
	for my $file (@sources) {
		my $depth = 0;
		my $pp_cont = 0;
		my $in_global_comment = 0;

		for my $line (strip_file($file)) {
			if ($in_global_comment) {
				print {$fh} "$line\n";
				$in_global_comment = 0 if $line =~ m{\*/};
				next;
			}
			if ($pp_cont) {
				$pp_cont = 0 unless $line =~ /\\\s*$/;
				next;
			}
			next if $line =~ /^\s*#include\s/;
			if (is_preprocessor($line)) {
				$pp_cont = 1 if $line =~ /\\\s*$/;
				next;
			}
			if ($depth == 0 && is_global($line) && !is_array($line)) {
				print {$fh} "$line\n";
				$in_global_comment = 1 if $line =~ m{/\*} && $line !~ m{\*/};
				next;
			}
			$depth = update_depth($depth, $line);
		}
	}
}

sub emit_function_signature {
	my ($fh, $sig) = @_;
	if ($sig !~ /\n\s*main\s*\(/ && $sig !~ /^\s*static\s/) {
		$sig =~ s/^\s*/static /;
	}
	print {$fh} "$sig\n";
}

sub emit_implementations {
	my ($fh) = @_;
	for my $file (@sources) {
		print {$fh} "\n/* $file */\n\n";
		my $depth = 0;
		my $pending = "";
		my $sig = "";
		my $sig_mode = 0;
		my $skip_array = 0;
		my $array_depth = 0;
		my $skip_global_comment = 0;

		for my $line (strip_file($file)) {
			if ($skip_global_comment) {
				$skip_global_comment = 0 if $line =~ m{\*/};
				next;
			}
			next if $line =~ /^\s*#include\s/;
			if ($sig_mode) {
				if ($line =~ /\{/) {
					emit_function_signature($fh, $sig);
					print {$fh} "$line\n";
					$sig = "";
					$sig_mode = 0;
					$depth = update_depth($depth, $line);
					next;
				}
				if ($line =~ /;/) {
					print {$fh} "$sig\n$line\n";
					$sig = "";
					$sig_mode = 0;
					next;
				}
				$sig .= "\n$line";
				next;
			}
			if ($skip_array) {
				$array_depth += count_brace_delta($line);
				$skip_array = 0 if $array_depth <= 0 && $line =~ /;/;
				next;
			}
			if ($depth == 0 && is_array($line)) {
				$pending = "";
				if ($line =~ /=/) {
					$skip_array = 1;
					$array_depth = count_brace_delta($line);
					$skip_array = 0 if $array_depth <= 0 && $line =~ /;/;
				}
				next;
			}
			if ($depth == 0 && is_global($line)) {
				$pending = "";
				$skip_global_comment = 1 if $line =~ m{/\*} && $line !~ m{\*/};
				next;
			}

			if ($pending ne "") {
				if ($line =~ /^\s*[A-Za-z_][A-Za-z0-9_]*\s*\(/ && $line !~ /\{/ && $line !~ /;/) {
					$sig = "$pending\n$line";
					$pending = "";
					$sig_mode = 1;
					next;
				}
				if ($line =~ /^\s*[A-Za-z_][A-Za-z0-9_]*\s*\(/ && $line =~ /;/) {
					print {$fh} "$pending\n$line\n";
					$pending = "";
					next;
				}
				if ($line =~ /^\s*[A-Za-z_][A-Za-z0-9_]*\s*\([^;]*\)\s*$/ ||
				    $line =~ /^\s*[A-Za-z_][A-Za-z0-9_]*\s*\([^;]*\)\s*\{/) {
					$pending = static_decl($pending);
					print {$fh} "$pending\n$line\n";
					$pending = "";
					next;
				}
				print {$fh} "$pending\n";
				$pending = "";
			}

			next if $depth == 0 && $line =~ /^\s*static\s+/ && is_one_line_proto($line);
			next if $depth == 0 && is_one_line_proto($line);
			if ($depth == 0 && is_return_line($line)) {
				$pending = $line;
				next;
			}

			print {$fh} "$line\n";
			$depth = update_depth($depth, $line);
		}
		print {$fh} "$pending\n" if $pending ne "";
	}
}

make_path(dirname($out));
open my $fh, ">", $out or die "$out: $!\n";

emit_license($fh);
write_section($fh, "includes");
emit_includes($fh);
write_section($fh, "macros");
emit_macros($fh);
write_section($fh, "enums and types");
emit_types($fh);
write_section($fh, "function declarations");
print {$fh} "/* clang-format off */\n";
emit_declarations($fh);
print {$fh} "/* clang-format on */\n";
write_section($fh, "arrays");
print {$fh} "/* clang-format off */\n";
emit_arrays($fh);
print {$fh} "/* clang-format on */\n";
write_section($fh, "globals");
emit_globals($fh);
write_section($fh, "func impls");
emit_implementations($fh);

close $fh;
print "generated $out\n";
