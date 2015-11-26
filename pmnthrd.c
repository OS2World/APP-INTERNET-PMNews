
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
#include  <os2.h>    /* for typedefs used below */
#include "defs.h"
#include "pmnews.h"


static int strip_off_part(char *str)
{
  char *ptr = str + strlen(str);

  /* strip off (case-insensitive) things like:
     - "Part01/10"
     - "Patch02a/04"
     - "Part 01/10"
     - "Part 01 of 10"
     - "(1 of 10)"
     - "1 of 10"
   */

  while ( ptr > str && ptr[-1] == ' ' )
    ptr--;

  if ( ptr > str && ptr[-1] == ')' )
    ptr--;

  while ( ptr > str && isdigit(ptr[-1]) )
    ptr--;

  if ( !isdigit(*ptr) )
    return 0;

  if ( ptr > str && ptr[-1] == '/' )
    ptr--;
  else if ( ptr > str + 3 && strnicmp(ptr - 4, " of ", 4) == 0 )
    ptr -= 4;
  else
    return 0;

  if ( ptr > str && 'a' <= ptr[-1] && ptr[-1] <= 'z' )
    ptr--;

  while ( ptr > str && isdigit(ptr[-1]) )
    ptr--;

  if ( !isdigit(*ptr) )
    return 0;

  if ( ptr > str && ptr[-1] == '(' )
    ptr--;

  while ( ptr > str && ptr[-1] == ' ' )
    ptr--;

  if ( ptr > str + 3 && strnicmp(ptr - 4, "Part", 4) == 0 )
    ptr -= 4;

  while ( ptr > str && ptr[-1] == ' ' )
    ptr--;

  if ( ptr > str && ptr[-1] == ',' )
    ptr--;
  else if ( ptr > str && ptr[-1] == ':' )
    ptr--;

  *ptr = 0;
  return 1;
}

static char *skip_vi(char *str)
{
  char *ptr = str;

  /* skip things like "v02i0027: " */

  if ( *ptr++ != 'v' )
    return str;

  if ( !isdigit(*ptr) )
    return str;

  while ( isdigit(*ptr) )
    ptr++;

  if ( *ptr++ != 'i' )
    return str;

  if ( !isdigit(*ptr) )
    return str;

  while ( isdigit(*ptr) )
    ptr++;

  if ( *ptr++ != ':' )
    return str;

  if ( *ptr++ != ' ' )
    return str;

  return ptr;
}

static int smartcmp(char *str1, char *str2)
{
  char s1[256], s2[256];

  strcpy(s1, str1);
  strcpy(s2, str2);

  if ( strip_off_part(s1) && strip_off_part(s2) )
  {
    str1 = skip_vi(s1);
    str2 = skip_vi(s2);
  }

  return stricmp(str1, str2);
}



/* threading algorithm returns a pointer which it wants
   passed to it with each  new subject line from the index */
void * thread_init( void )
{
  void * tmp_anch;

  tmp_anch = xmalloc( sizeof(ARTICLE* ) );
  *(ARTICLE**)tmp_anch = NULL;
  return tmp_anch;
}

/* called at completion of thread pass for the newsgroup */
void thread_end( void * anch )
{
  free( anch );
  return;
}


/* called for each new subject line.
   returns TRUE if it found a subject (ARTICLE) to thread-to,
   FALSE if it had to allocate an ARTICLE.
   'targ' is stored in all cases */

BOOL thread_compare( char * subjline, void * anch, ARTICLE **targ )
{

    ARTICLE  *tmp;
    ARTICLE **ptr;
    int     cmp;


    ptr = NULL;

    for ( tmp = *(ARTICLE **)anch; tmp != NULL; ) {

        cmp = smartcmp(subjline, tmp->header);

        if (cmp > 0) {
           if (tmp->left)
              tmp = tmp->left;
           else {
              ptr = &(tmp->left);
              break;
              }
           }
        else if (cmp < 0) {
                if (tmp->right)
                   tmp = tmp->right;
                else {
                   ptr = &(tmp->right);
                   break;
                   }
                }
        else {
              /* found this subject */
              *targ = tmp;
              return TRUE;
             }
    }


    /* not found - allocate new ARTICLE (thread) */

    tmp = xmalloc(sizeof (ARTICLE));
    *targ = tmp;

    if ( *(ARTICLE **)anch == NULL )
       *(ARTICLE **)anch = tmp;
    if ( ptr != NULL )
       *ptr = tmp;

    return FALSE;


}

