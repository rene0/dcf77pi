/*
Copyright (c) 2013 René Ladan. All rights reserved.

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

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include "config.h"

char *key[] = {
	"pin", "activehigh", "freq", "margin", "minlen", "maxlen",
	"summermonth", "wintermonth", "leapsecmonths"
};

#define NUM_KEYS (sizeof(key) / sizeof(key[0]))

char *value[NUM_KEYS];

int
getpos(char *kw)
{
	int i;

	for (i = 0; i < NUM_KEYS; i++)
		if (strcmp(key[i], kw) == 0)
			return i;
	return -1;
}

int
read_config_file(char *filename)
{
	int i;
	FILE *configfile;
	char k[20], v[60];

	for (i = 0; i < NUM_KEYS; i++)
		value[i] = NULL;

	configfile = fopen(filename, "r");
	if (configfile == NULL) {
		perror("fopen (configfile)");
		return errno;
	}

	while (feof(configfile) == 0) {
		if (fscanf(configfile, "%s = %s\n", k, v) != 2) {
			perror("read_config_file");
			return errno;
		}
		i = getpos(k);
		if (i == -1) {
			printf("read_config_file: skipping invalid key '%s'\n", k);
			continue;
		}
		if (value[i] != NULL)
			printf("read_config_file: overwriting value for key '%s'\n", k);
		value[i] = strdup(v);
	}
	for (i = 0; i < NUM_KEYS; i++)
		if (value[i] == NULL) {
			printf("read_config_file: missing value for key '%s'\n", key[i]);
			return 1;
		}
	return 0;
}

char *
get_config_value(char *keyword)
{
	int i;

	i = getpos(keyword);
	return i == -1 ? NULL : value[i];
}
