#!/usr/bin/env perl

use strict;
use warnings;

my $finput = shift;
my $ftmp = "$finput.tmp";

my $highlight = 0;
my $source = '';
my $lang;

-f $finput or die 'missing filename';
!-f $ftmp or die "$ftmp already exists";

open(my $fin, '<', $finput) or die "cannot open input file: $finput";
open(my $fout, '>', $ftmp) or die "cannot open output file: $ftmp";

while (<$fin>) {
    if (/(.*)?\\begin\{lstlisting\}\[language=([^,]+)/) {
        $highlight = 1;
        $source = '';
        $lang = $2;
        print $fout $1 if $1;
    } elsif (/(.*)\\begin\{fvlisting\}\[language=([^,]+)/) {
        # Included source code (used as a hack by asciidoc to handle source
        # code inside a table)
        $highlight = 2;
        $source = '';
        $lang = $2;
        print $fout $1 if $1;
    } elsif ($highlight && /^(.*)\\end\{(?:lstlisting|fvlisting)\}$/) {
        $source .= $1;

        # Write source to temp file
        open(my $fh, '>', 'highlight-source.tmp') or die 'cannot open file';
        print $fh $source;
        close($fh);

        # Dump source-highlight output
        # FIXME: source-highlight will break the special characters already
        # encoded by latex (unicode & co).
        $lang =~ s/{?([^\}]*)}?/$1/;
        my $res = `source-highlight -i highlight-source.tmp -s '$lang' -f latexcolor`;

        # Remove spurious comments and empty line at end of source (which
        # breaks output rendering)
        $res =~ s/^%.*\n//g;
        $res =~ s/(?: \\\\\n\\mbox\{\})* \\\\\n\\mbox\{\}\n$//;
        $res =~ s/\\noindent\n/\\noindent/;

        if ($highlight == 1) {
            # Use boxed source-code only for "normal" source-code
            $res = "\\begin{IsSourceBox}$res\\end{IsSourceBox}";
        } else {
            $res = "\\begin{IsSourceInline}$res\\end{IsSourceInline}";
        }

        print $fout $res;

        $highlight = 0;
    } elsif ($highlight) {
        if ($highlight == 2 && /\\VerbatimInput\{([^}]+)\}/) {
            open(my $fh_src, '<', "$1.tex") or die "cannot open file: $1.tex";

            while (<$fh_src>) {
                $source .= $_;
            }

            close($fh_src);
        } else {
            $source .= $_;
        }
    } else {
        print $fout $_;
    }
}

close($fin);
close($fout);

rename($ftmp, $finput);
