#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <termios.h>
#include <signal.h>
#include <sys/select.h>

#define MAX_ENTRIES 100
#define MAX_FIELD 80
#define MAX_LINE 256
#define ALERT_NONE 0
#define ALERT_DELAYED 1
#define ALERT_STALLED 2
#define FILTER_ALL 0
#define FILTER_EAST 1
#define FILTER_WEST 2

#define BOX_TL "\xe2\x95\x94"
#define BOX_TR "\xe2\x95\x97"
#define BOX_BL "\xe2\x95\x9a"
#define BOX_BR "\xe2\x95\x9d"
#define BOX_ML "\xe2\x95\xa0"
#define BOX_MR "\xe2\x95\xa3"
#define BOX_H  "\xe2\x95\x90"
#define BOX_V  "\xe2\x95\x91"
#define BOX_HS "\xe2\x94\x80"

#define R        "\033[0m"
#define RED_BG   "\033[41m"
#define BOLD_W   "\033[1;97m"
#define WHITE    "\033[97m"
#define DIM_W    "\033[2;37m"
#define B_RED    "\033[1;91m"
#define BLINK_R  "\033[5;1;91m"
#define YELLOW   "\033[1;93m"
#define CYAN     "\033[1;96m"
#define CLEAR    "\033[H\033[2J"
#define HIDE_C   "\033[?25l"
#define SHOW_C   "\033[?25h"

#define INNER_W 78
#define COL_ROUTE 20
#define COL_STATUS 9
#define COL_STOP 20
#define COL_DIR 3
#define COL_TIME 13

typedef struct {
    char route[MAX_FIELD];
    char stop[MAX_FIELD];
    char direction;
    time_t departure_time;
    int alert_type;
    int alert_ticks;
} Departure;

static Departure entries[MAX_ENTRIES];
static int entry_count = 0;
static int filter_mode = FILTER_ALL;
static struct termios orig_termios;

void restore_terminal(void) {
    tcsetattr(STDIN_FILENO, TCSANOW, &orig_termios);
    printf(SHOW_C);
}

void handle_signal(int sig) {
    (void)sig;
    restore_terminal();
    printf(CLEAR);
    exit(EXIT_SUCCESS);
}

void enable_raw_mode(void) {
    tcgetattr(STDIN_FILENO, &orig_termios);
    atexit(restore_terminal);
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ECHO | ICANON);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
}

void load_data(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (!fp) exit(EXIT_FAILURE);
    char line[MAX_LINE];
    time_t now = time(NULL);
    entry_count = 0;
    while (fgets(line, sizeof(line), fp) && entry_count < MAX_ENTRIES) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        char *route = strtok(line, ",");
        char *stop = strtok(NULL, ",");
        char *mins = strtok(NULL, ",");
        char *dir = strtok(NULL, ",");
        if (!route || !stop || !mins || !dir) continue;
        strncpy(entries[entry_count].route, route, MAX_FIELD - 1);
        strncpy(entries[entry_count].stop, stop, MAX_FIELD - 1);
        entries[entry_count].direction = dir[0];
        entries[entry_count].departure_time = now + (time_t)(atoi(mins) * 60);
        entries[entry_count].alert_type = ALERT_NONE;
        entries[entry_count].alert_ticks = 0;
        entry_count++;
    }
    fclose(fp);
}

void update_alerts(void) {
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].alert_ticks > 0) {
            entries[i].alert_ticks--;
            if (entries[i].alert_ticks == 0) entries[i].alert_type = ALERT_NONE;
        } else {
            if ((rand() % 100) < 10) {
                entries[i].alert_type = (rand() % 2 == 0) ? ALERT_DELAYED : ALERT_STALLED;
                entries[i].alert_ticks = 8 + (rand() % 7);
            }
        }
    }
}

int active_alert_count(void) {
    int n = 0;
    for (int i = 0; i < entry_count; i++) {
        if (entries[i].alert_type != ALERT_NONE) n++;
    }
    return n;
}

void repeat_box(const char *ch, int n) {
    for (int i = 0; i < n; i++) printf("%s", ch);
}

void bar_spaces(int n) {
    for (int i = 0; i < n; i++) printf(" ");
}

void bar_open(void) {
    printf(RED_BG BOLD_W "%s ", BOX_V);
}

void bar_close(void) {
    printf(RED_BG BOLD_W " %s\n" R, BOX_V);
}

void print_top_border(void) {
    printf(RED_BG BOLD_W "%s", BOX_TL);
    repeat_box(BOX_H, INNER_W);
    printf("%s\n" R, BOX_TR);
}

void print_mid_border(void) {
    printf(RED_BG BOLD_W "%s", BOX_ML);
    repeat_box(BOX_H, INNER_W);
    printf("%s\n" R, BOX_MR);
}

void print_bot_border(void) {
    printf(RED_BG BOLD_W "%s", BOX_BL);
    repeat_box(BOX_H, INNER_W);
    printf("%s\n" R, BOX_BR);
}

void row_title(const char *left, const char *right) {
    int gap = INNER_W - (int)strlen(left) - (int)strlen(right) - 2;
    bar_open();
    printf("%s", left);
    bar_spaces(gap);
    printf("%s", right);
    bar_close();
}

void fit_field(char *dst, const char *src, int max_chars) {
    int len = (int)strlen(src);
    int n = len > max_chars ? max_chars : len;
    memcpy(dst, src, (size_t)n);
    for (int i = n; i < max_chars; i++) dst[i] = ' ';
    dst[max_chars] = '\0';
}

void draw_col_header(void) {
    printf(RED_BG BOLD_W "%s ", BOX_V);
    printf(CYAN "%-*s " R RED_BG BOLD_W, COL_ROUTE, "ROUTE");
    printf(CYAN "%-*s " R RED_BG BOLD_W, COL_STATUS, "STATUS");
    printf(CYAN "%-*s " R RED_BG BOLD_W, COL_STOP, "STOP");
    printf(CYAN "%-*s " R RED_BG BOLD_W, COL_DIR, "DIR");
    printf(CYAN "%-*s" R RED_BG BOLD_W, COL_TIME, "DEPARTS IN");
    printf(" %s\n" R, BOX_V);
}

void draw_data_row(int idx, long remaining, int row_parity) {
    char route_f[COL_ROUTE + 1], stop_f[COL_STOP + 1], dir_f[COL_DIR + 1], time_f[COL_TIME + 1], status_f[COL_STATUS + 1];
    fit_field(route_f, entries[idx].route, COL_ROUTE);
    fit_field(stop_f, entries[idx].stop, COL_STOP);
    char dir_s[4]; snprintf(dir_s, sizeof(dir_s), "%c", entries[idx].direction);
    fit_field(dir_f, dir_s, COL_DIR);
    if (remaining <= 0) fit_field(time_f, "DEPARTING", COL_TIME);
    else {
        char tbuf[32]; long m = remaining / 60, s = remaining % 60;
        snprintf(tbuf, sizeof(tbuf), "%2ld min %02ld sec", m, s);
        fit_field(time_f, tbuf, COL_TIME);
    }
    const char *alert_raw = "";
    if (entries[idx].alert_type == ALERT_DELAYED) alert_raw = "DELAYED";
    else if (entries[idx].alert_type == ALERT_STALLED) alert_raw = "STALLED";
    fit_field(status_f, alert_raw, COL_STATUS);
    const char *row_col = (row_parity % 2 == 0) ? WHITE : DIM_W;
    printf("%s ", BOX_V);
    printf("%s%s " R, row_col, route_f);
    if (entries[idx].alert_type != ALERT_NONE) printf(BLINK_R "%s " R, status_f);
    else printf("%s ", status_f);
    printf("%s%s " R, row_col, stop_f);
    printf(YELLOW "%s " R, dir_f);
    if (remaining <= 0) printf(B_RED "%s" R, time_f);
    else if (entries[idx].alert_type != ALERT_NONE) printf(YELLOW "%s" R, time_f);
    else printf("%s%s" R, row_col, time_f);
    printf("    %s\n", BOX_V);
}

void draw_board(void) {
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char timebuf[32]; strftime(timebuf, sizeof(timebuf), "%H:%M:%S", t);
    int alerts = active_alert_count();
    const char *filter_str = (filter_mode == FILTER_EAST) ? "EASTBOUND" : (filter_mode == FILTER_WEST) ? "WESTBOUND" : "ALL ROUTES";
    char title_left[64]; snprintf(title_left, sizeof(title_left), "TTC REAL-TIME DEPARTURE BOARD");
    char title_right[32]; snprintf(title_right, sizeof(title_right), "Filter: %-10s", filter_str);
    char alert_detail[32]; alert_detail[0] = '\0';
    if (alerts > 0) snprintf(alert_detail, sizeof(alert_detail), "(%d alerts active)", alerts);
    int status_left_display = (alerts == 0) ? 25 : 29 + (int)strlen(alert_detail);
    int status_gap = INNER_W - status_left_display - 18;
    printf(CLEAR); print_top_border(); row_title(title_left, title_right); print_mid_border();
    bar_open();
    if (alerts == 0) printf(GREEN "\xe2\x97\x8f" R RED_BG BOLD_W " SYSTEM STATUS: " GREEN "NORMAL" R RED_BG BOLD_W);
    else printf(BLINK_R "\xe2\x96\xb2" R RED_BG BOLD_W " SYSTEM STATUS: " BLINK_R "DEGRADED " R RED_BG BOLD_W "%s", alert_detail);
    bar_spaces(status_gap); printf(" Time: %s", timebuf); bar_close();
    print_mid_border(); draw_col_header(); print_mid_border();
    int shown = 0;
    for (int i = 0; i < entry_count; i++) {
        char dir = entries[i].direction;
        if (filter_mode == FILTER_EAST && dir != 'E') continue;
        if (filter_mode == FILTER_WEST && dir != 'W') continue;
        draw_data_row(i, (long)(entries[i].departure_time - now), shown++);
    }
    if (shown == 0) printf("%s %s %-*s %s\n", BOX_V, DIM_W, INNER_W - 2, "No routes match the current filter.", BOX_V);
    print_mid_border();
    bar_open();
    printf(BOLD_W "[E]" R RED_BG " Eastbound " BOLD_W "[W]" R RED_BG " Westbound " BOLD_W "[A]" R RED_BG " All " BOLD_W "[Q]" R RED_BG " Quit");
    bar_spaces(INNER_W - 51); bar_close(); print_bot_border(); fflush(stdout);
}

int main(void) {
    srand((unsigned int)time(NULL));
    load_data("data.txt");
    enable_raw_mode(); printf(HIDE_C);
    while (1) {
        update_alerts();
        for (int i = 0; i < 10; i++) {
            struct timeval tv = {0, 100000};
            select(0, NULL, NULL, NULL, &tv);
            char ch = 0;
            if (read(STDIN_FILENO, &ch, 1) == 1) {
                if (ch == 'e' || ch == 'E') filter_mode = FILTER_EAST;
                else if (ch == 'w' || ch == 'W') filter_mode = FILTER_WEST;
                else if (ch == 'a' || ch == 'A') filter_mode = FILTER_ALL;
                else if (ch == 'q' || ch == 'Q' || ch == 3) { restore_terminal(); printf(CLEAR); return 0; }
            }
            draw_board();
        }
    }
    return 0;
}
