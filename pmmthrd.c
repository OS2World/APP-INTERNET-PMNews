
/*
    PMNEWS 1.0

    Threading functions for the PMNEWS news reader

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
#include  <string.h>
#include  <stdio.h>
#include  <stdlib.h>
#include  <os2.h>    /* for typedefs used below */
#include "defs.h"
#include "pmnews.h"

/*   Since this interface requires a one-pass algorithm
 *   we need to tokenize each subject and analyze it's
 *   meaingful tokens with respect to all previous tokens.
 *
 *   Init time:
 *        Allocate space for subject_token_space.
 *        Initialize anchor for subject line (ARTICLE) list
 *
 *   For each token:
 *        Search against 'trivial_token_space'.
 *        If found, iterate.
 *        Count the token.
 *        Hash against 'subject_token_space'.
 *        If not found,
 *           allocate and establish word_node.
 *           enter this token in the new reference list.
 *           iterate.
 *        enter this token in the new reference list.
 *        For each ARTICLE element on this word_node:
 *           Search for match in temp_hit_list (a tree).
 *           If found increment count.
 *           else insert element in tree and in one-way list.
 *        iterate.
 *        end (while tokens)
 *
 *   Search the temp_hit_list for the entry with the
 *     highest count via the one-way list.
 *
 *   If that count > 75% of the total token count
 *      free temp_hit_list and new_reference_list
 *      return TRUE with the associated ARTICLE ptr
 *   else
 *      allocate an ARTICLE.
 *      save the token count in the 'left' pointer
 *      queue the new_reference entries to their
 *        respective word_nodes (using above ARTICLE).
 *      free temp_hit_list and new_reference_list
 *      return TRUE
*/


#define MAXTOKL 32

typedef struct hitent {
  ARTICLE       *subjp;         /* pointer to base article */
  UINT           nhits;         /* number of times referenced */
  struct hitent *hpleft,        /* left branch of hit tree */
                *hpright,       /* right branch of hit tree */
                *hpnext;        /* one-way list */
  } HITENT;


typedef struct artref {
  struct artref * nextar;   /* next in this chain */
  struct artref * nextal;   /* next in allocation chain */
  ARTICLE       * thread;   /* pointer to thread containing this token */
  } ARTREF;

typedef struct tokel {
  char           toktxt[MAXTOKL+1];   /* uppercased token */
  BOOL           trivial;       /* token to be ignored */
  struct tokel  *overflow;      /* collision ptr */
  ARTREF        *subhead;       /* head of subject lines with this token */
  } TOKEL;

typedef struct newref {
  TOKEL         *reftok;        /* token referenced */
  struct newref *nrnext;        /* next new reference */
  } NEWREF;

typedef struct {
  TOKEL *  next_ovflo;          /* next available overflow */
  TOKEL *  subject_token_space; /* primary token area */
  TOKEL *  limit_addr;          /* limit of acquired area */
  ARTREF * headref;             /* allocation chain head for ARTREF */
  ARTICLE * mainseq;            /* main chain of articles */
  FILE  *   tlog;               /* debug file */
  }  THREAD_ANCHOR;

#define   HASH_WIDTH  1023

static  const char *trivia[] = {
  "THE",  "IN", "A", "OF",
  "WITH", "TO", "BE", "IS", "FOR", "ON",
  "RE:", "AN", "WAS:", "AND", "HOW"
  };
static const UINT numtriv = (sizeof(trivia)/sizeof(trivia[0]));
/* called at completion of thread pass for the newsgroup */
void thread_end( THREAD_ANCHOR * anch )
{
  ARTREF * warp, * warpn;

#ifdef DEBUG
  fclose( anch->tlog );
#endif

  warp = anch->headref;
  while ( warp != NULL )
     {
     warpn = warp;
     warp = warp->nextal;
     free( warpn );
     }
  free( anch->subject_token_space );
  free( anch );
  return;
}

/* valid token chars, binary 0 is invalid, \b is ignored
   else it's the upper-case of char */
static const char  sigchar[] = {
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\b', '\b', '\b',  '$' , '\b',  '&', '\b',
  '\b', '\b', '\b', '\b', '\b' , '\b', '\0', '\b',  /* period is whitespace */
   '0',  '1',  '2',  '3',  '4' ,  '5',  '6',  '7',
   '8',  '9', '\0', '\b', '\b' , '\b', '\b', '\b',

  '\b',  'A',  'B',  'C',  'D' ,  'E',  'F',  'G',
   'H',  'I',  'J',  'K',  'L' ,  'M',  'N',  'O',
   'P',  'Q',  'R',  'S',  'T' ,  'U',  'V',  'W',
   'X',  'Y',  'Z', '\b', '\b' , '\b', '\b', '\b',
  '\b',  'A',  'B',  'C',  'D' ,  'E',  'F',  'G',
   'H',  'I',  'J',  'K',  'L' ,  'M',  'N',  'O',
   'P',  'Q',  'R',  'S',  'T' ,  'U',  'V',  'W',
   'X',  'Y',  'Z', '\b', '\b' , '\b', '\b', '\0',

  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',

  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
  '\0', '\0', '\0', '\0', '\0' , '\0', '\0', '\0',
};

/* obtain the next subject token
   this involves upper-casing and removing any
   punctuation
   pos is where the current position in the output 'line'
     is stored.
   inp = current input position

   return hash value of token or -1 if end-of-line */

static int  nextokn( char ** inp, char **pos )
{
   char *cin, *cout, *hn;
   char  ch;
   UINT  hv = 0, hw = 0, cc = 0;

   cin = *inp; cout = *pos;
   hn  = (char *)&hw;

   /* locate 1st significant char */
   while ( ( *cin != '\0' ) && ( sigchar[*cin] <= '\b' ) )
           cin++;
   if ( *cin == '\0' )
      return -1;

   /* process chars until white-space, uppercasing */
   while ( ( *cin != '\0' ) && ( ( ch = sigchar[*cin] ) != '\0' ) )
         {
         if ( ch > '\b' )
            {
            cc++;
            *hn   = ch;  hn++;
            if  ( cc % sizeof(hv) == 0 )
                {  /* xor words together */
                hv ^= hw;  hn = (char *)&hw; hw = 0;
                }

            *cout = ch; cout++;
            }
         if ( cc == MAXTOKL )
            break;
         cin++;
         }
   *cout = '\0';
   *inp = cin;
   hv  ^= hw;
   return hv % HASH_WIDTH;
}


/* free the hitlist and the new reference list */
static void free_lists( HITENT * hp, NEWREF * nr )
{
   HITENT  *wh, *prvh;
   NEWREF  *wr, *prvr;

   wh = hp;
   while ( wh != NULL )
     {
     prvh = wh;
     wh = wh->hpnext;
     free( prvh );
     }

   wr = nr;
   while ( wr != NULL )
     {
     prvr = wr;
     wr = wr->nrnext;
     free( prvr );
     }
}

/* threading algorithm returns a pointer which it wants
   passed to it with each  new subject line from the index */
void * thread_init( void )
{
  THREAD_ANCHOR  * tmp_anch;
  int   hv;
  UINT  ix;
  char  outt[80];
  char  *op, *ip;
  TOKEL *tsp, *tsw;
  BOOL  found;

  tmp_anch = xmalloc( sizeof(THREAD_ANCHOR) );

  /* allocate subject token space */
  tmp_anch->subject_token_space = xmalloc( sizeof(TOKEL) * (HASH_WIDTH+1)*2 );
  tmp_anch->limit_addr = &(tmp_anch->subject_token_space[(HASH_WIDTH+1)*2]);
  tmp_anch->next_ovflo = &(tmp_anch->subject_token_space[HASH_WIDTH]);
  memset( tmp_anch->subject_token_space, 0, sizeof(TOKEL) * (HASH_WIDTH+1)*2 );
  tmp_anch->mainseq = NULL;
  tmp_anch->headref = NULL;


  /* add the trivial tokens to the token space, marking them */
  tsp = tmp_anch->subject_token_space;
  op = &outt[0];
  for ( ix = 0;  ix < numtriv; ix++ )
      {
      ip = (char *) trivia[ix];
      hv = nextokn( &ip, &op );

      tsw = tsp + hv;
      found = FALSE;
      while  ( tsw->toktxt[0] != '\0' )
        {
        if ( strcmp( tsw->toktxt, outt ) == 0 )
           {
           found = TRUE;
           break;
           }
        if ( tsw->overflow == NULL )
           break;
        tsw = tsw->overflow;
        }


      if ( !found )   /* ignore duplicates */
         {
         /* if this is a collision, allocate an overflow node */
         if ( (tsp + hv)->toktxt[0] != '\0' )
            {
            tsw = tmp_anch->next_ovflo;
            tmp_anch->next_ovflo++;
            /* push the new overflow onto the primary */
            tsw->overflow = (tsp + hv)->overflow;
            (tsp + hv)->overflow = tsw;
            }

         /* build a word node */
         strcpy( tsw->toktxt, outt );
         /* overflow and subhead should be NULL already */
         tsw->trivial = TRUE;
         }
      } /* for each trivial string */

#ifdef DEBUG
  tmp_anch->tlog = fopen("thread.log", "w" );
#endif

  return tmp_anch;
}


/* called for each new subject line.
   returns TRUE if it found a subject (ARTICLE) to thread-to,
   FALSE if it had to allocate an ARTICLE.
   'targ' is stored in all cases */

BOOL thread_compare( char * subjline, THREAD_ANCHOR * anch, ARTICLE **targ )
{
    char  *outp, *inp;
    UINT   tcnt = 0;  /* token counter */
    UINT   hcnt = 0;  /* hit counter */
    int    hashv, hvalu;
    TOKEL *tsp, *tsw;
    BOOL   found;
    ARTICLE *newa, *wap;
    ARTREF  *wart;
    HITENT  *hitanc,  /* tree anchor of hits for current subject */
            *hitlist, /* one-way list anchor */
            *hitw,    /* work ptr for hit searches */
            **hitp;   /* ptr to previous ptr in search */
    NEWREF  *anew,   /* anchor of new references for this entry */
            *refw;   /* work ptr for new refs */
    char   worka[257];  /* output work area for tokenizer */

    anew = NULL;
    hitanc = NULL;
    hitlist = NULL;
    inp = subjline;      /* start of input string  */
    outp = &(worka[0]);  /* output pointer start   */
    memset ( outp, 0, sizeof(worka) );
    tsp = anch->subject_token_space;

    /* obtain the next token */
    while ( ( hashv = nextokn( &inp, &outp ) ) != -1 )
      {
      /* disregard trivial tokens */

      /* look up the token in the subject token space */
      tsw = tsp + hashv;
      found = FALSE;

      while  ( tsw->toktxt[0] != '\0' )
        {
        if ( strcmp( tsw->toktxt, outp ) == 0 )
           {
           found = TRUE;
           break;
           }
        if ( tsw->overflow == NULL )
           break;
        tsw = tsw->overflow;
        }

      if ( found )
         {
         if ( tsw->trivial )
            continue;
         }
      else
         {  /* not found */
         /* if this is a collision, allocate an overflow node */
         if ( (tsp + hashv)->toktxt[0] != '\0' )
            {
            tsw = anch->next_ovflo;
            if ( tsw >= anch->limit_addr )
               { /* allocate ARTICLE struct and exit */
                newa = xmalloc( sizeof(ARTICLE) );
                *targ = newa;
                return FALSE;
               } /* cop out for too many tokens */
            anch->next_ovflo++;
            /* push the new overflow onto the primary */
            tsw->overflow = (tsp + hashv)->overflow;
            (tsp + hashv)->overflow = tsw;
            }

         /* build a word node */
         strcpy( tsw->toktxt, outp );
         /* overflow and subhead should be NULL already */
         tsw->trivial = FALSE;
         /* push this on the new reference list */
         refw = xmalloc( sizeof(NEWREF) );
         refw->reftok = tsw;
         refw->nrnext = anew;
         anew = refw;
         tcnt++;   /* count this token */
         continue;
         }

      /* push this on the new reference list -
         this is done whether the token is known or not
         in case this is a NEW thread.
         If the token is already on list, skip it */
      refw = anew;
      while ( refw != NULL )
        {
        if ( refw->reftok == tsw )
           break;
        refw = refw->nrnext;
        }
      if ( ( refw != NULL ) && ( refw->reftok == tsw ) )
         continue;
      refw = xmalloc( sizeof(NEWREF) );
      refw->reftok = tsw;
      refw->nrnext = anew;
      anew = refw;
      /* count this token */
      tcnt++;

      /* we found the token, scan thru the attached ARTICLEs and
         merge pointers to them into the hit tree */

      wart = tsw->subhead;
      while ( wart != NULL )
         {
         hitw = hitanc; hitp = NULL;
         while ( ( hitw != NULL ) && ( hitw->subjp != wart->thread ) )
           {
           if ( wart->thread > hitw->subjp )
              {
              hitp = &hitw->hpleft;
              hitw = hitw->hpleft;
              }
           else
              {
              hitp = &hitw->hpright;
              hitw = hitw->hpright;
              }
           }
         if ( hitw == NULL )
            { /* add new tree entry */
            hitw = xmalloc( sizeof(HITENT) );
            /* link into one-way list */
            hitw->hpnext = hitlist;  hitlist = hitw;
            /* if no previous node, set anchor, else store
               this element's ptr in correct branch of previous */
            if ( hitp == NULL ) hitanc = hitw;
            else *hitp = hitw;
            hitw->subjp = wart->thread;
            hitw->hpleft = hitw->hpright = NULL;
            hitw->nhits = 0;
            }

         hitw->nhits++;  /* increment hits */

         wart = wart->nextar;
         }

      } /* while tokens */


    /* search the hit tree as a list for the highest count
       within a similarly tokenized article */
    hitw = hitlist;  wap = NULL;  hvalu = 0;
    while ( hitw != NULL )
      {
      int hweight;
      hweight = hitw->nhits - (abs((int)hitw->subjp->left - tcnt));
      if ( hweight > hvalu )
         {
         hvalu = hweight;
         hcnt = hitw->nhits;
         wap = hitw->subjp;  /* save associated ARTICLE */
         }
      hitw = hitw->hpnext;
      }


    found = FALSE;
    if ( wap != NULL )
       {
       int    mcnt;

       /* retrieve the saved token count of the previous
          subject line with highest hit count (hcnt) */
       mcnt = (int)wap->left;

       /* if the current token count is 3 or less:
             all tokens must match (order irrelevant).
          else
             the saved token count must be within  1 of the number
             of hits in the current subject. */

       if ( tcnt <= 3 )
          {
          if ( ( hcnt == mcnt ) && ( tcnt == mcnt ) )
             found = TRUE;
          }
       else
       if ( abs(mcnt - hcnt) < 2 )
           found = TRUE;
       else
       if ( ( hcnt > 5 ) && ( abs(mcnt - hcnt) < 3 ) )
           found = TRUE;
       }

    /* see if we have enough tokens to be a match */
    if ( found )
       { /* subject to be threaded to a previous ARTICLE */
#ifdef DEBUG
       fprintf( anch->tlog,
        "match: %s\n %s\n hcnt: %d tcnt %d\n", subjline, wap->header, hcnt, tcnt );
#endif
       *targ = wap;  /* previous article */
       free_lists( hitlist, anew );
       return TRUE;
       }
    else
       {
       /* this is a 'new' thread */
#ifdef DEBUG
       fprintf(anch->tlog, "new: %s hcnt: %d tcnt %d\n", subjline, hcnt, tcnt );
#endif
       newa = xmalloc( sizeof(ARTICLE) );
       /* main program will fill in ARTICLE, but not
          modify 'left' or 'right' */
       newa->left = (ARTICLE *)tcnt;  /* save token count */
       *targ = newa;
       if ( anch->mainseq == NULL )
          anch->mainseq = newa;
       /* queue the new ARTICLE to its word nodes */
       refw = anew;
       while ( refw != NULL )
         {
         wart = xmalloc( sizeof(ARTREF) );
         wart->thread = newa;
         wart->nextal = anch->headref;
         anch->headref = wart;
         wart->nextar = refw->reftok->subhead;
         refw->reftok->subhead = wart;
         refw = refw->nrnext;
         }

       free_lists( hitlist, anew );
       return FALSE;
       }

}

