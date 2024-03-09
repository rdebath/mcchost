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
    # Use -v files=1 to create help files in help/*.txt
    if (files < 1) files=0;
    if (!files) {
	print "/* ⇉⇉⇉ This file was automatically generated.  Do not edit! ⇇⇇⇇ */"
	print "#include \"lib_text.h\"\n"
    }
}

# Allow files to be disabled with '#if 0'
/^#if 0/ { ifzero++; next; }
ifzero && /^#if/ { ifzero++; next; }
ifzero && /^#endif/ { ifzero--; next; }
ifzero { next; }

# Help files, including a single line marker for an empty file.
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

# Multi-line texts. Generate an array of char strings.
/^\/\*TEXT/ {
    if (flg) save_text();
    flg = 2;
    textname=$2;
    class="";
    text="";
}
/\*\// {
    if (flg) save_text();
    flg = 0;
}
/^\/\*TEXT/ {next;}

# Save up UCMD_ names
/^#define *UCMD_[A-Z0-9]*[	 ]*[\\{]/ {
    cmdlist = cmdlist "#ifdef " $2 "\n"
    cmdlist = cmdlist "    " $2 ",\n"
    cmdlist = cmdlist "#endif\n"
}

# Are we collecting text ?
flg==0 {next;}

{ text=text $0 "\n"; next }

#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#=#

function save_text() {
    if (flg == 1) save_help_text();
    if (flg == 2) save_data_text();
}

function save_help_text() {
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

	print "/* Help for "textname" from "FILENAME" */"
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

function save_data_text() {
    if (files) return;
    gsub("\n*$", "", text);
    gsub("\t", "        ", text); # Don't use tabs!!

    t1 = textname; sub(",.*", "", t1);
    t1 = "char *" t1 "[] = {"

    if (text != "") {
	gsub("\\\\", "\\\\\\\\", text); # Seriously!?
	gsub("\"", "\\\"", text);

	gsub("\n", "\",\n    \"", text);
	sub("$", "\",\n    0\n};", text);

	text = t1 "\n    \"" text
    }

    if (text == "") text = t1 "0};";

    print "/* Text constant "textname" in "FILENAME" */"
    print text
    print ""
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
