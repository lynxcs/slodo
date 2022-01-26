#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <inttypes.h>

typedef struct Date
{
    uint16_t year;
    uint8_t month, day;
} Date;

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
    TodoEntry *entries; // Entry array
    size_t capacity; // How many entries can currently be held
    size_t size; // How many entries there are
} Todo;

// Initialize Todo struct
int todoInit(Todo *todo, size_t initialSize);

// Initialize Todo struct from file
int todoInitFromFile(char *filename, Todo *todo);

// Save Todo struct to file
int todoSaveToFile(char *filename, Todo *todo);

// Parse string into todo entry
TodoEntry todoParse(char *string);

// Return index of appended entry
size_t todoAppend(Todo *todo, TodoEntry entry);

// Sets completion to `completionState` of entry at index
// Returns true if successful (false otherwise)
bool todoSetCompletion(Todo *todo, size_t entryIndex, bool completionState);

// Remove entry from todo
// Returns true if successful (false otherwise)
bool todoRemove(Todo *todo, size_t entryIndex);

// Free todo struct
void todoFree(Todo *todo);
