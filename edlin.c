
#include "edlin.h"

/*HELP edlin
Q
    quits edlin without saving.
E
    saves the file and quits edlin.
W
    (write) saves the file.
L
    lists the contents (e.g., 1,6L lists lines 1 through
    6). Each line is displayed with a line number in front
    of it.
I
    Inserts lines of text. (end with line that starts with
    "/" )
D
    deletes the specified line, again optionally starting
    with the number of a line, or a range of lines. E.g.:
    2,4d deletes lines 2 through 4. In the above example,
    line 7 was deleted.
P
    displays a listing of a range of lines. If no range
    is specified, P displays the complete file from the *
    to the end. This is different from L in that P changes
    the current line to be the last line in the range.

R
    is used to replace all occurrences of a piece of
    text in a given range of lines, for example, to
    replace a spelling error. Including the ? prompts
    for each change. E.g.: To replace 'prit' with 'print'
    and to prompt for each change: ?rprit^Zprint (the ^Z
    represents pressing CTRL-Z). It is case-sensitive.
S
    searches for given text. It is used in the same way as
    replace, but without the replacement text. A search
    for 'apple' in the first 20 lines of a file is typed
    1,20?sapple (no space, unless that is part of the
    search) followed by a press of enter. For each match,
    it asks if it is the correct one, and accepts n or y
    (or Enter).
T
    transfers another file into the one being edited, with
    this syntax: [line to insert at]t[full path to file].

*/
