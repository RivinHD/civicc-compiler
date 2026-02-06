#pragma once

#include <stdbool.h>
#include <stdio.h>

FILE *preprocessorStart();
void preprocessorEnd(FILE *fd);

bool parse_linemarker(char *linemarker, int *out_linenum, char **filename);
