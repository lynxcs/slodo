#ifndef TEXT_H
#define TEXT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EMPTY_TEXT "[ ] "

typedef struct
{
	char** data;
	size_t capacity;
	size_t size;
	size_t selected;
} todo_text_t;

int text_init(todo_text_t* t, size_t init_capacity)
{
	t->data = malloc(init_capacity * sizeof(char*));
	if (!t->data)
	{
		return -1;
	}

	t->size = 0;
	t->capacity = init_capacity;
	t->selected = 0;

	return 0;
}

int text_free(todo_text_t* t)
{
	for (size_t i = 0; i < t->size; ++i)
	{
		free(t->data[i]);
	}

	free(t->data);
	return 0;
}

// Append text line
// Creates empty line if text == NULL
int text_append(todo_text_t* t, const char* text)
{
	if (t->capacity == t->size)
	{
		if (t->capacity == 0)
		{
			t->capacity++;
		}

		t->data = realloc(t->data, t->capacity * sizeof(char*) * 2);
		t->capacity = t->capacity * 2;
	}

	if (text != NULL)
		t->data[t->size] = strdup(text);
    else
        t->data[t->size] = "";

	t->size++;
	return 0;
}

int text_remove(todo_text_t* t, size_t index)
{
	if (index >= t->size)
	{
		return -1;
	}

	// Free char* at removal index
	free(t->data[index]);

	// If index wasn't the last item, move memory
	if (index + 1 != t->size)
	{
		size_t copy_size = t->size - (index+1);
		memmove(&t->data[index], &t->data[index+1], copy_size * sizeof(char*));
	}

	t->size--;
	return 0;
}

// Swaps src text with dst text
int text_swap(todo_text_t* t, size_t src, size_t dst)
{
	if (src < 0 || dst < 0 || src >= t->size || dst >= t->size)
	{
		return -1;
	}

	if (src == dst)
	{
		return 0;
	}

	char* temp = t->data[dst];
	t->data[dst] = t->data[src];
	t->data[src] = temp;

	return 0;
}

void text_init_from_file(todo_text_t* t, const char* path)
{
	FILE* fp;
	fp = fopen(path, "r");
	if (fp == NULL)
	{
		// fprintf(stderr, "ERROR: Failed to open file (%s). Does it exist?\n", path);
		// exit(-1);
		// File doesn't exist? so only initialize text_init

		if (text_init(t, 2) == -1)
		{
			fclose(fp);
			fprintf(stderr, "ERROR: Failed to allocate memory for todo list");
			exit(-1);
		}
		return;
	}

	size_t lines = 0;
	while (EOF != (fscanf(fp, "%*[^\n]"), fscanf(fp,"%*c")))
	{
		++lines;
	}

	rewind(fp);

	if (text_init(t, lines + 2) == -1)
	{
		fclose(fp);
		fprintf(stderr, "ERROR: Failed to allocate memory for todo list");
		exit(-1);
	}

	char* line = NULL;
	ssize_t read;
	size_t len = 0;
	while((read = getline(&line, &len, fp)) != -1)
	{
		line[strcspn(line,"\n")] = 0;
		text_append(t, line);
	}

	fclose(fp);

	if(line)
	{
		free(line);
	}
}

void text_commit_to_file(todo_text_t* t, const char* path)
{
	FILE* fp;
	char* line = NULL;
	size_t len = 0;
	ssize_t read;
	size_t lines = 0;

	fp = fopen(path, "w");
	if (fp == NULL)
	{
		fprintf(stderr, "ERROR: Failed to open file (%s)", path);
		exit(-1);
	}

	for(size_t i = 0; i < t->size; ++i)
	{
		size_t length = strlen(t->data[i]);
		t->data[i][length] = '\n';
		fwrite(t->data[i], sizeof(char), length+1, fp);
		t->data[i][length] = '\0';
	}

	fclose(fp);
}

// Returns 1 if redraw is needed, 0 if toggle, -1 if nothing
int text_set_completion(todo_text_t* text)
{
	if (text->size != 0)
	{
		if (text->data[text->selected][1] == ' ')
		{
			text->data[text->selected][1] = 'X';
			return 0; // TOGGLE
		}
		else
		{
			text_remove(text, text->selected);
			if (text->selected == text->size)
			{
				text->selected--;
			}
			return 1; // REDRAW
		}
	}

	return -1; // NOTHING
}

int text_last_empty(todo_text_t* text)
{
	return strcmp(text->data[text->size - 1], EMPTY_TEXT) == 0;
}

#endif
