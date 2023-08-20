# This script is used to scan the C source files to find items to register
# in lists at compile time. This does allow specific commands to be deleted
# by removing (or renaming) their source files.
#
# Currently used for:
# "HELP" comments to convert to C strings for the help command.
#
#  UCMD_XXXXX defines to construct the list of available commands.
#

BEGIN{
    flg=0;count=0;ifzero=0;
    if (files < 1) files=0;
    if (!files) {
	print "/* ⇉⇉⇉ This file was automatically generated.  Do not edit! ⇇⇇⇇ */"
	print "#include \"lib_text.h\"\n"
    }
}

/^#if 0/ { ifzero++; next; }
ifzero && /^#if/ { ifzero++; next; }
ifzero && /^#endif/ { ifzero--; next; }
ifzero { next; }

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

/^#define *UCMD_[A-Z0-9]*[	 ]*[\\{]/ {
    cmdlist = cmdlist "#ifdef " $2 "\n"
    cmdlist = cmdlist "    " $2 ",\n"
    cmdlist = cmdlist "#endif\n"
}

flg==0 {next;}

{ text=text $0 "\n"; next }

function save_text() {
    gsub("\n*$", "", text);
    gsub("\t", "        ", text); # Don't use tabs!!

    if (!files) {
	print "#ifdef UCMD_HELP"
	t1 = textname; sub(",.*", "", t1);
	t1 = "static char *lines_" t1 "[] = {"

	if (text != "") {
	    gsub("\\\\", "\\\\\\\\", text); # Seriously!?
	    gsub("\"", "\\\"", text);

	    gsub("\n", "\",\n    \"", text);
	    sub("$", "\",\n    0\n};", text);

	    text = t1 "\n    \"" text
	}

	if (text == "") text = t1 "0};";

	print "/* Help for "textname" */"
	print text
	print "#endif"
	print ""
    }

    if (files && text != "") {
	c = split(textname, a, ",");
	for(j=0; j<c; j++) {
	    print text > ("help/" a[j+1] ".txt")
	}
    }

    list[count] = textname;
    aclass[count] = class;
    count++;
}

END{
    if (!files) {
	print "#ifdef UCMD_HELP"
	print "help_text_t helptext[] = {";
	for(i=0; i<count; i++) {
	    t1 = list[i]; sub(",.*", "", t1);
	    c = split(list[i], a, ",");
	    for(j=0; j<c; j++)
		print "    {", "\"" a[j+1] "\",", aclass[i]",", "lines_" t1, "},"
	}
	print "    {0,0,0}"
	print "};"
	print "#endif"

	print ""
	print "#define N .name= /*STFU*/"
	print "command_t command_list[] ="
	print "{"
	print cmdlist;
	print "    {N(0)}"
	print "};"
	print "#undef N"
    }
}
