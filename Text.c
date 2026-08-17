#include "fxlib.h"

/*
 * ============================================================
 * CASIO FX-9860G TEXT EDITOR
 * ============================================================
 *
 * DEL       Backspace
 * EXE       New line
 * ALPHA     Toggle lowercase / uppercase
 * SPACE     Space
 *
 * F1        Files
 * F2        Save
 * F3        Save As
 * F6        Exit
 *
 * Files are stored as:
 *
 *     \\fls0\\NAME.TXT
 *
 * ============================================================
 */


/* ============================================================
 * SETTINGS
 * ============================================================ */

#define MAX_TEXT       4096
#define MAX_FILES       32
#define MAX_NAME         8

#define SCREEN_LINES     6
#define SCREEN_CHARS    21


/* ============================================================
 * STRING CAST
 * ============================================================ */

#define U8(x) ((const unsigned char *)(x))


/* ============================================================
 * GLOBAL TEXT
 * ============================================================ */

static unsigned char text[MAX_TEXT + 1];

static int text_len;
static int cursor;
static int scroll_line;


/*
 * 0 = uppercase
 * 1 = lowercase
 */
static int lowercase_mode;


/* ============================================================
 * CURRENT FILE
 * ============================================================ */

static char current_file[16];


/* ============================================================
 * FILE LIST
 * ============================================================ */

static char file_list[MAX_FILES][16];

static int file_count;
static int file_selected;


/* ============================================================
 * STRING FUNCTIONS
 * ============================================================ */

static int str_len(const char *s)
{
    int n;

    n = 0;

    while (s[n] != 0)
        n++;

    return n;
}


static void str_copy(char *dst, const char *src)
{
    int i;

    i = 0;

    while (src[i] != 0)
    {
        dst[i] = src[i];
        i++;
    }

    dst[i] = 0;
}


/* ============================================================
 * TEXT
 * ============================================================ */

static void clear_text(void)
{
    int i;

    text_len = 0;
    cursor = 0;
    scroll_line = 0;

    for (i = 0; i <= MAX_TEXT; i++)
        text[i] = 0;
}


static int line_count(void)
{
    int i;
    int count;

    count = 1;

    for (i = 0; i < text_len; i++)
    {
        if (text[i] == '\n')
            count++;
    }

    return count;
}


static int line_start(int line)
{
    int i;
    int current;

    current = 0;

    if (line <= 0)
        return 0;

    for (i = 0; i < text_len; i++)
    {
        if (text[i] == '\n')
        {
            current++;

            if (current == line)
                return i + 1;
        }
    }

    return text_len;
}


static int line_end(int start)
{
    int i;

    for (i = start; i < text_len; i++)
    {
        if (text[i] == '\n')
            return i;
    }

    return text_len;
}


static int cursor_line(void)
{
    int i;
    int line;

    line = 0;

    for (i = 0; i < cursor; i++)
    {
        if (text[i] == '\n')
            line++;
    }

    return line;
}


static int cursor_column(void)
{
    int i;
    int column;

    column = 0;

    for (i = cursor - 1; i >= 0; i--)
    {
        if (text[i] == '\n')
            break;

        column++;
    }

    return column;
}


/* ============================================================
 * CHARACTER CASE
 * ============================================================ */

static unsigned char apply_case(unsigned char c)
{
    if (
        lowercase_mode &&
        c >= 'A' &&
        c <= 'Z'
    )
    {
        return c + 32;
    }

    return c;
}


/* ============================================================
 * INSERT CHARACTER
 * ============================================================ */

static void insert_char(unsigned char c)
{
    int i;

    if (text_len >= MAX_TEXT)
        return;

    /*
     * Letters follow the current ALPHA mode.
     */

    if (
        c >= 'A' &&
        c <= 'Z'
    )
    {
        c = apply_case(c);
    }

    /*
     * Move existing characters.
     */

    for (i = text_len; i > cursor; i--)
        text[i] = text[i - 1];

    text[cursor] = c;

    text_len++;
    cursor++;

    text[text_len] = 0;
}


/* ============================================================
 * BACKSPACE
 *
 * DEL uses this.
 * ============================================================ */

static void backspace(void)
{
    int i;

    if (cursor <= 0)
        return;

    cursor--;

    for (i = cursor; i < text_len - 1; i++)
        text[i] = text[i + 1];

    text_len--;

    text[text_len] = 0;
}


/* ============================================================
 * CURSOR
 * ============================================================ */

static void move_left(void)
{
    if (cursor > 0)
        cursor--;
}


static void move_right(void)
{
    if (cursor < text_len)
        cursor++;
}


static void move_up(void)
{
    int line;
    int column;
    int start;
    int end;

    line = cursor_line();

    if (line <= 0)
        return;

    column = cursor_column();

    start = line_start(line - 1);
    end = line_end(start);

    cursor = start + column;

    if (cursor > end)
        cursor = end;
}


static void move_down(void)
{
    int line;
    int column;
    int start;
    int end;

    line = cursor_line();

    if (line >= line_count() - 1)
        return;

    column = cursor_column();

    start = line_start(line + 1);
    end = line_end(start);

    cursor = start + column;

    if (cursor > end)
        cursor = end;
}


/* ============================================================
 * FILE PATH
 * ============================================================ */

static void make_path(
    FONTCHARACTER *path,
    const char *name
)
{
    int p;
    int i;

    p = 0;
    i = 0;

    path[p++] = '\\';
    path[p++] = '\\';

    path[p++] = 'f';
    path[p++] = 'l';
    path[p++] = 's';
    path[p++] = '0';

    path[p++] = '\\';

    while (name[i] != 0)
    {
        path[p++] =
            (FONTCHARACTER)name[i];

        i++;
    }

    path[p] = 0;
}


/* ============================================================
 * MESSAGE
 * ============================================================ */

static void message(const char *msg)
{
    unsigned int key;

    Bdisp_AllClr_DDVRAM();

    PrintXY(
        1,
        25,
        U8(msg),
        0
    );

    Bdisp_PutDisp_DD();

    GetKey(&key);
}


/* ============================================================
 * SAVE
 * ============================================================ */

static int save_file(void)
{
    FONTCHARACTER path[32];

    int handle;
    int result;

    if (current_file[0] == 0)
        return 0;

    make_path(
        path,
        current_file
    );

    /*
     * Remove previous copy.
     */

    Bfile_DeleteFile(path);

    /*
     * Create exact file size.
     */

    result = Bfile_CreateFile(
        path,
        text_len
    );

    if (result < 0)
        return 0;

    /*
     * Open.
     */

    handle = Bfile_OpenFile(
        path,
        _OPENMODE_WRITE
    );

    if (handle < 0)
        return 0;

    /*
     * IMPORTANT:
     *
     * Legacy SDK:
     *
     * Bfile_WriteFile(
     *     handle,
     *     buffer,
     *     size
     * );
     */

    if (text_len > 0)
    {
        result = Bfile_WriteFile(
            handle,
            text,
            text_len
        );

        if (result < 0)
        {
            Bfile_CloseFile(handle);
            return 0;
        }
    }

    Bfile_CloseFile(handle);

    return 1;
}


/* ============================================================
 * LOAD
 * ============================================================ */

static int load_file(const char *name)
{
    FONTCHARACTER path[32];

    int handle;
    int size;
    int result;

    make_path(
        path,
        name
    );

    handle = Bfile_OpenFile(
        path,
        _OPENMODE_READ
    );

    if (handle < 0)
        return 0;

    size = Bfile_GetFileSize(handle);

    if (size < 0)
    {
        Bfile_CloseFile(handle);
        return 0;
    }

    if (size > MAX_TEXT)
        size = MAX_TEXT;

    clear_text();

    if (size > 0)
    {
        result = Bfile_ReadFile(
            handle,
            text,
            size,
            0
        );

        if (result < 0)
        {
            Bfile_CloseFile(handle);
            clear_text();
            return 0;
        }

        text_len = result;
    }

    Bfile_CloseFile(handle);

    text[text_len] = 0;

    str_copy(
        current_file,
        name
    );

    cursor = 0;
    scroll_line = 0;

    return 1;
}


/* ============================================================
 * FILE LIST
 * ============================================================ */

static void clear_file_list(void)
{
    int i;

    file_count = 0;
    file_selected = 0;

    for (i = 0; i < MAX_FILES; i++)
        file_list[i][0] = 0;
}


static void add_file(
    const FONTCHARACTER *name
)
{
    int i;

    if (file_count >= MAX_FILES)
        return;

    for (i = 0; i < 15; i++)
    {
        if (name[i] == 0)
        {
            file_list[file_count][i] = 0;
            break;
        }

        file_list[file_count][i] =
            (char)(name[i] & 0xFF);
    }

    file_list[file_count][15] = 0;

    file_count++;
}


/* ============================================================
 * FIND TXT FILES
 * ============================================================ */

static void refresh_files(void)
{
    FONTCHARACTER search[32];
    FONTCHARACTER found[32];

    FILE_INFO info;

    int find_handle;
    int result;

    clear_file_list();

    /*
     * \\fls0\\*.TXT
     */

    search[0] = '\\';
    search[1] = '\\';

    search[2] = 'f';
    search[3] = 'l';
    search[4] = 's';
    search[5] = '0';

    search[6] = '\\';

    search[7] = '*';
    search[8] = '.';
    search[9] = 'T';
    search[10] = 'X';
    search[11] = 'T';

    search[12] = 0;

    find_handle = -1;

    result = Bfile_FindFirst(
        search,
        &find_handle,
        found,
        &info
    );

    if (result < 0)
        return;

    while (1)
    {
        add_file(found);

        if (file_count >= MAX_FILES)
            break;

        result = Bfile_FindNext(
            find_handle,
            found,
            &info
        );

        if (result < 0)
            break;
    }

    Bfile_FindClose(find_handle);

    if (file_count > 0)
        file_selected = 0;
}


/* ============================================================
 * DRAW EDITOR
 * ============================================================ */

static void draw_editor(void)
{
    int i;
    int actual_line;

    int start;
    int end;
    int pos;

    int line;
    int column;

    unsigned char buffer[SCREEN_CHARS + 1];

    Bdisp_AllClr_DDVRAM();

    /*
     * Filename
     */

    PrintXY(
        1,
        1,
        U8(current_file),
        0
    );

    /*
     * Current mode
     */

    if (lowercase_mode)
    {
        PrintXY(
            110,
            1,
            U8("abc"),
            0
        );
    }
    else
    {
        PrintXY(
            110,
            1,
            U8("ABC"),
            0
        );
    }

    line = cursor_line();
    column = cursor_column();

    /*
     * Scroll vertically.
     */

    if (line < scroll_line)
        scroll_line = line;

    if (line >= scroll_line + SCREEN_LINES)
    {
        scroll_line =
            line - SCREEN_LINES + 1;
    }

    /*
     * Draw text.
     */

    for (i = 0; i < SCREEN_LINES; i++)
    {
        actual_line =
            scroll_line + i;

        if (actual_line >= line_count())
            break;

        start =
            line_start(actual_line);

        end =
            line_end(start);

        pos = start;

        {
            int n;

            n = 0;

            while (
                pos < end &&
                n < SCREEN_CHARS
            )
            {
                buffer[n] =
                    text[pos];

                n++;
                pos++;
            }

            buffer[n] = 0;
        }

        PrintXY(
            1,
            9 + i * 7,
            buffer,
            0
        );
    }

    /*
     * Cursor.
     *
     * Keep it on-screen.
     */

    if (
        line >= scroll_line &&
        line < scroll_line + SCREEN_LINES
    )
    {
        int x;
        int y;

        if (column > SCREEN_CHARS)
            column = SCREEN_CHARS;

        x =
            1 + column * 6;

        y =
            9 +
            (line - scroll_line) * 7;

        Bdisp_SetPoint_VRAM(
            x,
            y,
            1
        );

        Bdisp_SetPoint_VRAM(
            x,
            y + 1,
            1
        );

        Bdisp_SetPoint_VRAM(
            x,
            y + 2,
            1
        );

        Bdisp_SetPoint_VRAM(
            x,
            y + 3,
            1
        );

        Bdisp_SetPoint_VRAM(
            x,
            y + 4,
            1
        );
    }

    /*
     * CLEAN BOTTOM BAR
     */

    PrintXY(
        1,
        56,
        U8("F1=FILE"),
        0
    );

    PrintXY(
        42,
        56,
        U8("F2=SAVE"),
        0
    );

    PrintXY(
        83,
        56,
        U8("F3=SAVE AS"),
        0
    );

    PrintXY(
        116,
        56,
        U8(""),
        0
    );

    Bdisp_PutDisp_DD();
}


/* ============================================================
 * DRAW FILE BROWSER
 * ============================================================ */

static void draw_file_browser(void)
{
    int i;
    int first;
    int index;

    Bdisp_AllClr_DDVRAM();

    PrintXY(
        1,
        1,
        U8("TEXT FILES"),
        0
    );

    if (file_count == 0)
    {
        PrintXY(
            1,
            18,
            U8("NO TXT FILES"),
            0
        );
    }
    else
    {
        first = 0;

        if (file_selected >= 6)
            first =
                file_selected - 5;

        for (i = 0; i < 6; i++)
        {
            index =
                first + i;

            if (index >= file_count)
                break;

            if (index == file_selected)
            {
                PrintXY(
                    1,
                    9 + i * 7,
                    U8(">"),
                    0
                );
            }

            PrintXY(
                9,
                9 + i * 7,
                U8(file_list[index]),
                0
            );
        }
    }

    /*
     * CLEAN BOTTOM BAR
     */

    PrintXY(
        1,
        56,
        U8("F2 NEW"),
        0
    );

    PrintXY(
        48,
        56,
        U8("EXE OPEN"),
        0
    );

    PrintXY(
        112,
        56,
        U8("F6 BACK"),
        0
    );

    Bdisp_PutDisp_DD();
}


/* ============================================================
 * FILENAME INPUT
 * ============================================================ */

static int filename_input(char *name)
{
    unsigned int key;

    int length;

    length = 0;

    name[0] = 0;

    while (1)
    {
        Bdisp_AllClr_DDVRAM();

        PrintXY(
            1,
            1,
            U8("FILE NAME"),
            0
        );

        PrintXY(
            1,
            16,
            U8(name),
            0
        );

        PrintXY(
            1,
            56,
            U8("EXE OK  DEL BACK"),
            0
        );

        Bdisp_PutDisp_DD();

        GetKey(&key);

        /*
         * Accept
         */

        if (key == KEY_CTRL_EXE)
        {
            if (length > 0)
                return 1;
        }

        /*
         * Cancel
         */

        else if (key == KEY_CTRL_EXIT)
        {
            return 0;
        }

        /*
         * Backspace
         */

        else if (key == KEY_CTRL_DEL)
        {
            if (length > 0)
            {
                length--;

                name[length] = 0;
            }
        }

        /*
         * Toggle case
         */

        else if (key == KEY_CTRL_ALPHA)
        {
            lowercase_mode =
                !lowercase_mode;
        }

        /*
         * Characters
         */

        else
        {
            char c;

            c = 0;

            switch (key)
            {
                case KEY_CHAR_0: c = '0'; break;
                case KEY_CHAR_1: c = '1'; break;
                case KEY_CHAR_2: c = '2'; break;
                case KEY_CHAR_3: c = '3'; break;
                case KEY_CHAR_4: c = '4'; break;
                case KEY_CHAR_5: c = '5'; break;
                case KEY_CHAR_6: c = '6'; break;
                case KEY_CHAR_7: c = '7'; break;
                case KEY_CHAR_8: c = '8'; break;
                case KEY_CHAR_9: c = '9'; break;

                case KEY_CHAR_A: c = 'A'; break;
                case KEY_CHAR_B: c = 'B'; break;
                case KEY_CHAR_C: c = 'C'; break;
                case KEY_CHAR_D: c = 'D'; break;
                case KEY_CHAR_E: c = 'E'; break;
                case KEY_CHAR_F: c = 'F'; break;
                case KEY_CHAR_G: c = 'G'; break;
                case KEY_CHAR_H: c = 'H'; break;
                case KEY_CHAR_I: c = 'I'; break;
                case KEY_CHAR_J: c = 'J'; break;
                case KEY_CHAR_K: c = 'K'; break;
                case KEY_CHAR_L: c = 'L'; break;
                case KEY_CHAR_M: c = 'M'; break;
                case KEY_CHAR_N: c = 'N'; break;
                case KEY_CHAR_O: c = 'O'; break;
                case KEY_CHAR_P: c = 'P'; break;
                case KEY_CHAR_Q: c = 'Q'; break;
                case KEY_CHAR_R: c = 'R'; break;
                case KEY_CHAR_S: c = 'S'; break;
                case KEY_CHAR_T: c = 'T'; break;
                case KEY_CHAR_U: c = 'U'; break;
                case KEY_CHAR_V: c = 'V'; break;
                case KEY_CHAR_W: c = 'W'; break;
                case KEY_CHAR_X: c = 'X'; break;
                case KEY_CHAR_Y: c = 'Y'; break;
                case KEY_CHAR_Z: c = 'Z'; break;

                /*
                 * SPACE
                 */

                case KEY_CHAR_SPACE:
                    c = ' ';
                    break;

                /*
                 * Punctuation
                 */

                case KEY_CHAR_DP:
                    c = '.';
                    break;

                case KEY_CHAR_PLUS:
                    c = '+';
                    break;

                case KEY_CHAR_MINUS:
                    c = '-';
                    break;

                case KEY_CHAR_MULT:
                    c = '*';
                    break;

                case KEY_CHAR_DIV:
                    c = '/';
                    break;

                case KEY_CHAR_LPAR:
                    c = '(';
                    break;

                case KEY_CHAR_RPAR:
                    c = ')';
                    break;

                case KEY_CHAR_COMMA:
                    c = ',';
                    break;
            }

            if (
                c != 0 &&
                length < MAX_NAME
            )
            {
                if (
                    lowercase_mode &&
                    c >= 'A' &&
                    c <= 'Z'
                )
                {
                    c += 32;
                }

                name[length] = c;

                length++;

                name[length] = 0;
            }
        }
    }
}


/* ============================================================
 * NEW FILE
 * ============================================================ */

static int new_file(void)
{
    char name[16];

    int n;

    /*
     * Start filenames uppercase.
     */

    lowercase_mode = 0;

    if (!filename_input(name))
        return 0;

    n = str_len(name);

    if (n == 0 || n > MAX_NAME)
        return 0;

    /*
     * Add .TXT
     */

    name[n++] = '.';
    name[n++] = 'T';
    name[n++] = 'X';
    name[n++] = 'T';

    name[n] = 0;

    clear_text();

    str_copy(
        current_file,
        name
    );

    if (!save_file())
    {
        current_file[0] = 0;
        return 0;
    }

    return 1;
}


/* ============================================================
 * SAVE AS
 * ============================================================ */

static int save_as(void)
{
    char name[16];

    int n;

    lowercase_mode = 0;

    if (!filename_input(name))
        return 0;

    n = str_len(name);

    if (n == 0 || n > MAX_NAME)
        return 0;

    name[n++] = '.';
    name[n++] = 'T';
    name[n++] = 'X';
    name[n++] = 'T';

    name[n] = 0;

    str_copy(
        current_file,
        name
    );

    return save_file();
}


/* ============================================================
 * FILE BROWSER
 * ============================================================ */

static void file_browser(void)
{
    unsigned int key;

    refresh_files();

    while (1)
    {
        draw_file_browser();

        GetKey(&key);

        /*
         * UP
         */

        if (key == KEY_CTRL_UP)
        {
            if (file_count > 0)
            {
                file_selected--;

                if (file_selected < 0)
                {
                    file_selected =
                        file_count - 1;
                }
            }
        }

        /*
         * DOWN
         */

        else if (key == KEY_CTRL_DOWN)
        {
            if (file_count > 0)
            {
                file_selected++;

                if (
                    file_selected >=
                    file_count
                )
                {
                    file_selected = 0;
                }
            }
        }

        /*
         * OPEN
         */

        else if (key == KEY_CTRL_EXE)
        {
            if (file_count > 0)
            {
                if (
                    load_file(
                        file_list[file_selected]
                    )
                )
                {
                    return;
                }

                message("LOAD ERROR");

                refresh_files();
            }
        }

        /*
         * NEW
         */

        else if (key == KEY_CTRL_F2)
        {
            if (new_file())
                return;

            message("CREATE ERROR");

            refresh_files();
        }

        /*
         * BACK
         */

        else if (key == KEY_CTRL_F6)
        {
            return;
        }
    }
}


/* ============================================================
 * EDITOR
 * ============================================================ */

static void editor(void)
{
    unsigned int key;

    while (1)
    {
        unsigned char c;

        c = 0;

        draw_editor();

        GetKey(&key);

        /*
         * LEFT
         */

        if (key == KEY_CTRL_LEFT)
        {
            move_left();
        }

        /*
         * RIGHT
         */

        else if (key == KEY_CTRL_RIGHT)
        {
            move_right();
        }

        /*
         * UP
         */

        else if (key == KEY_CTRL_UP)
        {
            move_up();
        }

        /*
         * DOWN
         */

        else if (key == KEY_CTRL_DOWN)
        {
            move_down();
        }

        /*
         * ====================================================
         * BACKSPACE
         * ====================================================
         */

        else if (key == KEY_CTRL_DEL)
        {
            backspace();
        }

        /*
         * ====================================================
         * NEW LINE
         * ====================================================
         */

        else if (key == KEY_CTRL_EXE)
        {
            insert_char('\n');
        }

        /*
         * ====================================================
         * LOWERCASE / UPPERCASE
         * ====================================================
         */

        else if (key == KEY_CTRL_ALPHA)
        {
            lowercase_mode =
                !lowercase_mode;
        }

        /*
         * ====================================================
         * FILE BROWSER
         * ====================================================
         */

        else if (key == KEY_CTRL_F1)
        {
            file_browser();
        }

        /*
         * ====================================================
         * SAVE
         * ====================================================
         */

        else if (key == KEY_CTRL_F2)
        {
            if (save_file())
                message("SAVED");
            else
                message("SAVE ERROR");
        }

        /*
         * ====================================================
         * SAVE AS
         * ====================================================
         */

        else if (key == KEY_CTRL_F3)
        {
            if (save_as())
                message("SAVED AS");
            else
                message("SAVE AS ERROR");
        }

        /*
         * ====================================================
         * EXIT
         * ====================================================
         */

        else if (key == KEY_CTRL_F6)
        {
            save_file();

            return;
        }

        /*
         * ====================================================
         * CHARACTERS
         * ====================================================
         */

        else
        {
            switch (key)
            {
                case KEY_CHAR_0:
                    c = '0';
                    break;

                case KEY_CHAR_1:
                    c = '1';
                    break;

                case KEY_CHAR_2:
                    c = '2';
                    break;

                case KEY_CHAR_3:
                    c = '3';
                    break;

                case KEY_CHAR_4:
                    c = '4';
                    break;

                case KEY_CHAR_5:
                    c = '5';
                    break;

                case KEY_CHAR_6:
                    c = '6';
                    break;

                case KEY_CHAR_7:
                    c = '7';
                    break;

                case KEY_CHAR_8:
                    c = '8';
                    break;

                case KEY_CHAR_9:
                    c = '9';
                    break;

                case KEY_CHAR_A:
                    c = 'A';
                    break;

                case KEY_CHAR_B:
                    c = 'B';
                    break;

                case KEY_CHAR_C:
                    c = 'C';
                    break;

                case KEY_CHAR_D:
                    c = 'D';
                    break;

                case KEY_CHAR_E:
                    c = 'E';
                    break;

                case KEY_CHAR_F:
                    c = 'F';
                    break;

                case KEY_CHAR_G:
                    c = 'G';
                    break;

                case KEY_CHAR_H:
                    c = 'H';
                    break;

                case KEY_CHAR_I:
                    c = 'I';
                    break;

                case KEY_CHAR_J:
                    c = 'J';
                    break;

                case KEY_CHAR_K:
                    c = 'K';
                    break;

                case KEY_CHAR_L:
                    c = 'L';
                    break;

                case KEY_CHAR_M:
                    c = 'M';
                    break;

                case KEY_CHAR_N:
                    c = 'N';
                    break;

                case KEY_CHAR_O:
                    c = 'O';
                    break;

                case KEY_CHAR_P:
                    c = 'P';
                    break;

                case KEY_CHAR_Q:
                    c = 'Q';
                    break;

                case KEY_CHAR_R:
                    c = 'R';
                    break;

                case KEY_CHAR_S:
                    c = 'S';
                    break;

                case KEY_CHAR_T:
                    c = 'T';
                    break;

                case KEY_CHAR_U:
                    c = 'U';
                    break;

                case KEY_CHAR_V:
                    c = 'V';
                    break;

                case KEY_CHAR_W:
                    c = 'W';
                    break;

                case KEY_CHAR_X:
                    c = 'X';
                    break;

                case KEY_CHAR_Y:
                    c = 'Y';
                    break;

                case KEY_CHAR_Z:
                    c = 'Z';
                    break;

                /*
                 * =================================================
                 * SPACE
                 * =================================================
                 */

                case KEY_CHAR_SPACE:
                    c = ' ';
                    break;

                /*
                 * Punctuation
                 */

                case KEY_CHAR_DP:
                    c = '.';
                    break;

                case KEY_CHAR_PLUS:
                    c = '+';
                    break;

                case KEY_CHAR_MINUS:
                    c = '-';
                    break;

                case KEY_CHAR_MULT:
                    c = '*';
                    break;

                case KEY_CHAR_DIV:
                    c = '/';
                    break;

                case KEY_CHAR_LPAR:
                    c = '(';
                    break;

                case KEY_CHAR_RPAR:
                    c = ')';
                    break;

                case KEY_CHAR_COMMA:
                    c = ',';
                    break;
            }

            if (c != 0)
                insert_char(c);
        }
    }
}


/* ============================================================
 * MAIN
 * ============================================================ */

int AddIn_main(
    int isAppli,
    unsigned short OptionNum
)
{
    clear_text();

    lowercase_mode = 0;

    str_copy(
        current_file,
        "UNTITLED.TXT"
    );

    editor();

    return 1;
}


/* ============================================================
 * SDK INITIALIZATION
 * ============================================================ */

#pragma section _BR_Size

unsigned long BR_Size;

#pragma section


#pragma section _TOP

int InitializeSystem(
    int isAppli,
    unsigned short OptionNum
)
{
    return INIT_ADDIN_APPLICATION(
        isAppli,
        OptionNum
    );
}

#pragma section