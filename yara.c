/*

Copyright(c) 2007. Victor M. Alvarez [plusvic@gmail.com].

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

*/

#ifndef WIN32
#include <sys/stat.h> 
#include <dirent.h>
#include <unistd.h>
#else
#include <windows.h>
#include "getopt.h"
#endif

#include <stdio.h>
#include <string.h>
#include <yara.h>
#include "config.h"

#ifndef MAX_PATH
#define MAX_PATH 255
#endif

int recursive_search = FALSE;
int show_tags = FALSE;
int show_specified_tags = FALSE;
int show_strings = FALSE;
TAG* specified_tag_list = NULL;


////////////////////////////////////////////////////////////////////////////////////////////////

void show_help()
{
    printf("usage:  yara [ -t tag ] [ -g ] [ -s ] [ -r ] [ -v ] [RULEFILE...] FILE\n");
    printf("options:\n");
	printf("  -t <tag>          Display rules tagged as <tag> and ignore the rest. This option can be used more than once.\n");
	printf("  -g                Display tags.\n");
	printf("  -s                Display strings.\n");
	printf("  -r                Recursively search directories.\n");
	printf("  -v                Show version information.\n");
	printf("\nReport bugs to: <%s>\n", PACKAGE_BUGREPORT);
}


////////////////////////////////////////////////////////////////////////////////////////////////

#ifdef WIN32

int is_directory(const char* path)
{
	if (GetFileAttributes(path) & FILE_ATTRIBUTE_DIRECTORY)
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void scan_dir(const char* dir, int recursive, RULE_LIST* rules, YARACALLBACK callback)
{
	WIN32_FIND_DATA FindFileData;
	HANDLE hFind;

	char full_path[MAX_PATH];
	static char path_and_mask[MAX_PATH];
	
	sprintf(path_and_mask, "%s\\*", dir);
	
	hFind = FindFirstFile(path_and_mask, &FindFileData);

	if (hFind != INVALID_HANDLE_VALUE)
	{
		do
		{
			sprintf(full_path, "%s\\%s", dir, FindFileData.cFileName);

			if (!(FindFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			{
				//printf("Processing %s...\n", FindFileData.cFileName);
				scan_file(full_path, rules, callback, full_path);
			}
			else if (recursive && FindFileData.cFileName[0] != '.' )
			{
				scan_dir(full_path, recursive, rules, callback);
			}

		} while (FindNextFile(hFind, &FindFileData));

		FindClose(hFind);
	}
}
	
#else

int is_directory(const char* path)
{
	struct stat st;
	
	if (stat(path,&st) == 0)
	{
		return S_ISDIR(st.st_mode);
	}
	
	return 0;
}

void scan_dir(const char* dir, int recursive, RULE_LIST* rules, YARACALLBACK callback)
{
	DIR *dp;
	struct dirent *de;
	struct stat st;
	char full_path[MAX_PATH];

	dp = opendir(dir);
	
	if (dp)
	{
		de = readdir(dp);
		
		while (de)
		{
			sprintf(full_path, "%s/%s", dir, de->d_name);
			
			int err = stat(full_path,&st);
			
			if (err == 0)
			{
				if(S_ISREG(st.st_mode))
				{
					//printf("Processing %s\n", de->d_name);		
					scan_file(full_path, rules, callback, full_path);
				}
				else if(recursive && S_ISDIR(st.st_mode) && de->d_name[0] != '.')
				{
					//printf("Entering %s\n", de->d_name);
					scan_dir(full_path, recursive, rules, callback);
				}
			}
			
			de = readdir(dp);
		}
		
		closedir(dp);
	}
}

#endif

void print_string(unsigned char* buffer, unsigned int buffer_size, unsigned int offset, unsigned int length)
{
	int i = offset;
	int len = 0;
	char* tmp;
		
	tmp = malloc(length + 1);
	
	memcpy(tmp, buffer + offset, length);
	
	tmp[length] = 0;
	
	printf("%s\n", tmp);
	
	free(tmp);
	
}

int callback(RULE* rule, unsigned char* buffer, unsigned int buffer_size, void* data)
{
	TAG* tag;
	STRING* string;
	MATCH* match;
	
	int show = TRUE;
		
	if (show_specified_tags)
	{
		show = FALSE;
		tag = specified_tag_list;
		
		while (tag != NULL)
		{
			if (lookup_tag(rule->tag_list_head, tag->identifier) != NULL)
			{
				show = TRUE;
				break;
			}
			
			tag = tag->next;
		}
	}
	
	if (show)
	{
		if (show_tags)
		{
			printf("%s", rule->identifier);
			
			tag = rule->tag_list_head;
			
			if (tag != NULL)
			{ 
				printf(" [");
			
				while(tag != NULL)
				{
					if (tag->next == NULL)
					{
						printf("%s", tag->identifier);
					}
					else
					{
						printf("%s,", tag->identifier);
					}
									
					tag = tag->next;
				}
				
				printf("]");
			}
			
			printf("   %s\n", (char*) data);	
			
		}
		else
		{
			printf("%s   %s\n", rule->identifier, (char*) data);
		}
		
		if (show_strings)
		{
			string = rule->string_list_head;

			while (string != NULL)
			{
				if (string->flags & STRING_FLAGS_FOUND)
				{
					match = string->matches;

					while (match != NULL)
					{
						printf("%08X: ", match->offset);
						
						if (IS_HEX(string))
						{
							//TODO: print_hex_string()
							printf("\n");
						}
						else if (IS_WIDE(string))
						{
							//TODO: print_wide_string()
							printf("\n");
						}
						else
						{
							print_string(buffer, buffer_size, match->offset, match->length);
						}
						
						match = match->next;
					}
				}

				string = string->next;
			}		
		}
	}
	
    return 0;
}

int process_cmd_line(int argc, char const* argv[])
{
	char c;	
	TAG* tag;
	opterr = 0;
 
	while ((c = getopt (argc, (char**) argv, "rsvgt:")) != -1)
	{
		switch (c)
	    {
			case 'v':
				printf("%s\n", PACKAGE_STRING);
				return 0;
		
			case 'r':
				recursive_search = TRUE;
				break;
				
			case 'g':
				show_tags = TRUE;
				break;
				
			case 's':
				show_strings = TRUE;
				break;
		
		   	case 't':
		
				show_specified_tags = TRUE;
				
				tag = malloc(sizeof(TAG));	
				
				if (tag != NULL)
				{
					tag->identifier = optarg;
					tag->next = specified_tag_list;
					specified_tag_list = tag;
				}
				else
				{
					fprintf (stderr, "Not enough memory.\n", optopt);
					return 0;
				}
	
		        break;
	
		    case '?':
	
		        if (optopt == 't')
				{
		        	fprintf (stderr, "Option -%c requires an argument.\n", optopt);
				}
		        else if (isprint (optopt))
				{
		           	fprintf (stderr, "Unknown option `-%c'.\n", optopt);
				}
		        else
				{
		            fprintf (stderr, "Unknown option character `\\x%x'.\n", optopt);
				}
	
		        return 0;
	
		     default:
		         abort ();
	     }
	}
	
	return 1;
	
}

int main(int argc, char const* argv[])
{
	int i, errors;
	RULE_LIST* rules;
	FILE* rule_file;
	TAG* tag;
	TAG* next_tag;
		
	if (!process_cmd_line(argc, argv))
	{
		return 0;
	}	
		
	if (argc == 1 || optind == argc)
	{
		show_help();
		return 0;
	}
			
	rules = alloc_rule_list();
	
	if (rules == NULL) 
		return 0;
		
			
	for (i = optind; i < argc - 1; i++)
	{
		rule_file = fopen(argv[i], "r");
		
		if (rule_file != NULL)
		{
			set_file_name(argv[i]);
			
			errors = compile_rules(rule_file, rules);
			
			fclose(rule_file);
			
			if (errors > 0) /* errors during compilation */
			{
				free_rule_list(rules);				
				return 0;
			}
		}
		else
		{
			fprintf(stderr, "could not open file: %s\n", argv[i]);
		}
	}
	
	if (optind == argc - 1)  /* no rule files, read rules from stdin */
	{
		set_file_name("stdin");
		
		errors = compile_rules(stdin, rules);
			
		if (errors > 0) /* errors during compilation */
		{
			free_rule_list(rules);				
			return 0;
		}		
	}
		
	init_hash_table(rules);
	
	if (is_directory(argv[argc - 1]))
	{
		scan_dir(argv[argc - 1], recursive_search, rules, callback);
	}
	else		
	{
		scan_file(argv[argc - 1], rules, callback, (void*) argv[argc - 1]);
	}
	
	free_hash_table(rules);	
	free_rule_list(rules);
	
	/* free tag list allocated by process_cmd_line */
	
	tag = specified_tag_list;
	
	while(tag != NULL)
	{
		next_tag = tag->next;
		free(tag);
		tag = next_tag;
	}
	
	return 1;
}

