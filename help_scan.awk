BEGIN{
    flg=0;count=0;
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

flg==0 {next;}

{ text=text $0 "\n"; next }

function save_text() {
    gsub("\t", "        ", text); # Don't use tabs!!
    gsub("\\\\", "\\\\\\\\", text); # Seriously!?
    gsub("\"", "\\\"", text);

    gsub("\n", "\",\n    \"", text);
    sub("\"$", "0};\n", text);
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
}
