#include "ini_file.h"

#if INTERFACE
typedef struct ini_file_t ini_file_t;
struct ini_file_t {
    ini_line_t * lines;
    char * filename;
    char * last_section;
    int size;
    int count;
    uint8_t quiet;
    uint8_t read_only;
    uint8_t appended;
};

struct ini_line_t {
    enum {ini_nil=0, ini_section=1, ini_item=2, ini_comment=4, ini_error=8} line_type;
    char * section;
    char * name;
    char * value;
    char * text_line;
}
#endif

/*HELP inifile
Empty lines are ignored.
Section syntax is normal [...]
    Section syntax uses "." as element separator within name.
    Elements may be numeric for array index.
    Other elements are identifiers.

Comment lines have a "#" or ";" at the start of the line
Labels are followed by "=" or ":"
Spaces and tabs before "=", ":", ";", "#" are ignored.

Value is trimmed at both ends for spaces and tabs.

Quotes are not special.
Backslashes are not special
Strings are generally limited to 64 cp437 characters by protocol.
Character set of file is UTF8
*/

int
load_ini_txt_file(ini_file_t * ini_file_p, char * filename, int load_opts)
{
    *ini_file_p = (ini_file_t){0}; // clear_ini_txt(ini_file_p) ???
    ini_file_p->quiet = ((load_opts&1) != 0);
    ini_file_p->read_only = ((load_opts&2) != 0);

    int rv = 0;
    FILE *ifd = fopen(filename, "r");
    if (!ifd) {
#ifdef TEST
	printf("# File not found: %s\n", filename);
#else
	if (!ini_file_p->quiet)
	    printf_chat("&WFile not found: &e%s", filename);
#endif
	return -1;
    }

    ini_file_p->filename = filename;
    char ibuf[BUFSIZ];
    *ibuf = 0;

    while(fgets(ibuf, sizeof(ibuf), ifd)) {
	if (load_ini_txt_line(ini_file_p, ibuf) != 0) {
	    rv++;
	}
    }

    ini_file_p->filename = 0;
    ini_file_p->last_section = 0;
    ini_file_p->quiet = 0;

    fclose(ifd);
    return rv;
}

// Load one ini line
// RV 0=> Line okay, 1=> Unknown option, 2=> Bad line, 3=> Bad file.
LOCAL int
load_ini_txt_line(ini_file_t * ini, char *ibuf)
{
    char * p = ibuf + strlen(ibuf);
    for(;p>ibuf && (p[-1] == '\n' || p[-1] == '\r' || p[-1] == ' ' || p[-1] == '\t');p--) p[-1] = 0;

    int lt = ini_nil;
    char label[64], *line2 = 0;
    char * curr_section = 0;

    for(p=ibuf; *p == ' ' || *p == '\t'; p++);
    if (*p == '#' || *p == ';' || *p == '/' || *p == 0) {
	lt = ini_comment;
    } else if (*p == '[') {
	p = ini_extract_section(&curr_section, p);
	if (p) for(; *p == ' ' || *p == '\t'; p++);
	if (p && *p) line2 = p;
	lt = ini_section;
    }
    if (curr_section) ini->last_section = curr_section;
    if (lt == ini_nil || (line2 && lt == ini_section))
    {
	if (lt == ini_section) p = line2;
	int rv = ini_decode_lable(&p, label, sizeof(label));
	if (!rv) {
#ifdef TEST
	    printf("Invalid label %s in %s section %s\n", ibuf, ini->filename, ini->last_section?:"-");
#else
	    if (ini->quiet)
		printlog("Invalid label %s in %s section %s", ibuf, ini->filename, ini->last_section?:"-");
	    else
		printf_chat("&WInvalid label &S%s&W in &S%s&W section &S%s&W", ibuf, ini->filename, ini->last_section?:"-");
#endif
	    lt = ini_error;
	} else {
	    if (lt == ini_section) lt = ini_item + ini_section;
	    else lt = ini_item;
	}
    }

    {
	int rv = add_ini_txt_check_count(ini, 1);
	if (rv) {
	    if (curr_section) free(curr_section);
	    return rv;
	}
    }

    // Flag to ignore comments early as we are going to only read this file.
    if (ini->read_only || lt == ini_nil) {
	if ((lt & (ini_section | ini_item)) == 0) {
	    return 0;
	}
    }

    ini_line_t * ln = &ini->lines[ini->count++];
    ln->line_type = lt;
    ln->text_line = strdup(ibuf);
    ln->section = curr_section;
    ln->name = 0;
    ln->value = 0;

    if ((lt & ini_item) == ini_item) {
	ln->name = strdup(label);
	ln->value = strdup(p); // TODO: Quotes?
    }

    return 0;
}

void
add_ini_txt_line(ini_file_t * ini_file_p, char * section, char * label, char * value)
{
    if (!ini_file_p) return;
    int lno, sectlno = -1;
    int insect = (section == 0 || *section == 0);
    ini_file_p->appended = 0;

    for(lno = 0; lno < ini_file_p->count; lno++) {
	if ((ini_file_p->lines[lno].line_type & ini_section) == ini_section) {
	    insect = (section == 0 || *section == 0);
	    if (ini_file_p->lines[lno].section != 0) {
		if (section == 0) insect = 0;
		else
		    insect = (strcasecmp(ini_file_p->lines[lno].section, section) == 0);
	    }
	    if (insect) sectlno = lno+1;
	}
	if (!insect || !ini_file_p->lines[lno].name) continue;
	if ((ini_file_p->lines[lno].line_type & ini_item) == ini_item) {
	    sectlno = lno+1;
	    if (strcasecmp(label, ini_file_p->lines[lno].name) == 0) {
		// Don't overwrite same value
		if (value != 0 && strcmp(ini_file_p->lines[lno].value, value) == 0)
		    return;

		if (ini_file_p->lines[lno].text_line) {
		    free(ini_file_p->lines[lno].text_line);
		    ini_file_p->lines[lno].text_line = 0;
		}
		if (ini_file_p->lines[lno].name) {
		    free(ini_file_p->lines[lno].name);
		    ini_file_p->lines[lno].name = 0;
		}
		if (ini_file_p->lines[lno].value) {
		    free(ini_file_p->lines[lno].value);
		    ini_file_p->lines[lno].value = 0;
		}
		ini_file_p->lines[lno].name = strdup(label);
		if (value == 0)
		    ini_file_p->lines[lno].line_type = ini_nil;
		else
		    ini_file_p->lines[lno].value = strdup(value);
		return;
	    }
	}
    }

    if (value == 0) return;

    if (sectlno < 0) {
	sectlno = insert_ini_txt_lines(ini_file_p, ini_file_p->count, 3);
	// Add blank line
	ini_file_p->lines[sectlno].line_type = ini_comment;
	ini_file_p->lines[sectlno].text_line = strdup("");

	// Add section
	sectlno++;
	ini_file_p->lines[sectlno].line_type = ini_section;
	ini_file_p->lines[sectlno].section = strdup(section?section:"");

	// Add line
	sectlno++;
	ini_file_p->lines[sectlno].line_type = ini_item;
	ini_file_p->lines[sectlno].name = strdup(label);
	ini_file_p->lines[sectlno].value = strdup(value);

	ini_file_p->appended = 1;
    } else {
	if (sectlno >= ini_file_p->count)
	    ini_file_p->appended = 1;

	sectlno = insert_ini_txt_lines(ini_file_p, sectlno, 1);

	// Add line
	ini_file_p->lines[sectlno].line_type = ini_item;
	ini_file_p->lines[sectlno].name = strdup(label);
	ini_file_p->lines[sectlno].value = strdup(value);
    }
}

/* This adds a comment just before the last item appended. */
void
add_ini_text_comment(ini_file_t * ini_file_p, char * comment)
{
    if (!ini_file_p) return;
    if (ini_file_p->size != 0 && !ini_file_p->appended) return;
    int lno;
    if (ini_file_p->count == 0 || ini_file_p->lines[ini_file_p->count-1].line_type == ini_comment)
	lno = insert_ini_txt_lines(ini_file_p, ini_file_p->count, 1);
    else
	lno = insert_ini_txt_lines(ini_file_p, ini_file_p->count-1, 1);
    ini_file_p->lines[lno].line_type = ini_comment;
    ini_file_p->lines[lno].text_line = strdup(comment);
    ini_file_p->appended = 1;
}

int
save_ini_txt_file(ini_file_t * ini_file_p, char * filename)
{
    int l = strlen(filename), do_ren = 0;
    char * nbuf = malloc(l + sizeof(pid_t)*3+10);
    strcpy(nbuf, filename);
    if (l > 4 && strcmp(nbuf+l-4, ".ini") == 0) {
	sprintf(nbuf+l-4, ".%d.tmp", getpid());
	do_ren = 1;
    }
    FILE *fd = fopen(nbuf, "w");
    if (!fd) {
	perror(nbuf);
	free(nbuf);
	return -1;
    }

    for(int lno = 0; lno < ini_file_p->count; lno++)
    {
	ini_line_t * ln = &ini_file_p->lines[lno];
	if (ln->line_type == ini_nil) continue; // Not a line anymore.

	fflush(fd);
	if (ln->text_line && ln->line_type != ini_error)
	    fprintf(fd, "%s\n", ln->text_line);
	else {
	    switch(ln->line_type & (ini_section + ini_item)) {
	    case ini_section:
		fprintf(fd, "[%s]\n", ln->section?ln->section:"");
		break;
	    case ini_item:
		fprintf(fd, "%s =%s%s\n",
		    ln->name?ln->name:"",
		    ln->value&&*ln->value?" ":"",
		    ln->value?ln->value:"");
		break;
	    case ini_section + ini_item:
		fprintf(fd, "[%s] %s=%s\n",
		    ln->section?ln->section:"",
		    ln->name?ln->name:"",
		    ln->value?ln->value:"");
		break;

	    default:
		// Huh? Comment without text?
		if (ln->section || ln->name || ln->value)
		    fprintf(fd, "; %s%s%s%s%s%s%s\n",
			ln->section?"[":"",
			ln->section?ln->section:"",
			ln->section?"]":"",
			ln->name&&ln->section?" ":"",
			ln->name?ln->name:"",
			ln->name?"=":"",
			ln->value?ln->value:"");
		else if (ln->text_line)
		    fprintf(fd, "; %s\n", ln->text_line);
		break;
	    }
	}
    }

    fclose(fd);

    if (do_ren) {
      if (rename(nbuf, filename) < 0)
	  perror(nbuf);
      unlink(nbuf);
    }

    free(nbuf);
    return 0;
}

void
clear_ini_txt(ini_file_t * ini_file_p)
{
    for(int i = 0; i< ini_file_p->count; i++)
    {
	if (ini_file_p->lines[i].section) {
	    free(ini_file_p->lines[i].section);
	    ini_file_p->lines[i].section = 0;
	}
	if (ini_file_p->lines[i].name) {
	    free(ini_file_p->lines[i].name);
	    ini_file_p->lines[i].name = 0;
	}
	if (ini_file_p->lines[i].value) {
	    free(ini_file_p->lines[i].value);
	    ini_file_p->lines[i].value = 0;
	}
	if (ini_file_p->lines[i].text_line) {
	    free(ini_file_p->lines[i].text_line);
	    ini_file_p->lines[i].text_line = 0;
	}
    }
    free(ini_file_p->lines);
    ini_file_p->lines = 0;
    ini_file_p->count = 0;
    ini_file_p->size = 0;
}

LOCAL char *
ini_extract_section(char ** curr_section, char *line)
{
    char buf[BUFSIZ];
    char *d = buf, *p = line;
    if (*p != '[') return p;
    for(p++; *p; p++) {
	if (*p == ' ' || *p == '_' || *p == '\t') continue;
	if (*p == ']') { p++; break; }
	if (*p >= 'A' && *p <= 'Z') {
	    *d++ = *p - 'A' + 'a';
	    continue;
	}
	if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || *p == '.' || *p == '-') {
	    *d++ = *p;
	    continue;
	}
	if (*p == '/' || *p == '\\' || *p == '-')
	    *d++ = '.';
	// Other chars are ignored.
    }
    *d = 0;
    *curr_section = strdup(buf);
    return p;
}

LOCAL int
ini_decode_lable(char **line, char *buf, int len)
{
    char * d = buf, *p = *line;
    for( ; *p; p++) {
	if (*p == ' ' || *p == '_' || *p == '\t') continue;
	if (*p == '=' || *p == ':') break;
	if (*p >= 'A' && *p <= 'Z') {
	    if (d<buf+len-1)
		*d++ = *p - 'A' + 'a';
	    continue;
	}
	if ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'z') || *p == '.' || *p == '-') {
	    if (d<buf+len-1)
		*d++ = *p;
	    continue;
	}
	// Other chars are ignored.
    }
    *d = 0;
    if (*buf == 0) return 0;
    if (*p != ':' && *p != '=') return 0;
    p++;

    while(*p == ' ' || *p == '\t') p++;
    *line = p;
    return 1;
}

LOCAL int
add_ini_txt_check_count(ini_file_t * ini, int extra)
{
    if (ini->count+extra >= ini->size) {
	int sz = ini->size?ini->size*2:64;
	void * np = realloc(ini->lines, sizeof(*ini->lines) * sz);
	if (np == 0) {
#ifdef TEST
	    printf("Cannot realloc() while loading file \"%s\"\n", ini->filename);
#else
	    if (ini->filename)
		printlog("Cannot realloc() while loading file \"%s\"", ini->filename);
	    else
		printlog("Cannot realloc() expanding ini file");
#endif
	    return 3;
	}
	ini->size = sz;
	ini->lines = np;
    }
    return 0;
}

LOCAL int
insert_ini_txt_lines(ini_file_t * ini, int posn, int num)
{
    if (posn < 0) posn = 0;
    if (posn >= ini->count) posn = ini->count;
    if (num <= 0) return posn;

    add_ini_txt_check_count(ini, num);

    for (int c = ini->count-1; c >= posn; c--)
	ini->lines[c+num] = ini->lines[c];
    for(int c = 0; c < num; c++)
	ini->lines[posn+c] = (ini_line_t){0};
    ini->count += num;
    return posn;
}

#ifdef TEST
int main(int argc, char ** argv) {
    ini_file_t ini[1];
    load_ini_txt_file(ini, "/tmp/initest.ini", 0);

    add_ini_txt_line(ini, "", "notUsed", "987654321");
    add_ini_txt_line(ini, "level", "SEED", "ss123456789");
    add_ini_txt_line(ini, "newsect", "newitem1", "aa456789");
    add_ini_txt_line(ini, "level", "extrafield", "bb3456789");
    add_ini_txt_line(ini, "newsect", "newitem2", "cc456789");

    save_ini_txt_file(ini, "/tmp/initest2.ini");

#if 1
    for(int i=0; i<ini->count; i++)
    {
	ini_line_t * ln = &ini->lines[i];
		printf("%3d:", i);
		printf("%d %s%s%s%s%s%s%s",
		    ln->line_type,
		    ln->section?"[":"",
		    ln->section?ln->section:"",
		    ln->section?"]":"",
		    ln->name&&ln->section?" ":"",
		    ln->name?ln->name:"",
		    ln->name?"=":"",
		    ln->value?ln->value:"");

		printf(" >>%s<<", ln->text_line);
		printf("\n");
    }
#endif
}
#endif
