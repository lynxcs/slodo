#include "todo.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>

// Separates string into words,
static char **splitString(char *string, char separator, size_t *wordCount)
{
    size_t length = strlen(string);
    *wordCount = 0;
    char **words = malloc(sizeof(char*) * length); // Worst case: word count matches strlen length
    size_t begin = 0;
    for(size_t i = 0; i < length; ++i)
    {
        if ((string[i] == separator || string[i] == '\0') && i >= begin)
        {
            if (i == begin)
            {
                ++begin;
                continue;
            }

            words[*wordCount] = malloc(sizeof(char) * (i - begin + 1));
            strncpy(words[*wordCount], &string[begin], i - begin);
            words[*wordCount][i - begin] = '\0';
            ++(*wordCount);
            begin = i + 1;
        }
    }

    words[*wordCount] = malloc(sizeof(char) * (length - begin + 1));
    strncpy(words[*wordCount], &string[begin], length - begin);
    words[*wordCount][length - begin] = '\0';
    ++(*wordCount);
    words = realloc(words, sizeof(char*) * *wordCount);
    return words;
}

static bool hasCompletion(char *word)
{
    if (strlen(word) == 1 && word[0] == 'x')
        return true;

    return false;
}

static bool hasPriority(char *word)
{
    if (strlen(word) == 3 && isupper(word[1]) && word[0] == '(' && word[2] == ')')
        return true;

    return false;
}

static bool hasDate(char *word)
{
    return isValid(createDate(word));
}

// Returns 1 if tag is either of type `+` or `@`
// 2 if tag is of key:value type
// Returns 0 if not tag
static int hasTag(char *word)
{
    const size_t wordLength = strlen(word);
    if (wordLength > 1 && (word[0] == '+' || word[0] == '@'))
    {
        return 1;
    }

    for(size_t i = 0; i < wordLength; ++i)
    {
        if(i < wordLength - 1 && word[i] == ':')
        {
            return 2;
        }
    }

    return 0;
}

// Assumes YYYY-MM-DD format
Date createDate(char *string)
{
    Date date;
    date.year = 0;
    date.month = 0;
    date.day = 0;

    if (strlen(string) == 10)
    {
        sscanf(string, "%4"SCNu16"-%2"SCNu8"-%2"SCNu8, &date.year, &date.month, &date.day);
    }

    return date;
}

bool isValid(Date date)
{
    if (!date.year || !date.month || !date.day)
        return false;

    return true;
}

Todo *todoInit(size_t initialSize)
{
    Todo *todo = malloc(sizeof(Todo));

    if (!todo)
        return NULL;

    if (initialSize)
        todo->entries = malloc(sizeof(TodoEntry) * initialSize);
    else
        todo->entries = NULL;

    todo->capacity = initialSize;
    todo->size = 0;

    return todo;
}

TodoEntry todoParse(char *string)
{
    TodoEntry entry;
    entry.priority = 0;
    entry.completed = false;
    entry.tagCount = 0;
    entry.tags = NULL;

    size_t wordCount;
    char **words = splitString(string, ' ', &wordCount);

    // Keep track of currently processed word
    size_t currentWord = 0;

    // Check if word is completion marker
    if (currentWord < wordCount && hasCompletion(words[currentWord]))
    {
        entry.completed = true;
        ++currentWord;
    }

    // Check if word is priority marker
    if (currentWord < wordCount && hasPriority(words[currentWord]))
    {
        entry.priority = words[currentWord][1];
        ++currentWord;
    }

    // Check if current / next word is date
    if (currentWord < wordCount && hasDate(words[currentWord]))
    {
        if (currentWord + 1 < wordCount && hasDate(words[currentWord+1]))
        {
            entry.completionDate = createDate(words[currentWord]);
            entry.creationDate = createDate(words[currentWord+1]);
            ++currentWord;
        }
        else
        {
            entry.creationDate = createDate(words[currentWord]);
        }

        ++currentWord;
    }

    entry.tags = malloc(sizeof(Tag) * (wordCount - currentWord));
    entry.description = calloc(1, strlen(string) + 1);
    for(size_t i = currentWord; i < wordCount; ++i)
    {
        // Copy word to todo description
        strcat(entry.description, words[i]);
        if (i != wordCount - 1)
            strcat(entry.description, " ");

        int tagType = hasTag(words[i]);
        if (!tagType)
            continue;

        if (tagType == 2)
        {
            size_t pairCount = 0;
            char **pair = splitString(words[i], ':', &pairCount);

            entry.tags[entry.tagCount].tagKey = pair[0];
            entry.tags[entry.tagCount].tag = pair[1];
        }
        else
        {
            entry.tags[entry.tagCount].tagKey = strndup(&words[i][0], 1);
            entry.tags[entry.tagCount].tag = strdup(&words[i][1]);
        }

        ++entry.tagCount;
    }

    entry.tags = realloc(entry.tags, sizeof(Tag) * entry.tagCount);
    entry.description = realloc(entry.description, strlen(entry.description) + 1);
    return entry;
}

size_t todoAppend(Todo *todo, TodoEntry entry)
{
    if (!todo)
        return 0;

    if (todo->capacity == todo->size)
    {
        if (todo->capacity == 0)
            ++todo->capacity;

        todo->entries = realloc(todo->entries, todo->capacity * sizeof(TodoEntry) * 2);
        todo->capacity *= 2;
    }

    todo->entries[todo->size] = entry;
    return todo->size++;
}

bool todoRemove(Todo *todo, size_t index)
{
    if (index >= todo->size)
        return false;

    free(todo->entries[index].description);
    free(todo->entries[index].tags);

    if (index + 1 != todo->size)
    {
        size_t copySize = todo->size - (index + 1);
        memmove(&todo->entries[index], &todo->entries[index + 1], copySize * sizeof(TodoEntry));
    }

    --todo->size;
    return true;
}

// TODO
bool todoSetCompletion(Todo *todo, size_t index, bool completionState)
{
    if (index >= todo->size)
        return false;

    todo->entries[index].completed = completionState;
    return true;
}
