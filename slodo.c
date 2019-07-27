#include <stdlib.h>
#include <inttypes.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>

#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>
#include <X11/keysym.h>
#include <X11/keysymdef.h>
#include <X11/Xlib.h>

typedef struct
{
    char** data;
    size_t capacity;
    size_t size;

    size_t selected;
} todo_text_t;

// Struct that contains everything needed for rendering a font
typedef struct
{
    xcb_gc_t font_gc;
    xcb_gc_t font_gc_inverted;
    uint16_t fontSize;
} font_full_t;

typedef struct
{
    uint16_t r, g, b;
} rgb_t ;

typedef struct
{
    uint16_t height;
    uint16_t width;
    uint16_t x;
    uint16_t y;
} window_geom_t;

// Redraw - redraw the whole window
// Move - movement, meaning that only 2 lines need to be redrawn
// Toggle - toggle completion, meaning that only 1 line needs to be redrawn
typedef enum
{
    DRAW_TYPE_REDRAW_E, DRAW_TYPE_MOVE_UP_E, DRAW_TYPE_MOVE_DOWN_E, DRAW_TYPE_TOGGLE_E, DRAW_TYPE_NOTHING_E
} draw_type_e;

static void testCookie(xcb_void_cookie_t cookie, xcb_connection_t* connection, char* errMessage)
{
    xcb_generic_error_t* error = xcb_request_check(connection, cookie);
    if (error)
    {
        fprintf(stderr, "ERROR: %s : %"PRIu8"\n", errMessage, error->error_code);
        xcb_disconnect(connection);
        exit(-1);
    }

    free(error);
}

static void drawText(xcb_connection_t* connection, xcb_screen_t* screen, xcb_window_t window, int16_t x1, int16_t y1, const char* label, xcb_gcontext_t gc)
{
    // Draw text
    xcb_void_cookie_t textCookie = xcb_image_text_8_checked(connection, strlen(label), window, gc, x1, y1, label);
    testCookie(textCookie, connection, "Can't draw text");
}

static window_geom_t get_window_geometry(xcb_connection_t* connection, xcb_window_t window)
{
    window_geom_t geom;

    xcb_get_geometry_cookie_t geom_cookie = xcb_get_geometry(connection, window);
    xcb_get_geometry_reply_t* geom_reply = xcb_get_geometry_reply(connection, geom_cookie, NULL);

    geom.x = geom_reply->x;
    geom.y = geom_reply->y;
    geom.height = geom_reply->height;
    geom.width = geom_reply->width;

    free(geom_reply);

    return geom;
}

static int get_line_count(xcb_connection_t* connection, xcb_window_t window, window_geom_t geometry, int text_height)
{
    int full_fit_count = (geometry.height - (geometry.height % text_height))/text_height;
    return full_fit_count; // Assuming that the top and bottom both have ~10px margin
}

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
    for(int i = 0; i < t->size; i++)
    {
        free(t->data[i]);
    }

    free(t->data);

    return 0;
}

int text_push_back(todo_text_t* t, const char* text)
{

    if (t->capacity == t->size)
    {
        t->data = realloc(t->data, t->capacity * sizeof(char*) * 2);
        t->capacity = t->capacity * 2;
    }

    t->data[t->size] = strdup(text);
    t->size++;

    return 0;
}

int text_push_back_empty(todo_text_t* t)
{
    if (t->capacity == t->size)
    {
        t->data = realloc(t->data, t->capacity * sizeof(char*) * 2);
        t->capacity = t->capacity * 2;
    }

    t->size++;
    return 0;
}

void text_print(todo_text_t* t)
{
    for (int i = 0; i < t->size; i++)
    {
        printf("%s\n", t->data[i]);
    }
}

int text_remove(todo_text_t* t, size_t index)
{
    if (index + 1 > t->size)
        return -1;

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

bool text_is_empty(todo_text_t* t)
{
    if (t->size == 0)
    {
        return true;
    } else
    {
        return false;
    }
}

// XCB Draw commands go here
void text_draw(xcb_connection_t* connection, xcb_screen_t* screen, xcb_window_t window, font_full_t font, todo_text_t* t, draw_type_e draw_type)
{
    window_geom_t geometry = get_window_geometry(connection, window);
    if (!text_is_empty(t))
    {
        if (draw_type == DRAW_TYPE_REDRAW_E)
        {
            /* xcb_clear_area(connection, 0, window, 0, 0, 1920, 1080); */
            xcb_clear_area(connection, 0, window, 0, 0, 300, 200);
            xcb_flush(connection);
            for (int i = 0; (i < get_line_count(connection, window, geometry, font.fontSize)) && (i < t->size); i++)
            {
                    drawText(connection, screen, window, 1, 10 + ((font.fontSize) * i), t->data[i], font.font_gc);
            }
            drawText(connection, screen, window, 1, 10 + ((font.fontSize) * t->selected), t->data[t->selected], font.font_gc_inverted);
        } else if (draw_type == DRAW_TYPE_TOGGLE_E)
        {
            /* drawText(connection, screen, window, 1, 10+ (font.fontSize * t->selected), t->data[t->selected], font.font_gc_inverted); */
            drawText(connection, screen, window, 1, 10 + ((font.fontSize) * t->selected), t->data[t->selected], font.font_gc_inverted);
        } else if (draw_type == DRAW_TYPE_MOVE_UP_E)
        {
            drawText(connection, screen, window, 1, 10+ (font.fontSize * (t->selected+1)), t->data[t->selected+1], font.font_gc);
            drawText(connection, screen, window, 1, 10 + ((font.fontSize) * t->selected), t->data[t->selected], font.font_gc_inverted);
        } else if (draw_type == DRAW_TYPE_MOVE_DOWN_E)
        {
            drawText(connection, screen, window, 1, 10+ (font.fontSize * (t->selected-1)), t->data[t->selected-1], font.font_gc);
            drawText(connection, screen, window, 1, 10 + ((font.fontSize) * t->selected), t->data[t->selected], font.font_gc_inverted);
        }
    }
    else
    {
        /* xcb_clear_area(connection, 0, window, 0, 0, 1920, 1080); */
        xcb_clear_area(connection, 0, window, 0, 0, 300, 200);
        xcb_flush(connection);
    }
}

// Swaps src text with dst text
int text_swap(todo_text_t* t, int src, int dst)
{
    if (src < 0 || dst < 0)
        return -1;

    if (src == dst)
        return 0;

    if (src >= t->size || dst >= t->size)
        return -1;

    char* temp = t->data[dst];
    t->data[dst] = t->data[src];
    t->data[src] = temp;

    return 0;
}

void text_init_from_file(todo_text_t* t, const char* path)
{
    FILE* fp;
    char* line = NULL;
    size_t len = 0;
    ssize_t read;
    size_t lines = 0;

    fp = fopen(path, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "ERROR: Failed to open file");
        exit(-1);
    }

    while (EOF != (fscanf(fp, "%*[^\n]"), fscanf(fp,"%*c")))
    {
        ++lines;
    }

    rewind(fp);

    text_init(t, lines + 2);

    while((read = getline(&line, &len, fp)) != -1)
    {
        line[strcspn(line,"\n")] = 0;
        text_push_back(t, line);
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
        fprintf(stderr, "ERROR: Failed to open file");
        exit(-1);
    }

    for(int i = 0; i < t->size; i++)
    {
        int length = strlen(t->data[i]);
        t->data[i][length] = '\n';
        fwrite(t->data[i], sizeof(char), length+1, fp);
        t->data[i][length] = '\0';
    }

    fclose(fp);
}

static font_full_t getFontFull(xcb_connection_t* connection, xcb_screen_t* screen, xcb_window_t window, const char* font_name, uint32_t background, uint32_t foreground)
{
    font_full_t fontFull;

    // Get font
    // TODO Add support for xft
    xcb_font_t font = xcb_generate_id(connection);
    xcb_void_cookie_t fontCookie = xcb_open_font_checked(connection, font, strlen(font_name), font_name);
    testCookie(fontCookie, connection, "Can't open font");

    xcb_query_font_cookie_t queryCookie = xcb_query_font(connection, font);
    xcb_query_font_reply_t* font_reply = xcb_query_font_reply(connection, queryCookie, NULL);

    fontFull.fontSize = font_reply->font_ascent + font_reply->font_descent;

    // Create graphics context
    fontFull.font_gc = xcb_generate_id(connection);
    uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
    uint32_t value_list[3] = { foreground, background, font };
    xcb_void_cookie_t gcCookie = xcb_create_gc_checked(connection, fontFull.font_gc, window, mask, value_list);
    testCookie(gcCookie, connection, "Can't create gc");

    // Create inverted graphics context
    fontFull.font_gc_inverted = xcb_generate_id(connection);
    uint32_t value_list_inverted[3] = { background, foreground, font };
    gcCookie = xcb_create_gc_checked(connection, fontFull.font_gc_inverted, window, mask, value_list_inverted);
    testCookie(gcCookie, connection, "Can't create gc");

    // Close font
    fontCookie = xcb_close_font(connection, font);
    testCookie(fontCookie, connection, "Can't close font");

    free(font_reply);

    return fontFull;
}

// Convert #RRGGBB string to color
static rgb_t hexToInt(const char* hex_rgb)
{
    rgb_t color;

    if (hex_rgb[0] != '#')
    {
        color.r = 0;
        color.g = 0;
        color.b = 0;
        return color;
    }

    char hex_str[strlen(hex_rgb) + 1];

    strcpy(&hex_str[1], hex_rgb);
    hex_str[0] = '0';
    hex_str[1] = 'x';
    uint32_t hex = strtol(hex_str, NULL, 16);

    color.r = ((hex >> 16) & 0xff) * 257;
    color.g = ((hex >> 8) & 0xff) * 257;
    color.b = ((hex) & 0xff) * 257;

    return color;
}

static uint32_t getColorPixel(xcb_connection_t* connection, xcb_screen_t* screen, const char* hex_rgb)
{
    uint32_t pixel;

    rgb_t color = hexToInt(hex_rgb);

    xcb_alloc_color_reply_t* reply = xcb_alloc_color_reply(connection, xcb_alloc_color(connection, screen->default_colormap, color.r, color.g, color.b), NULL);
    pixel = reply->pixel;
    free(reply);

    return pixel;
}

draw_type_e text_set_completion(todo_text_t* text)
{
    if (text->size != 0)
    {
        if (text->data[text->selected][1] == ' ')
        {
            text->data[text->selected][1] = 'X';
            return DRAW_TYPE_TOGGLE_E;
        } else
        {
            text_remove(text, text->selected);
            if (text->selected == text->size)
            {
                text->selected--;
            }
            return DRAW_TYPE_REDRAW_E;
        }
    }

    return DRAW_TYPE_NOTHING_E;
}

int main() {
    todo_text_t text;
    text_init_from_file(&text, "/home/void/notes/todo");

    // Get connection
    int screenNum;
    xcb_connection_t* connection = xcb_connect(NULL, &screenNum);
    if (!connection) {
        fprintf(stderr, "ERROR: Can't connect to an X server\n");
        return -1;
    }

    // Get current screen
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(connection));
    xcb_key_symbols_t* key_syms = xcb_key_symbols_alloc(connection);

    // We want the screen at index screenNum of the iterator
    for (int i = 0; i < screenNum; ++i) {
        xcb_screen_next(&iter);
    }

    xcb_screen_t* screen = iter.data;

    if (!screen) {
        fprintf(stderr, "ERROR: Can't get the current screen!\n");
        xcb_disconnect(connection);
        return -1;
    }

    xcb_window_t window = xcb_generate_id(connection);
    /* xcb_window_t window = screen->root; */

    /* xcb_key_symbols_t* syms = xcb_key_symbols_alloc(connection); */

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2];

    uint32_t backgroundPixel = getColorPixel(connection, screen, "#212626");
    uint32_t foregroundPixel = getColorPixel(connection, screen, "#dacea6");

    values[0] = backgroundPixel;
    values[1] = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE;

    xcb_void_cookie_t windowCookie = xcb_create_window_checked(connection,                    // Connection
                                                               screen->root_depth,            // Depth (same as root)
                                                               window,                        // Window Id
                                                               screen->root,                  // Parent window
                                                               1604, 12,                      // x, y
                                                               300, 200,                      // width, height
                                                               0,                             // border width
                                                               XCB_WINDOW_CLASS_INPUT_OUTPUT, // class
                                                               screen->root_visual,           // visual
                                                               mask, values);                 // masks, not used yet
    testCookie(windowCookie, connection, "Can't create window");

    xcb_void_cookie_t mapCookie = xcb_map_window_checked(connection, window);
    testCookie(mapCookie, connection, "Can't map window");

    char* label = "Slodo";
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(label), label);
    xcb_change_property(connection, XCB_PROP_MODE_REPLACE, window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, strlen(label), label);

    // Get graphics context
    font_full_t font = getFontFull(connection, screen, window, "fixed", backgroundPixel, foregroundPixel);

    // Make sure the commands are sent
    xcb_flush(connection);

    xcb_generic_event_t* event;
    enum
    {
        TODO_MANAGE_E, TODO_WRITE_E
    } current_state;

    current_state = TODO_MANAGE_E;

    bool upper_case = false;
    while(1)
    {
        if ( (event = xcb_wait_for_event(connection)) )
        {
            switch (event->response_type & ~0x80)
            {
                case XCB_EXPOSE:
                    text_draw(connection, screen, window, font, &text, DRAW_TYPE_REDRAW_E);
                    break;
                case XCB_KEY_RELEASE:
                    {
                        xcb_key_release_event_t* kr = (xcb_key_release_event_t*) event;
                        if (kr->detail == 50)
                        {
                            upper_case = false;
                        }
                        break;
                    }
                case XCB_KEY_PRESS:
                    {
                        xcb_key_press_event_t* kr = (xcb_key_press_event_t*) event;

                        if (kr->detail == 9)
                        {
                            if (current_state == TODO_WRITE_E)
                            {
                                if (strcmp(text.data[text.size-1], "[ ] ") == 0)
                                {
                                    text_remove(&text, text.size - 1);
                                }
                            }

                            free(event);
                            xcb_free_gc(connection, font.font_gc);
                            xcb_free_gc(connection, font.font_gc_inverted);
                            xcb_key_symbols_free(key_syms);
                            xcb_disconnect(connection);

                            if (!upper_case)
                                text_commit_to_file(&text, "/home/void/notes/todo");

                            text_free(&text);
                            return 0;
                        }

                        if (kr->detail == 50)
                        {
                            upper_case = true;
                        }
                        else
                        if (current_state == TODO_WRITE_E)
                        {
                            // TODO Figure out a way to allow infinite writing
                            // Maybe a helper "line" class?
                            static uint8_t current_char = 0;
                            xcb_keysym_t y = xcb_key_press_lookup_keysym(key_syms, kr, (int) upper_case);

                            if (kr->detail == 36) // Enter key
                            {
                                current_char = 0;
                                current_state = TODO_MANAGE_E;

                                // Nothing was typed, so we don't save it
                                if (strcmp(text.data[text.size-1], "[ ] ") == 0)
                                {
                                    text_remove(&text, text.size - 1);
                                    drawText(connection, screen, window, 1, 10+ (font.fontSize * (text.size)), "     ", font.font_gc);
                                }

                                text.selected = text.size-1;
                                drawText(connection, screen, window, 1, 10+ (font.fontSize * (text.selected)), text.data[text.selected], font.font_gc_inverted);
                            } else
                            {
                                char* string = XKeysymToString(y);

                                int offset = 0; // TODO Introduce offset when text passes border of window
                                if (y == 0 || strcmp(string, "space") == 0)
                                {
                                    text.data[text.size-1][current_char+4] = ' ';
                                    text.data[text.size-1][current_char+5] = '\0';
                                    drawText(connection, screen, window, 1, 10+ (font.fontSize * (text.size - 1)), &text.data[text.size-1][offset], font.font_gc);
                                    current_char++;
                                    drawText(connection, screen, window, 1 + ((font.fontSize-7) * (strlen(text.data[text.size-1]))), 10+ (font.fontSize * (text.size - 1)), " ", font.font_gc_inverted);
                                } else if (strcmp(string, "BackSpace") == 0)
                                {
                                    if (current_char != 0)
                                    {
                                        drawText(connection, screen, window, 1 + ((font.fontSize-7) * (strlen(text.data[text.size-1]))), 10+ (font.fontSize * (text.size - 1)), " ", font.font_gc);
                                        text.data[text.size-1][current_char+4] = '\0';
                                        text.data[text.size-1][current_char+3] = ' ';
                                        drawText(connection, screen, window, 1, 10+ (font.fontSize * (text.size - 1)), &text.data[text.size-1][offset], font.font_gc);

                                        text.data[text.size-1][current_char+3] = '\0';
                                        current_char--;
                                        drawText(connection, screen, window, 1 + ((font.fontSize-7) * (strlen(text.data[text.size-1]))), 10+ (font.fontSize * (text.size - 1)), " ", font.font_gc_inverted);
                                    }
                                } else
                                {
                                    text.data[text.size-1][current_char+4] = y;
                                    text.data[text.size-1][current_char+5] = '\0';
                                    drawText(connection, screen, window, 1, 10+ (font.fontSize * (text.size - 1)), text.data[text.size-1], font.font_gc);
                                    current_char++;
                                    drawText(connection, screen, window, 1 + ((font.fontSize-7) * (strlen(text.data[text.size-1]))), 10+ (font.fontSize * (text.size - 1)), " ", font.font_gc_inverted);
                                }
                            }

                        }
                        else // State = TODO_MANAGE_E
                        {
                            if (kr->detail == 32) // O
                            {
                                current_state = TODO_WRITE_E;

                                // Remove selection marker if present
                                if (text.size != 0)
                                {
                                    drawText(connection, screen, window, 1, 10+ (font.fontSize * (text.selected)), text.data[text.selected], font.font_gc);
                                }

                                text_push_back_empty(&text);
                                text.data[text.size - 1] = (char*) malloc(sizeof(char) * 255);
                                strcpy(text.data[text.size-1], "[ ] ");

                                drawText(connection, screen, window, 1, 10+ (font.fontSize * (text.size - 1)), "[ ] ", font.font_gc);
                                drawText(connection, screen, window, 1 + ((font.fontSize-7) * (strlen(text.data[text.size-1]))), 10+ (font.fontSize * (text.size - 1)), " ", font.font_gc_inverted);
                            }
                            if (!text_is_empty(&text))
                            {
                                if (kr->detail == 111 || kr->detail == 45) // Up key Or K
                                {
                                    if (upper_case)
                                    {
                                        text_swap(&text, text.selected, text.selected-1);
                                        if (text.selected != 0)
                                        {
                                            text.selected--;
                                        }
                                        text_draw(connection, screen, window, font, &text, DRAW_TYPE_REDRAW_E);
                                    }
                                    else
                                    {
                                        if (text.selected != 0)
                                        {
                                            text.selected--;
                                        }
                                        text_draw(connection, screen, window, font, &text, DRAW_TYPE_MOVE_UP_E);
                                    }
                                }
                                else if (kr->detail == 116 || kr->detail == 44) // Down key Or J
                                {
                                    if (upper_case)
                                    {
                                        text_swap(&text, text.selected, text.selected + 1);
                                        if (text.selected + 1 < text.size)
                                        {
                                            text.selected++;
                                        }
                                        text_draw(connection, screen, window, font, &text, DRAW_TYPE_REDRAW_E);
                                    }
                                    else
                                    {
                                        if (text.selected + 1 < text.size)
                                        {
                                            text.selected++;
                                        }
                                        text_draw(connection, screen, window, font, &text, DRAW_TYPE_MOVE_DOWN_E);
                                    }
                                }
                                else if (kr->detail == 36 || kr->detail == 46) // Enter key Or L
                                {
                                    draw_type_e draw_type = text_set_completion(&text);
                                    text_draw(connection, screen, window, font, &text, draw_type);
                                }
                            }
                        }
                    }
            }
            free(event);
        }
    }

    return 0;
}
