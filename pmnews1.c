
/*
    PMNEWS 1.0

    Service functions for the PMNEWS news reader

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
#include  <ctype.h>
#define   INCL_DOSSEMAPHORES
#define   INCL_DOSPROCESS
#define   INCL_DOSSESMGR
#define   INCL_DOSQUEUES
#include  <os2.h>
#include "defs.h"
#include "pmnews.h"
#include "pmnthrd.h"


static unsigned char   SysTermQ[] = "\\QUEUES\\PMNEWS\\EDITART.QUE";
HQUEUE SysTermH;  /* must be static, since it needs to be open
                     for the life of our thread */
/*--------------------------- new_session --------------------------------*/
/* start a new session (text-windowed, foreground, child )
   the boys who thought this up were some very sick puppies */
int  new_session( PSZ  cmd, PBYTE opt, PSZ title )
{

  STARTDATA  sd;
  ULONG sessnum = 0;
  PID   pid = 0;
  APIRET rc;
  REQUESTDATA rq;
  PVOID  dbuf;
  ULONG  dlen;
  BYTE   pri;

  if ( SysTermH == (HQUEUE)NULL )
  rc = DosCreateQueue( &SysTermH, 0L, &SysTermQ[0] );

  memset( &sd, 0, sizeof(STARTDATA) );
  sd.Length = sizeof(STARTDATA);
  sd.Related = SSF_RELATED_CHILD;
  sd.FgBg = SSF_FGBG_FORE;
  sd.TraceOpt = SSF_TRACEOPT_NONE;
  sd.PgmTitle = title;
  sd.PgmName  = cmd;
  sd.PgmInputs = opt;
  sd.TermQ = &SysTermQ[0];
  sd.Environment = NULL;
  sd.InheritOpt = SSF_INHERTOPT_PARENT;
  sd.SessionType = SSF_TYPE_DEFAULT;
  sd.IconFile = NULL;
  sd.PgmHandle = 0;

  rc = DosStartSession( &sd, &sessnum, &pid );
  if ( ( rc != 0 ) && ( rc != 457 ) )
     fprintf( errf, "startsess: %ld  num: %ld %x\n", rc, sessnum, pid );
  else
     do {
        rc = DosReadQueue( SysTermH, &rq, &dlen, &dbuf, 0L, DCWW_WAIT, &pri, (HEV)0L );
        DosFreeMem( dbuf );
        }  while ( rq.ulData != 0 && rc == 0 );

  return 0;
}

/*--------------------------- expand the tabs ----------------------------*/
void expand_tabs(char *buf, int max_len)
{
    int  l, k;
    char tmp[256], *p, *t;

    p = buf;
    t = &tmp[0];
    l = 0;

    while ((*p != '\x00') && (l < max_len - 1)) {
        if (*p != '\x09') {
            *t = *p;
            t++;
            p++;
            l++;
        } else {
            p++;
            k = ((l / 8) + 1) * 8;
            for ( ; l < k ; l++) {
                *t = ' ';
                t++;
                if (l >= max_len - 1) break;
            }
        }
    }

    *t = '\x00';
    strcpy(buf, tmp);
}


/*------------------- count unread articles in a group --------------------*/
int count_unread_in_group(ACTIVE *gp)
{
    /*
     *  Take the thread 'a' for the given group 'gp' and return the number
     *  of unread articles
     */

    int articles, ct, i;

    ct = 0;
    articles = (int)(gp->hi_num - gp->lo_num);

    for (i = 0; i < articles; i++) {
        if ( (gp->read_list) && (*((gp->read_list)+i) == FALSE) )
            ct++;
    }

    return(ct);
}


/*------------------- count unread articles in a thread --------------------*/
int count_unread_in_thread(ACTIVE *gp, ARTICLE *a)
{
    /*
     *  Take the thread 'a' for the given group 'gp' and return the number
     *  of unread articles
     */

    ART_ID *id;
    int    ct, idx;

    ct = 0;
    id = a->art_num;

    while (id != NULL) {
        idx = (int)(id->id - gp->lo_num - 1);
        if (*((gp->read_list)+idx) == FALSE)
            ct++;
        id = id->next_art;
    }

    return(ct);
}

/********************************************************/
void mark_group_as_read(ACTIVE *gp)
{

    int articles, i;


    articles = (int)(gp->hi_num - gp->lo_num);

    for (i = 0; i < articles; i++)
        *((gp->read_list)+i) = TRUE;


}


/*------------------- mark articles read in a thread --------------------*/
void mark_read_in_thread(ACTIVE *gp, ARTICLE *a)
{
    /*
     *  Take the thread 'a' for the given group 'gp' and mark the articles
     *  as read.
     */

    ART_ID *id;
    int     idx;

    id = a->art_num;

    while (id != NULL) {
        idx = (int)(id->id - gp->lo_num - 1);
        *((gp->read_list)+idx) = TRUE;
        id = id->next_art;
    }

    return;
}




/*-------------------- release the subject structures ---------------------*/
void free_header(ARTICLE *start)
{
    /*
     *  Work our way through the subject structure releasing all the
     *  memory
     */

    ARTICLE *a, *t;
    ART_ID  *u, *v;

    a = start;

    while (a != NULL) {

        u = a->art_num;
        while (u != NULL) {
            v = u->next_art;
            free(u);
            u = v;
        }

        t = a->next;
        free(a->header);
        free(a);
        a = t;
    }
}

static char  noauth[] = "** no one **";
static char  noname[] = "** unknown **";

/*---------------------- deallocate article memory ------------------------*/
void free_article(TEXT *t)
{

    aLINE *l, *k;

    l = t->top;
    while (l != NULL) {
        k = l;
        l = l->next;
        free(k);
    }

    if ( t->author != noauth ) free( t->author );
    if ( t->rname != noname ) free( t->rname );
    free(t);
}


/*------------------------ clean up subject line ----------------------------*/
void eat_gunk(char *buf)
{
    /*
     *  This routine takes the header line, and strips the
     *  word header word, 'Re:', and any extra blanks, trim to 59 chars
     */

    char *p, tmp[256];

    buf[58] = 0;


    /* strip the Subject: etc off the front */
    while ((strstr(buf, "Re:") != NULL) ||
        (strnicmp(buf, "From:", 5) == 0) ||
        (strnicmp(buf, "Path:", 5) == 0) ||
        (strnicmp(buf, "Subject:", 8) == 0) ||
        (strnicmp(buf, "Organization:", 13) == 0) ||
        (strnicmp(buf, "Organisation:", 13) == 0) ||
        (strnicmp(buf, "Followup-To:", 12) == 0)) {
        p = strstr(buf, ":");
        strcpy(buf, p+1);
    }

    strcpy(tmp, buf);
    *buf = '\x00';
    p = strtok(tmp, " \n\r");

    while (p != NULL) {

        if (stricmp(p, "Re:") != 0) {
            strcat(buf, p);
            strcat(buf, " ");
        }
        p = strtok(NULL, " \n\r");
    }

    if (strlen(buf) <= 1)
        strcpy(buf, "** no subject **");
}



/*----------------------- get stuf off article header --------------------*/
void get_his_stuff(TEXT *txs, char *author, char *msg_id)
{
    /*
     *  Retrieve the author and msg_id from the article
     */

    aLINE *ln;
    char *p;
    char buf[100];
    char *null_name = {" ** none ** "};

    strcpy(author, null_name);
    strcpy(msg_id, " <none> ");

    ln = txs->top;
    while (ln != NULL) {
        strcpy(buf, ln->data);
        p = strtok(buf, " :\n\r");
        p = strtok(NULL, " :\n\r");
        if (strnicmp(ln->data, "Message-ID:", 11) == 0) {
            strcpy(msg_id, p);
        }

        if ((strnicmp(ln->data, "From:", 5) == 0) &&
          (strcmp(author, null_name) == 0)) {
            strcpy(author, p);
        }

        if (strnicmp(ln->data, "Reply-To:", 5) == 0) {
            strcpy(author, p);
        }

        if (strlen(ln->data) < 2) break;
        ln = ln->next;
    }
}

/*------------------------- read the headers --------------------------*/
ARTICLE *get_headers(ACTIVE *gp)
{
    /*
     *  Read the files and get the headers
     */

    char *fn;
    char buf[256], fnx[256], *buf_p;
    long g;
    FILE *tmp_file;

    ARTICLE *start, *that, *tmp;
    ART_ID  *art_this, *new;
    int     ct_art;
    void    *user;

    ct_art = 0;
    start  = NULL;

    user  = thread_init();

    fn = make_news_group_name(gp->group);
    sprintf(fnx, "%s.IDX", fn);

    if ((tmp_file = fopen(fnx, "rb")) != NULL) {

        for (g = gp->lo_num+1; g <= gp->hi_num; g++) {


            /*
             *  Read the index
             *  Search the linked list for the subject
             *    - allocate a new subject if necessary
             *    - add to the existing list
             */

            if (fgets(buf, 255, tmp_file) == NULL) {
                message( "Index file is corrupt", "PMNews error", MB_CANCEL );
                exit(1);
            }

            /* check all is in sync */
            if (g != atol(buf+9)) {
                sprintf( fnx, "Article %ld found when %ld expected",
                               atol(buf+9), g );
                message( fnx, "PMNews error", MB_CANCEL );
                exit(1);
            }

            /* skip the two eight digit numbers and the 9 and the spaces */
            buf_p = buf+28;

            eat_gunk(buf_p);

            if ( thread_compare( buf_p, user, &tmp ) )
               {
                    /* found this subject */
                    tmp->num_articles++;

                    /* allocate new article number */
                    new = xmalloc(sizeof (ART_ID));
                    new->id = g;
                    new->art_off = atol(buf);

                    /* place it at the end */
                    art_this = tmp->art_num;
                    while (art_this->next_art != NULL) {
                        art_this = art_this->next_art;
                    }
                    art_this->next_art = new;

                    new->last_art = art_this;
                    new->next_art = NULL;

               }

            else
               {
               /* not found - allocate new thread */

                if (start == NULL) {
                    start = tmp;  /* side-effect of thread_compare */
                    that = start;
                    that->last = NULL;
                } else {
                    ct_art++;
                    that->next = tmp; /* side-effect of thread_compare */
                    that->next->last = that;
                    that = that->next;
                }

                that->next = NULL;
                that->index = ct_art;
                /* that->left and that->right may be modified
                   by thread_compare */

                /* store article data */
                that->header = xmalloc(strlen(buf_p) + 1);
                strcpy(that->header, buf_p);
                that->num_articles = 1;

                that->art_num = xmalloc(sizeof (ART_ID));
                that->art_num->last_art = NULL;
                that->art_num->next_art = NULL;
                that->art_num->id = g;
                that->art_num->art_off = atol(buf);

            }
        }

        thread_end( user );
        fclose(tmp_file);

    } else {
        sprintf( buf, "Can't open index file %s", fnx );
        message( buf, "PMNews error", MB_CANCEL );
        exit(1);
    }

    return(start);

}


/*------------------------- read in an article -----------------------------*/
TEXT *load_article(char *fnx, long offset)
{
    /*
     *  Open the file and read it.  Save the author and organisation
     *  fill in the structures
     */

    FILE *tmp_file;
    char buf[256], lnbuf[256], *p, *rp;
    TEXT *txn;
    aLINE *ln, *lz;
    int  ct, i;

    txn = NULL;
    ct = 0;

    if ((tmp_file = fopen(fnx, "rb")) != NULL) {

        fseek(tmp_file, offset, SEEK_SET);

        txn = xmalloc(sizeof(TEXT));
        txn->top = NULL;
        txn->start = NULL;


        strcpy(txn->follow_up, "");
        txn->author =  &noauth[0];
        txn->rname  =  &noname[0];

        strcpy(txn->organisation, " ** none ** ");

        while (fgets(buf, 255, tmp_file) != NULL) {

            if (strncmp(buf, "@@@@END", 7) == 0) break;

            expand_tabs(buf, 255);

            /*
             *  We now have a line of input.  If the line is two long
             *  it is wrapped at spaces or '!'.  The lines of text are
             *  stored in LINE structures
             */
            p = &buf[0];
            while (strlen(p) > 0) {

                strcpy(lnbuf, p);
                if (strlen(p) <= 80) {
                    strcpy(lnbuf, p);
                    *p = '\x00';
                } else {
                    p += 79;
                    for (i = 79; i > 50; i--) {
                        if ((lnbuf[i] == ' ') || (lnbuf[i] == '!'))
                            break;
                        p--;
                    }
                    lnbuf[i] = '\x00';
                }

                /* is it the first line - if so int the TEXT structure */
                if (ct == 0) {
                    ln = xmalloc(sizeof(aLINE));
                    ln->last = NULL;
                    txn->top = ln;
                } else {
                    lz = ln;
                    ln->next = xmalloc(sizeof(aLINE));
                    ln = ln->next;
                    ln->last = lz;
                }

                lnbuf[79] = '\x00';
                ln->index = ct;
                strcpy(ln->data, lnbuf);

                if ((strlen(lnbuf) == 1) && (txn->start == NULL))
                    txn->start = ln;

                ct++;

                /* save the header info */
                if ((txn->start == NULL) && (strncmp("From:", lnbuf, 5) == 0)) {
                    eat_gunk(lnbuf);
                    rp = strchr(lnbuf, '(');
                    txn->rname = NULL;
                    if ( rp )
                       {
                       rp++;
                       txn->rname = xmalloc( strlen( rp ) + 1 );
                       strcpy( txn->rname, rp );
                       if ( ( rp = strchr( txn->rname, ')' ) ) != NULL )
                          *rp = '\0';
                       }

                    txn->author = xmalloc(strlen(lnbuf) + 1);
                    lnbuf[WHO_LENGTH-1] = '\x00';
                    strcpy(txn->author, lnbuf);
                }
                if ((txn->start == NULL) &&
                  ((strncmp("Organisation:", lnbuf, 13) == 0) ||
                  (strncmp("Organization:", lnbuf, 13) == 0))) {
                    eat_gunk(lnbuf);
                    lnbuf[WHO_LENGTH-1] = '\x00';
                    strcpy(txn->organisation, lnbuf);
                }

                if ((txn->start == NULL) && (strncmp("Followup-To:", lnbuf, 12) == 0)) {
                    eat_gunk(lnbuf);
                    lnbuf[79] = '\x00';
                    strcpy(txn->follow_up, lnbuf);
                }
            }

        }

        ln->next = NULL;
        txn->lines = ct;

        fclose(tmp_file);
    }

    return(txn);
}

static char *addressee;

/********************* mail current article ************************/
void mail_to_someone(TEXT *tx, char * who )
{
    /*
     *  Mail this article to someone
     */

    ULONG  pcnt;
    FILE  *tmp;
    aLINE *ln;
    char fn[80];
    char buf[256];
    char author[80], msg_id[80];

    strcpy(fn, "reply.tmp");

    if ((tmp= fopen(fn, "w")) != NULL) {

        get_his_stuff(tx, author, msg_id);

        /* add the quoted message */

        fprintf(tmp, "\nThis article was forwarded to you by %s@%s (%s):\n\n",
            my_stuff.user, my_stuff.my_domain, my_stuff.my_name);
        fprintf(tmp,"--------------------------------- cut here -----------------------------\n\n");
        ln = tx->top;
        while (ln != NULL) {
            fprintf(tmp, "%s", ln->data);
            ln = ln->next;
        }

        fprintf(tmp,"--------------------------------- cut here -----------------------------\n\n");

        fclose(tmp);

        pcnt = 0;

        addressee = who;

        WinPostMsg( hwndArt, WM_USER, MPFROMLONG(MAIL_VERIFY),
                     MPFROMP(addressee) );


        DosWaitEventSem( work_to_do, -1L );
        DosResetEventSem( work_to_do, &pcnt );

        if ( queryans == MBID_YES )
            {
            sprintf(buf, "mail %s <%s", who, fn);
            system(buf);
            }
        remove(fn);

    } else {
        WinPostMsg( hwndArt, WM_USER, (MPARAM) MAIL_FILERR, (MPARAM) 0L );
    }

}



/*-------------------------- reply to article ---------------------------*/
void reply_to_article(TEXT *tx)
{
    /*
     *  Mail reply to article
     */

    FILE *sig;
    char sig_fn[80];

    FILE *tmp;
    aLINE *ln;
    ULONG  pcnt;
    char fn[80];
    char buf[256];
    char author[80], msg_id[80];
    char *cp;

    strcpy(fn, "reply.tmp");

    if ((tmp = fopen(fn, "w")) != NULL)
        {
        get_his_stuff(tx, author, msg_id);


        /* add the quoted message */

        WinPostMsg( hwndArt, WM_USER, (MPARAM) REPART_QUOTE, (MPARAM) 0L );
        DosWaitEventSem( work_to_do, -1L );
        DosResetEventSem( work_to_do, &pcnt );

        if ( queryans == MBID_YES )
           {

            fprintf(tmp, "In article %s you write:\r\n", msg_id);
            ln = tx->start;
            while (ln != NULL) {
                fprintf(tmp, "> %s", ln->data);
                ln = ln->next;
                }
           }


        /* append the signature if there is one */
        strcpy(sig_fn, my_stuff.home);
        strcat(sig_fn, my_stuff.signature);
        if ((sig= fopen(sig_fn, "r")) != NULL) {
            fputs("\r\n--\r\n", tmp);
            while (fgets(buf, 79, sig) != NULL)
                fputs(buf, tmp);
            fclose(sig);
            }

        fclose(tmp);

        /* trim the editor program name */
        cp = my_stuff.editor;
        while ( *cp <= ' ' ) cp++;
        strcpy( buf, cp );
        cp = buf;
        while ( *cp > ' ' ) cp++;
        *cp = '\0';
        if ( strrchr( buf, '.' ) == NULL )
           strcat( buf, ".exe" );

        new_session( buf, fn, "Edit Reply" );

        WinPostMsg( hwndArt, WM_USER, MPFROMLONG( MAIL_VERIFY ),
                             MPFROMP( author ) );
        DosWaitEventSem( work_to_do, -1L );
        DosResetEventSem( work_to_do, &pcnt );

        if ( queryans == MBID_YES )
           {
           strcpy(msg_id, artp->header);
           eat_gunk(msg_id);
           sprintf(buf, "mail -s \"Re: %s\" %s <%s 2>nul",
                    msg_id, author, fn);
           system(buf);
           }
        remove(fn);

    } else {
        WinPostMsg( hwndArt, WM_USER, (MPARAM) MAIL_FILERR, (MPARAM) 0L );
        }

}


/*--------------------------- post an article ---------------------------*/
void post_it(FILE *article, char *newsgroups, char *subject, char *dist,
                  char *msg_id)
{
    /*
     *  Post an article.  The article is passed as an open file,
     *  with the other header data.  The header is added to the file
     *  and the article sent.
     */

    FILE   *tmp, *local, *log;
    char   buf[256], *p;
    char   ng[256];
    char   d_name[20], x_name[20];
    char   short_d_name[20], short_x_name[20];
    int    ct, seq;
    time_t t;
    struct tm *gmt;
    long   where;
    ACTIVE *gp;

    static char *dow[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };
    static char *mth[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep",
        "Oct", "Nov", "Dec"
    };

    ct = 0;

    seq = post_sequence();
    sprintf(d_name, "D.%s%04x", my_stuff.my_site, seq);
    sprintf(x_name, "X.%s%04x", my_stuff.my_site, seq);

    sprintf(short_d_name, "D.%.3s", my_stuff.my_site);
    sprintf(short_x_name, "X.%.3s", my_stuff.my_site);

    /* count the lines */
    rewind(article);
    while (fgets(buf, 255, article) != NULL)
        ct++;


    /*
     *  Make the final article file.  This posting stuff is a horrible
     *  kludge (blush).  If you get a around to fixing this, please
     *  post me a copy :)
     */
    if ((tmp = fopen(short_d_name, "w+b")) != NULL) {

        fprintf(tmp, "Path: %s!%s\n", my_stuff.my_site, my_stuff.user);
        fprintf(tmp, "From: %s@%s (%s)\n", my_stuff.user, my_stuff.my_domain,
            my_stuff.my_name);
        fprintf(tmp, "Newsgroups: %s\n", newsgroups);
        fprintf(tmp, "Subject: %s\n", subject);
        fprintf(tmp, "Distribution: %s\n", dist);

        fprintf(tmp, "Message-ID: <%ldsnx@%s>\n", time(&t), my_stuff.my_domain);

        if (strlen(msg_id) > 0)
            fprintf(tmp, "References: %s\n", msg_id);

        time(&t);
        gmt = gmtime(&t);
        fprintf(tmp, "Date: %s, %02d %s %02d %02d:%02d:%02d GMT\n",
                dow[gmt->tm_wday],
                gmt->tm_mday, mth[gmt->tm_mon], (gmt->tm_year % 100),
                gmt->tm_hour, gmt->tm_min, gmt->tm_sec);

        fprintf(tmp, "Organization: %s\n", my_stuff.my_organisation);
        fprintf(tmp, "Lines: %d\n\n", ct);

        /* copy the rest */
        rewind(article);
        while (fgets(buf, 255, article) != NULL) {
            fputs(buf, tmp);
        }

        /* ok, now post it locally */
        strcpy(ng, newsgroups);
        p = strtok(ng, " ,");
        while (p != NULL) {

            if (fseek(tmp, 0L, SEEK_SET) != 0) {
                WinPostMsg( hwndArt, WM_USER, (MPARAM) MAIL_FILERR, (MPARAM) 0L );
                return;
            }

            local = open_out_file(p);
            where = ftell(local);
            while (fgets(buf, 255, tmp) != NULL)  {
                fputs(buf, local);
            }
            fprintf(local, "\n@@@@END\n");
            fclose(local);

            /* save the data in the index file */
            local = open_index_file(p);
            gp = find_news_group(p);
            fprintf(local,"%08ld %08ld %09ld %s\n", where, gp->hi_num, t,
                subject);
            fclose(local);

            p = strtok(NULL, " ,");
        }


        /* finally log it */
        strcpy(buf, my_stuff.news_dir);
        strcat(buf, "post.log");
        if ((log = fopen(buf, "at")) != NULL) {
            fprintf(log, "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01"
                         "\x01\x01\x01\x01\x01\x01\x01\x01\x01\x01\n");

            fseek(tmp, 0L, SEEK_SET);
            while (fgets(buf, 255, tmp) != NULL)  {
               fputs(buf, log);
            }

            fclose(log);
        }

        fclose(tmp);


        /* now the the X... file */
        if ((tmp = fopen(short_x_name, "wb")) != NULL) {
            fprintf(tmp, "U news %s\n", my_stuff.my_site);
            fprintf(tmp, "Z\n");
            fprintf(tmp, "F %s\n", d_name);
            fprintf(tmp, "I %s\n", d_name);
            fprintf(tmp, "C rnews\n");
            fclose(tmp);

            sprintf(buf, "uucp -C %s %s!%s ", short_d_name, my_stuff.mail_server,
                    d_name);
            system(buf);
            sprintf(buf, "uucp -C %s %s!%s ", short_x_name, my_stuff.mail_server,
                    x_name);
            system(buf);

        } else {

            WinPostMsg( hwndArt, WM_USER, (MPARAM) MAIL_FILERR, (MPARAM) 0L );
        }

        remove(short_d_name);
        remove(short_x_name);

    } else {
        WinPostMsg( hwndArt, WM_USER, (MPARAM) MAIL_FILERR, (MPARAM) 0L );
    }
}



/* quote article to 'post.tmp' file if required, then schedule
   edit of new article and wait for it.
   Then return to mainline for posting query, waiting for answer,
   the post if yes */

void post_art( USHORT doquote )

{
    FILE *sig;
    char sig_fn[80];

    FILE *article;
    aLINE *ln;
    ULONG  pcnt;
    char fn[80];
    char buf[256];
    char subject[256];
    char author[80], msg_id[80];
    char *cp;

    strcpy(fn, "post.tmp");

    if ((article = fopen(fn, "w")) != NULL)
        {
        get_his_stuff(tx, author, msg_id);

        if ( strnicmp(nsubj, "Re: ", 4) != 0 )
           sprintf(subject, "Re: %s", nsubj);
        else
           strcpy(subject, nsubj);

        /* add the quoted message if quotans = yes  */
        if ( quotans == MBID_YES )
           {
           fprintf(article, "%s writes in article %s:\n", author, msg_id);
           ln = tx->start;
           while (ln != NULL)
                 {
                 fprintf(article, "> %s", ln->data);
                 ln = ln->next;
                 }
           }

        /* append the signature if there is one */
        strcpy(sig_fn, my_stuff.home);
        strcat(sig_fn, my_stuff.signature);
        if ((sig = fopen(sig_fn, "r")) != NULL)
           {
           fputs("\n--\n", article);
           while (fgets(buf, 79, sig) != NULL)
                 fputs(buf, article);
           fclose(sig);
           }

        fclose(article);

        /* trim the editor program name */
        cp = my_stuff.editor;
        while ( *cp <= ' ' ) cp++;
        strcpy( buf, cp );
        cp = buf;
        while ( *cp > ' ' ) cp++;
        *cp = '\0';
        if ( strrchr( buf, '.' ) == NULL )
           strcat( buf, ".exe" );

        new_session( buf, fn, "Edit Post" );

        WinPostMsg( hwndArt, WM_USER, MPFROMLONG( POST_VERIFY ),
                             MPFROMP( NULL ) );
        DosWaitEventSem( work_to_do, -1L );
        DosResetEventSem( work_to_do, &pcnt );

        if ( queryans == MBID_YES )
           {
           article = fopen("post.tmp", "r");
           post_it( article, ngroup, subject, "world", msg_id );
           fclose( article );
           }

        remove(fn);

        }
    else
        WinPostMsg( hwndArt, WM_USER, (MPARAM) MAIL_FILERR, (MPARAM) 0L );
}

/* search the given news group for the specified string.
   Return an article header with a chain of ART_IDs
   linked from it, if the string is found, else return
   null */

ARTICLE *search_group( ACTIVE *gp, char *srcharg, BOOL sensi )
{
/* open the index and data files for the group */
/* search each article for the requested string,
   building a linked list of containing articles */

  ARTICLE *thrd, *head, *outp, *tmph, *lasth;
  ART_ID  *tmpd, *tmpa, *lasta, *new;
  BOOL     build_art;

  char    *fn;
  TEXT    *tmpt;
  aLINE   *lp;
  int      ln, nart, topic;

  /* work area for case insensitive compare */
  char     wrkline[255];
  char     wrkarg[80];

  if (!sensi)
     {
     strcpy( wrkarg, srcharg);
     strlwr( wrkarg );
     }

  fn   = make_news_group_name( gp->group );
  head = thrd = get_headers( gp );
  tmpa = lasta = NULL;  /* list of articles where string found */
  outp = lasth = NULL;          /* final article list */
  nart = 0;
  topic = 0;

  while ( thrd != NULL )
     {
     build_art = FALSE;
     tmpd = thrd->art_num;
     while ( tmpd != NULL )
       {
       tmpt = load_article( fn, tmpd->art_off );

       /* search the loaded lines for arg string */
       lp = tmpt->start;
       ln = strlen( srcharg );
       while ( lp != NULL )
         {
         if ( strlen( lp->data ) >= ln )
            {
            if ( ( sensi ) && ( strstr( lp->data, srcharg ) != NULL ) )
               break;
            else
               {
               strcpy( wrkline, lp->data );
               strlwr( wrkline );
               if ( strstr( wrkline, wrkarg ) != NULL )
                  break;
               }
            }
         lp = lp->next;
         }

       free_article( tmpt );
       if ( lp != NULL )
          {  /* put a copy ART_ID on our output list */
          new = xmalloc( sizeof(ART_ID) );
          new->id = tmpd->id;
          new->art_off = tmpd->art_off;
          build_art = TRUE;
          nart++;
          if ( tmpa == NULL )
             {
             tmpa = lasta = new;
             new->next_art = new->last_art = NULL;
             }
          else
             {
             lasta->next_art = new;
             new->last_art = lasta;
             new->next_art = NULL;
             lasta = new;
             }
          }

       tmpd = tmpd->next_art;
       }

     if ( build_art )
        {
        /* build an ARTICLE struct */
        tmph = xmalloc( sizeof(ARTICLE) );
        tmph->num_articles = nart;
        tmph->index = ++topic;
        tmph->art_num = tmpa;
        tmpa = lasta = NULL;  /* reset art_id list  */
        nart = 0;             /* and art_id count */

        /* copy the header (subject) */
        tmph->header = xmalloc( strlen( thrd->header ) + 1 );
        strcpy( tmph->header, thrd->header );

        if ( outp == NULL )
           {
           outp = lasth = tmph;
           tmph->next = tmph->last = NULL;
           }
        else
           {
           lasth->next = tmph;
           tmph->last = lasth;
           tmph->next = NULL;
           lasth = tmph;
           }

        }

     thrd = thrd->next;
     }

  free_header( head );

  return outp;
}
