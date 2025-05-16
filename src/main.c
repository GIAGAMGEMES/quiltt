#include <gtk/gtk.h>
#include <vte/vte.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>

#define QUILTT_TERMINAL_ID "quiltt"

typedef struct {
    char *font;
    GdkRGBA foreground;
    GdkRGBA background;
    GdkRGBA cursor;
    GdkRGBA highlight;
    GdkRGBA palette[16];
    int scrollback_lines;
    gboolean bold_is_bright;
    gboolean cursor_blink;
    int reload_interval;
} Config;

static Config config;

void init_config() {
    config.font = strdup("Monospace 12");
    config.foreground = (GdkRGBA){0.8, 0.8, 0.8, 1.0};
    config.background = (GdkRGBA){0.0, 0.0, 0.0, 1.0};
    config.cursor = (GdkRGBA){0.8, 0.8, 0.8, 1.0};
    config.highlight = (GdkRGBA){0.2, 0.2, 0.4, 1.0};
    config.scrollback_lines = 10000;
    config.bold_is_bright = TRUE;
    config.cursor_blink = TRUE;
    config.reload_interval = 1;
}

gboolean hex_to_rgba(const char *hex, GdkRGBA *color) {
    if (!hex || hex[0] != '#') return FALSE;

    int len = strlen(hex);
    if (len != 7 && len != 9) return FALSE;

    char *endptr;
    long hex_value = strtol(hex + 1, &endptr, 16);
    if (*endptr != '\0') return FALSE;

    if (len == 7) {
        color->red = ((hex_value >> 16) & 0xFF) / 255.0;
        color->green = ((hex_value >> 8) & 0xFF) / 255.0;
        color->blue = (hex_value & 0xFF) / 255.0;
        color->alpha = 1.0;
    } else {
        color->red = ((hex_value >> 24) & 0xFF) / 255.0;
        color->green = ((hex_value >> 16) & 0xFF) / 255.0;
        color->blue = ((hex_value >> 8) & 0xFF) / 255.0;
        color->alpha = (hex_value & 0xFF) / 255.0;
    }

    return TRUE;
}

void init_default_palette() {
    hex_to_rgba("#000000", &config.palette[0]);
    hex_to_rgba("#800000", &config.palette[1]);
    hex_to_rgba("#008000", &config.palette[2]);
    hex_to_rgba("#808000", &config.palette[3]);
    hex_to_rgba("#000080", &config.palette[4]);
    hex_to_rgba("#800080", &config.palette[5]);
    hex_to_rgba("#008080", &config.palette[6]);
    hex_to_rgba("#c0c0c0", &config.palette[7]);
    hex_to_rgba("#808080", &config.palette[8]);
    hex_to_rgba("#ff0000", &config.palette[9]);
    hex_to_rgba("#00ff00", &config.palette[10]);
    hex_to_rgba("#ffff00", &config.palette[11]);
    hex_to_rgba("#0000ff", &config.palette[12]);
    hex_to_rgba("#ff00ff", &config.palette[13]);
    hex_to_rgba("#00ffff", &config.palette[14]);
    hex_to_rgba("#ffffff", &config.palette[15]);
}

static void set_color_from_string(lua_State *L, int index, GdkRGBA *color) {
    const char *hex = lua_tostring(L, index);
    if (!hex_to_rgba(hex, color)) {
        g_printerr("Invalid color format: %s\n", hex);
    }
}

static void load_palette(lua_State *L) {
    lua_getglobal(L, "palette");
    if (lua_istable(L, -1)) {
        for (int i = 0; i < 16; i++) {
            lua_rawgeti(L, -1, i + 1);
            if (lua_isstring(L, -1)) {
                set_color_from_string(L, -1, &config.palette[i]);
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
}

void load_config() {
    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw->pw_dir;
    }

    char config_path[1024];
    snprintf(config_path, sizeof(config_path), "%s/.config/quiltt/config.lua", home);

    if (access(config_path, F_OK) != 0) {
        g_printerr("Config file not found: %s\n", config_path);
        return;
    }

    lua_State *L = luaL_newstate();
    luaL_openlibs(L);
    init_default_palette();

    if (luaL_loadfile(L, config_path) || lua_pcall(L, 0, 0, 0)) {
        g_printerr("Error loading config: %s\n", lua_tostring(L, -1));
        lua_close(L);
        return;
    }

    // Get reload interval
    lua_getglobal(L, "config_reload_interval");
    if (lua_isnumber(L, -1)) {
        config.reload_interval = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    // Get font
    lua_getglobal(L, "font");
    if (lua_isstring(L, -1)) {
        free(config.font);
        config.font = strdup(lua_tostring(L, -1));
    }
    lua_pop(L, 1);

    // Get colors
    lua_getglobal(L, "foreground");
    if (lua_isstring(L, -1)) {
        set_color_from_string(L, -1, &config.foreground);
    }
    lua_pop(L, 1);

    lua_getglobal(L, "background");
    if (lua_isstring(L, -1)) {
        set_color_from_string(L, -1, &config.background);
    }
    lua_pop(L, 1);

    lua_getglobal(L, "cursor");
    if (lua_isstring(L, -1)) {
        set_color_from_string(L, -1, &config.cursor);
    }
    lua_pop(L, 1);

    lua_getglobal(L, "highlight");
    if (lua_isstring(L, -1)) {
        set_color_from_string(L, -1, &config.highlight);
    }
    lua_pop(L, 1);

    load_palette(L);

    // Get other settings
    lua_getglobal(L, "scrollback_lines");
    if (lua_isnumber(L, -1)) {
        config.scrollback_lines = lua_tointeger(L, -1);
    }
    lua_pop(L, 1);

    lua_getglobal(L, "bold_is_bright");
    if (lua_isboolean(L, -1)) {
        config.bold_is_bright = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    lua_getglobal(L, "cursor_blink");
    if (lua_isboolean(L, -1)) {
        config.cursor_blink = lua_toboolean(L, -1);
    }
    lua_pop(L, 1);

    lua_close(L);
}

static void apply_config(VteTerminal *terminal) {
    PangoFontDescription *font_desc = pango_font_description_from_string(config.font);
    vte_terminal_set_font(terminal, font_desc);
    pango_font_description_free(font_desc);

    vte_terminal_set_color_foreground(terminal, &config.foreground);
    vte_terminal_set_color_background(terminal, &config.background);
    vte_terminal_set_color_cursor(terminal, &config.cursor);
    vte_terminal_set_color_highlight(terminal, &config.highlight);

    // Set the 16-color palette at once
    vte_terminal_set_colors(terminal,
                              NULL, // foreground colors (can be NULL)
                              NULL, // background colors (can be NULL)
                              config.palette, // palette colors
                              16);            // number of palette colors

    vte_terminal_set_scrollback_lines(terminal, config.scrollback_lines);
    vte_terminal_set_bold_is_bright(terminal, config.bold_is_bright);
    vte_terminal_set_cursor_blink_mode(terminal,
        config.cursor_blink ? VTE_CURSOR_BLINK_ON : VTE_CURSOR_BLINK_OFF);
}

static gboolean check_config_file(gpointer user_data) {
    static time_t last_mtime = 0;
    struct stat st;
    char config_path[1024];
    VteTerminal *terminal = VTE_TERMINAL(user_data);

    if (config.reload_interval <= 0) return G_SOURCE_CONTINUE;

    const char *home = getenv("HOME");
    if (!home) {
        struct passwd *pw = getpwuid(getuid());
        home = pw->pw_dir;
    }

    snprintf(config_path, sizeof(config_path), "%s/.config/quiltt/config.lua", home);

    if (stat(config_path, &st) == 0) {
        if (st.st_mtime > last_mtime) {
            last_mtime = st.st_mtime;
            g_print("Reloading config...\n");
            load_config();
            apply_config(terminal);
        }
    } else {
        g_printerr("Error checking config file: %s\n", strerror(errno));
    }

    return G_SOURCE_CONTINUE;
}

static void child_ready(VteTerminal *terminal, GPid pid, GError *error, gpointer user_data) {
    if (!terminal) return;
    if (pid == -1) g_printerr("Failed to start shell: %s\n", error ? error->message : "Unknown error");
}
static void child_ready_wrapper(VteTerminal *terminal, int exit_status, GError *error, gpointer user_data) {
    if (exit_status != 0 || error != NULL) {
        g_printerr("Failed to start shell: %s (exit status: %d)\n", error ? error->message : "Unknown error", exit_status);
        if (error != NULL) {
            g_error_free(error);
        }
    }
    // You can perform other actions here after the child is ready
}

int main(int argc, char *argv[]) {
    GtkWidget *window, *terminal;
    gchar **envp;
    const gchar *shell;
    gchar *command[] = {NULL, NULL};

    gtk_init(&argc, &argv);

    // Initialize configuration
    init_config();

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Quiltt Terminal");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    terminal = vte_terminal_new();

    // Set the terminal's unique ID
    gtk_widget_set_name(GTK_WIDGET(terminal), QUILTT_TERMINAL_ID);

    // Load and apply initial config
    load_config();
    apply_config(VTE_TERMINAL(terminal));

    // Set up config file monitoring
    if (config.reload_interval > 0) {
        g_timeout_add_seconds(config.reload_interval, check_config_file, terminal);
    }

    // Spawn shell
    envp = g_get_environ();
    shell = g_environ_getenv(envp, "SHELL");
    if (!shell) shell = "/bin/sh";
    command[0] = g_strdup(shell);

    // Correct spawn_async call for VTE 0.78.4
    vte_terminal_spawn_async(
        VTE_TERMINAL(terminal),
        0,  // VtePtyFlags (VTE_PTY_DEFAULT is 0)
        NULL,             // working directory
        command,          // command
        envp,             // environment
        G_SPAWN_SEARCH_PATH,
        NULL,             // child setup
        NULL,             // child setup data
        NULL,             // child setup data destroy
        -1,               // timeout
        NULL,             // cancellable
        (VteTerminalSpawnAsyncCallback)child_ready_wrapper, // callback (cast to the correct type)
        NULL              // user_data
    );

    g_strfreev(envp);
    g_free(command[0]);

    gtk_container_add(GTK_CONTAINER(window), terminal);
    gtk_widget_show_all(window);

    gtk_main();

    free(config.font);
    return 0;
}