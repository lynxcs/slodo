#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

typedef struct Date
{
    uint16_t year;
    uint8_t month, day;
} Date;

bool isValid(Date date);

// Format string: YYYY-MM-DD
Date createDate(char *string);

typedef struct Tag
{
    char *tagKey; // +, @ or `key`
    char *tag; // Tag
} Tag;

typedef struct TodoEntry
{
    bool completed;
    char priority;
    Date completionDate, creationDate;
    char *description;
    Tag *tags;
    size_t tagCount;
} TodoEntry;

typedef struct Todo
{
    TodoEntry *entries;
    size_t capacity;
    size_t size;
} Todo;

// Initialize Todo struct
Todo *todoInit(size_t initialSize);

// Parse string into todo entry
TodoEntry todoParse(char *string);

// Return index of appended entry
size_t todoAppend(Todo *todo, TodoEntry entry);

// Remove entry from todo
// Returns true if successful (false otherwise)
bool todoRemove(Todo *todo, size_t entryIndex);

// Sets completion to `completionState` of entry at index
// Returns true if successful (false otherwise)
bool todoSetCompletion(Todo *todo, size_t entryIndex, bool completionState);
