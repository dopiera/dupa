# dupa(1) - duplicate analyzer

<pre><code><b>dupa</b> [<i>OPTION</i>]... <i>DIR1</i> [<i>DIR2</i>]</code></pre>

# Description

**dupa**
helps in identifying duplicate files, similar directories or finding differences
between directories. Only non-empty regular files and directories are
considered. Everything else is ignored.

If *DIR2* is not specified,
**dupa**
will analyze *DIR1* in search of duplicate files or subdirectories
similar to each other.

If *DIR2* is specified,
**dupa**
will do a comparison between the 2 specified directories.

## Duplicate detection

If you don't specify *DIR2*, 
**dupa**
will detect duplicates in *DIR1*.
The output will produce 2 parts.

In the first part each line contains space-separated list of directories or
files which are similar to each other. The lines are sorted by how big the
duplicate files or directories are. Depending on the **--use_size** setting
this is either total sum of sizes of files in a directory or the number of
files.

The second part contains a list of directories (one per line). Most files in
each of those directories have duplicates scattered among other directories.

If you chose to dump the results to a database (by using **--sql_out** option), 2
tables will be created: 
**Node and**
**EqClass.**

**Node**
represents files and directories. It has the following fields:

* id  
  unique identifier of this file
* name  
  basename of this file's path
* path  
  the file's path
* type  
  either "FILE" or "DIR" indicting whether it's a regular file or directory
* cksum  
  checksum of the file
* unique_fraction  
  a measure of how unique the contents of this directory are; it is a number
  between 0 (all of the files are duplicated elsewhere) and 1 (the files
  in this directory have no duplicates outside of it)
* eq_class  
  identifier of the equivalence class in 
  **EqClass**
  table.

**EqClass**
represents an equivalence class of files or directories. If directories
fall into the same equivalence class they are considered similar. It has the
following fields:

* id  
  unique identifier of the equivalence class
* nodes  
  number of files or directories belonging to this equivalence class
* weight  
  average number of files in directories belonging to this class if
  **--use_size** is unset or average size of directories otherwise
* interesting  
  it is set to 1 if at least one of the classes elements' parent directories is
  not a duplicate of something else; usually looking at classes where it is 0
  makes less sense than their parent directories

Usually it makes most sense to join these two tables on
**Node.eq_class == EqClass.id.**
For examples on how to use them, please take a look at the
.SM
**EXAMPLES**
section.


## Directory comparison

Directory comparison is straight-forward. Specify 2 directories and
**dupa**
will generate a report on the differences. The report lists actions that could
have led to transforming *DIR1* into *DIR2*

* OVERWRITTEN_BY *f* CANDIDATES: *candidates*...  
  *f* existed and was overwritten by one of the *candidates*; all
  *candidates* are identical
* COPIED_FROM: *f* CANDIDATES: *candidates*...  
  *f* didn't exist and now is a copy of one of the *candidates*;
  all *candidates* are identical
* RENAME_TO: *f* CANDIDATES: *candidates*...  
  *f* was renamed to one of the *candidates*; all
  *candidates* are identical
* CONTENT_CHANGED: *f*  
  content of *f* was changed to something unseen in *DIR1*
* REMOVED: *f*  
  *f* was removed
* NEW_FILE: *f*  
  *f* was created and its content had not been seen in *DIR1*

If you chose to dump the results to a database (by using **--sql_out**
option), 6 tables will be created: Removed, NewFile, ContentChanged,
OverwrittenBy, CopiedFrom, RenameTo. Their content is analogous to what is
printed, hence is should be self-explanatory.

# Options

*DIR1* and *DIR2* can either be paths to directories on a
filesystem, or paths to SQLite3 databases preceded by a 'db:' prefix. To treat
arguments starting with 'db:' as filesystem paths rather than databases, please
use the **-r** option. The databases format is described in
.SM
**DESCRIPTION**
section.

Mandatory arguments to long options are mandatory for short options too.

* **-h**, **--help**  
  produce help message and exit
* **-c**, **--read_cache_from**=*ARG*  
  path to the file from which to read checksum cache; it will be used if the file
  name, size and mtime match; mind that the cache doesn't have the names in any
  canonical form, so if you call
  **dupa**
  in a different working directory, than at the time of producing the cache, the
  cache will be useless
* **-C**, **--dump_cache_to**=*ARG*  
  path to which to dump the checksum cache; such a cache can be used in further
  invocations to avoid recalculating checksums of all the files or to even act a
  list of files
* **-o**, **--sql_out**=*ARG*  
  if set, path to where SQLite3 results will be dumped, refer to
  .SM
  **DESCRIPTION**
  section for how it looks like
* **-1**, **--cache_only**  
  only generate checksums cache; this option only makes sense if **-C** is
  specified too and *DIR2* is not specified; it will scan the directory,
  dump cache and not analyze the data; this is useful if you want to just generate
  a list of files for further use
* **-s**, **--use_size**  
  use file size rather than number of files as a measure of directory sizes; refer
  to
  .SM
  **INTERNALS**
  section to learn what it does specifically
* **-r**, **--ignore_db_prefix**  
  when parsing *DIR1* or *DIR2* positional arguments treat them as
  directory paths even if they start with a "db:" prefix
* **-w**, **--skip_renames**  
  when comparing directories, don't print renames; this is useful if you're
  comparing mostly similar directories with large numbers of files differently
  named files
* **-v**, **--verbose**  
  be verbose
* **-j**, **--concurrency**=*ARG*  
  number of concurrently computed checksums (4 by default)
* **-t**, **--tolerable_diff_pct**=*ARG*  
  directories different by this percent or less will be considered duplicates (20
  by default); refer to
  .SM
  **INTERNALS**
  section for more details

# Exit Status

Provided that the arguments were correct,
**dupa**
always returns 0.

# Consequences of Errors

Failures to read files and directories are reported on stderr and don't affect
the exit status.

# Internals

Some details were intentionally left out - consult the code for them.

The building block of
**dupa**
are SHA1 hashes. Files are considered identical iff their hashes are equal.
Potential conflicts are ignored. Empty files are ignored, and due to
implementation details so are files, whose SHA1 hash is 0.

Comparing 2 directories is straight-forward - we compute hashes of files of both
directories and we then traverse the directory trees  to print the differences.
The rest of this section covers how analyzing of a single directory works.

We split all files and directories into equivalence (or similarity) classes.
Every file, directory and equivalence class gets a "weight" assigned. Depending
on the **--use_size** setting - it is supposed to either resemble the number
of files in a directory or its total size. More specifically, equivalence class'
weight is defined as an average of all its element's weight. Every directory's
weight is defined as a sum of weights of all files in it (including
subdirectories). File's weight is either 1 or the file's size.

We build the split into equivalence classes bottom up. All empty directories
fall into a single equivalence class with weight 0. For every observed file
hash, we create an equivalence class, to which all the files with the respective
hash belong. Having classified all files and empty directories, we can classify
non-empty directories by comparing their contents.

To compare directories contents we introduce a similarity metric which we define
as the sum of weights of symmetric set difference of equivalence classes of
entries in both directories divided by the sum of weights of the set union of
equivalence classes of entries in both directories. The similarity metric
becomes 0 for identical directories and 1 for directories, whose entries share
no equivalence class.

When we're picking an equivalence class for a directory we chose the 
most similar (according to our similarity metric) directories' equivalence
class, but only if the distance is smaller than **--tolerable_diff_pct**
percent.  Otherwise, we create a new equivalence class for it.

It is not clear to the author, whether this way of assigning directories to
equivalence classes is stable, whether it depends on the order of looking at
directories, etc. but it has proven good enough in practice to not care.

We consider an equivalence class uninteresting, if all of the class' members
parent directories have duplicates. The reason is that it is more interesting
to look and those parent directories in such a case. We skip printing the
uninteresting equivalence classes.

For every directory we define a uniqueness factor. It is the proportion of the
sum of weights of its descendants (including subdirectories) which don't have
duplicates outside of the analyzed directory to the sum of weights of all its
descendants. If the uniqueness fraction is smaller than
**--tolerable_diff_pct** percent, we consider this directory to be mostly
scattered among other directories. It is computed in a brute-force manner.

# Examples

<pre><code><b>dupa -C /tmp/home_cache.sqlite3 "$HOME"</b>

</code></pre>
Detect duplicates on home directory, print a report and dump cache containing
file hashes to
**/tmp/home_cache.sqlite3.**

<pre><code><b>dupa -c /tmp/home_cache.sqlite3 "$HOME/some_subdir"</b>

</code></pre>
Detect duplicates on home subdirectory using the previously generated cache so
that it doesn't process the contents of the files there again

<pre><code><b>dupa -1 -C /tmp/some_dir.sqlite3 some_dir</b>
<b>scp /tmp/some_dir.sqlite3 other_machine:/tmp/</b>
<b>ssh other_machine dupa some_other_dir db:/tmp/some_dir.sqlite3</b>

</code></pre>
Prepare a list of files on the first machine, dump them to
**/tmp/some_dir.sqlite3,**
then copy that file to
**other_machine**
and eventually compare the contents of this database with a directory on
**other_machine.**
That way you can easily compare directories on different machines.

<pre><code><b>dupa db:/tmp/some_dir1.sqlite3 -o /tmp/analysis.sqlite3</b>

</code></pre>
Analyze contents of a previously generate database and dump the results to
**/tmp/analysis.sqlite3.**

To analyze the database generated in the previous example these SQL queries
might be useful.

<pre><code><b>"SELECT"</b>
<b>"  EqClass.weight as weight,"</b>
<b>"  GROUP_CONCAT(path, '   ') as paths"</b>
<b>"FROM"</b>
<b>"  EqClass JOIN Node ON EqClass.id = Node.eq_class"</b>
<b>"WHERE"</b>
<b>"  nodes > 1 AND interesting = 1"</b>
<b>"GROUP BY"</b>
<b>"  EqClass.id"</b>
<b>"ORDER BY weight DESC"</b>
<b>"LIMIT 10;"</b>

</code></pre>
Get 10 equivalence classes with the largest weight.

<pre><code><b>"SELECT"</b>
<b>"  path,"</b>
<b>"  EqClass.weight as num_files,"</b>
<b>"  unique_fraction"</b>
<b>"FROM"</b>
<b>"  EqClass JOIN Node ON EqClass.id = Node.eq_class"</b>
<b>"WHERE"</b>
<b>"  unique_fraction &lt; 0.2"</b>
<b>"ORDER BY weight DESC"</b>
<b>"LIMIT 10;"</b>

</code></pre>
Get 10 directories with the largest weight whose contents duplicates are
scattered outside of them:

# See Also

fdupes(1)
