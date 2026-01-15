#include "scanparse/preprocessor.h"
#include "global/globals.h"
#include "palm/ctinfo.h"
#include "palm/str.h"
#include <ccngen/ast.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

FILE *preprocessorStart()
{
    if (!global.preprocessor_enabled)
    {
        return fopen(global.input_file, "r");
    }

    // Check if input file is a valid file. This also acts as input sanitization.
    FILE *file = fopen(global.input_file, "r");
    if (file == NULL)
    {
        CTI(CTI_ERROR, true, "Cannot open file '%s'.", global.input_file);
        CTIabortOnError();
    }
    fclose(file);

    // -C : keep comments
    // -P : remove linemarkers
    // -nostdinc : do not search standard sytem directories
    char *command = STRfmt("cpp -nostdinc -P -C %s", global.input_file);
    FILE *fd_preprocess = popen(command, "r");
    free(command);

    if (fd_preprocess == NULL)
    {
        CTI(CTI_ERROR, true,
            "Cannot run the C preprocessor (cpp) on file '%s'. Returned with errno '%d'.",
            global.input_file, errno);
        CTIabortOnError();
    }

    return fd_preprocess;
}

void preprocessorEnd(FILE *fd)
{
    int status = pclose(fd);
    if (status == -1)
    {
        // Error from pclose
        CTI(CTI_ERROR, true,
            "Failed to close C preprocessor (cpp) process with file '%s'. Returned with errno "
            "'%d'.",
            global.input_file, errno);
        CTIabortOnError();
    }
    else
    {
        int exit_status = WEXITSTATUS(status);
        if (exit_status != 0)
        {
            CTI(CTI_ERROR, true,
                "The C preprocessor (cpp) faild to process the file '%s' and exited with the "
                "status '%d'",
                global.input_file, exit_status);
            CTIabortOnError();
        }

        int signal = WIFSIGNALED(status);
        if (signal)
        {
            CTI(CTI_ERROR, true,
                "The C preprocessor (cpp) was kill with signal '%d' while processin the file '%s'.",
                WTERMSIG(status), global.input_file);
            CTIabortOnError();
        }
    }
}
