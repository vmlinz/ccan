/* This is test for grab_file() function
 */

#include 	<stdlib.h>
#include 	<stdio.h>
#include 	<err.h>
#include 	<sys/stat.h>
#include 	"string/string.h"
#include 	"string/string.c"
#include 	"tap/tap.h"

int 
main(int argc, char *argv[])
{
	unsigned int	i;
	char 		**split, *str;
	int 		length;
	struct 		stat st;

	str = grab_file(NULL, "ccan/string/test/run-grab.c", NULL);
	split = strsplit(NULL, str, "\n", NULL);
	length = strlen(split[0]);
	ok1(streq(split[0], "/* This is test for grab_file() function"));
	for (i = 1; split[i]; i++)	
		length += strlen(split[i]);
	ok1(streq(split[i-1], "/* End of grab_file() test */"));
	if (stat("ccan/string/test/run-grab.c", &st) != 0) 
		err(1, "Could not stat self");
	ok1(st.st_size == length + i);
	
	return 0;
}

/* End of grab_file() test */