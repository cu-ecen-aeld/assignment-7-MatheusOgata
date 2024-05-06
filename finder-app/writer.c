#include <stdio.h> 
#include <string.h>
#include <syslog.h>
#include <errno.h>

#define NUMBER_OF_ARGS		3

int main(int argc, char *argv[])
{

	openlog(NULL, 0, LOG_USER);

	if(argc != NUMBER_OF_ARGS)
	{
		syslog(LOG_ERR, "Invalid number of arguments\n");
		printf("ERROR: Invalid number of arguments. \n");
		return 1;
	}

	const char *file_path = argv[1];
	const char *string_to_write = argv[2];

	FILE* file_dir = fopen(file_path, "w+r");

	if(file_dir == NULL)
	{
		perror("perror returned on attempting open the file");
		syslog(LOG_ERR, "It's not possible to open the file \n");
		return 1;
	}

	syslog(LOG_DEBUG, "Writing %s to %s", string_to_write, file_path);

	if(fwrite(string_to_write, strlen(string_to_write), 1, file_dir) == -1)
	{	
		perror("perror returned on attempting writing to the file");
		syslog(LOG_ERR, "It's not possible to write to the file\n");
		return 1;
	}


	fclose(file_dir);

	return 0;
}
