/*
    SNEWS 1.0

    History routines


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


#include "defs.h"
#include "history.h"


FILE *hist;
HIST_LIST *hlist;


/*----------------------- open hist file for writing ------------------------*/
FILE *open_hist_file(void)
{
    /*
     *  This routine opens the history file for writing, positioned after
     *  the last record.
     */

    char fn[256];

    sprintf(fn, "%shistory", my_stuff.news_dir);

    if ((hist = fopen(fn, "ab")) == NULL) {
        fprintf(stderr, "history: cannot open file %s for append\n", fn);
        exit(1);
    }

    return(hist);
}



/*---------------------------- close hist file ---------------------------*/
void close_hist_file(void)
{
    /*
     *  This routine closes the history file
     */

    fclose(hist);
}


/*---------------------------- add record -------------------------------*/
void add_hist_record(char *msg_id, char *ng)
{
    /*
     *  This routine adds a record to the history files.  It is passed the
     *  message id, and the newsgroup list.  The newsgroup list is unwound,
     *  and the article numbers are added to the record.
     *
     *  If there is only one newsgroup no history record is written
     *
     *  We assume that this routine is called after the article has been
     *  posted, so that the article counters are correct.
     */


    char   *p, buf[512];
    ACTIVE *a;
    time_t t;
    int    ct;

    time(&t);

    /* count the newsgroups */
    strcpy(buf, ng);
    p = strtok(buf, " \t,\n\r");
    ct = 0;

    while (p != NULL) {
        ct++;
        p = strtok(NULL, " \t,\n\r");
    }


    if (ct > 1) {

        strcpy(buf, ng);
        p = strtok(buf, " \t,\n\r");

        if ((p != NULL) && (strlen(msg_id) > 0)) {

            fprintf(hist, "%s %09ld ", msg_id, t);

            while (p != NULL) {
                a = find_news_group(p);
                fprintf(hist, "%s %08ld  ", a->group, a->hi_num);
                p = strtok(NULL, " \t,\n\r");
            }
            fprintf(hist, "\n");
        }
    }
}





/*--------------------- read history file into ram -------------------------*/
HIST_LIST *load_history_list(void)
{
    /*
     *  This routine opens and reads the history file, building and index
     *
     *  Load this after active and ng files.  Set HIST_MEM_LIMIT in defs.h
     *  to ensure there is enough memory left
     */

    char      buf[512], *p;
    long      where;
    HIST_LIST *h;

    sprintf(buf, "%shistory", my_stuff.news_dir);

    /* open the file */
    if ((hist = fopen(buf, "rb")) == NULL) {
        fprintf(stderr, "history: cannot open file %s for reading\n", buf);
        exit(1);
    }

    where = 0;
    hlist = h = NULL;

    while (fgets(buf, 511, hist)) {

        p = strtok(buf, " \t\n\r");

        if ( p != NULL )
           {
           if (hlist == NULL) {
              hlist = xmalloc(sizeof(HIST_LIST));
              h = hlist;
           } else {
              h->next = xmalloc(sizeof(HIST_LIST));
              h = h->next;
              }

           h->mid = hash_msg_id(p);
           h->offset = where;

           where = ftell(hist);
           }
    }

    if ( h )
        h->next = NULL;

    return(hlist);
}




/*------------------------- release the history list ----------------------*/
void free_hist_list(void)
{
    /*
     *  Close the history file and free the memory list
     */
    HIST_LIST *h;

    close_hist_file();

    while (hlist != NULL) {
        h = hlist->next;
        free(hlist);
        hlist = h;
    }
}




/*-------------------------- find history entry -----------------------------*/
HIST_LIST *find_msg_id(char *msg_id)
{
    /*
     *  Look up the history list and return the entry for the req'd msg id
     *  or NULL if not found.
     */

    HIST_LIST *h;
    long      hashed_id;

    hashed_id = hash_msg_id(msg_id);
    h = hlist;

    while (h != NULL) {
        if (h->mid == hashed_id) break;
        h = h->next;
    }

    if ( h == NULL ) return NULL;

    if (h->mid == hashed_id)
        return(h);
    else
        return(NULL);
}




/*----------------------- lookup cross posts --------------------------------*/
CROSS_POSTS *look_up_history(char *msg_id, char *ng)
{
    /*
     *  This routine returns a linked list of the groups to which
     *  an article has been crossposted.  Self is excluded.  To do
     *  this we:
     *      - hash the msg id
     *      - search the list of id's
     *      - if a hit is found we read it from the hist file
     *      - decode the record, excluding self
     *      - build a CROSS_POSTS list
     *
     *  NULL is returned if there were no crossposts.  ASSUMES global
     *  file 'hist' is open.  If it isn't this routine always returns
     *  NULL
     */

    long        hid;
    HIST_LIST   *h;
    CROSS_POSTS *cx, *c;
    char        buf[512], *p;

    hid = hash_msg_id(msg_id);

    /* look for the crosspost entry */
    h = hlist;
    while (h != NULL) {
        if (h->mid == hid) break;
        h = h->next;
    }

    /* now look up the history file */
    c = cx = NULL;
    if ( (h) && (h->mid == hid) ) {
        fseek(hist, h->offset, SEEK_SET);
        if (fgets(buf, 511, hist) != NULL) {

            /*
             *  Compare the target msg id with the one on the file.  If they
             *  are different then the must be a hash collision - just abort
             */
            p = strtok(buf, " \t\n\r");
            if (stricmp(p, msg_id) == 0) {


                /* skip the date field and leave pointiing to the first group */
                p = strtok(NULL, " \t\n\r");
                p = strtok(NULL, " \t\n\r");

                while (p != NULL) {

                    /* exclude self */
                    if (stricmp(p, ng) != 0) {
                        if (cx == NULL) {
                            cx = xmalloc(sizeof(CROSS_POSTS));
                            c = cx;
                        } else {
                            c->next = xmalloc(sizeof(CROSS_POSTS));
                            c = c->next;
                        }

                        strcpy(c->group, p);
                        p = strtok(NULL, " \t\n\r");
                        c->art_num = atol(p);
                        c->next = NULL;

                    } else {
                        /* eat article number */
                        p = strtok(NULL, " \t\n\r");
                    }

                    p = strtok(NULL, " \t\n\r");
                }

            }
        }
    }

    return(cx);

}



/*----------------------- deallocate crosspost list --------------------------*/
void free_cross_post_list(CROSS_POSTS *cx)
{
    CROSS_POSTS *c;

    while (cx != NULL) {
        c = cx->next;
        free(cx);
        cx = c;
    }

}






/*------------------------ random number generator -------------------------*/
long seed;

long xrand(void)
{
    seed = (seed * 16807) & 0x7FFFFFFFL;
    return( seed );
}



/*----------------------------- hash a key -----------------------------------*/
long hash_key(long s, char *key)
{
    long hash_num;
    int  i;

    hash_num = 0;
    seed = s;

    for (i = 0; i < strlen(key); i++) {
        hash_num += xrand() * (key[i] + 0xFF00);
    }

    hash_num = hash_num & 0x7FFFFFFFL;
    if (hash_num == 0) hash_num++;

    return(hash_num);
}


/*----------------------------- hash the message id -------------------------*/
long hash_msg_id(char *msg_id)
{

    return( hash_key(hash_key(26l, msg_id), msg_id) );
}
