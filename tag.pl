#!/usr/bin/perl

while (<>) {
#    s/;\/\/(tag|direct|indirect)/ __attribute__ (($1));/;
    s/;\/\/(tag|direct|indirect|state|skip|string|istate)/ $1;/;
    print;
}
