/** ZCT/cmd.c--Created 060205 **/

/** Copyright 2008 Zach Wegner **/
/*
 * This file is part of ZCT.
 *
 * ZCT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * ZCT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ZCT.  If not, see <http://www.gnu.org/licenses/>.
 */
#include "zct.h"
#include "functions.h"
#include "globals.h"
#include "cmd.h"

/* Prototypes for this file */
COMMAND *cmd_lookup(char *input);

#ifndef ZCT_POSIX
char *strsep(char **stringp, const char *delim);
#endif

/* Command-globals */
CMD_INPUT cmd_input = { NULL, NULL, NULL, 0 };
PROTOCOL last_protocol;
COMMAND tune_commands[] =
{
	{ 0, "exit", "Exit the tuning process.", 1, cmd_null },
	{ 0, NULL, NULL, 0, NULL }
};
/* Locals */
COMMAND *commands[] = { def_commands, xb_commands, uci_commands, an_commands,
	an_commands, dbg_commands, tune_commands };
COMMAND cmd_table[CMD_TABLE_SIZE];

/**
command():
Processes user input.
Created 060205; last modified 083108
**/
int command(char *input)
{
	COMMAND *cmd;

	/* Shell escape. */
	if (input[0] == '!')
	{
		system(input + 1);
		return CMD_GOOD;
	}
	set_cmd_input(input);
	/* Check for comments, and truncate the rest of the line for them. */
	if (strchr(cmd_input.input, '#') != NULL)
		*strchr(cmd_input.input, '#') = '\0';
	cmd_parse(" \t\n");
	/* No commands. Just return, don't print an error message. */
	if (cmd_input.arg_count == 0)
		return CMD_GOOD;
	/* Try to find the command in the table for the current protocol. */
	cmd = cmd_lookup(cmd_input.arg[0]);
	if (cmd != NULL)
	{
		/* Some commands require a search to be stopped before they
			are interpreted, like "new". Some require a ponder search
			to end, like "easy" and "go". */
		if ((cmd->search_flag == 1 && zct->engine_state != IDLE) ||
			(cmd->search_flag == 2 && zct->engine_state == PONDERING))
			return CMD_STOP_SEARCH;
		/* Execute the command. */
		cmd->cmd_func();
		return CMD_GOOD;
	}
	return CMD_BAD;
}

/**
set_cmd_input():
This function is just a wrapper for allocating the internal input buffer and
copying input buffers into it.
Created 051108; last modified 083108
**/
void set_cmd_input(char *input)
{
	if (cmd_input.old_input != NULL)
	{
		free(cmd_input.old_input);
		cmd_input.old_input = NULL;
	}
	cmd_input.old_input = cmd_input.input;
	cmd_input.input = malloc(strlen(input) + 1);
	strcpy(cmd_input.input, input);
}

/**
initialize_cmds():
Initializes the internal command tables.
Created 070105; last modified 080308
**/
void initialize_cmds(void)
{
	COMMAND *cmd;
	STR_HASHKEY key;

	for (cmd = cmd_table; cmd < cmd_table + CMD_TABLE_SIZE; cmd++)
		cmd->cmd_name = NULL;
	for (cmd = commands[zct->protocol]; cmd->cmd_name != NULL; cmd++)
	{
		for (key = str_hashkey(cmd->cmd_name, CMD_TABLE_SIZE);
			key < CMD_TABLE_SIZE && cmd_table[key].cmd_name != NULL; key++)
			;
		if (key < CMD_TABLE_SIZE)
		{
			cmd_table[key] = *cmd;
			cmd_table[key].hashkey = key;
		}
		else
			fatal_error("fatal error: could not allocate "
				"command hash table.\n");
	}
}

/**
cmd_parse():
Parses an input string into arguments.
Created 081205; last modified 083108
**/
void cmd_parse(char *delim)
{
	char *next;
	char *str;

	/* Free old argument lists. */
	if (cmd_input.args != NULL)
	{
		free(cmd_input.args);
		cmd_input.args = NULL;
	}
	if (cmd_input.arg != NULL)
	{
		free(cmd_input.arg);
		cmd_input.arg = NULL;
	}
	/* Check for blank input. If we're not reading from a file, duplicate
		the last command given. */
	if (!zct->source && strlen(cmd_input.input) == 0 &&
		cmd_input.old_input != NULL)
	{
		cmd_input.input = malloc(strlen(cmd_input.old_input) + 1);
		strcpy(cmd_input.input, cmd_input.old_input);
	}

	/* Allocate argument lists. */
	cmd_input.args = (char *)malloc(strlen(cmd_input.input) + 1);
	/* Allocate enough space for as many arguments as there are characters. */
	cmd_input.arg = (char **)malloc(strlen(cmd_input.input) * sizeof(char *));
	strcpy(cmd_input.args, cmd_input.input);
	str = cmd_input.args;
	for (cmd_input.arg_count = 0; (next = strsep(&str, delim)) != NULL;
		cmd_input.arg_count++)
	{
		if (*next != '\0')
		{
			cmd_input.arg[cmd_input.arg_count] = next;
			/* Check if the next argument is enclosed in quotes. */
			if (str != NULL && *str == '"')
			{
				str++; /* Advance past the quote character */
				cmd_input.arg_count++;
				cmd_input.arg[cmd_input.arg_count] = str;
				if ((str = strchr(str, '"')) != NULL)
					*str++ = '\0'; /* Delete the ending quote character */
			}	
		}
		else
			cmd_input.arg_count--;
	}
}

/**
display_help():
Displays a list of commands in the current zct->protocol for the user.
Created 071207; last modified 111408
**/
void display_help(void)
{
	COMMAND *cmd;
	static char help_str[BUFSIZ];

	for (cmd = commands[zct->protocol]; cmd->cmd_func != NULL; cmd++)
	{
		if (cmd->help_text != NULL)
		{
			sprint(help_str, sizeof(help_str),
				"%-10s    %s", cmd->cmd_name, cmd->help_text);
			line_wrap(help_str, sizeof(help_str), 16);
			print("%s\n", help_str);
		}
	}
}

/**
cmd_lookup():
Look up command on internal hash table.
Created 070105; last modified 102506
**/
COMMAND *cmd_lookup(char *input)
{
	STR_HASHKEY key;
	for (key = str_hashkey(input, CMD_TABLE_SIZE); key < CMD_TABLE_SIZE &&
		cmd_table[key].cmd_name != NULL; key++)
		if (!strcmp(input, cmd_table[key].cmd_name))
			return &cmd_table[key];
	return NULL;
}

/**
str_hashkey():
Creates a hashkey from a given string for a lookup table of the given size.
Created 060205; last modified 080308
**/
STR_HASHKEY str_hashkey(char *input, int size)
{
	int h;
	h = strlen(input) * 319;
	h += input[0];
	h *= input[strlen(input) >> 1];
	h -= input[strlen(input) - 1];
	return h % size;
}

#if !defined(ZCT_POSIX)
/**
strsep():
This is just a filler function for non-POSIX systems.
Created 042908; last modified 042908
**/
char *strsep(char **string, const char *delim)
{
	char *str;
	char *token;
	char *d;
	char c;
	char t;

	if (*string == NULL)
		return NULL;
	token = str = *string;
	while (TRUE)
	{
		/* Take the next char in the string and see if it matches
			something in the set delimiter characters. */
		c = *str++;
		d = delim;
		do
		{
			/* look at the next char in delim */
			if ((t = *d++) == c)
			{
				if (c == '\0')
					str = NULL;
				else
					*(str - 1) = '\0';
				*string = str;
				return token;
			}
		} while (t != '\0');
	}
}
#endif
