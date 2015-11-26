/*
    SNEWS 1.0

    General public decls


    Copyright (C) 1991  John McCombs, Christchurch, NEW ZEALAND
                        john@ahuriri.gen.nz
                        PO Box 2708, Christchurch, NEW ZEALAND

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

#define  _CLIP_WIN_

/* #include <alloc.h> */
#include <time.h>
#include <string.h>
#include <conio.h>
/* #include <dos.h> */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <stddef.h>
/* #include <dir.h>    */



#define ACTIVE_NUM_LEN   8      /* length of the numbers in the active file */

#define TRUE     1
#define FALSE    0


/* this is the data we get from the UUPC .rc files */
typedef struct {
    char temp_name[80];           /* unbatch temp file             */
    char news_dir[80];            /* news base directory           */
    char incoming_dir[80];        /* incoming news spool directory */
    char user[80];                /* current user id               */
    char my_name[80];             /* my full name                  */
    char my_domain[80];           /* our domain                    */
    char my_site[80];             /* site name                     */
    char my_organisation[80];     /* organisation                  */
    char mail_server[80];         /* where posts are routed to     */
    char editor[80];              /* system editor                 */
    char home[80];                /* home mail directory           */
    char signature[80];           /* signature file                */
    char my_alias[80];            /* alias file                    */
} INFO;


/* NOTE - if hi_num and lo_num are the same there are no articles */
typedef struct active {
    char   group[60];           /* group name                               */
    char   gp_file[9];          /* name of the file that the data is in     */
    long   lo_num;              /* lowest number less one                   */
    long   hi_num;              /* highest number                           */
    long   num_pos;             /* file offset of the numbers               */
    struct active *next;        /* next entry                               */
    struct active *last;        /* last entry                               */
    int    index;               /* which number in the list, from 0         */
    char   *read_list;          /* array hi_num-lo_num long. TRUE=read it   */
} ACTIVE;



/*
 *  This singly linked list is used to store the names of the groups
 *  we can post to.
 */
typedef struct post_groups {
    char   group[60];           /* group name                               */
    struct post_groups *next;   /* next entry                               */
} POST_GROUPS;



/*
 *  READ LIST:
 *      The list of articles which has been seen by a user is kept in an
 *      ascii file, which has a newsgroup name followed by the list
 *      of article numbers which have been seen.
 *
 *      The file is read by 'load_read_list', which allocates and array of
 *      flags, one per article, and plugs these into the ACTIVE structure.
 *      The flags are set to TRUE when a user has seen an article.
 *
 *      On shutdown a new 'user.nrc' file is written
 */




/*
 *  This structure is an index to the history file.  'mid' is a 32bit hash
 *  of the message id.  'offset' is the offset into the history file, and
 *  'next' makes the linked list
 */
typedef struct hist_list {
    long mid;
    long offset;
    struct hist_list *next;
} HIST_LIST;


/*
 *  This linked list is returned by 'look_up_history'.  It is a list
 *  of the groups to which an article has been crossposted.  It does
 *  not include self
 */
typedef struct cross_posts {
    char   group[60];            /* group name                               */
    long   art_num;              /* article number in this group             */
    struct cross_posts *next;    /* next entry                               */
} CROSS_POSTS;


/*
 *  Linked list of mailing aliases, located via environment variable
 *  UUPCUSRPC .
 *
 */
typedef struct alii {
    struct alii *nextp;
    char   nick[16];
    char   name[48];
    char   addr[128];
}   ALII;

/* global data */
#ifdef MAIN_FILE
#define MAIN_DEFINED
#else
#define MAIN_DEFINED extern
#endif
MAIN_DEFINED INFO my_stuff;    /* data from .rc files */
MAIN_DEFINED FILE     *errf;   /* debug file */
MAIN_DEFINED int    szalias;  /*  largest alias */

/* function list */
ACTIVE *load_active_file(void);
ALII   *load_alias_file(void);
void close_active_file(void);
void close_active(void);
ACTIVE *find_news_group(char *group);
void update_active_entry(ACTIVE *a);
char *make_news_group_name(char *ng);

void save_read_list(void);
void load_read_list(void);

int load_stuff(char **errfld, char **errmsg);

FILE *open_out_file(char *ng);
FILE *open_index_file(char *ng);

int post_sequence(void);

void *xmalloc(size_t size);

int check_valid_post_group(char *ng);

void free_ng(void);

FILE *open_hist_file(void);
void close_hist_file(void);
void add_hist_record(char *msg_id, char *ng);

HIST_LIST *load_history_list(void);
void free_hist_list(void);
HIST_LIST *find_msg_id(char *msg_id);
CROSS_POSTS *look_up_history(char *msg_id, char *ng);
void free_cross_post_list(CROSS_POSTS *cx);
