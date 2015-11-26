/*
    SNEWS 1.0

    active.c - routines to manipulate the active and ng files


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
#include "active.h"

extern INFO my_stuff;


/*
 *  These private variables are used to do all the i/o on the active
 *  file.
 */
static FILE        *active_file;
static ACTIVE      *local_head;
static POST_GROUPS *pg;


/*--------------------------- load the active file --------------------------*/
ACTIVE *load_active_file(void)
{
    /*
     *  This routine opens the active file.  It reads the data, allocating
     *  ACTIVE elements in a linked list.  Returns a pointer to the head of
     *  the linked list.
     */
    char   fn[80], buf[81];
    char   *p;
    ACTIVE *this, *head = NULL ;
    long   posn = 0;
    int    ct = 0;
    int    ct_gp = 0;

    /* open the file */
    strcpy(fn, my_stuff.news_dir);
    strcat(fn, "active");
    if ((active_file = fopen(fn, "rb")) == NULL) {
        fprintf(errf, "cannot open %s\n", fn);
        exit(1);
    }

    /* read and store */
    while (fgets(buf, 80, active_file) != NULL) {

        /* exit on ^Z on column 1 */
        if (buf[0] == '\x1A')
            break;

        ct++;

        if (strlen(buf) > 0) {

            if (head == NULL) {
                head = this = xmalloc(sizeof (ACTIVE));
                head->last = NULL;
                head->index = ct_gp;
            } else {
                ct_gp++;
                this->next = xmalloc(sizeof (ACTIVE));
                this->next->last = this;
                this = this->next;
                this->index = ct_gp;
            }

            if ((this) == NULL) {
                fprintf(errf, "cannot allocate memory for active list\n");
                exit(1);
            }


            if ((p = strtok(buf, " ")) == NULL) {
                fprintf(errf, "active file corrupt at line %d\n", ct);
                exit(1);
            }
            strcpy(this->group, p);

            if ((p = strtok(NULL, " ")) == NULL) {
                fprintf(errf, "active file corrupt at line %d\n", ct);
                exit(1);
            }
            strcpy(this->gp_file, p);

            if ((p = strtok(NULL, " ")) == NULL) {
                fprintf(errf, "active file corrupt at line %d\n", ct);
                exit(1);
            }
            this->lo_num = atol(p);

            if ((p = strtok(NULL, " ")) == NULL) {
                fprintf(errf, "active file corrupt at line %d\n", ct);
                exit(1);
            }
            this->hi_num = atol(p);

            this->num_pos = posn;
            this->read_list = NULL;
        }

        posn = ftell(active_file);
    }

    this->next = NULL;

    local_head = head;

    /* load up the posting list */
    pg = post_group_file();

    /* the file will be opened for output if posting occurs */
    fclose(active_file);

    return(head);
}




/*------------------------- close the active file ---------------------------*/
void close_active_file(void)
{
    /*
     *  Deallocate the linked list of active newsgroups
     *  and free the 'ng' data structures
     *  the actual file should be closed.
     */

    ACTIVE *this;

    this = local_head;

    while (this != NULL) {
        local_head = this;
        this = this->next;
        free(local_head);
    }

    free_ng();

}


/*--------------------------- load the newsgroups file --------------------------*/
POST_GROUPS *post_group_file(void)
{
    /*
     *  This routine opens the 'ng' file.  It reads the data, allocating
     *  POST_GROUPS elements in a linked list.  Returns a pointer to the head
     *  of the linked list.
     */
    char        fn[80], buf[81], *p;
    POST_GROUPS *this, *head = NULL ;
    int         ct = 0;
    FILE        *ngf;

    /* open the file - if none, retun null, ie no posts allowed */
    strcpy(fn, my_stuff.news_dir);
    strcat(fn, "ng");
    if ((ngf = fopen(fn, "rb")) == NULL) {
        return(NULL);
    }

    /* read and store */
    while (fgets(buf, 80, ngf) != NULL) {

        /* exit on ^Z on column 1 */
        if (buf[0] == '\x1A')
            break;

        ct++;

        if (strlen(buf) > 0) {

            if (head == NULL) {
                head = this = xmalloc(sizeof (ACTIVE));
            } else {
                this->next = xmalloc(sizeof (ACTIVE));
                this = this->next;
            }

            if ((this) == NULL) {
                fprintf(errf, "cannot allocate memory for newsgroup list\n");
                exit(1);
            }


            if ((p = strtok(buf, " \n\r\t")) == NULL) {
                fprintf(errf, "newsgroup 'ng' file corrupt at line %d\n", ct);
                exit(1);
            }

            strcpy(this->group, p);
        }

    }

    this->next = NULL;

    fclose(ngf);

    return(head);
}




/*------------------------- close the active file ---------------------------*/
void free_ng()
{
    /*
     *  deallocate the post_groups linked list
     */

    POST_GROUPS *this;

    this = pg;

    while (this != NULL) {
        pg = this;
        this = this->next;
        free(pg);
    }

    pg = NULL;

}




/*------------------------- check group in post list -------------------------*/
int check_valid_post_group(char *ng)
{
    /*
     *  Check a string as a valid newsgroup name.  Returns TRUE if found
     */

    POST_GROUPS *this;

    this = pg;

    while (this != NULL) {
        if (strcmp(ng, this->group) == 0)
            return(TRUE);
        this = this->next;
    }

    return (FALSE);
}




/*-------------------- find a newsgroup in active list ----------------------*/
ACTIVE *find_news_group(char *group)
{
    /*
     *  This routine searches the active structure for the specified
     *  newsgroup, and returns a pointer to the entry, or to group
     *  junk if not found.  The search for junk is made via a recursive
     *  call.  Fatal if junk not found
     */

    ACTIVE *this;

    this = local_head;

    while ((this != NULL) && (stricmp(group, this->group) != 0)) {
        this = this->next;
    }

    if (this == NULL) {
        if (stricmp(group, "junk") != 0) {
            this = find_news_group("junk");
        } else {
            fprintf(errf, "active file must have newsgroup junk\n");
            exit(1);
        }
    }

    return(this);

}




/*-------------------------- update active file ---------------------------*/
void update_active_entry(ACTIVE *a)
{
    /*
     *  This routine takes a pointer to an active entry and updates
     *  its data on disk
     *  The file is opened and closed here with exclusive access
     *
     */

    int  n;
    long where;
    char buf[(ACTIVE_NUM_LEN*2) + 2];
    char fn[80];

    /* open the file */
    strcpy(fn, my_stuff.news_dir);
    strcat(fn, "active");
    if ((active_file = fopen(fn, "r+b")) == NULL) {
        /* post the error to PM and return */
        fprintf(errf, "cannot open %s\n", fn);
        exit(1);
    }


    sprintf(buf, "%08ld %08ld", a->lo_num, a->hi_num);

    n = (ACTIVE_NUM_LEN*2) + 1;
    where = a->num_pos + strlen(a->group) + 1 + strlen(a->gp_file) + 1;
    fseek(active_file, where, SEEK_SET);
    if (fwrite(buf, 1, n, active_file) != n) {
        fprintf(errf, "active file update failed for %s\n", a->group);
        exit(1);
    }

    fflush(active_file);
    fclose(active_file);
}





/*------------------- make newsgroup name and directory --------------------*/
char *make_news_group_name(char *ng)
{
    /*
     *  This routine takes the newsgroup name, replaces the '.' with
     *  '\' and creates the directory if none exists.  The returned name
     *  has a trailing '\'
     */

    static char fn[512];
    ACTIVE *tmp;


    tmp = find_news_group(ng);

    sprintf(fn, "%snewsbase\\%s", my_stuff.news_dir, tmp->gp_file);

    return(&fn[0]);
}





/*-------------------------- save the seen list -------------------------*/
void load_read_list(void)
{
    /*
     *  Load the user's list of seen articles
     */

    FILE   *tmp_file;
    ACTIVE *act;
    int    i, continue_flag;
    int    articles;
    char   *a, buf[256], *p, real_name[80];

    /* allocate the arrays and set to unread, ie FALSE */
    act = local_head;
    while (act != NULL) {

        articles = (int)(act->hi_num - act->lo_num);
        if (articles > 0) {
            a = act->read_list = xmalloc(articles * sizeof(int));
            for (i = 0; i < articles; i++) {
                *(a+i) = FALSE;
            }
        } else {
            act->read_list = NULL;
        }
        act = act->next;
    }

    /* read and process the file - if not present, just carry on */
    strcpy(buf, my_stuff.news_dir);
    strcat(buf, my_stuff.user);
    strcat(buf, ".nrc");
    if ((tmp_file = fopen(buf, "r")) != NULL) {

        continue_flag = FALSE;
        while (fgets(buf, 255, tmp_file) != NULL) {

            p = strtok(buf, " \n\r");

            if (!continue_flag) {

                strcpy(real_name, p);
                act = find_news_group(p);
                articles = (int)(act->hi_num - act->lo_num);

                /* if no articles or unknown group eat the rest */
                p = strtok(NULL, " \n\r");

            }

            /* scan the rest of the line getting numbers and setting flags */
            continue_flag = FALSE;
            while (p != NULL) {

                /* check for continuation backslash */
                if (*p != '\\') {
                    i = (int) (atol(p) - (act->lo_num + 1));
                    if ((i >= 0) && (i < articles) &&
                    ((stricmp(act->group, "junk") != 0) ||
                    (stricmp(real_name, "junk") == 0))) {
                        *((act->read_list)+i) = TRUE;
                    }
                } else {
                    continue_flag = TRUE;
                    break;
                }
                p = strtok(NULL, " \n\r");
            }
        }

        fclose(tmp_file);
    }
}





/*-------------------------- load the seen list -------------------------*/
void save_read_list(void)
{
    /*
     *  Save the user's list of read articles and deallocate storage
     */


    FILE   *tmp_file;
    ACTIVE *act;
    int    i, articles, ct;
    char   buf[256];

    /* open the file */
    strcpy(buf, my_stuff.news_dir);
    strcat(buf, my_stuff.user);
    strcat(buf, ".nrc");
    if ((tmp_file = fopen(buf, "w")) == NULL) {
        fprintf(errf, "can't open user's rc file for output\n");
        exit(1);
    }

    /* write out the lists and deallocate the arrays */
    act = local_head;
    while (act != NULL) {

        articles = (int)(act->hi_num - act->lo_num);
        if (articles > 0) {
            fprintf(tmp_file, "%s ", act->group);

            ct = 0;

            for (i = 0; i < articles; i++) {
                if(*((act->read_list)+i)) {
                    ct++;
                    fprintf(tmp_file, "%d ", i+act->lo_num+1);
                    if ((ct % 10) == 0)
                        fprintf(tmp_file, "\\ \n");
                }
            }

            fprintf(tmp_file, "\n");
            if (act->read_list != NULL) {
                free(act->read_list);
            }
        }
        act = act->next;
    }

    fclose(tmp_file);

}

/* required keywords table
   bmask is the identifying bit for a
   particular function.  This allows
   synonyms to be defined */
static const struct {
   char         *pstr;   /* required keyword string */
   unsigned int  fldoff; /* offset of target field in my_stuff */
   unsigned int  bmask;  /* ident bit */
   unsigned char attr;   /* processing attribute */
   #define  VANILLA  '\0'
   #define  PATHSLASH '\x01'
   #define  UNBATCHD  '\x02'
   #define  ALLTOKS   '\x03'
   }  reqparm [] = {
      { "mailserv", offsetof(INFO, mail_server), 0x80000000, VANILLA },
      { "nodename", offsetof(INFO,my_site), 0x40000000, VANILLA },
      { "newsdir", offsetof(INFO,incoming_dir), 0x20000000, PATHSLASH },
      { "domain", offsetof(INFO,my_domain), 0x10000000, VANILLA },
      { "tempdir", offsetof(INFO,temp_name), 0x08000000, UNBATCHD },
      { "mailbox", offsetof(INFO,user), 0x04000000, VANILLA },
      { "Signature", offsetof(INFO,signature), 0x02000000, VANILLA },
      { "name", offsetof(INFO,my_name), 0x01000000, ALLTOKS },
      { "Organization", offsetof(INFO,my_organisation), 0x00800000, ALLTOKS },
      { "Editor", offsetof(INFO,editor), 0x00400000, ALLTOKS },
      { "32BITOS2.Editor", offsetof(INFO,editor), 0x00400000, ALLTOKS },
      { "Home", offsetof(INFO,home), 0x00200000, PATHSLASH },
      { "Aliases", offsetof(INFO,my_alias), 0x00100000, VANILLA }
   };
#define NKEYWD  (sizeof(reqparm) / sizeof(reqparm[0]))

/*------------------------- load UUPC rc files ---------------------------*/
int load_stuff( char **errfld, char **errmsg )
{
    /*
     *  Trawl the UUPC files to get the stuff we need - return TRUE
     *  if completed ok
     */

    int  i;
    char buf[256];
    char *fn, *p, *v, *cp;
    FILE *tmp;

    unsigned long kwmask = 0;  /* keyword-present bits */


    /* news base directory */
    if ((fn = getenv("UUPCNEWS")) == NULL) {
        fprintf(errf, "UUPCNEWS environment variable undefined\n");
        exit(1);
    }

    /* give it a trailing \ */
    strcpy(my_stuff.news_dir, fn);
    if (my_stuff.news_dir[ strlen(my_stuff.news_dir)-1 ] != '\\')
        strcat(my_stuff.news_dir, "\\");



    /* read the system file first */
    for (i = 0; i < 2; i++) {

        /* choose the file to open */
        if (i == 0) {
            fn = getenv("UUPCSYSRC");
            if (fn == NULL) {
                fprintf(errf, "Enviroment variable UUPCSYSRC not defined\n");
            }
        } else {
            fn = getenv("UUPCUSRRC");
            if (fn == NULL) {
                fprintf(errf, "Enviroment variable UUPCUSRRC not defined\n");
            }
        }


        if ((tmp = fopen(fn, "r")) != NULL) {

            while (fgets(buf, 255, tmp)) {
                p = strtok(buf, " =\r\n");
                if ( (p != NULL) && (*p != '#') ) {
                    unsigned pix;
                    unsigned tmask;

                    v = strtok(NULL, " =\r\n");

                    for ( pix = 0; pix < NKEYWD; pix++ )
                        {
                        if (stricmp(p, reqparm[pix].pstr) == 0) {
                           /* process a keyword */
                           tmask = reqparm[pix].bmask;
                           if ( tmask & kwmask ) /* duplicate */
                              {
                              *errfld = reqparm[pix].pstr;
                              *errmsg = "Duplicate keyword %s\n";
                              return FALSE;
                              }
                           kwmask |= tmask;
                           /* copy string to target field */
                           cp = (char *)&my_stuff;
                           cp += reqparm[pix].fldoff;
                           strcpy( cp, v);

                           /* attribute processing */
                           switch ( reqparm[pix].attr )
                             {
                             case VANILLA:  break;  /* we were done */
                             case PATHSLASH: { /* add backslash to end if needed */
                               if ( cp[ strlen(cp) - 1] != '\\' )
                                  strcat( cp, "\\" );
                               break;
                               }
                             case UNBATCHD: { /* the temp unbatch dir */
                               strcat( cp, "\\$unbatch" );
                               break;
                               }
                             case ALLTOKS:  { /* get all value tokens */
                               v = strtok(NULL, " =\r\n");
                               while (v != NULL) {
                                   strcat( cp, " ");
                                   strcat( cp, v);
                                   v = strtok(NULL, " =\r\n");
                                   }
                               }

                             }  /* end of attr switch */
                           break;
                           }  /* end of keyword compare if */
                        } /* end of for pix */
                    /* unknown keyword will be ignored */

                }
            }
            fclose (tmp);

        } else {
            fprintf(errf, "Cannot open %s\n", fn);
        }
    }

    if ( kwmask == 0xFFF00000 )
       return TRUE;
    for ( i = 0; i < NKEYWD; i++ )
        {
        if ( ! ( kwmask & reqparm[i].bmask ) )
           { /* non-specified bit */
           *errfld = reqparm[i].pstr;
           *errmsg = "Keyword %s not specified\n";
           return FALSE;
           }
        }
    /* should HALT here */
    *errfld = "";
    *errmsg = "Unknown RC error\n";
    return FALSE;
}



/*--------------------------- unpack the batch ------------------------*/
FILE *open_out_file(char *ng)
{
    /*
     *  This routine creates a filename from the newsgroup name.
     *  The active file counter are updated.
     */

    ACTIVE *gp;
    char   *fn;
    FILE   *tmp;

    gp = find_news_group(ng);
    fn = make_news_group_name(gp->group);

    (gp->hi_num)++;
    update_active_entry(gp);

    if ((tmp = fopen(fn, "r+b")) == NULL) {
        fprintf(errf,"active: cannot open text file %s\n", fn);
        exit(1);
    }
    fseek(tmp, 0, SEEK_END);

    return(tmp);


}


/*--------------------------- unpack the batch ------------------------*/
FILE *open_index_file(char *ng)
{
    /*
     *  This routine open the index file for the newsgroup
     */

    ACTIVE *gp;
    char   fnx[256], *fn;
    FILE   *tmp;

    /* printf("news: ng found = %s\n", ng); */
    gp = find_news_group(ng);
    fn = make_news_group_name(gp->group);
    sprintf(fnx, "%s.IDX", fn);

    if((tmp = fopen(fnx, "r+b")) == NULL) {
        fprintf(errf, "active: cannot open index file %s\n", fn);
        exit(1);
    }
    fseek(tmp, 0, SEEK_END);

    return(tmp);

}



/*------------------------- post sequence number ----------------------------*/
int post_sequence(void)
{
    /*
     *  Get the sequnce number from the seq file if it exists - if
     *  not create it
     */

    FILE *seq_file;
    char fn[256];
    int  seq;

    strcpy(fn, my_stuff.news_dir);
    strcat(fn, "nseq");

    if ((seq_file = fopen(fn, "r+")) != NULL) {
        fscanf(seq_file, "%d", &seq);
        seq++;
        rewind(seq_file);
    } else {
        seq = 0;
        seq_file = fopen(fn, "w");
    }

    fprintf(seq_file, "%d", seq);

    fclose(seq_file);
    return(seq);
}


/*-------------------------------- safeish malloc --------------------------*/
void *xmalloc(size_t size)
{
    void *p;
    if ((p = malloc(size)) == NULL) {
        exit(1);
    }

    return(p);
}


ALII  *load_alias_file( void )
{
  FILE *  afile;
  ALII   *wp, *pp;
  char    buf[256], cpy[256];
  char   *cp;
  int     nicklen;

  szalias = 0;
  pp = NULL;
  if ((afile = fopen(my_stuff.my_alias, "r")) != NULL)
       {
       while (fgets(buf, 255, afile))
             {
             if ( ( strlen( buf ) > 3 ) && ( buf[0] != '#' ) )
                {
                wp = (ALII *)xmalloc( sizeof(ALII) );

                strcpy( cpy, buf );
                strcpy( wp->nick, strtok( cpy, " " ) );
                nicklen = strlen( wp->nick );
                if ( nicklen > szalias )  szalias = nicklen;

                strcpy( cpy, buf );
                cp = strchr( cpy, '"' );
                if ( cp )
                   {
                   strcpy( wp->name, strtok( cp, "\"" ) );
                   strcpy( cpy, buf );
                   cp = strchr( cpy, '<' );
                   if ( cp )
                      strcpy( wp->addr, strtok( cp, "<>" ) );
                   else
                      wp->addr[0] = '\0';
                   }
                else
                   wp->name[0] = '\0';

                wp->nextp = pp;  pp = wp;
                }
             }

       }

  fclose( afile );
  return pp;

}
