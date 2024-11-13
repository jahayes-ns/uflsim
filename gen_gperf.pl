#!/usr/bin/perl

$which = shift;
$include_file = shift;


for (qw/char short int long float double signed unsigned FILE/) {
    $builtin{$_} = 1;
}

sub print_vars
{
    my ($aref, $struct, $offset) = @_;
    push @struct, $struct;
    $struct_or_union{$struct} = $in;
    my $last_var = "";
    for (@$aref) {
	my ($type, $var, $dimref, $count, $ptr, $t, $d, $i, $st, $sk, $str, $init) = @$_;
	die "no count for $var ($d $i)" if $ptr && !$d && !$i && $type !~ /^(char|FILE)$/ && $last_var ne "${var}_count";
	my $aname = "0";
	if (@$dimref) {
	    $" = "\n";
	    my $akey = "@$dimref";
	    if ($decl{$akey}) {
		$aname = $aname{$akey};
	    }
	    else {
		$aname = sprintf "_d%d", $dim_array_index++;
		$aname{$akey} = $aname;
	    }
	    push @$dimref, 0;
	    $" = ',';
	    $decl{$akey} = "static int $aname\[\] = {@$dimref};\n";
	}
	my $suf = $ptr ? "_star" : ($str ? "_string" : "");
	my $arg345 = $builtin{$type} ? "ato_$type$suf, str_$type$suf, 0" : "0, 0, \"$type\"";
	push @out1,  "$struct.$var, $offset$var), $arg345, $aname, sizeof ($type), $count, $ptr, $t, $d, $i, $st, $sk, $str, $init\n";
	push @{$members{$struct}}, $var;
	$last_var = $var;
    }
    @vars = ();
}

sub push_flds
{
    return 0 if $#_ != 7;
    my ($aref, $dimref, $type, $star, $var, $sub, $cnt, $tdi) = @_;
    return 0 if $type eq "FILE" || $type eq "void" && $star eq "*";
    push @$aref, [$type, $var, $dimref, @$dimref ? (pop @$dimref) : 1,
		  $star eq '*' ?1:0,
		  $tdi  eq "tag" ?1:0,
		  $tdi  eq "direct" ?1:0,
		  $tdi  eq "indirect" ?1:0,
		  ($tdi  eq "state" || $tdi eq "istate") ?1:0,
		  ($tdi  eq "skip" || $tdi eq "iskip") ?1:0,
		  $tdi  eq "string" ?1:0,
		  (($tdi ne "state" && $tdi ne "skip") || $tdi eq "istate" || $tdi eq "iskip") ?1:0];
    return 1;
}

while (<>) {
    next if /^\s*$/;
    my @dims;
    @dims = ();
    while (s/\[\s*([^\]]*)\]//) {
	push @dims, $1;
    }
    my $pattern = '(\w+)\s*(\*| )\s*(\w+)\s*(\[\s*(.*)\])?\s*(tag|direct|indirect|state|skip|string|istate)?;';
    if ($in) {
	if (($struct) = /\}\s*(\w+)/) {
	    print_vars (\@vars, $struct, "offsetof ($struct,");
	    $in = 0;
	}
	elsif (push_flds (\@vars, \@dims, /^\s*$pattern/)) {
#	    print "match against $_";
	}
	else {
#	    print "no match against $_";
	}
    }
    elsif (/typedef\s+(struct|union)/) {
	$in = $1;
	next;
    }
    elsif (push_flds (\@gvars, \@dims, /^\s*extern\s+$pattern/)) {
    }
    else {
#	print STDERR;
    }
}

$global = $include_file;
$global =~ s/\.h$/_global/;
#print_vars (\@gvars, $global, "(long)&(");


if ($which eq "info") {
    print "%{\n";
    print "#include \"hash.h\"\n";
    print "#include \"$include_file\"\n";
    print "#include \"stddef.h\"\n";
    print "#include \"string.h\"\n";
    print values %decl;
    print "%}\n";
    print "struct StructInfo;\n";
    print "%%\n";
    print @out1;
    print "%%\n";
    exit;
}
if ($which ne "members") {
    print STDERR "first argument must be \"info\" or \"members\"\n";
    exit 1;
}
    
$choose_src = $include_file;
$choose_src =~ s/\.h$/_choose.c/;

print "%{\n";
for $struct (@struct) {
    print "char *members_$struct\[\] = {\n";
    for $member (@{$members{$struct}}) {
	print "    \"$member\",\n";
    }
    print "    0\n};\n";
    if ($struct_or_union{$struct} eq "union") {
	print "#include \"$choose_src\"\n" if $did_include++ == 0;
    }
}
print "%}\n";

print  "struct StructMembers;\n";
print  "%%\n";
for $struct (@struct) {
    $choose = $struct_or_union{$struct} eq "union" ? "choose_$struct" : "0";
    print "\"$struct\", $choose, members_$struct\n";
}
print  "%%\n";
