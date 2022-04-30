#include "lib_text.h"

/* Help for quit */
static char *lines_quit[] = {
    "&T/quit [Reason]",
    "Logout and leave the server",
    "Aliases: /rq",
    0};

/* Help for rq */
static char *lines_rq[] = {
    "&T/rq",
    "Logout with RAGEQUIT!!",
    0};

/* Help for crash */
static char *lines_crash[] = {
    "&T/crash",
    "Crash the server &T/crash 666&S really do it!",
    0};

/* Help for commands,cmds,cmdlist */
static char *lines_commands[] = {
    "&T/commands",
    "List all available commands",
    "Aliases: /cmds /cmdlist",
    0};

/* Help for reload */
static char *lines_reload[] = {
    "&T/reload [all]",
    "&T/reload&S -- Reloads your session",
    "&T/reload all&S -- Reloads everybody on this level",
    0};

/* Help for chars */
static char *lines_chars[] = {
    "___0123456789ABCDEF0123456789ABCDEF_",
    "0:|␀☺☻♥♦♣♠•◘○◙♂♀♪♫☼▶◀↕‼¶§▬↨↑↓→←∟↔▲▼|",
    "1:| !\"#$%&'()*+,-./0123456789:;<=>?|",
    "2:|@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_|",
    "3:|`abcdefghijklmnopqrstuvwxyz{|}~⌂|",
    "4:|ÇüéâäàåçêëèïîìÄÅÉæÆôöòûùÿÖÜ¢£¥₧ƒ|",
    "5:|áíóúñÑªº¿⌐¬½¼¡«»░▒▓│┤╡╢╖╕╣║╗╝╜╛┐|",
    "6:|└┴┬├─┼╞╟╚╔╩╦╠═╬╧╨╤╥╙╘╒╓╫╪┘┌█▄▌▐▀|",
    "7:|αßΓπΣσµτΦΘΩδ∞φε∩≡±≥≤⌠⌡÷≈°∙·√ⁿ²■ |",
    0};

/* Help for edlin */
static char *lines_edlin[] = {
    "Q",
    "    quits edlin without saving.",
    "E",
    "    saves the file and quits edlin.",
    "W",
    "    (write) saves the file.",
    "L",
    "    lists the contents (e.g., 1,6L lists lines 1 through",
    "    6). Each line is displayed with a line number in front",
    "    of it.",
    "I",
    "    Inserts lines of text. (end with line that starts with",
    "    \"/\" )",
    "D",
    "    deletes the specified line, again optionally starting",
    "    with the number of a line, or a range of lines. E.g.:",
    "    2,4d deletes lines 2 through 4. In the above example,",
    "    line 7 was deleted.",
    "P",
    "    displays a listing of a range of lines. If no range",
    "    is specified, P displays the complete file from the *",
    "    to the end. This is different from L in that P changes",
    "    the current line to be the last line in the range.",
    "",
    "R",
    "    is used to replace all occurrences of a piece of",
    "    text in a given range of lines, for example, to",
    "    replace a spelling error. Including the ? prompts",
    "    for each change. E.g.: To replace 'prit' with 'print'",
    "    and to prompt for each change: ?rprit^Zprint (the ^Z",
    "    represents pressing CTRL-Z). It is case-sensitive.",
    "S",
    "    searches for given text. It is used in the same way as",
    "    replace, but without the replacement text. A search",
    "    for 'apple' in the first 20 lines of a file is typed",
    "    1,20?sapple (no space, unless that is part of the",
    "    search) followed by a press of enter. For each match,",
    "    it asks if it is the correct one, and accepts n or y",
    "    (or Enter).",
    "T",
    "    transfers another file into the one being edited, with",
    "    this syntax: [line to insert at]t[full path to file].",
    "",
    0};

/* Help for help */
static char *lines_help[] = {
    "Commands can be listed with &T/commands",
    "To see help for a command type &T/help CommandName",
    "Also see /faq, /news, /view and /rules",
    0};

/* Help for other */
static char *lines_other[] = {
    "Other help files: chars, inifile, edlin",
    0};

/* Help for view */
static char *lines_view[] = {
    "The view command show special files for this system,",
    "none are currently installed.",
    0};

/* Help for faq */
static char *lines_faq[] = {
    "&cFAQ&f:",
    "&fExample: What does this server run on? This server runs on MCCHost",
    0};

/* Help for news */
static char *lines_news[] = {
    "No news today.",
    0};

/* Help for rules */
static char *lines_rules[] = {
    "No rules found.",
    0};

/* Help for inifile */
static char *lines_inifile[] = {
    "Empty lines are ignored.",
    "Section syntax is normal [...]",
    "    Section syntax uses \".\" as element seperator within name.",
    "    Elements may be numeric for array index.",
    "    Other elements are identifiers.",
    "",
    "Comment lines have a \"#\" or \";\" at the start of the line",
    "Labels are followed by \"=\" or \":\"",
    "Spaces and tabs before \"=\", \":\", \";\", \"#\" are ignored.",
    "",
    "Value is trimmed at both ends for spaces and tabs.",
    "",
    "Quotes are not special.",
    "Backslashes are not special",
    0};

/* Help for Welcome */
static char *lines_Welcome[] = {
    "",0};

/* Help for place,pl */
static char *lines_place[] = {
    "&T/place b [x y z] [X Y Z]",
    "Places the Block numbered &Tb&S at your feet or at &T[x y z]&S",
    "With both &T[x y z]&S and &T[X Y Z]&S it places a",
    "cuboid between those points.",
    "Alias: &T/pl",
    0};

help_text_t helptext[] = {
    { "quit", 0, lines_quit },
    { "rq", H_CMD, lines_rq },
    { "crash", H_CMD, lines_crash },
    { "commands", 0, lines_commands },
    { "cmds", 0, lines_commands },
    { "cmdlist", 0, lines_commands },
    { "reload", H_CMD, lines_reload },
    { "chars", 0, lines_chars },
    { "edlin", 0, lines_edlin },
    { "help", H_CMD, lines_help },
    { "other", 0, lines_other },
    { "view", H_CMD, lines_view },
    { "faq", H_CMD, lines_faq },
    { "news", H_CMD, lines_news },
    { "rules", H_CMD, lines_rules },
    { "inifile", 0, lines_inifile },
    { "Welcome", 0, lines_Welcome },
    { "place", H_CMD, lines_place },
    { "pl", H_CMD, lines_place },
    {0,0,0}
};
