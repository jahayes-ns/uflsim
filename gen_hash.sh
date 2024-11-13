#!/bin/sh

nm=${1%.h}

#undef INODE_H
#ifndef OLD_INODE_H
echo "./tag.pl $1 | cpp -DGENHASHDEF=1  -P - -imacros $1 | ./gen_gperf.pl info $1 | gperf -k "*" -t -E -N ${nm}_struct_info -H ihash" > /tmp/gh
./tag.pl $1 | cpp -DGENHASHDEF=1 -P - -imacros $1 | ./gen_gperf.pl info $1 | gperf -k "*" -t -E -N ${nm}_struct_info -H ihash
./tag.pl $1 | cpp -DGENHASHDEF=1 -P - -imacros $1 | ./gen_gperf.pl members $1 | gperf -k "*" -t -E -N ${nm}_struct_members -H mhash

cat > ${nm}_hash.h <<EOF
struct StructInfo *
${nm}_struct_info (const char *str, unsigned int len);

struct StructMembers *
${nm}_struct_members (const char *str, unsigned int len);
EOF
