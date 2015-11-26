/*
    PMNEWS 1.0

    Private decls the PMNEWS news reader

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 1, as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    See the file COPYING, which contains a copy of the GNU General
    Public License.

 */

#include <stdlib.h>
#include <process.h>
#include <io.h>
#include "pmdefs.h"


/* #define INCLUDE_SIG */   /* enable this to have the reply function put */
                            /* your sig on the reply - mail ususally does */
                            /* this                                       */



#define EX_DONE             1
#define EX_QUIT             2
#define EX_SAVE             3
#define EX_NEXT             4
#define EX_NEXT_UNREAD      5
#define EX_ROT13            6
#define EX_DUMMY            7

#define TEXT_LINE           5


#define PAGE_HEADER         4
#define PAGE_LENGTH         19

/* default article window size */
#define ARTWIDTH            620
#define ARTHEIGHT           450
/* default articlel winidow location */
#define ARTX                20
#define ARTY                20

/* if you change these see show_help */
#define HELP_GROUP          0
#define HELP_THREAD         1
#define HELP_ARTICLES       2

#pragma linkage (main,optlink)
#define MSGBOXID    1001

#define PMNEWS_CLASS "PMNews"


/* WM_SEM2 messages - to HlistProc */
#define GET_FOCUS    0x00000004
#define STOP_READING 0x00000003
#define NEXT_ARTICLE 0x00000002
#define NEXT_UNREAD  0x00000001


/* WM_USER messages - to ArticleProc from util task */
#define MAIL_VERIFY   0x00000004
#define MAIL_FILERR   0x00000005
#define REPART_QUOTE  0x00000006
#define WE_CANCELLED  0x00000007
#define POST_ARTICLE  0x00000008
#define POST_VERIFY   0x00000009
#define MAIL_ARTICLE  0x0000000A
#define START_SEARCH  0x0000000B
#define DISP_LIST     0x0000000C
#define PREV_ARTICLE  0x0000000D
#define ART_FOCUS     0x0000000E
#define START_CSEARCH 0x0000000F


/*
 *  This structure allows the creation of linked list of article numbers
 */
typedef struct art_id {
    long   id;                  /* article number                 */
    long   art_off;             /* offset of the article          */
    struct art_id *next_art;    /* pointer to next article number */
    struct art_id *last_art;    /* pointer to prev article number */
} ART_ID;

/*
 *  This structure is a doubly linked list of the unique article headers.  The
 *  linked list of article numbers is built on 'art_num'.  This system
 *  is allows flexible use of memory, but will get slower by n'ish
 *  and there is a fair degree of allocation overhead in the ART_ID structure
 *  But hey, it's simple
 */
typedef struct article {
    char   *header;              /* article header              */
    int    num_articles;        /* number with this header     */
    ART_ID *art_num;            /* pointer to list of articles */
    struct article *next;       /* next topic                  */
    struct article *last;       /* last topic                  */
    int    index;               /* topic number from start     */
    struct article *left;       /* for threading               */
    struct article *right;      /*     search                  */
} ARTICLE;





/*
 *  This structure is a linked list of lines that make up an article. The
 *  file is read in and the linked list is built
 */
typedef struct line {
    char   data[80];            /* line of text                */
    struct line *next;          /* next line                   */
    struct line *last;          /* last line                   */
    int    index;               /* line number from start      */
} aLINE;


/*
 *  This structure is the handle for an article in ram.  The file
 *  is read in and the linked list built.
 */
#define WHO_LENGTH 35
typedef struct {
    char  *author;                      /* From address                  */
    char  *rname;                       /* real name of author           */
    char  organisation[WHO_LENGTH];     /* truncated organisation        */
    char  follow_up[80];      /* group for follow-up article             */
    int   lines;              /* total lines in file                     */
    aLINE  *top;               /* points to start of article, incl header */
    aLINE  *start;             /* points to start of text                 */
} TEXT;





ARTICLE *get_headers(ACTIVE *gp);
void eat_gunk(char *buf);
void free_header(ARTICLE *start);


int count_unread_in_thread(ACTIVE *gp, ARTICLE *a);
int count_unread_in_group(ACTIVE *gp);
void mark_read_in_thread(ACTIVE *gp, ARTICLE * ap );
void mark_group_as_read(ACTIVE *gp);

USHORT message(PSZ text, PSZ label, ULONG mstyle );

TEXT *load_article(char *fnx, long offset);
void free_article(TEXT *t);

void save_to_disk(TEXT *tx);
void reply_to_article(TEXT *tx);
void get_his_stuff(TEXT *tx, char *author, char *msg_id);


void post(TEXT *tx, char *newsgroups, char *subject);
void post_it(FILE *article, char *newsgroups, char *subject, char *dist,
                  char *msg_id);

void rot13(TEXT *tx);

void expand_tabs(char *buf, int max_len);

int newsgroups_valid(char *ng);

void mail_to_someone(TEXT *tx, char * who);
void post_art( USHORT doquote );
ARTICLE *search_group( ACTIVE *gp, char *archarg, BOOL sensi );

/* common data area */
HWND hwndClient=0L;                     /* Client area window handle    */
HWND hwndArt=0L;                        /* Article window handle          */

HEV  work_to_do,  /* schedule utility task */
     eojsem;      /* terminate util task */
USHORT  queryans;   /* last message box query result */

ARTICLE * artp;               /* selected thread */
TEXT    * tx;                   /* current article text info */
/* MBID_YES or no -- quote current article in post */
USHORT quotans;


#define RECIPL  80
char   recipient[RECIPL+1];   /* mail recipient */
char   ngroup[128];   /* post group */
char   nsubj[128];   /* post subject */

