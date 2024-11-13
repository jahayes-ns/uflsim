/*(Copyright 2005-2020 Kendall F. Morris

This file is part of the USF Neural Simulator suite.

    The Neural Simulator suite is free software: you can redistribute
    it and/or modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation, either
    version 3 of the License, or (at your option) any later version.

    The suite is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with the suite.  If not, see <https://www.gnu.org/licenses/>.
*/


#define _GNU_SOURCE
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <stdio.h>
#include "fileio.h"
#include "hash.h"
#include "util.h"
#include "c_globals.h"

#include "inode_hash.h"

#if HAVE_CONFIG_H
#  include <config.h>
#endif /* HAVE_CONFIG_H */

#define FFMT "%.17g"

#if SIZEOF_SIZE_T == 8
#define Z "l"
#else
#define Z ""
#endif

#define CHAT 0

#include "lin2ms.h"

int Version;
int SubVersion;


char *
str_char (void *p)
{
  char *s;
  if (asprintf (&s, "%d", *(char *)p) == -1) exit (1);
  return s;
}

char *
str_float (void *p)
{
  char *s;
  float v;
  memcpy (&v, p, sizeof v);
  if (asprintf (&s, FFMT, v) == -1) exit (1);
  return s;
}

char *
str_double (void *p)
{
  char *s;
  double v;
  memcpy (&v, p, sizeof v);
  if (asprintf (&s, FFMT, v) == -1) exit (1);
  return s;
}

char *
str_int (void *p)
{
  char *s;
  int v;
  memcpy (&v, p, sizeof v);
  if (asprintf (&s, "%d", v) == -1) exit (1);
  return s;
}

char *
str_short (void *p)
{
  char *s;
  short v;
  memcpy (&v, p, sizeof v);
  if (asprintf (&s, "%d", v) == -1) exit (1);
  return s;
}

char *
str_long (void *p)
{
  char *s;
  long v;
  memcpy (&v, p, sizeof v);
  if (asprintf (&s, "%ld", v) == -1) exit (1);
  return s;
}

char *
str_float_star (void *p)
{
  char *s;
  float v;
  memcpy (&v, p, sizeof v);
  if (asprintf (&s, FFMT, v) == -1) exit (1);
  return s;
}

char *
str_double_star (void *p)
{
  char *s;
  double v;
  memcpy (&v, p, sizeof v);
  if (asprintf (&s, "%f", v) == -1) exit (1);
  return s;
}

char *
str_int_star (void *p)
{
  char *s;
  int v;
  memcpy (&v, p, sizeof v);
  if (asprintf (&s, "%d", v) == -1) exit (1);
  return s;
}

char *
str_FILE_star (void *p)
{
  return 0;
}

char *
str_char_star (void *p)
{
  char *s;
  char *v;
  memcpy (&v, p, sizeof v);
  if (asprintf (&s, "%"Z"u\n%s\n", strlen (v), v) == -1) exit (1);
  return s;
}

char *
str_char_string (void *p)
{
  char *s;
  
  if (asprintf (&s, "%"Z"u\n%s\n", strlen ((char *)p), (char *)p) == -1) exit (1);
  return s;
}

void
ato_char (char *valstr, void *buf, int size)
{
  char *endptr;
  int n;
  
  for (n = 0; n < size / (int) sizeof (char); n++)
    ((char *)buf)[n] = strtol (valstr, &endptr, 10), valstr = endptr;
}

void
ato_float (char *valstr, void *buf, int size)
{
  char *endptr;
  int n;
  float v;

  for (n = 0; n < size / (int) sizeof (float); n++) {
    v = strtod (valstr, &endptr), valstr = endptr;
    memcpy (buf + n * sizeof v, &v, sizeof v);
  }
}

void
ato_double (char *valstr, void *buf, int size)
{
  char *endptr;
  int n;
  double v;

  for (n = 0; n < size / (int) sizeof (double); n++) {
    v = strtod (valstr, &endptr), valstr = endptr;
    memcpy (buf + n * sizeof v, &v, sizeof v);
  }
}

void
ato_int (char *valstr, void *buf, int size)
{
  char *endptr;
  int n;
  int v;

  for (n = 0; n < size / (int) sizeof (int); n++) {
    v = strtol (valstr, &endptr, 10), valstr = endptr;
    memcpy (buf + n * sizeof v, &v, sizeof v);
  }
}

void
ato_short (char *valstr, void *buf, int size)
{
  char *endptr;
  int n;
  short v;
  for (n = 0; n < size / (int) sizeof (short); n++) {
    v = strtol (valstr, &endptr, 10), valstr = endptr;
    memcpy (buf + n * sizeof v, &v, sizeof v);
  }
}

void
ato_long (char *valstr, void *buf, int size)
{
  char *endptr;
  int n;
  long v;

  for (n = 0; n < size / (int) sizeof (long); n++) {
    v = strtol (valstr, &endptr, 10), valstr = endptr;
    memcpy (buf + n * sizeof v, &v, sizeof v);
  }
}

void
ato_FILE_star (char *valstr, void *ptr, int size)
{
}

void
ato_float_star (char *valstr, void *ptr, int size)
{
  char *endptr;
  int n;
  float v;
  void *p;

  memcpy (&p, ptr, sizeof p);
  size = malloc_usable_size (p);
  for (n = 0; n < size / (int) sizeof (float); n++) {
    v = strtod (valstr, &endptr), valstr = endptr;
    memcpy (p + n * sizeof v, &v, sizeof v);
  }
}

void
ato_int_star (char *valstr, void *ptr, int size)
{
  char *endptr;
  int n;
  int v;
  void *p;

  memcpy (&p, ptr, sizeof p);
  
  size = malloc_usable_size (p);
  for (n = 0; n < size / (int) sizeof (int); n++) {
    v = strtol (valstr, &endptr, 10), valstr = endptr;
    memcpy (p + n * sizeof v, &v, sizeof v);
  }
}

void
ato_char_star (char *valstr, void *ptr, int size)
{
  void *p;
  if (valstr == 0) {
    memset (ptr, 0, sizeof (char *));
    return;
  }
  size = strlen (valstr) + 1;
  memcpy (&p, ptr, sizeof p);
  TREALLOC (p, size);
  memcpy (ptr, &p, sizeof p);

  //  size = malloc_usable_size (p);
  if (size) {
    strncpy (p, valstr, size);
    ((char *)p)[size-1] = 0;
  }
}

void
ato_char_string (char *valstr, void *ptr, int size)
{
  if (size) {
    strncpy (((char *)ptr), valstr, size);
    ((char *)ptr)[size-1] = 0;
  }
}

typedef struct
{
  void *base;
  StructInfo *v;
} TagInfo;

void
print_member (FILE *f, char *struct_name, char *member, void *struc)
{
  char *varname;
  char *valstr;
  StructInfo *v;

  if (asprintf (&varname, "%s.%s", struct_name, member) == -1) exit (1);
  v = struct_info_fn (varname, strlen (varname));
  free (varname);
  if (v == 0) return;
  fprintf (f, "%s %s\n", member, valstr = v->str (struc + v->offset));
  free (valstr);
}

static char *white = " \r\n\t";

static TagInfo *
tag (char *number_string, StructInfo *v, void *struct_ptr)
{
  static int tag_info_count;
  static TagInfo* tag_info;
  int old_count = tag_info_count;
  int index = atoi (number_string + 1);

  if (index < 0)
    return 0;
  if (v == 0)
    return index < tag_info_count ? tag_info + index : 0;
  if (index + 1 > tag_info_count)
    TREALLOC (tag_info, tag_info_count = (index + 1) * 2);
  memset (tag_info + old_count, 0, (tag_info_count - old_count) * sizeof (tag_info));
  tag_info[index].base = struct_ptr + v->offset;
  tag_info[index].v = v;
  return 0;
}

static void
tagref (char *tag_string, char *ref_string, StructInfo *v, void *struct_ptr)
{
  char *index_string = strtok (0, white);
  int tag_index = atoi (tag_string + 1);
  int index = index_string ? atoi (index_string) : 0;
  TagInfo *ti = tag (tag_string, 0, 0);
  void *base;
  int direct = tag_string[0] == 'd';
  int indirect = tag_string[0] == 'i';
  int ptr_index = 0;

  if (!ti) {
    fprintf (stdout, "lookup of tagref %s %s failed\n", v->name, ref_string);
    DIE;
  }
  ti += tag_index;
  if (!v->ptr) {
    fprintf (stdout, "%s is not a pointer, tagref %s %s ignored\n", v->name, ref_string, index_string);
    return;
  }
  if (indirect && !ti->v->ptr) {
    memset (struct_ptr + v->offset, 0, sizeof (void *));
    fprintf (stdout, "tagref %s %s is indirect, but the target is not a pointer.\nSet to 0.\n", 
      v->name, ref_string);
    return;
  }
  if ((direct && ti->v->ptr && v->size != sizeof (void *))
      || (!(direct && ti->v->ptr) && v->size == ti->v->size)) {
    index = 0;
    fprintf (stdout, "tagref %s %s size does not match size of target.\nUsing index 0.\n", 
      v->name, ref_string);
  }
  if (index >= ti->v->count) {
    index = 0;
    fprintf (stdout, "tagref %s %s index too large, using index 0.\n", 
      v->name, ref_string);
  }

  if (indirect && ti->v->count > 1) {
    char *index_string_2 = strtok (0, white);
    if (index_string == 0) {
      fprintf (stdout, "tagref %s %s is indirect and count > 1, but there is no ptr_index\n", 
        v->name, ref_string);
      DIE;
    }
    ptr_index = index;
    index = atoi (index_string_2);
  }

  if (direct)
    base = ti->base;
  else
    memcpy (&base, ti->base + ptr_index * sizeof (void *), sizeof base);


  if (indirect && malloc_usable_size (base) < (index + 1) * v->size) {
    index = 0;
    fprintf (stdout, "tagref %s %s index too large, using index 0.\n", 
      v->name, ref_string);
  }
  memcpy (struct_ptr + v->offset, ti->base + v->size * index, sizeof (void *));
}

static int tag_info_count;
static int tag_info_alloc;
static TagInfo* tag_info;

static int
new_tag (StructInfo *v, void *struct_ptr)
{
  int index = tag_info_count++;

  if (tag_info_count > tag_info_alloc) {
    TREALLOC (tag_info, tag_info_alloc = tag_info_count * 2);
    memset (tag_info + tag_info_count, 0, (tag_info_alloc - tag_info_count) * sizeof (TagInfo));
  }
  tag_info[index].base = struct_ptr + v->offset;
  tag_info[index].v = v;
  return index;
}

typedef struct
{
  void *first;
  void *last;
  int tag_index;
  int ptr_index;
} TagRange;

static int tr_cmp_up (const void *a, const void *b)
{
  const TagRange *key = a;
  const TagRange *member = b;
  return key->first < member->first ? -1 : (key->last > member->last); /* ascending */
}

static TagRange *tag_range, *tag_range_p;
static int tag_range_alloc, tag_range_p_alloc;
static int tag_range_count, tag_range_p_count;

static void
make_tagrange_table (void)
{
  int tn, rn, rnp;;
  int n;

  TMALLOC (tag_range, tag_range_alloc = tag_info_count);
  TMALLOC (tag_range_p, tag_range_p_alloc = tag_info_count);
  rnp = rn = 0;
  for (tn = 0; tn < tag_info_count; tn++) {
    TagInfo *ti = tag_info + tn;
    int size = ti->v->ptr ? sizeof (void *) : ti->v->size;
    if (rn + 1 > tag_range_alloc)
      TREALLOC (tag_range, tag_range_alloc *= 2);
    if (rnp + ti->v->count > tag_range_p_alloc)
      TREALLOC (tag_range, tag_range_p_alloc *= 2);
    tag_range[rn].first = ti->base;
    tag_range[rn].last = tag_range[rn].first + (ti->v->count * size) - 1;
    tag_range[rn].tag_index = tn;
    tag_range[rn].ptr_index = 0;
    rn++;
    for (n = 0; n < ti->v->count; n++) {
      memcpy (&tag_range_p[rnp].first, ti->base + n * sizeof (void *), sizeof (void *));
      if (tag_range_p[rnp].first != 0) {
 size = malloc_usable_size (tag_range_p[rnp].first);
 tag_range_p[rnp].last = tag_range_p[rnp].first + size - 1; 
 tag_range_p[rnp].tag_index = tn;
 tag_range_p[rnp].ptr_index = n;
 rnp++;
      }
    }
  }
  qsort (tag_range, tag_range_count = rn, sizeof *tag_range, (int(*)(const void*,const void*))tr_cmp_up);
  qsort (tag_range_p, tag_range_p_count = rnp, sizeof *tag_range_p, (int(*)(const void*,const void*))tr_cmp_up);
  for (n = 1; n < tag_range_count; n++)
    tag_range[n].first > tag_range[n-1].last || DIE;
  for (n = 1; n < tag_range_p_count; n++)
    tag_range_p[n].first > tag_range_p[n-1].last || DIE;
}

static void
tag_lookup (FILE *f, StructInfo *v, void **ptrp, char *ref_index_string)
{
  void *ptr = *ptrp;
  TagRange *tr, trkey;
  int index;
  TagInfo *ti = 0;
  int found = 0;

  v->ptr || DIE;
  trkey.first = ptr;
  trkey.last = ptr + v->size - 1;

  if (v->direct) {
    (tr = bsearch (&trkey, tag_range, tag_range_count, sizeof *tag_range, tr_cmp_up)) || DIE;
    ti = tag_info + tr->tag_index;
    (ti->v->ptr && v->size == sizeof (void)) || (!ti->v->ptr && v->size == ti->v->size) || DIE;
    (ptr - ti->base) % ti->v->size == 0 || DIE;
    index = (ptr - ti->base) / ti->v->size;
    index < ti->v->count || DIE;
    fprintf (f, " d%d%s %d\n", tr->tag_index, ref_index_string, index);
    found = 1;
  }
  else if (v->indirect) {
    (tr = bsearch (&trkey, tag_range_p, tag_range_p_count, sizeof *tag_range_p, tr_cmp_up)) || DIE;
    ti = tag_info + tr->tag_index;
    ti->v->ptr || DIE;
    v->size == ti->v->size || DIE;
    index = (ptr - tr->first) / ti->v->size;
    if (ti->v->count > 1)
      fprintf (f, " i%d%s %d %d\n", tr->tag_index, ref_index_string, tr->ptr_index, index);
    else
      fprintf (f, " i%d%s %d\n", tr->tag_index, ref_index_string, index);
    found++;
  }
  v->indirect || v->direct || DIE;
  if (!found) {
    fflush (f);
    DIE;
  }
}

int save_all, save_state, save_init;
#define SAVE (save_all || (save_state && v->state) || (save_init && v->init))
#define TAGSAVE (!just_refs && SAVE)
#define REFSAVE (just_refs && SAVE)
#define SKIP (v->skip && !v->init)

static void
print_array (StructInfo *v, void *struct_ptr, int dimidx, int offset_count, FILE *f, char *prefix)
{
  int n;

  if (CHAT)
     if (strcmp (v->name, "S_NODE.s_eq_potential") == 0)
     {printf ("print_array dimidx %d v->count %d, v->size %d\n", dimidx, v->count, v->size); fflush(stdout);}
  if (v->dim == 0 || v->dim[dimidx] == 0) {
    void *offset = struct_ptr + v->offset + offset_count * v->count * v->size;
    if (v->size == 1 && v->string) {
      char *s = (char *)(offset);
      fprintf (f, "%s %"Z"u\n%s\n", prefix, strlen (s), s);
    }
    else {
      fprintf (f, "%s", prefix);
      for (n = 0; n < v->count; n++) {
 char *valstr;
 fprintf (f, " %s", valstr = v->str (offset + n * v->size));
 free (valstr);
      }
      fprintf (f, "\n");
    }
    return;
  }
  for (n = 0; n < v->dim[dimidx]; n++) {
    char *s;
    if (asprintf (&s, "%s %d", prefix, n) == -1) exit (1);
    print_array (v, struct_ptr, dimidx + 1, offset_count * v->dim[dimidx] + n, f, s);
    free (s);
  }
}

static int level;


static void
do_save_struct (FILE *f, char *struct_name, void *struct_ptr, int just_refs, void *parent)
{
  StructMembers *m;
  int n;
  char* member_name = 0;
  int *count = 0, count_count = 0;

  if (CHAT) {printf ("do_save_struct: saving %s, level %d, address %08lx\n",
   struct_name, level, (unsigned long)struct_ptr); fflush(stdout);}
  level++;

  m = struct_members_fn (struct_name, strlen (struct_name));
  if (m->choose)
    member_name = m->choose (parent);
  for (n = 0; (m->choose && member_name && n == 0) || (!m->choose && (member_name = m->member[n])); n++) {
    static char *varname;
    char *valstr;
    StructInfo *v;
    int n, ptr_n, len;

    if (CHAT) {printf ("member %s\n", member_name);fflush(stdout);}
    free (varname);
    if (asprintf (&varname, "%s.%s", struct_name, member_name) == -1) exit (1);
    v = struct_info_fn (varname, len = strlen (varname));
    if (v == 0)
      continue;
    if (TAGSAVE && v->ptr && v->tag) {
      fprintf (f, "%s t%d\n", member_name, new_tag (v, struct_ptr));
      fflush (f);
    }
    if (v->ptr && v->size == 1 && !v->direct && !v->indirect) {
      if (TAGSAVE) {
 char *s;
 memcpy (&s, struct_ptr + v->offset, sizeof (char *));
 if (s == 0)
   fprintf (f, "%s %d\n", member_name, -1); /* # */
 else
   fprintf (f, "%s %"Z"u\n%s\n", member_name, strlen (s), s); /* # */
      }
    }
    else if (v->ptr && (v->direct || v->indirect)) {
      if (REFSAVE) {
 if (v->count == 1) {
   fprintf (f, "%s", member_name);
   //   fprintf (f, "tag_lookup %d", (unsigned)(struct_ptr + v->offset));
   tag_lookup (f, v, struct_ptr + v->offset, "");
 }
 else for (n = 0; n < v->count; n++) {
   char *s;
   fprintf (f, "%s", member_name);
   if (asprintf (&s, " %d", n) == -1) exit (1); /* [] */
   tag_lookup (f, v, struct_ptr + v->offset + n * sizeof (void *), s);
   free (s);
 }
      }
    }      
    else if (!v->ptr && !v->struct_name) {
      if (v->size == sizeof (int) && strcmp (varname + len - 6, "_count") == 0)
 count = struct_ptr + v->offset, count_count = v->count;
      if (TAGSAVE)
 print_array (v, struct_ptr, 0, 0, f, member_name);
    }
    else if (!v->ptr && v->struct_name) {
      if (!SKIP) 
 for (n = 0; n < v->count; n++) {
   fprintf (f, "%s %d\n", member_name, n); /* [] */
   do_save_struct (f, v->struct_name, struct_ptr + v->offset + n * v->size, just_refs, struct_ptr);
 }
      count = 0;
    }
    else if (v->ptr && !v->struct_name) {
      (count && count_count == v->count) || DIE;
      if (TAGSAVE) 
 for (ptr_n = 0; ptr_n < v->count; ptr_n++) {
   fprintf (f, "%s", member_name);
   if (v->count > 1)
     fprintf (f, " %d", ptr_n); /* [] */
   for (n = 0; n < count[ptr_n]; n++) {
     void *t;
     memcpy (&t, struct_ptr + v->offset + ptr_n * sizeof (void *), sizeof (void*));
     fprintf (f, " %s", valstr = v->str (t + n * v->size));
     free (valstr);
   }
   fprintf (f, "\n");
 }
      count = 0;
    }
    else if (v->ptr && v->struct_name) {
      (count && count_count == v->count) || DIE;
      if (!SKIP)
 for (ptr_n = 0; ptr_n < v->count; ptr_n++) {
   for (n = 0; n < count[ptr_n]; n++) {
     void **array_ptr = struct_ptr + v->offset + ptr_n * sizeof (void *);
     if (*array_ptr != 0) {
       void *substruct_ptr = *array_ptr + n * v->size;
       if (array_ptr != 0) {
  fprintf (f, "%s", member_name);
  if (v->count > 1)
    fprintf (f, " %d", ptr_n); /* [] */
  fprintf (f, " %d\n", n); /* [] */
  do_save_struct (f, v->struct_name, substruct_ptr , just_refs, struct_ptr);
       }
     }
   }
 }
      count = 0;
    }    
    //    if (TAGSAVE && strcmp (member_name, "fiberpop_count") == 0 ) exit (0);
  }
  fprintf (f, "\n");
  level--;
}

void
save_struct (FILE *f, char *struct_name, void *struct_ptr)
{
  do_save_struct (f, struct_name, struct_ptr, 0, 0);
  make_tagrange_table ();
  do_save_struct (f, struct_name, struct_ptr, 1, 0);
  free (tag_range); tag_range = 0; tag_range_count = 0; tag_range_alloc = 0;
  free (tag_range_p); tag_range_p = 0; tag_range_p_count = 0; tag_range_p_alloc = 0;
  free (tag_info); tag_info = 0; tag_info_count = 0; tag_info_alloc = 0;
}

static char *
get_multiline (FILE *f)
{
  static char *s = 0;
  static size_t slen = 0;
  static char *buf;
  static int buflen;
  int len, space_left;
  char *p;

  p = buf;
  if ((space_left = buflen) == 0)
    TCALLOC (buf, space_left = buflen = 4096);

  while ((len = getline (&s, &slen, f)) > 0) {
    if (len == 0 || len == strspn (s, white))
      return buf;
    if (len + 1 > space_left)
      TREALLOC (buf, buflen *= 2);
    strcpy (p, s);
    p += len;
    space_left -= len;
  }
  return buf;
}

static inline void
maybe_array_size (char *varname, StructInfo *v, char *number_string, void *struct_ptr)
{
  int len = strlen (varname);
  char *ptrname;
  StructInfo *vp;
  if (CHAT) {printf("%s   %s\n",varname,number_string); fflush(stdout);}
  if (v->size != sizeof (int) || v->count != 1 || v->struct_name != 0
      || len < 6 || strcmp (varname + len - 6, "_count") != 0)
    return;
  ptrname = strdup (varname);
  ptrname[len - 6] = 0;
  if ((vp = struct_info_fn (ptrname, len - 6)) != 0 && vp->ptr) {
    int new_count, old_count, old_size, new_size, usable_size, new_start, new_usable_size;
    void *t;
    new_count = atoi (number_string);
    memcpy (&old_count, struct_ptr + v->offset, sizeof old_count);
    old_size = old_count * vp->size;
    new_size = new_count * vp->size;
    memcpy (&t, struct_ptr + vp->offset, sizeof t);
    usable_size = malloc_usable_size (t);
    new_start = MIN (old_size, usable_size);
    new_usable_size = usable_size;

    if (usable_size < new_size) {
      void *t;
      memcpy (&t, struct_ptr + vp->offset, sizeof t);
      TREALLOC (t, new_size);
      memcpy (struct_ptr + vp->offset, &t, sizeof t);
      new_usable_size = malloc_usable_size (t);
    }
    if (new_start < new_usable_size) {
      void *t;
      memcpy (&t, struct_ptr + vp->offset, sizeof t);
      memset (t + new_start, 0, new_usable_size - new_start);
    }
  }
  free (ptrname);
}

static char *
get_string (FILE *f, int count)
{
  static char *buf;
  static int buflen;

  int txt;
  if (count == -1)
    return 0;
  if (buflen < count + 1)
    TREALLOC (buf, buflen = count + 1);
  for (int byte=0; byte < count+1; ++byte)
  {
     txt = fgetc(f);
     if (txt == '\r') // if a dos file, this is not included in count
     {
        --byte;
        continue;
     }
     buf[byte] = txt;
  }
  buf[count] = 0;  // overwrite '\n'
  return buf;
#if 0
// was this, but the count is too small because \r is not counted
  if (count == -1)
    return 0;
  if (buflen < count + 1)
    TREALLOC (buf, buflen = count + 1);
  if (fread (buf, 1, count + 1, f) != count + 1) {
    fprintf (stdout, "string count %d is too big\n", count);
    exit (1);
  }
  buf[count] = 0;
  return buf;
#endif
}

static int
get_offset (StructInfo *v, int n, int *sizep, char **number_string_ptr)
{
  int size, offset, index;
  if (v->dim == 0 || v->dim[n] == 0) {
    *sizep = v->size * v->count;
    return 0;
  }
  index = strtol (*number_string_ptr, number_string_ptr, 10);
  offset = get_offset (v, n + 1, &size, number_string_ptr);
  *sizep = size * v->dim[n];
  return offset + index * size;
}

void
load_struct (FILE *f, char *struct_name, void *struct_ptr, int ptr_valid)
{
  static char *s = 0;
  static size_t slen = 0;
  struct StructInfo *v;
  char *varname, *number_string, *member_name;
  int len;

  while ((len = getline (&s, &slen, f)) > 0) {
    int ptr_index = 0;
    if ((member_name = strtok (s, white)) == 0)
      return;
    number_string = member_name + strlen (member_name) + 1;
    if (CHAT) {printf("%s %s\n",struct_name,member_name); fflush(stdout);}
    number_string < s + len || DIE;
    if (number_string[0] == '&')
      number_string = get_multiline (f);
    if (asprintf (&varname, "%s.%s", struct_name, member_name) == -1) exit (1);
    v = struct_info_fn (varname, strlen (varname));
    if (v == 0) {
      fprintf (stdout, "load_struct: %s not found in hash\n", varname);
      continue;
    }
    if (ptr_valid)
      maybe_array_size (varname, v, number_string, struct_ptr);
    free (varname);
    if (strchr ("t", number_string[0]) && ptr_valid) {
      tag (number_string, v, struct_ptr);
      continue;
    }
    if (strchr ("di", number_string[0]) && ptr_valid) {
      char *ref_string = strdup (number_string);
      char *tag_string = strtok (number_string, white);
      char *ref_index_string;
      int ref_index = 0;
      if (v->count > 1) {
 ref_index_string = strtok (0, white);
 if (ref_index_string == 0) {
   fprintf (stdout, "tagref %s %s has no indexes\n", v->name, ref_string);
   DIE;
 }
 ref_index = atoi (ref_index_string);
      }
      tagref (tag_string, ref_string, v, struct_ptr + ref_index * sizeof (void *));
      free (ref_string);
      continue;
    }
    ptr_index = 0;
    if (v->ptr && v->size > 1 && v->count > 1)
      ptr_index = strtol (number_string, &number_string, 10);
    if (v->struct_name) {
      int this_ptr_valid = ptr_valid;
      int struct_index = atoi (number_string);
      void *this_struct_ptr = 0;
      if (ptr_valid) {
 if (v->ptr) { /* member points to a struct */
   void *first_struct_ptr;
   memcpy (&first_struct_ptr, struct_ptr + v->offset + ptr_index * sizeof (void *), sizeof (void *));
   this_struct_ptr = first_struct_ptr + struct_index * v->size;
   this_ptr_valid = malloc_usable_size (first_struct_ptr) >= (struct_index + 1) * v->size;
 }
 else {   /* member is a struct */
   this_struct_ptr = struct_ptr + v->offset + struct_index * v->size;
   this_ptr_valid = struct_index >= 0 && struct_index < v->count;
 }
      }
      if (this_ptr_valid)
        memset (this_struct_ptr, 0, v->size);
      load_struct (f, v->struct_name, this_struct_ptr, this_ptr_valid);
    }
    else if (ptr_valid) {
      int size, aoffset;
      aoffset = get_offset (v, 0, &size, &number_string);
      if (v->string)
 number_string = get_string (f, atoi (number_string));
      v->val (number_string, struct_ptr + v->offset + aoffset + ptr_index * sizeof (void *), v->size * v->count);
    }
  }
}


// the inode_global struct does not have a field for the file version, it
// is the first line of text in the output file.
FILE *
save_struct_open (char *filename)
{
  FILE *f;

  if ((f = fopen (filename, "w")) == 0)
    return 0;
  fprintf (f, "file_format_version %d\n", FILEIO_FORMAT_VERSION6);
  return f;
}

static char *load_struct_filename = "the .sim file";


// the simulator engine uses the next two functions
int
load_struct_read_version (FILE *f)
{
   int format_version = 0;

   fscanf(f, "file_format_version %d\n", &format_version);
   Version = format_version;
   if (format_version != FILEIO_FORMAT_VERSION6) {
      fprintf (stdout, "The format of %s is an earlier version which\nthe simbuild package does not support.\n"
        "Use the newsned package to use this file,\nor run simbuild to create a new version of this file.\nThis file will be ignored. Exiting. . .\n",
        load_struct_filename);
    return 0;
   }
   return 1;
}

FILE *
load_struct_open (char *filename)
{
   FILE *f;
   int version_ok;
   if ((f = fopen (filename, "r")) == 0)
      return 0;
   load_struct_filename = filename;
   version_ok = load_struct_read_version (f);
   load_struct_filename = "the .sim file";
   if (version_ok)
      return f;
   fclose (f);
   return 0;
}

// simbuild will read old files, simrun will not, so each gets a custom version of these
int
load_struct_read_version_simbuild (FILE *f)
{
   int format_version;

   if (fscanf (f, "file_format_version %d\n", &format_version) != 1)
      return 0;
   if (format_version != FILEIO_FORMAT_VERSION5 && format_version != FILEIO_FORMAT_VERSION6) {
      fprintf (stdout, "The format version of %s (%d) does not match (it should be %d or %d)\n"
        "The file will be ignored\n",
        load_struct_filename, format_version, FILEIO_FORMAT_VERSION5,FILEIO_FORMAT_VERSION6);
    return 0;
   }
   Version = format_version;
   return 1;
}

FILE *
load_struct_open_simbuild (char *filename)
{
   FILE *f;
   int version_ok;

   if ((f = fopen (filename, "r")) == 0)
      return 0;
   load_struct_filename = filename;
   version_ok = load_struct_read_version_simbuild(f);
   load_struct_filename = "the .sim file";
   if (version_ok)
      return f;
   fclose (f);
   return 0;
}
