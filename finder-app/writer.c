#include <stdio.h>
#include <string.h>
#include <syslog.h>

int main(int argc, char* argv[])
{
    if (argc == 3)
    {
        FILE* fWriteFile = fopen(argv[1], "w");
        if (fWriteFile != NULL)
        {
            fwrite(argv[2], strlen(argv[2]), 1, fWriteFile);
            syslog(LOG_DEBUG, "Writing %s to %s", argv[2], argv[1]);
            fclose(fWriteFile);
        }
	else
        {
            syslog(LOG_ERR, "Unable to write %s to %s", argv[2], argv[1]);
	    return 1;
        }
    }
    else
    {
    	syslog(LOG_ERR, "ERROR: Invalid number of arguments.\n"
               "Total number of arguments should be 2.\n"
               "The order of the arguments should be:\n"
               "   1)File Path of file to create\n"
               "   2)String to be added to specified file.\n");
        return 1;
    }
    return 0;
}
