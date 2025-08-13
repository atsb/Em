/*
 * Unix 6
 * Editor
 * Original code by Ken Thompson
 *
 * QMC mods Feb. 76, by  George Coulouris
 * mods are:
    prompts (suppress with '-p' flag)
    ",%,&, to display a screen full of context
    'x' - as 's' but interactive
    'n' flag when appended to 's' or 'x' commands prints number of replacements

 * also mods by jrh 26 Feb 76
    % == current file name in ! commands
    !! == repeat the last ! command you executed
    -e flag == "elfic" mode :-
        no "w", "r\n" commands, auto w before q

    More mods by George March 76:

    'o' command for text input with local editing via control keys
    'b' to set a threshold for automatic line breaks in 'o' mode.
    'h' displays a screen full of help with editor commands
        (the help is in /usr/lib/emhelp)

    Additional tty patches and merges from other variants ats 2019

bugs:
    should not use printf in substitute()
    (for space reasons).
 */

 /* ATS 2019 - The following patches were applied
  *
  * Steve Eisen of CUNY/UCC - 5/77
  * 	'~' command is equivalent to '&' command, but leaves dot
         on the first line printed.
     'e' with no argument re-edits the current file.

  *	Modified Dec 1977 Peter C of KENT
  *	To stop quiting if any file changes have been made
  *	The algorithm for this is not perfect and will ask
  *	Are you sure? in some cases where no changes have been made
  *	but in general this will stop the deletion of lots of
  *	data which has been typed in by typing 'q' accidentally
  *	Also adds 'j' command joins two (or more lines)
  *	Nobody is using 'l' command so remove it for space reasons

  * 	Mods: Ian Johnstone (AGSM)
  *	September, 77
  *
  *	When receive terminate (14) signal, output current file
  *	to "saved.file" and tell user about this - then exit.
  */

  /* this file contains all of the code except that used in the 'o' command.
      that is in a second segment called em2.c */

/* ATS 2025 - ported to ncurses and multiplatform */

/*

MIT License

Copyright (c) 1975 - 1976 George Coulouris
Copyright (c) 1976 - 1978 Other Universities
Copyright (c) 2017 - 2025 atsb

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#ifdef _WIN32
#include <windows.h>
#include <process.h>
#define getpid _getpid
#ifndef MAX_PATH
#define MAX_PATH 260
#endif
#endif
#ifndef PATH_MAX
#ifdef _WIN32
#define PATH_MAX MAX_PATH
#else
#define PATH_MAX 4096
#endif
#endif
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <setjmp.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <curses.h>

#ifndef _WIN32
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#else
#include <io.h>
#define open  _open
#define read  _read
#define write _write
#define close _close
#define lseek _lseek
#define creat _creat
#define S_IRUSR _S_IREAD
#define S_IWUSR _S_IWRITE
#define S_IRGRP _S_IREAD
#define S_IWGRP _S_IWRITE
#define S_IROTH _S_IREAD
#define S_IWOTH _S_IWRITE
#endif

#define EM_EOF -1
#define EM_LINES 18
#define LENGTH   80
#define SPLIT    '-'
#define PROMPT   '>'
#define CONFIRM  '.'
#define SCORE    "^"
#define FORM     014

#define FNSIZE   64
#define LBSIZE   512
#define ESIZE    128
#define GBSIZE   256
#define NBRA     5

#define CBRA     1
#define CCHR     2
#define CDOT     4
#define CCL      6
#define NCCL     8
#define CDOL     10
#define CEOF     11
#define CKET     12

#define STAR     01

#define error    errfunc()

#define READ     0
#define WRITE    1

#define UNIXBUFL 100

extern int  margin;
int   elfic = 0;
int   firstime = 1;

int   peekc;
int   lastc;
int   lastkey;
int   peekkey;
int   keyboard = 2;

char  unixbuffer[UNIXBUFL];
char  savedfile[FNSIZE];
char  file[FNSIZE];
char  linebuf[LBSIZE];
char  rhsbuf[LBSIZE / 2];
char  expbuf[ESIZE + 4];
int   circfl;

int* zero;
int* dot;
int* dol;
int* addr1;
int* addr2;

char  genbuf[LBSIZE];
long  count;
char* nextip;
char* linebp;
int   ninbuf;
int   io;
int   pflag;

#ifndef NOL
int   listf;
#endif

int   col;
char* globp;
int   tfile = -1;
int   tline;
char  tfname[PATH_MAX];
char* loc1;
char* loc2;
char* locs;
char  ibuff[512];
int   iblock = -1;
char  obuff[512];
int   oblock = -1;
int   ichanged;
int   nleft;
char  TMPERR[] = "TMP";
int   names[26];
char* braslist[NBRA];
char* braelist[NBRA];

struct finode {
    int  fill[2];
    int  flags;
    char links;
    char fuid;
    char fgid;
    char s0;
    int  fill1[13];
};

int   oldmode = 0666;
char  termsarea[38];
int   pstore;

#ifndef _WIN32
sigjmp_buf jmpbuf;
#endif

unsigned   nlall = 128;

void op(size_t inglob);
void putch(char ch);

int* address();
int   advance(char* alp, char* aep);
int   append(int (*f)(), int* a);
void  blkio(int b, char* buf, size_t(*iofcn)(int, void*, size_t));
void  breaks(char* p);
int   cclass(char* aset, char ac, int af);
void  commands(int prompt);
void  compile(int aeof);
int   compsub();
int   confirmed();
void  delete();
void  donothing();
void  dosub();
void  errfunc();
int   execute(int gf, int* addr);
void  exfile();
void  filename();
char* getblock(int atl, int iof);
char  getchr();
int   getcopy();
int   getfile();
char* em_getline(int tl);
int   getsub();
int   gettty();
void  global(int k);
void  init();
void  em_move(int cflag);
void  newline();
void  nonzero();
char* place(char* asp, char* al1, char* al2);
void  putchr(int ac);
void  putd();
void  putfile();
int   putline();
void  putstr(char* as);
void  reverse(int* aa1, int* aa2);
void  screensplit();
void  setall();
void  setdot();
void  setnoaddr();
void  substitute(size_t inglob);
void  em_underline(char* line, char* l1, char* l2, char* score);
void  callunix();
void  terminate();
void  filelist(char*);
int   join(void);
int   getyes(void);

#ifndef _WIN32
static void onpipe(int);
static void onhup(int);
static void onintr(int);
static jmp_buf savej;
#else
static jmp_buf savej;
#endif

static int em_create_tmpfile(void) {
#ifdef _WIN32
    char tmpdir[MAX_PATH];
    DWORD len = GetTempPathA(MAX_PATH, tmpdir);
    if (len == 0 || len > MAX_PATH) return -1;

    char tmpfile[MAX_PATH];

    if (GetTempFileNameA(tmpdir, "em", 0, tmpfile) == 0) return -1;
    strncpy(tfname, tmpfile, sizeof(tfname));
    tfname[sizeof(tfname) - 1] = '\0';

    return open(tfname, O_RDWR | O_BINARY | O_TRUNC);
#else
    snprintf(tfname, sizeof(tfname), "/tmp/emXXXXXX");
    int fd = mkstemp(tfname);
    return fd; /* -1 on failure */
#endif
}

static int curses_active = 0;

static void ensure_curses(void) {
    if (!curses_active) {
        initscr();
        curses_active = 1;

        nocbreak();
        echo();
        keypad(stdscr, FALSE);
        nodelay(stdscr, FALSE);
        scrollok(stdscr, TRUE);
    }
}

static void curses_set_raw(void) {
    ensure_curses();
    raw();
    noecho();
    keypad(stdscr, TRUE);
}

static void curses_set_cooked(void) {
    ensure_curses();
    nocbreak();
    echo();
    keypad(stdscr, FALSE);
    nodelay(stdscr, FALSE);
}

static void curses_suspend(void) {
    if (!curses_active) return;
    def_prog_mode();
    endwin();
}

static void curses_resume(void) {
    if (!curses_active) {
        ensure_curses();
        return;
    }
    reset_prog_mode();
    curses_set_raw();
    refresh();
}

void putch(char ch) {
    ensure_curses();
    if (ch == '\r') {
        int y, x; getyx(stdscr, y, x); (void)x;
        move(y, 0);
    }
    else {
        addch((unsigned char)ch);
    }
    refresh();
}

static void flush_linebuf_to_screen(const char* buf, size_t n) {
    ensure_curses();
    if (n) addnstr(buf, (int)n);
    refresh();
}

void putstr(char* as) {
    ensure_curses();
    if (as && *as) addstr(as);
    addch('\n');
    refresh();
}

char getchr() {
    if ((lastc = peekc)) {
        peekc = 0;
        return (lastc);
    }
    if (globp) {
        if ((lastc = *globp++) != 0) return lastc;
        globp = 0;
        return (char)EM_EOF;
    }
    ensure_curses();
    int c = getch();
    if (c == ERR) return (char)EM_EOF;

#ifdef KEY_ENTER
    if (c == KEY_ENTER) return '\n';
#endif
    if (c == '\r') return '\n';

#ifdef KEY_MIN
    if (c >= KEY_MIN && c <= KEY_MAX) return 0;
#endif
    lastc = (char)(c & 0xFF);
    return lastc;
}

int getkey(void) {
    if ((lastkey = peekkey)) {
        peekkey = 0;
        return lastkey;
    }
    ensure_curses();
    int c = getch();
    if (c == ERR) return (int)EM_EOF;
    lastkey = (char)(c & 0xFF);
    return lastkey;
}

#ifndef _WIN32
static void onpipe(int sig) { (void)sig; error; }

static void onintr(int sig) {
    (void)sig;
    signal(SIGINT, onintr);
    putchr('\n');
    lastc = '\n';
    error;
}

static void onhup(int sig) {
    (void)sig;
    signal(SIGINT, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    if (dol > zero && filealtered == 1) {
        addr1 = zero + 1;
        addr2 = dol;
        io = creat("em.hup",
            S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (io > 0) putfile();
    }
    filealtered = 0;
    exit(0);
}
#endif

int vflag = 1;
int xflag = 0;
int filealtered = 0;

static char* home;

int main(int argc, char** argv)
{
    char* p1, * p2;

#ifndef _WIN32
    void (*oldhup)() = signal(SIGHUP, SIG_IGN);
    void (*oldintr)() = signal(SIGINT, SIG_IGN);
    void (*oldquit)() = signal(SIGQUIT, SIG_IGN);
    void (*oldpipe)() = signal(SIGPIPE, onpipe);
    signal(SIGTERM, terminate);
#endif

    int lastc_local = 0;
    while (*(*argv + lastc_local + 1) != '\0') lastc_local++;
    if (*(*argv + lastc_local) == 'm') vflag = 1;

    argv++;
    if (argc > 1 && **argv == '-') {
        p1 = *argv + 1;
        home = getenv("HOME");
        while (*p1) {
            switch (*p1++) {
            case 'q':
#ifndef _WIN32
                signal(SIGQUIT, SIG_DFL);
#endif
                break;
            case 'e': elfic = 1; break;
            case 'p': vflag = 0; break;
            case 's': vflag = -1; break;
            }
        }
        if (!(*argv)[1]) vflag = -1;
        argv++; argc--;
    }

    if (argc > 1) {
        p1 = *argv;
        p2 = savedfile;
        while ((*p2++ = *p1++));
        breaks(p1 - 3);
        globp = (char*)"r";
    }

    zero = (int*)malloc(nlall * sizeof(int));
    if (!zero) { fprintf(stderr, "out of mem\n"); return 1; }

    if (vflag > 0) putstr("Editor");
    init();

#ifndef _WIN32
    if (signal(SIGINT, SIG_IGN) != SIG_IGN)
        signal(SIGINT, onintr);
    if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
        signal(SIGHUP, onhup);
#endif

    setjmp(savej);
    commands(vflag);
#ifdef _WIN32
    _unlink(tfname);
#else
    unlink(tfname);
#endif
    return 0;
}

void terminate()
{
    if (dol != zero)
        creat("saved.file", 0666);
    putstr("System going down - tmp file written to \"saved.file\"");
#ifdef _WIN32
    _unlink(tfname);
#else
    unlink(tfname);
#endif
    exit(1);
}

void commands(int prompt)
{
    int* a1, c;
    char* p1, * p2;

    ensure_curses();
    curses_set_cooked();

    for (;;) {
        if (pflag) {
            pflag = 0;
            addr1 = addr2 = dot;
            goto print;
        }
        if (prompt > 0 && globp == 0) putch(PROMPT);
        addr1 = 0;
        addr2 = 0;
        xflag = 0;
        do {
            addr1 = addr2;
            if ((a1 = address()) == 0) {
                c = getchr();
                break;
            }
            addr2 = a1;
            if ((c = getchr()) == ';') {
                c = ',';
                dot = a1;
            }
        } while (c == ',');
        if (addr1 == 0) addr1 = addr2;
        if (c >= 'A' && c <= 'Z') c |= 040;

        switch (c) {

        case 'a':
            setdot(); newline();
            append(gettty, addr2);
            continue;

        case 'b':
            if ((c = peekc = getchr()) == '+' || c == '-') peekc = 0;
            else if (c != '\n') error;
            margin = c == '-' ? LBSIZE - 40 : LENGTH - 20;
            newline();
            continue;

        case 'c':
            setdot(); newline(); delete(); append(gettty, addr1 - 1);
            continue;

        case 'd':
            setdot(); newline(); delete();
            continue;

        case 'e':
            if (elfic) error;
            setnoaddr();
            if ((peekc = getchr()) != ' ')
                savedfile[0] = 0;
            init();
            addr2 = zero;
            goto caseread;

        case 'f':
            if (elfic) error;
            setnoaddr();
            if ((c = getchr()) == ' ') {
                peekc = c;
                savedfile[0] = 0;
                filename();
            }
            putstr(savedfile);
            continue;

        case 'g':
            global(1); continue;

        case 'h':
            newline(); filelist((char*)"emhelp"); continue;

        case 'i':
            setdot(); nonzero(); newline(); append(gettty, addr2 - 1);
            continue;

        case 'j':
            if (addr2 == 0) { addr1 = dot; addr2 = dot + 1; }
            else if (addr1 == addr2) addr2++;
            else if (addr1 > addr2) error;
            nonzero(); newline();
            if (join()) error;
            continue;

        case 'k':
            if ((c = getchr()) < 'a' || c > 'z') error;
            newline(); setdot(); nonzero(); names[c - 'a'] = *addr2 | 01;
            continue;

        case 'm':
            em_move(0); continue;

        case '\n':
            if (addr2 == 0) addr2 = dot + 1;
            addr1 = addr2;
            goto print;

#ifndef NOL
        case 'l':
            listf++;
#endif
        case 'p':
            newline();
        print:
            setdot(); nonzero();
            a1 = addr1;
            do putstr(em_getline(*a1++));
            while (a1 <= addr2);
            dot = addr2;
#ifndef NOL
            listf = 0;
#endif
            continue;

        case 'o':
            setdot();
            op((size_t)globp);
            continue;

        case 'q':
            setnoaddr(); newline();
            if (filealtered && elfic == 0) {
                if (!getyes()) continue;
            }
            if (elfic) { firstime = 1; goto writeout; }
        quitit:
#ifdef _WIN32
            _unlink(tfname);
#else
            unlink(tfname);
#endif
            if (curses_active) { endwin(); curses_active = 0; }
            exit(0);

        case 'r':
        caseread:
            c = filealtered;
            if (!firstime) c = 1;
            filename();
            if ((io = open(file, 0)) < 0) { lastc = '\n'; error; }
            setall();
            ninbuf = 0;
            append(getfile, addr2);
            exfile();
            filealtered = c;
            continue;

        case 'x': xflag = 1;
        case 's':
            setdot(); nonzero(); substitute((size_t)globp); xflag = 0;
            continue;

        case 't':
            em_move(1); continue;

        case 'v':
            global(0); continue;

        case 'w': {
            if (elfic) error;
            c = getchr(); /* support wq */
            if (c == 'q' || c == 'Q') { c = 'q'; }
            else peekc = c;
        writeout:
            setall(); nonzero();
            if (elfic) {
                p1 = savedfile;
                if (*p1 == 0) error;
                p2 = file;
                while ((*p2++ = *p1++));
            }
            else {
                filename();
            }
            filealtered = 0;
            if ((io = creat(file, 0666)) < 0) error;
            putfile(); exfile();
#ifndef _WIN32
            if (oldmode != 0666) chmod(file, oldmode);
#endif
            if (elfic || c == 'q') goto quitit;
            continue;
        }

        case '"':
            setdot(); newline(); nonzero();
            dot = addr1;
            if (dot == dol) error;
            addr1 = dot + 1;
            addr2 = dot + EM_LINES - 1;
            addr2 = addr2 > dol ? dol : addr2;
        outlines:
            putchr(FORM);
            a1 = addr1 - 1;
            while (++a1 <= addr2) putstr(em_getline(*a1));
            if (c == '~') dot = addr1;
            else dot = addr2;
            continue;

        case '~':
        case '&':
            setdot(); newline(); nonzero();
            dot = addr1;
            addr1 = dot - (EM_LINES - 2);
            addr2 = dot;
            addr1 = addr1 > zero ? addr1 : zero + 1;
            goto outlines;

        case '%': {
            newline(); setdot(); nonzero();
            dot = addr1;
            addr1 = dot - (EM_LINES / 2 - 2);
            addr2 = dot + (EM_LINES / 2 - 2);
            addr1 = addr1 > zero ? addr1 : zero + 1;
            addr2 = addr2 > dol ? dol : addr2;
            a1 = addr1 - 1;
            putchr(FORM);
            while (++a1 <= addr2) {
                if (a1 == dot) screensplit();
                putstr(em_getline(*a1));
                if (a1 == dot) screensplit();
            }
            continue;
        }

        case '>':
            newline();
            vflag = vflag > 0 ? 0 : vflag;
            longjmp(savej, 1);

        case '<':
            newline();
            vflag = 1;
            longjmp(savej, 1);

        case '=':
            setall(); newline();
            count = (addr2 - zero) & 077777;
            putd(); putchr('\n');
            continue;

        case '!':
            callunix();
            continue;

        case EM_EOF:
            if (prompt == -2) return;
            continue;
        }
        error;
    }
}

int* address()
{
    int* a1, minus, c;
    int n, relerr;

    minus = 0;
    a1 = 0;
    for (;;) {
        c = getchr();
        if ('0' <= c && c <= '9') {
            n = 0;
            do { n = n * 10 + (c - '0'); } while ((c = getchr()) >= '0' && c <= '9');
            peekc = c;
            if (a1 == 0) a1 = zero;
            if (minus < 0) n = -n;
            a1 += n;
            minus = 0;
            continue;
        }
        relerr = 0;
        if (a1 || minus) relerr++;
        switch (c) {
        case ' ':
        case '\t':
            continue;

        case '+':
            minus++;
            if (a1 == 0) a1 = dot;
            continue;

        case '-':
        case '^':
            minus--;
            if (a1 == 0) a1 = dot;
            continue;

        case '?':
        case '/':
            compile(c);
            a1 = dot;
            for (;;) {
                if (c == '/') {
                    a1++;
                    if (a1 > dol) a1 = zero;
                }
                else {
                    a1--;
                    if (a1 < zero) a1 = dol;
                }
                if (execute(0, a1)) break;
                if (a1 == dot) { putchr('?'); error; }
            }
            break;

        case '$':
            a1 = dol; break;

        case '.':
            a1 = dot; break;

        case '\'':
            if ((c = getchr()) < 'a' || c > 'z') error;
            for (a1 = zero; a1 <= dol; a1++)
                if (names[c - 'a'] == (*a1 | 01)) break;
            break;

        default:
            peekc = c;
            if (a1 == 0) return(0);
            a1 += minus;
            if (a1<zero || a1>dol) error;
            return(a1);
        }
        if (relerr) error;
    }
}

void setdot() {
    if (addr2 == 0) addr1 = addr2 = dot;
    if (addr1 > addr2) error;
}

void setall() {
    if (addr2 == 0) {
        addr1 = zero + 1;
        addr2 = dol;
        if (dol == zero) addr1 = zero;
    }
    setdot();
}

void setnoaddr() { if (addr2) error; }

void nonzero() { if (addr1 <= zero || addr2 > dol) error; }

void newline()
{
    int c = getchr();
    if (c == '\n') return;
    c = (c >= 'A' && c <= 'Z') ? (c + 32) : c;
#ifdef NOL
    if (c == 'p') {
#endif
#ifndef NOL
        if (c == 'p' || c == 'l') {
#endif
            pflag++;
#ifndef NOL
            if (c == 'l') listf++;
#endif
            if (getchr() == '\n') return;
        }
        error;
    }

    void filename()
    {
        char* p1, * p2;
        int c;

        count = 0;
        c = getchr();
        if (c == '\n' || c == EM_EOF) {
            if (elfic && !firstime) error;
            else firstime = 0;
            p1 = savedfile;
            if (*p1 == 0) error;
            p2 = file;
            while ((*p2++ = *p1++));
            return;
        }
        if (c != ' ') error;
        while ((c = getchr()) == ' ');
        if (c == '\n') error;
        p1 = file;
        do { *p1++ = c; } while ((c = getchr()) != '\n');
        *p1++ = 0;
        if (savedfile[0] == 0) {
            p1 = savedfile; p2 = file;
            while ((*p1++ = *p2++));
            breaks(p1 - 3);
        }
    }

    void breaks(char* p) {
        if (*p++ == '.')
            if (*p == 'r' || *p == 'n') margin = LENGTH - 20;
    }

    void exfile()
    {
        close(io);
        io = -1;
        if (vflag >= 0) { putd(); putchr('\n'); }
    }

    void errfunc()
    {
        int c;
#ifndef NOL
        listf = 0;
#endif
        putstr("?");
        count = 0;
        pflag = 0;
        if (globp) lastc = '\n';
        globp = 0;
        peekc = lastc;

        ensure_curses(); flushinp();
        while ((c = getchr()) != '\n' && c != EM_EOF) { }
        if (io > 0) { close(io); io = -1; }
        longjmp(savej, 1);
    }

    int gettty()
    {
        int c; char* gf; char* p;
        p = linebuf; gf = globp;
        while ((c = getchr()) != '\n') {
            if (c == EM_EOF) { if (gf) peekc = c; return c; }
            if ((c &= 0177) == 0) continue;
            *p++ = c;
            if (p >= &linebuf[LBSIZE - 2]) error;
        }
        *p++ = 0;
        if (linebuf[0] == '.' && linebuf[1] == 0) return(EM_EOF);
        return(0);
    }

    int getfile()
    {
        int c; char* lp, * fp;
        lp = linebuf; fp = nextip;
        do {
            if (--ninbuf < 0) {
                if ((ninbuf = read(io, genbuf, LBSIZE) - 1) < 0) return(EM_EOF);
                fp = genbuf;
            }
            if (lp >= &linebuf[LBSIZE]) error;
            if ((*lp++ = c = *fp++ & 0177) == 0) { lp--; continue; }
            count++;
        } while (c != '\n');
        *--lp = 0;
        nextip = fp;
        return(0);
    }

    void putfile()
    {
        int* a1; char* fp, * lp; int nib;
        nib = 512; fp = genbuf; a1 = addr1;
        do {
            lp = em_getline(*a1++);
            for (;;) {
                if (--nib < 0) {
                    write(io, genbuf, (int)(fp - genbuf));
                    nib = 511; fp = genbuf;
                }
                count++;
                if ((*fp++ = *lp++) == 0) { fp[-1] = '\n'; break; }
            }
        } while (a1 <= addr2);
        write(io, genbuf, (int)(fp - genbuf));
    }

    int append(int (*f)(), int* a)
    {
        int* a1, * a2, * rdot; int nline, tl;
        filealtered = 1;
        nline = 0; dot = a;
        while ((*f)() == 0) {
            if ((dol - zero) + 1 >= (int)nlall) {
                int* ozero = zero;
                nlall += 512;
                int* newz = (int*)realloc((char*)zero, nlall * sizeof(int));
                if (!newz) { lastc = '\n'; zero = ozero; error; }
                zero = newz;
                dot += zero - ozero;
                dol += zero - ozero;
            }
            tl = putline(); nline++;
            a1 = ++dol; a2 = a1 + 1; rdot = ++dot;
            while (a1 > rdot) *--a2 = *--a1;
            *rdot = tl;
        }
        return nline;
    }

    void delete()
    {
        int* a1, * a2, * a3;
        filealtered = 1;
        nonzero();
        a1 = addr1; a2 = addr2 + 1; a3 = dol;
        dol -= a2 - a1;
        do *a1++ = *a2++; while (a2 <= a3);
        a1 = addr1; if (a1 > dol) a1 = dol; dot = a1;
    }

    char* em_getline(int tl)
    {
        char* bp, * lp; int nl;
        lp = linebuf; bp = getblock(tl, READ); nl = nleft; tl &= ~0377;
        while (*lp++ = *bp++)
            if (--nl == 0) { bp = getblock(tl = +0400, READ); nl = nleft; }
        return linebuf;
    }

    int putline()
    {
        char* bp, * lp; int nl; int tl;
        lp = linebuf; tl = tline; bp = getblock(tl, WRITE); nl = nleft; tl &= ~0377;
        while (*bp = *lp++) {
            if (*bp++ == '\n') { *--bp = 0; linebp = lp; break; }
            if (--nl == 0) { bp = getblock(tl += 0400, WRITE); nl = nleft; }
        }
        nl = tline;
        tline += (((int)(lp - linebuf) + 03) >> 1) & 077776;
        return nl;
    }

    char* getblock(int atl, int iof)
    {
        int bno, off;
        bno = (atl >> 8) & 0377;
        off = (atl << 1) & 0774;
        if (bno >= 255) { putstr(TMPERR); error; }
        nleft = 512 - off;
        if (bno == iblock) { ichanged |= iof; return ibuff + off; }
        if (bno == oblock) return obuff + off;
        if (iof == READ) {
            if (ichanged) blkio(iblock, ibuff, (size_t(*)(int, void*, size_t))write);
            ichanged = 0; iblock = bno; blkio(bno, ibuff, (size_t(*)(int, void*, size_t))read);
            return ibuff + off;
        }
        if (oblock >= 0) blkio(oblock, obuff, (size_t(*)(int, void*, size_t))write);
        oblock = bno;
        return obuff + off;
    }

    void blkio(int b, char* buf, size_t(*iofcn)(int, void*, size_t))
    {
        lseek(tfile, (off_t)b * 512, SEEK_SET);
        if ((*iofcn)(tfile, buf, 512) != 512) { putstr(TMPERR); error; }
    }

    void init()
    {
        if (tfile >= 0)
            close(tfile);
        tline = 0;
        iblock = -1;
        oblock = -1;
        ichanged = 0;
        tfile = em_create_tmpfile();
        if (tfile < 0) {
            putstr("TMP");
            error;
        }
        dot = dol = zero;
    }

    void global(int k)
    {
        char* gp; int c; int* a1; char globuf[GBSIZE];
        if (globp) error;
        setall(); nonzero();
        if ((c = getchr()) == '\n') error;
        compile(c);
        gp = globuf;
        while ((c = getchr()) != '\n') {
            if (c == EM_EOF) error;
            if (c == '\\') {
                c = getchr();
                if (c != '\n') *gp++ = '\\';
            }
            *gp++ = c;
            if (gp >= &globuf[GBSIZE - 2]) error;
        }
        *gp++ = '\n'; *gp++ = 0;
        for (a1 = zero; a1 <= dol; a1++) {
            *a1 &= ~01;
            if (a1 >= addr1 && a1 <= addr2 && execute(0, a1) == k) *a1 |= 01;
        }
        for (a1 = zero; a1 <= dol; a1++) {
            if (*a1 & 01) {
                *a1 &= ~01;
                dot = a1;
                globp = globuf;
                commands(-2);
                a1 = zero;
            }
        }
    }

    void substitute(size_t inglob)
    {
        int gsubf, * a1, nl;
        int nflag, nn;

        gsubf = compsub();
        nflag = gsubf > 1 ? 1 : 0;
        nn = 0;
        gsubf &= 01;
        gsubf |= xflag;
        for (a1 = addr1; a1 <= addr2; a1++) {
            if (execute(0, a1) == 0) continue;
            inglob |= 01;
            if (confirmed()) { dosub(); nn++; }
            else donothing();
            if (gsubf) {
                while (*loc2) {
                    if (execute(1, (int*)0) == 0) break;
                    if (confirmed()) { dosub(); nn++; }
                    else donothing();
                }
            }
            *a1 = putline();
            nl = append(getsub, a1);
            a1 += nl;
            addr2 += nl;
        }
        if (inglob == 0) { putchr('?'); error; }
        if (nflag) {
            char tmp[64];
            snprintf(tmp, sizeof(tmp), " %d ", nn);
            putstr(tmp);
        }
    }

    void donothing() {
        char t1, t2;
        t1 = rhsbuf[0]; t2 = rhsbuf[1];
        rhsbuf[0] = '&'; rhsbuf[1] = 0;
        dosub();
        rhsbuf[0] = t1; rhsbuf[1] = t2;
    }

    int confirmed()
    {
        int ch;
        if (xflag) {
            putstr(linebuf);
            em_underline(linebuf, loc1, loc2, (char*)SCORE);
            ch = getchr();
            if (ch != '\n') { while (getchr() != '\n'); if (ch != CONFIRM) putstr("? '.' to confirm"); }
            return (ch == CONFIRM ? 1 : 0);
        }
        return 1;
    }

    void em_underline(char* line, char* l1, char* l2, char* score)
    {
        char* p = line;
        char* ll = l1;
        char* ch = (char*)" ";
        int i = 2;
        ensure_curses();
        while (i--) {
            while (*p && p < ll) {
                addch((*p == '\t') ? '\t' : *ch);
                p++;
            }
            ch = score;
            ll = l2;
        }
        addch('\n');
        refresh();
    }

    void screensplit()
    {
        int a = LENGTH;
        while (a--) putchr(SPLIT);
        putchr('\n');
    }

    int compsub()
    {
        int seof, c; char* p; int gsubf;
        gsubf = 0;
        if ((seof = getchr()) == '\n') error;
        compile(seof);
        p = rhsbuf;
        for (;;) {
            c = getchr();
            if (c == '\\') c = getchr() | 0200;
            if (c == '\n') error;
            if (c == seof) break;
            *p++ = c;
            if (p >= &rhsbuf[LBSIZE / 2]) error;
        }
        *p++ = 0;
        if (((peekc = getchr()) | 040) == 'g') { peekc = 0; gsubf |= 1; }
        if (((peekc = getchr()) | 040) == 'n') { peekc = 0; gsubf |= 2; }
        newline();
        return gsubf;
    }

    int getsub()
    {
        char* p1, * p2;
        p1 = linebuf;
        if ((p2 = linebp) == 0) return(EM_EOF);
        while (*p1++ = *p2++);
        linebp = 0;
        return 0;
    }

    void dosub()
    {
        char* lp, * sp, * rp;
        int c;
        lp = linebuf; sp = genbuf; rp = rhsbuf;
        while (lp < loc1) *sp++ = *lp++;
        while ((c = *rp++)) {
            if (c == '&') { sp = place(sp, loc1, loc2); continue; }
            else if (c < 0 && (c &= 0177) >= '1' && c < NBRA + '1') {
                sp = place(sp, braslist[c - '1'], braelist[c - '1']); continue;
            }
            *sp++ = c & 0177;
            if (sp >= &genbuf[LBSIZE]) error;
        }
        lp = loc2;
        loc2 = sp + (size_t)linebuf - (size_t)genbuf;
        while (*sp++ = *lp++)
            if (sp >= &genbuf[LBSIZE]) error;
        lp = linebuf; sp = genbuf;
        while (*lp++ = *sp++);
    }

    char* place(char* asp, char* al1, char* al2)
    {
        char* sp = asp, * l1 = al1, * l2 = al2;
        filealtered = 1;
        while (l1 < l2) {
            *sp++ = *l1++;
            if (sp >= &genbuf[LBSIZE]) error;
        }
        return sp;
    }

    void em_move(int cflag)
    {
        int* adt, * ad1, * ad2;
        setdot(); nonzero();
        if ((adt = address()) == 0) error;
        newline();
        ad1 = addr1; ad2 = addr2;
        if (cflag) { ad1 = dol; append(getcopy, ad1++); ad2 = dol; }
        ad2++;
        if (adt < ad1) {
            dot = adt + (ad2 - ad1);
            if ((++adt) == ad1) return;
            reverse(adt, ad1); reverse(ad1, ad2); reverse(adt, ad2);
        }
        else if (adt >= ad2) {
            dot = adt++; reverse(ad1, ad2); reverse(ad2, adt); reverse(ad1, adt);
        }
        else error;
    }

    void reverse(int* aa1, int* aa2)
    {
        int* a1 = aa1, * a2 = aa2, t;
        for (;;) {
            t = *--a2;
            if (a2 <= a1) return;
            *a2 = *a1;
            *a1++ = t;
        }
    }

    int getcopy()
    {
        if (addr1 > addr2) return(EM_EOF);
        em_getline(*addr1++);
        return 0;
    }

    void compile(int aeof)
    {
        int eof, c; char* ep; char* lastep = 0; char bracket[NBRA], * bracketp; int nbra; int cclcnt;
        ep = expbuf; eof = aeof; bracketp = bracket; nbra = 0;
        if ((c = getchr()) == eof) { if (*ep == 0) error; return; }
        circfl = 0;
        if (c == '^') { c = getchr(); circfl++; }
        if (c == '*') goto cerror;
        peekc = c;
        for (;;) {
            if (ep >= &expbuf[ESIZE]) goto cerror;
            c = getchr();
            if (c == eof) { *ep++ = CEOF; return; }
            if (c != '*') lastep = ep;
            switch (c) {
            case '\\':
                if ((c = getchr()) == '(') {
                    if (nbra >= NBRA) goto cerror;
                    *bracketp++ = nbra; *ep++ = CBRA; *ep++ = nbra++; continue;
                }
                if (c == ')') {
                    if (bracketp <= bracket) goto cerror;
                    *ep++ = CKET; *ep++ = *--bracketp; continue;
                }
                *ep++ = CCHR; if (c == '\n') goto cerror; *ep++ = c; continue;

            case '.': *ep++ = CDOT; continue;
            case '\n': goto cerror;

            case '*':
                if (*lastep == CBRA || *lastep == CKET) error;
                *lastep |= STAR; continue;

            case '$':
                if ((peekc = getchr()) != eof) goto defchar;
                *ep++ = CDOL; continue;

            case '[':
                *ep++ = CCL; *ep++ = 0; cclcnt = 1;
                if ((c = getchr()) == '^') { c = getchr(); ep[-2] = NCCL; }
                do {
                    if (c == '\n') goto cerror;
                    *ep++ = c; cclcnt++;
                    if (ep >= &expbuf[ESIZE]) goto cerror;
                } while ((c = getchr()) != ']');
                lastep[1] = cclcnt; continue;

            defchar:
            default:
                *ep++ = CCHR; *ep++ = c;
            }
        }
    cerror:
        expbuf[0] = 0; error;
    }

    int execute(int gf, int* addr)
    {
        char* p1, * p2, c;
        if (gf) {
            if (circfl) return 0;
            p1 = linebuf; p2 = genbuf; while (*p1++ = *p2++); locs = p1 = loc2;
        }
        else {
            if (addr == zero) return 0;
            p1 = em_getline(*addr); locs = 0;
        }
        p2 = expbuf;
        if (circfl) { loc1 = p1; return advance(p1, p2); }
        if (*p2 == CCHR) {
            c = p2[1];
            do {
                if (*p1 != c) continue;
                if (advance(p1, p2)) { loc1 = p1; return 1; }
            } while (*p1++);
            return 0;
        }
        do { if (advance(p1, p2)) { loc1 = p1; return 1; } } while (*p1++);
        return 0;
    }

    int advance(char* alp, char* aep)
    {
        char* lp = alp, * ep = aep, * curlp;
        for (;;) switch (*ep++) {
        case CCHR:
            if (*ep++ == *lp++) continue; return 0;
        case CDOT:
            if (*lp++) continue; return 0;
        case CDOL:
            if (*lp == 0) continue; return 0;
        case CEOF:
            loc2 = lp; return 1;
        case CCL:
            if (cclass(ep, *lp++, 1)) { ep += *ep; continue; } return 0;
        case NCCL:
            if (cclass(ep, *lp++, 0)) { ep += *ep; continue; } return 0;
        case CBRA:
            braslist[*ep++] = lp; continue;
        case CKET:
            braelist[*ep++] = lp; continue;
        case CDOT | STAR:
            curlp = lp; while (*lp++); goto star;
        case CCHR | STAR:
            curlp = lp; while (*lp++ == *ep); ep++; goto star;
        case CCL | STAR:
        case NCCL | STAR:
            curlp = lp; while (cclass(ep, *lp++, ep[-1] == (CCL | STAR))); ep += *ep; goto star;
        star:
            do {
                lp--;
                if (lp == locs) break;
                if (advance(lp, ep)) return 1;
            } while (lp > curlp);
            return 0;
        default:
            error;
        }
    }

    int cclass(char* aset, char ac, int af)
    {
        char* set = aset, c; int n;
        if ((c = ac) == 0) return 0;
        n = *set++;
        while (--n) if (*set++ == c) return af;
        return !af;
    }

    void putd()
    {
        int r = (int)(count % 10);
        count /= 10;
        if (count) putd();
        putchr(r + '0');
    }

    char line[70];
    char* linp = line;

    void putchr(int ac)
    {
        char* lp = linp; int c = ac;
#ifndef NOL
        if (listf) {
            col++;
            if (col >= 72) { col = 0; *lp++ = '\\'; *lp++ = '\n'; }
            if (c == '\t') { c = '>'; goto esc; }
            if (c == '\b') {
                c = '<';
            esc:
                *lp++ = '-'; *lp++ = '\b'; *lp++ = c; goto out;
            }
            if (c < ' ' && c != '\n') {
                *lp++ = '\\';
                *lp++ = (c >> 3) + '0';
                *lp++ = (c & 07) + '0';
                col = +2;
                goto out;
            }
        }
#endif
        * lp++ = c;
    out:
        if (c == '\n' || lp >= &line[64]) {
            linp = line;
            flush_linebuf_to_screen(line, (size_t)(lp - line));
            return;
        }
        linp = lp;
    }

    int getyes(void)
    {
        char result, c;
        putstr((char*)"Are you sure?");
        result = c = (char)getkey();
        while (c != '\n') c = (char)getkey();
        return ((result & ~040) == 'Y');
    }

    /* Routine to list a file */
    void filelist(char* fi)
    {
        int fd, n;
        char* sp;
        char listbuf[512];
        if ((fd = open(fi, 0)) < 0) {
            sp = fi; while (*sp) putchr(*sp++);
            putstr((char*)" not found");
            return;
        }
        while ((n = (int)read(fd, listbuf, 512)) > 0) {
            flush_linebuf_to_screen(listbuf, (size_t)n);
        }
        close(fd);
    }

    /* Routine to join lines - added Peter C Dec 1977 */
    int join(void)
    {
        char* s; char* d; char* a1;
        d = genbuf; a1 = (char*)addr1;
        do {
            s = em_getline(*((int*)a1)++);
            while ((*d++ = *s++))
                if (d >= &genbuf[LBSIZE]) return 1;
            d--;
        } while ((int*)a1 <= addr2);
        delete();
        linebp = genbuf;
        append(getsub, addr1 - 1);
        return 0;
    }

    void callunix()
    {
        int  pidflag = 0;
        int  retcode = 0;
        char c, * lp, * fp;

        c = getchr();
        if (c != '!') {
            lp = unixbuffer;
            do {
                if (c != '%') *lp++ = c;
                else { /* % expands to savedfile */
                    pidflag = 1;
                    fp = savedfile;
                    while ((*lp++ = *fp++));
                    lp--;
                }
                c = getchr();
            } while (c != '\n');
            *lp = '\0';
        }
        else {
            pidflag = 1;
            while (getchr() != '\n');
        }

        if (pidflag) {
            putchr('!'); putstr(unixbuffer);
        }

        setnoaddr();
        curses_suspend();

#ifndef _WIN32
        pid_t pid = fork();
        if (pid == 0) {
            /* child */
            signal(SIGHUP, SIG_DFL);
            signal(SIGQUIT, SIG_DFL);
            execl("/bin/sh", "sh", "-c", unixbuffer, (char*)0);
            _exit(0100);
        }
        else if (pid > 0) {
            int rpid;
            void (*savint)() = signal(SIGINT, SIG_IGN);
            while ((rpid = wait(&retcode)) != (int)pid && rpid != -1);
            signal(SIGINT, savint);
        }
        else {
            /* fork failed; fall back to system() */
            retcode = system(unixbuffer);
        }
#else
        retcode = system(unixbuffer);
#endif

        curses_resume();
        putstr((char*)"!");
    }
