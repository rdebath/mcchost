# This script is used to scan the C source files to find items to register
# in lists at compile time. This does allow specific commands to be deleted
# by removing (or renaming) their source files.
#
# Currently used for:
# "HELP" comments to convert to C strings for the help command.
#
#  CMD_XXXXX defines to construct the list of available commands.
#

BEGIN{
    flg=0;count=0;
    print "/* ⇉⇉⇉ This file was automatically generated.  Do not edit! ⇇⇇⇇ */"
    print "#include \"lib_text.h\"\n"
}

/^\/\*HELP/ {
    if (flg) save_text();
    flg = 1;
    textname=$2;
    class=$3;
    if (class == "") class = "0";
    text="";
}
/\*\// {
    if (flg) save_text();
    flg = 0;
}
/^\/\*HELP/ {next;}

/^#define\ *CMD_[A-Z0-9]*[	 ]*[\\{]/ {
    cmdlist = cmdlist "    " $2 ",\n"
}

flg==0 {next;}

{ text=text $0 "\n"; next }

function save_text() {
    gsub("\n*$", "", text);
    gsub("\t", "        ", text); # Don't use tabs!!
    gsub("\\\\", "\\\\\\\\", text); # Seriously!?
    gsub("\"", "\\\"", text);

    gsub("\n", "\",\n    \"", text);
    sub("$", "\",\n    0\n};\n", text);
    if (text == "") text = "\",0};\n";

    t1 = textname; sub(",.*", "", t1);
    t1 = "static char *lines_" t1 "[] = {"
    text = t1 "\n    \"" text

    print "/* Help for "textname" */"
    print text

    list[count] = textname;
    aclass[count] = class;
    count++;
}

END{
    print "help_text_t helptext[] = {";
    for(i=0; i<count; i++) {
	t1 = list[i]; sub(",.*", "", t1);
	c = split(list[i], a, ",");
	for(j=0; j<c; j++)
	    print "    {", "\"" a[j+1] "\",", aclass[i]",", "lines_" t1, "},"
    }
    print "    {0,0,0}"
    print "};"

    print ""
    print "#define N .name= /*STFU*/"
    print "command_t command_list[] ="
    print "{"
    print cmdlist;
    print "    {N(0)}"
    print "};"
    print "#undef N"
}
