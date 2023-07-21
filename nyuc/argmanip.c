#include <string.h>
#include <stdlib.h>
#include <stdarg.h>

char **manipulate_args(int argc, const char *const *argv, int(*const manip)(int))
{
    // I read documentations for argc, argv
    char **output = (char **) malloc(sizeof(char *) * (argc+1));

    // Source: https://en.cppreference.com/w/c/language/main_function#Parameters
    for (int i = 0; i < argc; i++)
    {
        // manipulation of each strings
        int len = strlen(argv[i]);
        output[i] = (char *)malloc(len + 1);

        // add to each string in output
        for (int j = 0; j < len; j++)
        {
            output[i][j] = manip(argv[i][j]);
        }
        output[i][len] = '\0';
    }
    output[argc] = NULL;
    return output;
}

void free_copied_args(char **first_arg, ...)
{
    va_list args;
    va_start(args, first_arg);

    // source: https://stackoverflow.com/questions/37538/how-do-i-determine-the-size-of-my-array-in-c
    char **pointer = first_arg;
    while (pointer!= NULL)
    {
        for (int i = 0; pointer[i] != NULL; i++)
        {//free strings
            free(pointer[i]);
        }//free output
        free(pointer);
        pointer = va_arg(args, char**);
    }
    va_end(args);
}
