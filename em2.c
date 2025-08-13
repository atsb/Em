//*	This is the second and final segment of the QMC Unix Editor - em */
#define EM_EOF -1
#define LBSIZE  512
#define SIGHUP  1
#define SIGINTR 2
#define SIGQUIT 3
#define UNIXBUFL 100
#define error  errfunc() /*goto errlab pgas: don't think you can goto a pointer label in modern C*/
#define TABSET  7    /* this should be determined dynamically */
#define OPEN    '/'
#define BELL    07
#define ESCAPE  033
#define SPACE   040
#define BACKSL  0134
#define RUBOUT  0177
#define CTRLA   01
#define CTRLB   02
#define CTRLC   03
#define CTRLD   04
#define CTRLE   05
#define CTRLF   06
#define CTRLH   010
#define CTRLI   011
#define CTRLQ   021
#define CTRLR   022
#define CTRLS   023
#define CTRLV   026
#define CTRLW   027
#define CTRLX   030
#define CTRLZ   032

#define ITT 0100000

#include <curses.h>
#include <stdio.h>
#include <string.h>

extern char peekc, * linebp, * loc2, linebuf[LBSIZE], genbuf[LBSIZE],
unixbuffer[UNIXBUFL];
extern int* zero, * addr1, * addr2;
extern int* errlab;

/*extern */
int append(int (*f)(), int* a);
void compile(int aeof);
void errfunc();
int execute(int gf, int* addr);
void delete();
char* em_getline(int tl);
void setdot();
void nonzero();
int putline();
void putchr(int ac);
void putstr(char* as);
char getchr();

/* forward declaration */
void setraw();
void setcook();
int getnil();
int getopen();
int gopen();
void help();
void putb(char* ptr);
int rescan();
int inword(char c);
void putch(char ch);

void suspend_to_shell(void);
void resume_from_shell(void);

int margin = LBSIZE - 40;
int oflag;
char* threshold, * savethresh;
char* lnp, * gnp, * brp;

static int curses_active = 0;

static void ensure_curses(void)
{
    if (!curses_active) {
        initscr();
        curses_active = 1;
        raw();
        noecho();
        keypad(stdscr, TRUE);
    }
}

void setraw()
{
    ensure_curses();
    raw();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, FALSE);
}

void setcook()
{
    if (!curses_active) return;
    noraw();
    echo();
}

void suspend_to_shell(void)
{
    if (!curses_active) return;
    def_prog_mode();
    endwin();
}

void resume_from_shell(void)
{
    if (!curses_active) {
        ensure_curses();
        return;
    }
    reset_prog_mode();
    raw();
    noecho();
    keypad(stdscr, FALSE);
    refresh();
}

void putb(char* ptr)   /*display string */
{
    ensure_curses();
    if (!ptr || *ptr == '\0') return;
    addnstr(ptr, (int)strlen(ptr));
    refresh();
}

void op(size_t inglob)
{
    int* a1;
    char* lp, * sp;
    char seof, ch;
    int t, nl;

    threshold = genbuf + margin;
    savethresh = 0;

    switch (ch = peekc = getchr()) {

    case BACKSL:
        t = 1;
        delete();
        addr2 = addr1;
        break;

    case ';':
    case '+':
        t = 0;
        break;

    case '-':
        t = 1;
        break;

    default:
        goto normal;

    }

    peekc = 0;
    if (addr1 != addr2) error;
    oflag = 0;
    append(getnil, addr2 - t);
    addr1 = addr2 -= (t - 1);
    setdot();
    nonzero();

normal:
    setdot();
    nonzero();
    seof = getchr();
    if (seof == '\n' || seof == '\r') {
        loc2 = linebuf - 1;
        seof = 0;
    }
    else {
        compile(seof);
    }
    setraw(); /* terminal into raw mode*/
    for (a1 = addr1; a1 <= addr2; a1++) {
        if (seof) { if (execute(0, a1) == 0) continue; }
        else em_getline(*a1);
        putstr("\\\r");
        lp = linebuf;
        sp = genbuf;
        inglob |= 01;
        while (lp < loc2) { putch(*lp); *sp++ = *lp++; }
        lnp = lp;
        gnp = sp;

        oflag = gopen(); /* open the current line */
        *a1 = putline(); /* write revised line */
        nl = append(getopen, a1);
        a1 += nl;
        addr2 += nl;
    }
    setcook(); /* terminal in cook mode */
    putchr('\n');
    if (inglob == 0) { putchr('?'); error; }
}

int getnil()
{
    if (oflag == EM_EOF) return EM_EOF;
    linebuf[0] = '\0';
    oflag = EM_EOF;
    return 0;
}

int inword(char c)
{
    if (c - '0' < 0) return 0;
    if (c - '9' <= 0) return 1;
    c &= 0137;
    if (c - 'A' < 0) return 0;
    if (c - 'Z' <= 0) return 1;
    return 0;
}

int rescan()
{
    char* lp, * sp;

    if (savethresh) { threshold = savethresh; savethresh = 0; }
    lp = linebuf;
    sp = genbuf;
    while (*lp++ = *sp++) if (lp > linebuf + LBSIZE) {
        *(--lp) = 0;
        return 0;
    }
    return 0;
}

int gopen()
/* leaves revised line in linebuf,
   returns 0 if more to follow, EM_EOF if last line */
{
    char* lp, * sp, * rp;
    char ch, * br = 0;
    int tabs;

    lp = lnp;
    sp = gnp;
    tabs = 0;
    for (rp = genbuf; rp < sp; rp++) if (*rp == CTRLI) tabs += TABSET;

    for (;;) {
        switch (ch = getchr()) {

        case CTRLD:
            /* close the line (see case '\n' also) */
            putb(lp);
            while ((*sp++ = *lp++));
            rescan();
            return EM_EOF;

        verify:
            *sp = '\0';
            addstr("\r");
            if (genbuf[0]) addstr(genbuf);
            else if (lp && *lp) addstr(lp);
            else if (lnp && *lnp) addstr(lnp);
            else if (linebuf[0]) addstr(linebuf);
            clrtoeol();
            refresh();
            continue;

        case CTRLA:
            *sp = '\0';
            clear();
            move(0, 0);
            if (genbuf[0]) {
                addstr(genbuf);
            }
            else if (lp && *lp) {
                addstr(lp);
            }
            else if (lnp && *lnp) {
                addstr(lnp);
            }
            else if (linebuf[0]) {
                addstr(linebuf);
            }
            refresh();
            continue;

        case CTRLB: /* back a word */
            if (sp == genbuf) goto backquery;

            while ((*(--lp) = *(--sp)) == SPACE)
                if (sp < genbuf) goto out;
            if (inword(*sp)) {
                while (inword((*(--lp) = *(--sp))))
                    if (sp < genbuf) goto out;
                if (*sp == SPACE)
                    while ((*(--lp) = *(--sp)) == SPACE)
                        if (sp < genbuf) goto out;
            }
            else while (sp >= genbuf && !inword(*sp))
                if ((*lp-- = *sp--) == CTRLI) tabs -= TABSET;
        out:
            sp++;
            lp++;
            goto verify;

        case CTRLC:
        case CTRLQ: /* forward one char */
            if (*lp == '\0') { putch(BELL); goto verify; }
            /* don’t wrap the screen when near margin */
            if (*lp == SPACE && sp + tabs > threshold) { putch(BELL); goto verify; }
            putch(*lp);
        forward:
            if (*lp == CTRLI) tabs += TABSET;
            *sp++ = *lp++;              /* one character */
            if (sp + tabs >= threshold) putch(BELL);
            continue;

        case CTRLF:
            /* delete forward */
            while (*lp++);
            lp--;
            goto verify;

        case CTRLE:
            putb(lp);
            refresh();
            continue;

        case CTRLH:
            help();
            refresh();
            continue;

        case CTRLR:  /* margin release */
            if (threshold - genbuf < LBSIZE - 40) {
                savethresh = threshold;
                threshold = genbuf + LBSIZE - 40;
            }
            else goto backquery;
            continue;

        case CTRLS:  /* re-set to start of line */
            while ((*sp++ = *lp++));
            rescan();
            lp = linebuf; sp = genbuf;
            tabs = 0;
            goto verify;

        case CTRLW: /* forward one word */
            if (*lp == '\0') { putch(BELL); goto verify; }
            while (*lp == SPACE)
                putch(*sp++ = *lp++);
            if (inword(*lp)) {
                while (inword(*lp)) {
                    /* stop at margin; don’t insert newlines */
                    if (sp + tabs >= threshold) { putch(BELL); goto verify; }
                    putch(*sp++ = *lp++);
                }
                if (*lp == SPACE) {
                    /* at margin, just beep and don’t consume */
                    if (sp + tabs > threshold) { putch(BELL); goto verify; }
                    if (*lp == SPACE)
                        while (*(lp + 1) == SPACE)
                            putch(*sp++ = *lp++);
                }
            }
            else while (*lp && !inword(*lp)) {
                if (*lp == CTRLI) tabs += TABSET;
                if (sp + tabs >= threshold) { putch(BELL); goto verify; }
                putch(*sp++ = *lp++);
            }
            break;

        case CTRLZ: /* delete a word */
            if (sp == genbuf) goto backquery;

            while (*(--sp) == SPACE) if (sp < genbuf) goto zout;
            if (inword(*sp)) {
                while (inword(*(--sp)))
                    if (sp < genbuf) goto zout;
                if (*sp == SPACE)
                    while (*(--sp) == SPACE)
                        if (sp < genbuf) goto zout;
            }
            else while (sp >= genbuf && !inword(*sp))
                if (*sp-- == CTRLI) tabs -= TABSET;
        zout:
            sp++;
            goto verify;

        case '@':  /* delete displayed line (backwards) */
            sp = genbuf;
            tabs = 0;
            goto verify;

        case ESCAPE:
            putstr("\\\r");
            setcook();
            error;

        case CTRLX:
            putch('#');
        case '#':
            if (sp == genbuf) goto backquery;
            if (*(--sp) == CTRLI) tabs -= TABSET;
            if (ch == CTRLX) goto verify;
            continue;

        case '\n':
        case '\r':
            putch('\n');
            ch = '\n';
            *sp++ = ch;
            br = sp;
            break;

        case BACKSL: /* special symbols */
            switch (ch = peekc = getchr()) {
            case '(':  ch = '{'; peekc = 0; break;
            case ')':  ch = '}'; peekc = 0; break;
            case '!':  ch = '|'; peekc = 0; break;
            case '^':  ch = '~'; peekc = 0; break;
            case '\'': ch = '`'; peekc = 0; break;
            case BACKSL:
            case '#':
            case '@':  peekc = 0; break;

            default:
                if (ch >= 'a' && ch <= 'z') { peekc = 0; ch -= 040; }
                else { *(--lp) = BACKSL; goto forward; }
            }
            /* FALLTHRU */

        default: {
            /* typed char: echo immediately and insert safely */
            if (sp + tabs >= threshold) { putch(BELL); goto verify; }

            if (lp == linebuf) {
                /* at start-of-line: avoid underflow of (--lp) */
                *lp = ch;               /* reflect into source */
                putch(ch);              /* live echo */
                goto forward;
            }

            *(--lp) = ch;               /* insert before current source pos */
            putch(ch);                   /* live echo */
            goto forward;
        }
        }

        if (ch == '\n') { /* split line */
            if (*(br - 1) != '\n') putstr("!!"); /* debugging only */
            lnp = sp;
            while ((*sp++ = *lp++)); /* move the rest over */
            brp = linebuf + (br - genbuf);
            lnp = linebuf + (lnp - br);
            rescan();
            *(brp - 1) = '\0';
            return 0;
        }
        else continue;

    backquery:
        putch(CTRLZ);
    } /* end for */
}

int getopen()  /* calls gopen, deals with multiple lines etc. */
{
    char* lp, * sp;
    if (oflag == EM_EOF) return EM_EOF;

    /* otherwise, multiple lines */

    lp = linebuf;
    sp = brp;
    while (*lp++ = *sp++); /*move it down */
    sp = genbuf;
    lp = linebuf;
    while (lp < lnp) *sp++ = *lp++;
    gnp = sp;
    /* should check whether empty line returned */
    oflag = gopen();
    return 0;
}

void help()
{
    putstr("\n");
    putstr("    ^A    display Again        ^Q    next character");
    putstr("    ^B    backup word          ^R    Release margin");
    putstr("    ^S    re-scan from Start");
    putstr("    ^D  close line and exit                          ");
    putstr("    ^E    display to End       ^W    next Word");
    putstr("    ^F    delete line Forward  ^Z    delete word");
    putstr("    ^H    Help                 # or ^X delete character");
    putstr("    ESCAPE exit unchanged      @     delete line backward\n");
    putstr("    Other characters (including RETURN) inserted as typed");
}
