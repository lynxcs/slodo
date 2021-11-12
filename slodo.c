#include <stdbool.h>

#include <X11/Xlib.h>
#include <xcb/xcb.h>
#include <xcb/xcb_keysyms.h>

#include "text.h"

// Modify background and foreground colors if needed
#define BG_COLOR "#262626"
#define FG_COLOR "#dacea6"
#define FONT_NAME "fixed"

#define SHIFT_KEY 50
#define ESCAPE_KEY 9

// Struct that contains everything needed for rendering a font
typedef struct
{
    xcb_gc_t font_gc;
    xcb_gc_t font_gc_inverted;
    uint16_t font_size;
} font_full_t;

typedef struct
{
    uint16_t r, g, b;
} rgb_t;

typedef struct
{
    uint16_t width, height, x, y;
} window_geom_t;

typedef struct
{
    xcb_connection_t* connection;
    xcb_screen_t* screen;
    xcb_window_t window;
} xcb_main;

void test_cookie(xcb_main main, xcb_void_cookie_t cookie, char* err_msg)
{
    xcb_generic_error_t* error = xcb_request_check(main.connection, cookie);
    if (error)
    {
        fprintf(stderr, "ERROR: %s : %u\n", err_msg, error->error_code);
        xcb_disconnect(main.connection);
        exit(-1);
    }

    free(error);
}

void draw_text_internal(xcb_main main, int16_t x1, int16_t y1, const char* label, xcb_gcontext_t gc)
{
    xcb_void_cookie_t text_cookie = xcb_image_text_8_checked(main.connection, strlen(label), main.window, gc, x1, y1, label);
    test_cookie(main, text_cookie, "Can't draw text");
}

window_geom_t get_window_geometry(xcb_main main)
{
    xcb_get_geometry_cookie_t geom_cookie = xcb_get_geometry(main.connection, main.window);
    xcb_get_geometry_reply_t* geom_reply = xcb_get_geometry_reply(main.connection, geom_cookie, NULL);

    window_geom_t geom;
    geom.x = geom_reply->x;
    geom.y = geom_reply->y;
    geom.height = geom_reply->height;
    geom.width = geom_reply->width;

    free(geom_reply);
    return geom;
}

int get_line_count(xcb_main main, int text_height)
{
    // TODO Allow for custom margins
    window_geom_t geometry = get_window_geometry(main);
    int full_fit_count = (geometry.height - (geometry.height % text_height))/text_height;
    return full_fit_count; // Assuming that the top and bottom both have ~10px margin
}

int get_char_count(xcb_main main, int text_width)
{
    window_geom_t geometry = get_window_geometry(main);
    text_width -= 7; // Counteracts font kerning?
    int full_fit_count = (geometry.width - (geometry.width % text_width))/text_width;
    return full_fit_count;
}

int text_draw_base(xcb_main main, font_full_t font, todo_text_t* t, bool flush)
{
    if (flush)
    {
        window_geom_t geometry = get_window_geometry(main);
        xcb_clear_area(main.connection, 0, main.window, 0, 0, geometry.width, geometry.height);
        xcb_flush(main.connection);
    }

    if (t->size)
        return 1;

    return 0;
}

// XCB Draw commands go here
void text_draw_redraw(xcb_main main, font_full_t font, todo_text_t* t)
{
    if(text_draw_base(main, font, t, true))
    {
        for (int i = 0; (i < get_line_count(main, font.font_size)) && (i < t->size); ++i)
        {
            draw_text_internal(main, 1, 10 + ((font.font_size) * i), t->data[i], font.font_gc);
        }
        draw_text_internal(main, 1, 10 + ((font.font_size) * t->selected), t->data[t->selected], font.font_gc_inverted);
    }
}

void text_draw_redraw_write(xcb_main main, font_full_t font, todo_text_t* t)
{
    if(text_draw_base(main, font, t, true))
    {
        for (int i = 0; (i < get_line_count(main, font.font_size)) && (i < t->size); ++i)
        {
            draw_text_internal(main, 1, 10 + ((font.font_size) * i), t->data[i], font.font_gc);
        }
        draw_text_internal(main, 1 + ((font.font_size-7) * (strlen(t->data[t->size-1]))), 10+ (font.font_size * (t->size - 1)), " ", font.font_gc_inverted);
    }
}
void text_draw_toggle(xcb_main main, font_full_t font, todo_text_t* t)
{
    if(text_draw_base(main, font, t, false))
    {
        draw_text_internal(main, 1, 10 + ((font.font_size) * t->selected), t->data[t->selected], font.font_gc_inverted);
    }
}
void text_draw_move(xcb_main main, font_full_t font, todo_text_t* t, bool up)
{
    if(text_draw_base(main, font, t, false))
    {
        if (up)
            draw_text_internal(main, 1, 10+ (font.font_size * (t->selected+1)), t->data[t->selected+1], font.font_gc);
        else
            draw_text_internal(main, 1, 10+ (font.font_size * (t->selected-1)), t->data[t->selected-1], font.font_gc);
        draw_text_internal(main, 1, 10 + ((font.font_size) * t->selected), t->data[t->selected], font.font_gc_inverted);
    }
}

font_full_t get_font_full(xcb_main main, const char* font_name, uint32_t background, uint32_t foreground)
{
    font_full_t font_full;

    // Get font
    // TODO Add support for xft
    xcb_font_t font = xcb_generate_id(main.connection);
    xcb_void_cookie_t font_cookie = xcb_open_font_checked(main.connection, font, strlen(font_name), font_name);
    test_cookie(main, font_cookie, "Can't open font");

    xcb_query_font_cookie_t query_cookie = xcb_query_font(main.connection, font);
    xcb_query_font_reply_t* font_reply = xcb_query_font_reply(main.connection, query_cookie, NULL);

    font_full.font_size = font_reply->font_ascent + font_reply->font_descent;

    // Create graphics context
    if (background != 0 && foreground != 0)
    {
        font_full.font_gc = xcb_generate_id(main.connection);
        uint32_t mask = XCB_GC_FOREGROUND | XCB_GC_BACKGROUND | XCB_GC_FONT;
        uint32_t value_list[3] = { foreground, background, font };
        xcb_void_cookie_t gc_cookie = xcb_create_gc_checked(main.connection, font_full.font_gc, main.window, mask, value_list);
        test_cookie(main, gc_cookie, "Can't create gc");

        // Create inverted graphics context
        font_full.font_gc_inverted = xcb_generate_id(main.connection);
        uint32_t value_list_inverted[3] = { background, foreground, font };
        gc_cookie = xcb_create_gc_checked(main.connection, font_full.font_gc_inverted, main.window, mask, value_list_inverted);
        test_cookie(main, gc_cookie, "Can't create gc");
    }
    // Close font
    font_cookie = xcb_close_font(main.connection, font);
    test_cookie(main, font_cookie, "Can't close font");

    free(font_reply);
    return font_full;
}

rgb_t hex_to_int(const char* hex_rgb)
{
    rgb_t color;
    if (hex_rgb[0] != '#')
    {
        color.r = 0;
        color.g = 0;
        color.b = 0;
    }
    else
    {
        char hex_str[strlen(hex_rgb) + 2];
        strncpy(&hex_str[1], hex_rgb, 7);
        hex_str[0] = '0';
        hex_str[1] = 'x';
        hex_str[8] = '\0';
        uint32_t hex = strtol(hex_str, NULL, 16);

        color.r = ((hex >> 16) & 0xff) * 257;
        color.g = ((hex >> 8) & 0xff) * 257;
        color.b = ((hex) & 0xff) * 257;
    }

    return color;
}

uint32_t get_color_pixel(xcb_main main, const char* hex_rgb)
{
    rgb_t color = hex_to_int(hex_rgb);
    xcb_alloc_color_reply_t* reply = xcb_alloc_color_reply(main.connection, xcb_alloc_color(main.connection, main.screen->default_colormap, color.r, color.g, color.b), NULL);
    if (!reply)
    {
        fprintf(stderr, "Error creating color pixel!\n");
        xcb_disconnect(main.connection);
        exit(-1);
    }

    uint32_t pixel = reply->pixel;
    free(reply);
    return pixel;
}

// Creates the needed xcb_main struct, ensuring correct width, height, etc.
xcb_main create_xcb_main(int line_count)
{
    xcb_main main;

    // Get connection
    int screen_num;
    main.connection = xcb_connect(NULL, &screen_num);
    if (!main.connection)
    {
        fprintf(stderr, "ERROR: Can't connect to an X server\n");
        exit(-1);
    }

    // We want the screen at index screenNum of the iterator
    xcb_screen_iterator_t iter = xcb_setup_roots_iterator(xcb_get_setup(main.connection));
    for (int i = 0; i < screen_num; ++i)
    {
        xcb_screen_next(&iter);
    }

    // Set current screen
    main.screen = iter.data;
    if (!main.screen)
    {
        fprintf(stderr, "ERROR: Can't get the current screen!\n");
        xcb_disconnect(main.connection);
        exit(-1);
    }

    main.window = xcb_generate_id(main.connection);

    uint32_t mask = XCB_CW_BACK_PIXEL | XCB_CW_EVENT_MASK;
    uint32_t values[2];

    char* background_color = BG_COLOR;
    values[0] = get_color_pixel(main, background_color);
    values[1] = XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE | XCB_EVENT_MASK_EXPOSURE;

    font_full_t font = get_font_full(main, FONT_NAME, 0, 0);

    xcb_void_cookie_t windowCookie = xcb_create_window_checked(main.connection,
            main.screen->root_depth,
            main.window,
            main.screen->root,
            1604, 32,                             // x, y screen coordinates TODO: calculate these by using screen resolution
            300, (line_count+1) * font.font_size, // width, height
            0,                                    // border width
            XCB_WINDOW_CLASS_INPUT_OUTPUT,
            main.screen->root_visual,
            mask, values);
    test_cookie(main, windowCookie, "Can't create window");

    xcb_void_cookie_t mapCookie = xcb_map_window_checked(main.connection, main.window);
    test_cookie(main, mapCookie, "Can't map window");

    // Add label to make it easier to set custom rules for window managers
    char* label = "Slodo";
    xcb_change_property(main.connection, XCB_PROP_MODE_REPLACE, main.window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8, strlen(label), label);
    xcb_change_property(main.connection, XCB_PROP_MODE_REPLACE, main.window, XCB_ATOM_WM_CLASS, XCB_ATOM_STRING, 8, strlen(label), label);

    return main;
}

// Increase window size by one text line height
void increase_window_size(xcb_main main, font_full_t font)
{
    uint32_t value =  get_window_geometry(main).height + font.font_size;
    value = value < font.font_size ? font.font_size : value;
    xcb_configure_window(main.connection, main.window, XCB_CONFIG_WINDOW_HEIGHT, &value);
}

// Decrease window size by one text line height
void decrease_window_size(xcb_main main, font_full_t font)
{
    uint32_t value =  get_window_geometry(main).height - font.font_size;
    value = value < font.font_size ? font.font_size : value;
    xcb_configure_window(main.connection, main.window, XCB_CONFIG_WINDOW_HEIGHT, &value);
}

// Returns true if mode is write, false if manage
bool process_event_write(xcb_main main, xcb_key_press_event_t *kp, xcb_key_symbols_t *key_syms, font_full_t font, todo_text_t* text, bool upper_case)
{
    static uint8_t current_char = 0;
    if (kp->detail == 36) // Enter key
    {
        current_char = 0;

        draw_text_internal(main, 1 + ((font.font_size-7) * (strlen(text->data[text->size-1]))), 10+ (font.font_size * (text->size - 1)), " ", font.font_gc);

        // Nothing was typed, so we don't save it
        if (text_last_empty(text))
        {
            text_remove(text, text->size - 1);
            draw_text_internal(main, 1, 10+ (font.font_size * (text->size)), "     ", font.font_gc);
            decrease_window_size(main, font);
        }

        if (text->size != 0)
        {
            text->selected = text->size-1;
            draw_text_internal(main, 1, 10+ (font.font_size * (text->selected)), text->data[text->selected], font.font_gc_inverted);
        }
        else
        {
            decrease_window_size(main, font);
        }
        draw_text_internal(main, 1, 10 + (font.font_size * text->size), "NORMAL", font.font_gc);

        return false;
    }

    xcb_keysym_t y = xcb_key_press_lookup_keysym(key_syms, kp, (int) upper_case);
    char* string = XKeysymToString(y);

    // TODO Allow user to scroll right of current line
    int offset = current_char + 6 - get_char_count(main, font.font_size);
    offset = offset < 0 ? 0 : offset;

    if (y == 0 || strcmp(string, "space") == 0)
    {
        text->data[text->size-1][current_char+4] = ' ';
        text->data[text->size-1][current_char+5] = '\0';
        draw_text_internal(main, 1, 10+ (font.font_size * (text->size - 1)), &text->data[text->size-1][offset], font.font_gc);
        current_char++;
        draw_text_internal(main, 1 + ((font.font_size-7) * (strlen(text->data[text->size-1]))), 10+ (font.font_size * (text->size - 1)), " ", font.font_gc_inverted);
    }
    else if (strcmp(string, "BackSpace") == 0)
    {
        if (current_char != 0)
        {
            offset -= 2; // Don't know why it's -2, just how it is.
            offset = offset < 0 ? 0 : offset;

            draw_text_internal(main, 1 + ((font.font_size-7) * (strlen(text->data[text->size-1]))), 10+ (font.font_size * (text->size - 1)), " ", font.font_gc);
            text->data[text->size-1][current_char+3] = '\0';
            draw_text_internal(main, 1, 10+ (font.font_size * (text->size - 1)), &text->data[text->size-1][offset], font.font_gc);
            draw_text_internal(main, 1 + ((font.font_size-7) * (strlen(text->data[text->size-1]))), 10+ (font.font_size * (text->size - 1)), " ", font.font_gc_inverted);

            current_char--;
        }
    }
    else
    {
        text->data[text->size-1][current_char+4] = y;
        text->data[text->size-1][current_char+5] = '\0';
        draw_text_internal(main, 1, 10+ (font.font_size * (text->size - 1)), &text->data[text->size-1][offset], font.font_gc);
        current_char++;
        draw_text_internal(main, 1 + ((font.font_size-7) * (strlen(text->data[text->size-1]))), 10+ (font.font_size * (text->size - 1)), " ", font.font_gc_inverted);
    }

    draw_text_internal(main, 1, 10 + (font.font_size * text->size), "INSERT", font.font_gc);

    return true;
}

bool process_event_manage(xcb_main main, xcb_key_press_event_t *kp, font_full_t font, todo_text_t* text, bool upper_case)
{
    if (kp->detail == 32) // O
    {
        // Remove selection marker if present
        if (text->size != 0)
        {
            text_draw_base(main, font, text, text->size);
            draw_text_internal(main, 1, 10+ (font.font_size * (text->selected)), text->data[text->selected], font.font_gc);
        }
        increase_window_size(main, font);

        text_append(text, NULL);
        text->data[text->size - 1] = (char*) malloc(sizeof(char) * 255);
        /* strcpy(text->data[text->size-1], "[ ] "); */
        strncpy(text->data[text->size-1], "[ ] ", 5);

        draw_text_internal(main, 1, 10 + (font.font_size * (text->size - 1)), "[ ] ", font.font_gc);
        draw_text_internal(main, 1 + ((font.font_size-7) * (strlen(text->data[text->size-1]))), 10+ (font.font_size * (text->size - 1)), " ", font.font_gc_inverted);
        draw_text_internal(main, 1, 10 + (font.font_size * text->size), "INSERT", font.font_gc);

        return true;
    }

    if (text->size)
    {
        if (kp->detail == 45 && text->selected > 0) // K
        {
            --text->selected;

            if (upper_case)
            {
                text_swap(text, text->selected+1, text->selected);
                text_draw_redraw(main, font, text);
            }
            text_draw_move(main, font, text, true);
        }
        else if (kp->detail == 44 && text->selected + 1 < text->size) // J
        {
            ++text->selected;
            if (upper_case)
            {
                text_swap(text, text->selected - 1, text->selected);
                text_draw_redraw(main, font, text);
            }

            text_draw_move(main, font, text, false);
        }
        else if (kp->detail == 40) // D
        {
            if (text_set_completion(text))
            {
                decrease_window_size(main, font);
                text_draw_redraw(main, font, text);
            }
            else
                text_draw_toggle(main, font, text);
        }

        draw_text_internal(main, 1, 10 + (font.font_size * text->size), "NORMAL", font.font_gc);
    }

    return false;
}

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "TODO file not specified!\n");
        fprintf(stderr, "Usage: slodo <FILE>\n");
        return -1;
    }
    char *todo_file = argv[1]; 
    todo_text_t text;
    text_init_from_file(&text, todo_file);

    xcb_main main = create_xcb_main(text.size);
    xcb_key_symbols_t *key_syms = xcb_key_symbols_alloc(main.connection);

    // Set background and foreground color
    uint32_t background_pixel = get_color_pixel(main, BG_COLOR);
    uint32_t foreground_pixel = get_color_pixel(main, FG_COLOR);

    font_full_t font = get_font_full(main, FONT_NAME, background_pixel, foreground_pixel);

    // Make sure the commands are sent
    xcb_flush(main.connection);

    text_draw_base(main, font, &text, true);
    text_draw_redraw(main, font, &text);
    draw_text_internal(main, 1, 10 + (font.font_size * text.size), "NORMAL", font.font_gc);

    xcb_generic_event_t *event;

    bool write = false;
    bool upper_case = false;
    while(1)
    {
        if ((event = xcb_wait_for_event(main.connection)))
        {
            uint8_t type = event->response_type & ~0x80;
            if (type == XCB_EXPOSE && text.size >= 1)
            {
                if (write)
                    text_draw_redraw_write(main, font, &text);
                else
                    text_draw_redraw(main, font, &text);

                char* mode_text = write ? "INSERT" : "NORMAL";
                draw_text_internal(main, 1, 10 + (font.font_size * text.size), mode_text, font.font_gc);
            }
            else if (type == XCB_KEY_RELEASE && ((xcb_key_release_event_t*) event)->detail == SHIFT_KEY)
                upper_case = false;
            else if (type == XCB_KEY_PRESS)
            {
                xcb_key_press_event_t *kp = (xcb_key_press_event_t*) event;
                if (kp->detail == SHIFT_KEY)
                    upper_case = true;
                else if (kp->detail == ESCAPE_KEY)
                {
                    if (write && text_last_empty(&text))
                    {
                        text_remove(&text, text.size - 1);
                    }

                    break;
                }
                else if (write)
                    write = process_event_write(main, (xcb_key_press_event_t*) event, key_syms, font, &text, upper_case);
                else
                    write = process_event_manage(main, (xcb_key_press_event_t*) event, font, &text, upper_case);
            }

            free(event);
        }
    }

    free(event);
    xcb_free_gc(main.connection, font.font_gc);
    xcb_free_gc(main.connection, font.font_gc_inverted);
    xcb_key_symbols_free(key_syms);
    xcb_disconnect(main.connection);

    if (!upper_case)
    {
        text_commit_to_file(&text, todo_file);
    }

    text_free(&text);
    return 0;
}
