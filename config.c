/*
Copyright (c) 2013-2014, 2016 René Ladan. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.
*/

#include "config.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_KEYS 8
static const char * const key[NUM_KEYS] = {
	"pin", "iodev", "activehigh", "freq",
	"summermonth", "wintermonth", "leapsecmonths", "outlogfile"
};

#define MAX_KEY_LEN 20
#define MAX_VAL_LEN 255
static const unsigned max_len = (MAX_KEY_LEN + 3 + MAX_VAL_LEN + 2);
    /* "k = v\n\0" */

static char *value[8];

static int
getpos(const char * const kw)
{
	for (int i = 0; i < NUM_KEYS; i++)
		if (strcmp(key[i], kw) == 0)
			return i;
	return -1;
}

static char * const
strip(char *s)
{
	while (s[0] == ' ' || s[0] == '\n' || s[0] == '\r' || s[0] == '\t')
		s++;
	for (int i = (int)(strlen(s) - 1); s[i] == ' ' || s[i] == '\n' ||
	    s[i] == '\r' || s[i] == '\t'; i--)
		s[i] = '\0';
	return s;
}

int
read_config_file(const char * const filename)
{
	FILE *configfile;
	char *k, *v;
	char *line, *freeptr;

	k = v = NULL;
	line = malloc(max_len);
	if (line == NULL) {
		perror("malloc(configfile)");
		return errno;
	}
	freeptr = line;
	for (int i = 0; i < NUM_KEYS; i++)
		value[i] = NULL;

	configfile = fopen(filename, "r");
	if (configfile == NULL) {
		perror("fopen (configfile)");
		free(freeptr);
		return errno;
	}

	while (feof(configfile) == 0) {
		int i;

		if (fgets(line, max_len, configfile) == NULL) {
			if (feof(configfile) != 0)
				break;
			printf("read_config_file: error reading file\n");
			(void)fclose(configfile);
			free(freeptr);
			return -1;
		}
		if ((k = strsep(&line, "=")) != NULL)
			v = line;
		else {
			printf("read_config_file: no key/value pair found\n");
			(void)fclose(configfile);
			free(freeptr);
			return -1;
		}
		i = (int)strlen(k);
		k = strip(k);
		v = strip(v);
		if (i > MAX_KEY_LEN + 1 || strlen(k) == 0 ||
		    strlen(k) > MAX_KEY_LEN) {
			printf("read_config_file: item with bad key length\n");
			(void)fclose(configfile);
			free(freeptr);
			return -1;
		}
		i = getpos(k);
		if (i == -1) {
			printf("read_config_file: skipping invalid key '%s'\n",
			    k);
			continue;
		}
		if (strlen(v) > MAX_VAL_LEN) {
			printf("read_config_file: item with too long value\n");
			(void)fclose(configfile);
			free(freeptr);
			return -1;
		}
		if (value[i] != NULL)
			printf("read_config_file: overwriting value for key"
			    " '%s'\n", k);
		value[i] = strdup(v);
	}
	for (int i = 0; i < NUM_KEYS; i++)
		if (value[i] == NULL) {
			printf("read_config_file: missing value for key '%s'\n",
			    key[i]);
			(void)fclose(configfile);
			free(freeptr);
			return -1;
		}
	(void)fclose(configfile);
	free(freeptr);
	return 0;
}

/*@null@*/char *
get_config_value(const char * const keyword)
{
	int i;

	i = getpos(keyword);
	return i == -1 ? NULL : value[i];
}
