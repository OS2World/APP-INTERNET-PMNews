/*
    PMNEWS 1.0

    pmnews - a simple threaded news reader for PM


    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License, version 1, as
    published by the Free Software Foundation.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    See the file COPYING, which contains a copy of the GNU General
    Public License.


    Compiled with IBM CSET/2.
    Organized(?) in the Pascal fashion (main on bottom).

 */

#define MAIN_FILE

#define INCL_DOSSEMAPHORES
#define INCL_WIN
#define INCL_WINHELP
#define INCL_WINERRORS
#define INCL_GPI

#include <os2.h>                        /* PM header file               */

#include "defs.h"
#include "pmnews.h"


#define DO_UTIL( thing )  \
        if ( util_func == TASK_FREE ) { \
           util_func = thing; DosPostEventSem( work_to_do ); }


/*------------------- file global data  -------------------------------*/

int  util_thread;    /* tid of utility task */

enum { TASK_FREE, MAIL, REPART, POST_EDIT, PRINT, SEARCH,
         SENSEARCH, SAVED, EOJ
       } util_func = TASK_FREE;

HINI init;                              /* .ini handle */
HAB  hab;                               /* PM anchor block handle       */
HWND hwndFrame=0L;                      /* Frame window handle          */
HWND hwndLbox=0L;                       /* group window handle          */
HWND hwndNextA=0L;                      /* next article button */
HWND hwndNextU=0L;                      /* next unread button */
HWND hwndRetHdr=0L;                     /* return to hdrs button */
HWND hwndNextAr=0L;                     /* next article */
HWND hwndNextUr=0L;                     /* next unread  */

/******************************************/
/* Required IPF Structures For Help Text. */
/******************************************/
HELPINIT hmiHelpData;
HWND hwndHelpInstance;

ACTIVE  *gp;                            /* current group */

ALII *nicks;                            /* mailing aliases */
CHAR *szString;                         /* procedure.                   */

PSZ  pszErrMsg;
#define MSGBOXID    1001

static struct {
   USHORT artwidth;     /* dimensions of article window */
   USHORT artheight;
   USHORT artx, arty;  /* article window location */
   USHORT grpx, grpy;  /* group window location */
   USHORT thrx, thry;  /* thread window location */
   } artloc = { ARTWIDTH, ARTHEIGHT, ARTX, ARTY };

static char  savepath[80];   /* where to save article */

/*------------------- external routines -------------------------------*/
char *make_news_group_name(char *ng);


/*------------------- log PM error code to pmnews.err -----------------*/
VOID logWinErr( VOID )
  {
  USHORT sev, id;
  ERRORID ecode;


  ecode = WinGetLastError( hab );
  id = ERRORIDERROR( ecode );
  sev  = ERRORIDSEV( ecode );
  fprintf( errf, "PM Window error %x severity %x  \n", id, sev );
  }


/*-------------------- issue message box ------------------------------*/
USHORT message( PSZ text, PSZ label, ULONG mstyle )
{
   USHORT tmpret;

   tmpret =
   WinMessageBox(HWND_DESKTOP,         /* Parent window is desk top */
                 hwndFrame,            /* Owner window is our frame */
                 text,                 /* PMWIN Error message       */
                 label,                /* Title bar message         */
                 MSGBOXID,             /* Message identifier        */
                 MB_MOVEABLE | MB_CUACRITICAL | mstyle ); /* Flags */
   if (tmpret == MBID_ERROR)
      logWinErr();
   return tmpret;
}


/*----------------------------Abort Snews -----------------------------*/
VOID AbortSN( HWND hwndFrame, HWND hwndClient, UINT errn  )
{
PERRINFO  pErrInfoBlk;
PSZ       pszOffSet;

      DosBeep(100,10);

      fprintf( errf, "PMNews error %d \n", errn );
      fclose (errf );

      if ((pErrInfoBlk = WinGetErrorInfo(hab)) != (PERRINFO)NULL)
         {
         pszOffSet = ((PSZ)pErrInfoBlk) + pErrInfoBlk->offaoffszMsg;
         pszErrMsg = ((PSZ)pErrInfoBlk) + *((PSHORT)pszOffSet);
         if((INT)hwndFrame && (INT)hwndClient)
            message( (PSZ)pszErrMsg, "PMNews Error", MB_CANCEL );
         WinFreeErrorInfo(pErrInfoBlk);
         }
      WinPostMsg(hwndClient, WM_QUIT, (MPARAM)0, (MPARAM)0);

} /* End of AbortSN */


/* log the last PM error */


/************* open the Article Window  ****************/
HWND    open_art_win( PHWND pClient, PSZ title )
{

static ULONG   ArtFlags;                /* Window creation control flags*/
static char    title_bar[68];

HWND whwnd;
int  rem, ix;


  ArtFlags = FCF_TITLEBAR      | FCF_SYSMENU  |
             FCF_SIZEBORDER    | FCF_MINMAX   |
             FCF_TASKLIST |
             FCF_VERTSCROLL    | FCF_MENU;

  strncpy( title_bar, title, 40 );
  title_bar[40] = '\0';

  if ( ( tx != NULL ) && ( tx->rname != NULL ) )
     {
     rem = sizeof(title_bar) - strlen(title) - strlen(tx->rname);
     if ( rem > 0 )
        {
        for ( ix = strlen(title_bar); ix < rem-2; ix++ ) title_bar[ix] = ' ';
        title_bar[ix] = '\0';
        strcat( title_bar, tx->rname );
        }
     }

  whwnd = WinCreateStdWindow( HWND_DESKTOP, WS_VISIBLE,
                              &ArtFlags, PMNEWS_CLASS,
                              title_bar, 0L,
                              (HMODULE)0L, ID_MENUBAR, pClient );

  if ( whwnd == (HWND)0L )
     {
     logWinErr();
     AbortSN(hwndFrame, *pClient, 9 ); /* Terminate the application */
     }


  if( !WinSetWindowPos( whwnd, HWND_TOP,
           artloc.artx, artloc.arty, artloc.artwidth, artloc.artheight,
           SWP_ZORDER | SWP_SIZE | SWP_MOVE ) )
         logWinErr();

  WinPostMsg( whwnd, WM_USER, (MPARAM) ART_FOCUS, (MPARAM)0 );
  return whwnd;

}


/* the header list dialog window */
static  HWND hdrwin;
/* the current list of articles */
static  ARTICLE *artlist;



static  USHORT  itmx;                 /* selected item index */
static  int     unread;               /* unread in selected thread */
static  ART_ID *curr_article = NULL;  /* current processed article */


/* copy current article to an output file
   the file is already open; will be closed here */
void copy_article_out( FILE * tmp, TEXT * txt )
{
    aLINE   *ln;



    ln = txt->top;
    while (ln != NULL) {
          fputs(ln->data, tmp);
                ln = ln->next;
          }
    fputs("\n\n", tmp);

    fclose(tmp);
}

/******************************************************************************/
/*                                                                            */
/*  close_art_win  deletes current article window and structure               */
/*                                                                            */
/******************************************************************************/
VOID close_art_win( HWND hwndHL )
{
int    idx;
CROSS_POSTS *h, *h0;
ACTIVE *gx;
char   author[128], msg_id[128], wstr[128];


  /* if utility thread is processing, stop here */
  if ( ( util_func != TASK_FREE ) && ( util_func != SEARCH ) )
     message( "Utility thread in process", "Sorry", MB_OK );


  /* delete current article window */
  WinDestroyWindow( hwndArt );
  hwndArt = (HWND)0L;


  /* update the header count of unread in thread */
  if ( unread > 0 ) --unread;
  if ( unread )
     {
     sprintf( wstr, "%03d %03d     %s",
              unread,
              artp->num_articles, artp->header );
     }
  else
     sprintf( wstr, "--- %03d     %s",
              artp->num_articles, artp->header );

  WinSendDlgItemMsg( hwndHL, IDD_HRLIST, LM_SETITEMTEXT,
                           MPFROMSHORT( itmx ),
                           MPFROMP( wstr ) );

  /* mark this article as read */
  idx = (int) ((curr_article->id) - gp->lo_num - 1);
  *((gp->read_list)+idx) = TRUE;

  /* mark the crossposts */
  get_his_stuff(tx, author, msg_id);

  if ((h0 = look_up_history(msg_id, gp->group)) != NULL) {

       h = h0;
       while (h != NULL) {
             gx = find_news_group(h->group);
             if ( (gx) && (gx->read_list) ) {
                idx = (int) ((h->art_num) - gx->lo_num - 1);
                *((gx->read_list)+idx) = TRUE;
             }
            h = h->next;
            }

        free_cross_post_list(h0);
        }



  free_article(tx);
  tx = NULL;
}

ARTICLE * select_next_unread( ARTICLE * cart, USHORT numi, HWND hwnd )
{
  USHORT nnx;
  ARTICLE *tmpart;

  if ( cart == NULL ) return NULL;
  nnx = numi;  tmpart = cart->next;
  while ( tmpart != NULL )
     {
     nnx++;
     if ( count_unread_in_thread( gp, tmpart ) > 0 )
        {
        WinSendDlgItemMsg( hwnd, IDD_HRLIST, LM_SELECTITEM,
                        MPFROMSHORT( nnx ),
                        MPFROMSHORT( TRUE ) );
        return tmpart;
        }
     tmpart  = tmpart->next;
     }
  return NULL;
}

/******************************************************************************/
/*                                                                            */
/*  HListProc  receives msgs from IDD_HLIST  (header window )                 */
/*                                                                            */
/******************************************************************************/
MRESULT EXPENTRY HListProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
static char    *fn;                     /* name of that file */
static int     a_ct;                    /* article count in thread */
static int     firstu;                  /* first unread thread in group */

int    idx;
SHORT  uact;
BOOL   get_first_unread;
char   wstr[128];


/*---------------------------              ----------------------------*/

  switch( msg )
    {
    case WM_ACTIVATE:
         WinSendMsg( hwnd, HM_SET_ACTIVE_WINDOW, (MPARAM) hwnd, (MPARAM)hwnd);
         break;

    case WM_INITDLG: {
         ARTICLE *thrd;
         int  urct, hct;

         hdrwin = hwnd;  /* save this window's handle outside */

         /* get the headers in this group, unless already built */
         if ( artlist == NULL )
            {
            gp = (ACTIVE *)mp2;
            artlist = get_headers( gp );
            }

         /* set group name */
         WinSetDlgItemText( hwnd, IDD_GNAME, gp->group );

         /* insert them in list box */
         thrd = artlist;
         firstu = -1;  hct = 0;
         while ( thrd != NULL )
           {
           if ( mp2 == (MPARAM)0L )
              urct = 0;
           else
              urct = count_unread_in_thread( gp, thrd );
           if ( urct )
              {
              sprintf( wstr, "%03d %03d     %s", urct,
                       thrd->num_articles, thrd->header );
              if ( firstu == -1 ) firstu = hct;
              }
           else
              sprintf( wstr, "--- %03d     %s",
                       thrd->num_articles, thrd->header );
           WinSendDlgItemMsg( hwnd, IDD_HRLIST, LM_INSERTITEM,
                           MPFROM2SHORT( LIT_END, 0 ),
                           MPFROMP( wstr ) );

           hct++;
           thrd = thrd->next;
           }


         /* now put the highlite on the first unread thread */
         if ( firstu != -1 )
            {
            WinSendDlgItemMsg( hwnd, IDD_HRLIST, LM_SELECTITEM,
                           MPFROMSHORT( firstu ),
                           MPFROMSHORT( TRUE ) );
            }

         WinSetFocus( HWND_DESKTOP, hwnd );
         return (MRESULT)TRUE;
         }

    case WM_USER:  {

        /* if this is to obtain the focus, do so
           and break */
        if ( (ULONG)mp1 == GET_FOCUS )
           {
           WinSetFocus( HWND_DESKTOP, hwnd );
           break;
           }

        /* remove current article stuff */

        close_art_win( hwnd );

        if ((ULONG)mp1 == NEXT_UNREAD || (ULONG)mp1 == NEXT_ARTICLE ||
            (ULONG)mp1 == PREV_ARTICLE )
            {
            if ( (ULONG)mp1 == NEXT_UNREAD )
                while (curr_article != NULL) {
                      idx = (int)(curr_article->id - gp->lo_num - 1);
                      if ( *((gp->read_list)+idx) == FALSE) break;
                      a_ct++;
                      curr_article = curr_article->next_art;
                      }
            else
            if ( ( (ULONG)mp1 == PREV_ARTICLE ) &&
                 ( curr_article->last_art != NULL ) )
               {
               a_ct--;
               curr_article = curr_article->last_art;
               }
            else
               {
               a_ct++;
               curr_article = curr_article->next_art;
               WinSetFocus( HWND_DESKTOP, hwnd );
               }

            if (curr_article == NULL)
               {
               if ( (ULONG)mp1 == NEXT_UNREAD )
                  {
                  artp = select_next_unread( artp, itmx, hwnd );
                  if ( artp == NULL )
                     {
                     WinSendMsg( hwnd, WM_CLOSE, 0L, 0L );
                     WinDestroyWindow( hwnd );
                     }
                  else
                     WinPostMsg( hwnd, WM_USER, (MPARAM) GET_FOCUS, (MPARAM)0 );
                  }
               break;
               }
            else
               {
               a_ct++;
               tx = load_article(fn, curr_article->art_off);


               hwndArt = open_art_win( &hwndClient, artp->header );

               break;
               }
            }



         if ((ULONG)mp1 == STOP_READING)
            {
            WinSetFocus( HWND_DESKTOP, hwnd );
            return 0;
            }

         break;
         }  /* end WM_USER */


    case WM_COMMAND: {
         USHORT command;                   /* WM_COMMAND command value     */

        command = SHORT1FROMMP(mp1);      /* Extract the command value    */

        switch (command)
          {
          case IDD_HMARK:   {
             /* mark all articles in current thread as read */
             if ( artp != NULL )
                {
                mark_read_in_thread( gp, artp );
                unread = 0;
                sprintf( wstr, "--- %03d     %s",
                 artp->num_articles, artp->header );

                WinSendDlgItemMsg( hwnd, IDD_HRLIST, LM_SETITEMTEXT,
                           MPFROMSHORT( itmx ),
                           MPFROMP( wstr ) );
                /* set focus back to this window from 'mark' */
                WinPostMsg( hwnd, WM_USER, (MPARAM) GET_FOCUS, (MPARAM)0 );
                }
             return 0;
             }
          case IDD_HOPEN:
             WinPostMsg( hwnd, WM_CONTROL, MPFROM2SHORT(IDD_HRLIST, LN_ENTER),
                         (MPARAM)0 );
             return 0;
          default: return 0;
          }
        break;
        }

    /* indicates user wants the next unread thread */
    case WM_BUTTON2DOWN:
        /* if an article window is active, close it */
        if ( hwndArt != (HWND)0L )
           {
            close_art_win( hwnd );
           }
        /* select next unread thread */
        artp = select_next_unread( artp, itmx, hwnd );
        if ( artp == NULL )
           {
            WinSendMsg( hwnd, WM_CLOSE, 0L, 0L );
            WinDestroyWindow( hwnd );
           }
        else
           WinPostMsg( hwnd, WM_USER, (MPARAM) GET_FOCUS, (MPARAM)0 );
        break;



    /* process ESC and TAB as in SNEWS */
    case WM_CHAR: {
      USHORT keyflag;
      USHORT vkey, ch;
      UCHAR  scan;

      keyflag = SHORT1FROMMP( mp1 );
      vkey    = SHORT2FROMMP( mp2 );
      ch      = SHORT1FROMMP( mp2 );
      scan    = CHAR1FROMMP( mp1 );



      if ( !( keyflag & KC_KEYUP ) )
         {
         switch ( vkey )
           {

           /* keys for SNEWS emulation */
           case VK_ESC:
              WinSendMsg( hwnd, WM_CLOSE, 0L, 0L );
              WinDestroyWindow( hwnd );
              break;
           case VK_TAB:
              /* select next unread thread */
              artp = select_next_unread( artp, itmx, hwnd );
              if ( artp == NULL )
                 {
                 WinSendMsg( hwnd, WM_CLOSE, 0L, 0L );
                 WinDestroyWindow( hwnd );
                 }
              else
                 WinPostMsg( hwnd, WM_USER, (MPARAM) GET_FOCUS, (MPARAM)0 );
              break;
           default: {
              if ( ( scan == 5 ) && ( ch == 0x2b ) ) /* far plus key */
                 {
                 artp = select_next_unread( artp, itmx, hwnd );
                 if ( artp == NULL )
                    {
                    WinSendMsg( hwnd, WM_CLOSE, 0L, 0L );
                    WinDestroyWindow( hwnd );
                    }
                 else
                    WinPostMsg( hwnd, WM_USER, (MPARAM) GET_FOCUS, (MPARAM)0 );
                 }
              break;
              }
           }
         }


      break;
      }

    case WM_CONTROL: {
      USHORT  cname, nix;

      cname = SHORT1FROMMP( mp1 );
      get_first_unread = TRUE;
      if  ( cname == IDD_HRLIST )
          {
          uact = SHORT2FROMMP( mp1 );
          if ( ( uact == LN_ENTER ) ||
               ( uact == LN_SELECT ) )
             {
          Rbutton:
             /* if an article window is active, close it */
             if ( hwndArt != (HWND)0L )
                {
                close_art_win( hwnd );
                }

             /* a thread has been selected */
             itmx = (USHORT) WinSendDlgItemMsg( hwnd, IDD_HRLIST,
                           LM_QUERYSELECTION,
                           NULL, NULL );

             nix = 0;  artp = artlist;
             while ( ( artp != NULL ) && ( nix != itmx ) )
                 {
                 nix++;
                 artp  = artp->next;
                 }

             if ( ( artp != NULL ) && ( uact == LN_ENTER ) )
                {  /* read 1st  article in the thread */

                fn = make_news_group_name(gp->group);

                unread = count_unread_in_thread( gp, artp );

                curr_article = artp->art_num;
                a_ct = 1;
                if ( get_first_unread )
                while (curr_article != NULL) {
                      idx = (int)(curr_article->id - gp->lo_num - 1);
                      if ( *((gp->read_list)+idx) == FALSE) break;
                      a_ct++;
                      curr_article = curr_article->next_art;
                      }

                if ( curr_article == NULL )
                   curr_article = artp->art_num;
                tx = load_article(fn, curr_article->art_off);

                hwndArt = open_art_win( &hwndClient, artp->header );
                }
             }

          }   /* end IDD_HRLIST */

      break;
      }


    case WM_CLOSE:   {
             if ( hwndArt != (HWND)0L )
                close_art_win( hwnd );
         }
    }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );

}  /* end HlistProc */

static HWND hwndSrch;
static char Srchstr[41];
/******************************************************************************/
/*                                                                            */
/*  SrchProc obtains the string for a newsgroup search                        */
/*                                                                            */
/******************************************************************************/
MRESULT EXPENTRY SrchProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  static  HWND  psrchstr;
  static  BOOL  casesens;

  switch( msg )
  {
  case WM_INITDLG:  {

     psrchstr = WinWindowFromID( hwnd, IDE_SRCHSTR );
     casesens = FALSE;
     WinSendMsg( psrchstr, EM_SETTEXTLIMIT,
                 MPFROMLONG( 40 ), MPFROMLONG( 0 ) );

     WinSetWindowText( psrchstr, Srchstr );

     WinSetFocus( HWND_DESKTOP, psrchstr );
     return (MRESULT)TRUE;
     }


    case WM_CONTROL:
      {
      if ( SHORT1FROMMP(mp1) == IDE_SBCASE )
         {
         /* toggle state of casesens */
         casesens = !(casesens);
         }
      break;
      }

    case WM_CHAR: {
      USHORT keyflag;
      USHORT vkey;

      keyflag = SHORT1FROMMP( mp1 );
      vkey    = SHORT2FROMMP( mp2 );

      if ( !( keyflag & KC_KEYUP ) )
         {
         switch ( vkey )
           {
           case VK_NEWLINE:
                WinQueryWindowText( psrchstr,
                             sizeof(Srchstr), Srchstr );
                if ( casesens )
                   WinSendMsg( hwndFrame, WM_USER,
                        MPFROMLONG(START_CSEARCH), MPFROMLONG(0L) );
                else
                   WinSendMsg( hwndFrame, WM_USER,
                        MPFROMLONG(START_SEARCH), MPFROMLONG(0L) );
                WinDestroyWindow( hwndSrch );
           }
         }
      break;
      }

    case WM_COMMAND: {
      USHORT cmd;

      cmd = SHORT1FROMMP( mp1 );

      if ( cmd == IDE_SBOK )
         {
         WinQueryWindowText( psrchstr,
                             sizeof(Srchstr), Srchstr );
         if ( casesens )
            WinSendMsg( hwndFrame, WM_USER,
                     MPFROMLONG(START_CSEARCH), MPFROMLONG(0L) );
         else
            WinSendMsg( hwndFrame, WM_USER,
                     MPFROMLONG(START_SEARCH), MPFROMLONG(0L) );
         WinDestroyWindow( hwndSrch );
         }
      else
      if ( cmd == IDE_SBCAN )
         {
         /* indicate to requestor that we cancelled */
         WinSendMsg( hwndFrame, WM_USER,
                     MPFROMLONG(WE_CANCELLED), MPFROMLONG(0L) );
         WinDestroyWindow( hwndSrch );
         }

      break;
      }

  }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


/******************************************************************************/
/*                                                                            */
/*  GListProc  receives msgs from IDD_GLIST  (Group window )                  */
/*                                                                            */
/******************************************************************************/
MRESULT EXPENTRY GListProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  static  ACTIVE  *head;
  static  ACTIVE  *anchor;
  static  USHORT   iix = 0;
  static  USHORT   num_groups = 0;

  USHORT           nix;
  int              ur, totla;
  ACTIVE          *gptr, *fgptr;
  char             *rcfld, *rcmsg;  /* data returned from load_stuff */
  char             stats[80];

  switch( msg )
    {


    case WM_INITDLG: {
      /*  load up the SNEWS .rc stuff.  Do this single-threaded now;
          later, it can be started in another thread and send us a msg
          when it's done */
    int      firstg;     /* first group with unread */

    hwndLbox = hwnd;
    if (load_stuff( &rcfld, &rcmsg )) {
        head = load_active_file();
        load_read_list();
        nicks = load_alias_file();
        load_history_list();
        }
      else
        {
        sprintf( stats, rcmsg, rcfld );
        message( stats, "PMNews RC file Error", MB_CANCEL );
        /* Terminate the application    */
        AbortSN(hwndFrame, hwndClient, MSGBOXID+1 );
        }

      /* fill the list box with news groups */
      WinSendDlgItemMsg( hwnd, IDD_GRLIST, LM_DELETEALL, NULL, NULL );
      gptr = head;
      iix = 0;
      firstg = -1;
      while  ( gptr != NULL )
        {
        ur = count_unread_in_group(gptr);
        totla = gptr->hi_num - gptr->lo_num;
        if ( ur )
           {
           if ( firstg == -1 )
              {
              firstg = iix;  fgptr = gptr;
              }
           sprintf( stats, "%04d (%04d)     ", ur, totla );
           }
        else
           sprintf( stats, "---- (%04d)     ", totla );
        strcat( stats, gptr->group );
        WinSendDlgItemMsg( hwnd, IDD_GRLIST, LM_INSERTITEM,
                           MPFROM2SHORT( LIT_END, 0 ),
                           MPFROMP( stats ) );


        iix++;
        gptr = gptr->next;
        }

      anchor = NULL;
      num_groups = iix;

      /* now put the highlite on the first unread group */
      if ( firstg != -1 )
         {
         anchor = fgptr;
         WinSendDlgItemMsg( hwnd, IDD_GRLIST, LM_SELECTITEM,
                           MPFROMSHORT( firstg ),
                           MPFROMSHORT( TRUE ) );
         }

      /* WinSetFocus( HWND_DESKTOP, hwnd ); */
      WinPostMsg( hwnd, WM_USER, (MPARAM) GET_FOCUS, (MPARAM)0 );
      break;
      }


    case WM_COMMAND: {
      USHORT  cmd;
      cmd = SHORT1FROMMP( mp1 );

      switch ( cmd )
        {
        case IDD_GMARK: {
          /* look up the corresponding group and mark
             everything read */
          nix = 0; anchor = head;
          while ( ( anchor != NULL ) && ( nix != iix ) )
                {
                nix++;
                anchor  = anchor->next;
                }
          if ( nix == iix )
             {
             totla = anchor->hi_num - anchor->lo_num;
             mark_group_as_read( anchor );
             sprintf( stats, "---- (%04d)     ", totla );
             strcat( stats, anchor->group );
             WinSendDlgItemMsg( hwnd, IDD_GRLIST, LM_SETITEMTEXT,
                           MPFROMSHORT( iix ),
                           MPFROMP( stats ) );
             }
          break;
          }  /* end gmark */

        case IDD_SEARCH: {
          /* pop open search entry dialog */
          hwndSrch = WinLoadDlg( HWND_DESKTOP, HWND_DESKTOP,
                        (PFNWP) SrchProc,
                        (HMODULE)0L, IDD_SRCHBOX,
                         NULL );
          break;
          }  /* end search */

        }
      break;
      }

    /* select the next unread group */
    case WM_BUTTON2DOWN:
      {
      int  cx;

      cx = 0;
      gptr = head;
      iix++;
      if ( iix >= num_groups )
         iix = 0;
      /* synch gptr with iix value */
      while  (( gptr != NULL ) && ( cx != iix ))
        {
        cx++;
        gptr = gptr->next;
        }

      /* from this position scan groups for unread articles */
      cx = 0;
      while ( cx < num_groups )
          {
          if ( count_unread_in_group(gptr) > 0 )
             break;
          cx++;
          iix++;
          gptr = gptr->next;
          if ( gptr == NULL )
             {
             iix = 0;
             gptr = head;
             }
          }

      if ( cx < num_groups )
         WinSendDlgItemMsg( hwnd, IDD_GRLIST, LM_SELECTITEM,
                           MPFROMSHORT( iix ),
                           MPFROMSHORT( TRUE ) );
      break;
      }

    case WM_USER:    {

      switch ( (ULONG)mp1) {

         case NEXT_UNREAD:
            {
            /* scan from current index */
            return 0;
            }
         case WE_CANCELLED:
            {
            return 0;
            }
         case START_SEARCH:
            {
            gp = anchor;
            DO_UTIL( SEARCH );
            return 0;
            }
         case START_CSEARCH:
            {
            gp = anchor;
            DO_UTIL( SENSEARCH );
            return 0;
            }
         case DISP_LIST:
            {
            WinDlgBox( HWND_DESKTOP, hwnd,
                       (PFNWP) HListProc,
                       (HMODULE)0L, IDD_HLIST,
                       MPFROMLONG( 0L ) );  /* 0 indicates artlist loaded */
            free_header( artlist );
            WinSetFocus( HWND_DESKTOP, hwnd );
            return 0;
            }

         case MAIL_FILERR: {
            message( (PSZ) PVOIDFROMMP(mp2), "Sorry", MB_OK );
            return 0;
            }
         case GET_FOCUS:
            WinSetFocus( HWND_DESKTOP, hwnd );
            return 0;
         }
      break;
      }


    case WM_CONTROL: {
      USHORT  cname;

      cname = SHORT1FROMMP( mp1 );

      if  ( cname == IDD_GRLIST )
          {
          if ( ( SHORT2FROMMP( mp1 ) == LN_SELECT ) ||
               ( SHORT2FROMMP( mp1 ) == LN_ENTER ) )

             {
             iix = (USHORT) WinSendDlgItemMsg( hwnd, IDD_GRLIST, LM_QUERYSELECTION,
                           NULL, NULL );

             nix = 0; anchor = head;
             while ( ( anchor != NULL ) && ( nix != iix ) )
                 {
                 nix++;
                 anchor  = anchor->next;
                 }
             if ( ( SHORT2FROMMP( mp1 ) == LN_ENTER ) &&
                ( nix == iix ) )
                {  /* open a list box for this group */
                   artlist = NULL;
                   if ( WinDlgBox( HWND_DESKTOP, hwnd,
                            (PFNWP) HListProc,
                            (HMODULE)0L, IDD_HLIST,
                            MPFROMP( anchor ) ) != 0L )
                            {
                            if ( artlist != NULL )
                               {
                               free_header( artlist );
                               }
                            WinInvalidateRect( hwnd, NULL, FALSE );
                            WinSetFocus( HWND_DESKTOP, hwnd );
                            }
                   else logWinErr();

                   ur = count_unread_in_group(anchor);
                   totla = anchor->hi_num - anchor->lo_num;
                   if ( ur )
                      sprintf( stats, "%04d (%04d)     ", ur, totla );
                   else
                      sprintf( stats, "---- (%04d)     ", totla );

                   strcat( stats, anchor->group );
                   WinSendDlgItemMsg( hwnd, IDD_GRLIST, LM_SETITEMTEXT,
                           MPFROMSHORT( iix ),
                           MPFROMP( stats ) );

                }
             }

          }

      break;
      }

    case WM_DESTROY: {
      free_hist_list();
      save_read_list();
      close_active_file();
      }

    }

  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


/******************************************************************************/
/*                                                                            */
/*  FileProc handles selection for saving an article to disk                  */
/*  the path name will be saved in the global savepath                                                                          */
/*  it returns a yea or nay for continuing to save.                          */
/*                                                                            */
/******************************************************************************/
BOOL   FileProc( HWND hwnd )
{
  FILEDLG  gsave;
  HWND     ret;

  memset( &gsave, 0, sizeof(FILEDLG) );
  gsave.cbSize = sizeof(FILEDLG);
  gsave.fl = FDS_SAVEAS_DIALOG | FDS_CENTER;
  strcpy( gsave.szFullFile, "*.SPB" );
  gsave.pszTitle = "Save Article in File";
  gsave.pszOKButton = "Save Article";

  ret = WinFileDlg( hwnd, hwnd, &gsave );
  if ( ret == 0 )
     {
     message( "Unable to open File Dialog", "Sorry", MB_CANCEL );
     return FALSE;
     }

  if ( gsave.lReturn == DID_OK )
     {
     strcpy( savepath, gsave.szFullFile );
     return TRUE;
     }
  else
     {
     savepath[0] = '\0';
     return FALSE;
     }
}

/******************************************************************************/
/*                                                                            */
/*  MboxProc handles queries for mail addressee                               */
/*                                                                            */
/******************************************************************************/
MRESULT EXPENTRY MboxProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  static  HWND  mailad;

  switch( msg )
  {
  case WM_INITDLG:  {
     ALII  *ap;
     char   aliastr[196];
     SHORT  itx;
     MRESULT mrs;

     recipient[0] = '\0';  /* set null to start */

     mailad = WinWindowFromID( hwnd, IDE_MAILADR );
     WinSendMsg( mailad, EM_SETTEXTLIMIT,
                 MPFROMLONG( 80 ), MPFROMLONG( 0 ) );

     /* fill the list box with alias entries */
     WinSendDlgItemMsg( hwnd, IDD_MAILIST, LM_DELETEALL, NULL, NULL );
     ap = nicks;
     while  ( ap != NULL )
        {
        sprintf( aliastr, " %-*s   %s ", szalias+3, ap->nick, ap->name );
        mrs = WinSendDlgItemMsg( hwnd, IDD_MAILIST, LM_INSERTITEM,
                           MPFROM2SHORT( LIT_SORTASCENDING, 0 ),
                           MPFROMP( &aliastr[0] ) );

        itx = SHORT1FROMMR( mrs );
        WinSendDlgItemMsg( hwnd, IDD_MAILIST, LM_SETITEMHANDLE,
                           MPFROMSHORT( itx ),
                           MPFROMP( ap ) );
        ap = ap->nextp;
        }

     WinSetFocus( HWND_DESKTOP, mailad );
     return (MRESULT)TRUE;
     }


  case WM_COMMAND: {
     USHORT cmd;

     cmd = SHORT1FROMMP( mp1 );

     if ( cmd == IDE_GSOK2 )
        {
         WinQueryWindowText( mailad,
                             sizeof(recipient), recipient );
         WinSendMsg( hwndClient, WM_USER,
                     MPFROMLONG(MAIL_ARTICLE), MPFROMLONG(0L) );
         WinDestroyWindow( hwnd );
        }

      if ( cmd == IDE_GSCAN2 )
         {
         /* indicate to requestor that we cancelled */
         WinSendMsg( hwndClient, WM_USER,
                     MPFROMLONG(WE_CANCELLED), MPFROMLONG(0L) );
         WinDestroyWindow( hwnd );
         }

      break;
      }

    case WM_CONTROL: {
      USHORT  cname, iix;
      ALII   *tp;

      cname = SHORT1FROMMP( mp1 );

      if  ( cname == IDD_MAILIST )
          {
          if ( ( SHORT2FROMMP( mp1 ) == LN_ENTER ) ||
               ( SHORT2FROMMP( mp1 ) == LN_SELECT ) )
             {
             iix = SHORT1FROMMR( WinSendDlgItemMsg( hwnd, IDD_MAILIST, LM_QUERYSELECTION,
                           NULL, NULL ) );
             tp = (ALII *) WinSendDlgItemMsg( hwnd, IDD_MAILIST, LM_QUERYITEMHANDLE,
                           MPFROMSHORT( iix ), NULL );


             strcpy( recipient, tp->nick );

             if  ( SHORT2FROMMP( mp1 ) == LN_ENTER )
                 {
                 WinSendMsg( hwndClient, WM_USER,
                     MPFROMLONG(MAIL_ARTICLE), MPFROMLONG(0L) );
                 WinDestroyWindow( hwnd );
                 }
             break;
             }
          }
      break;
      }

  }
  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}

static HWND   hwndSubj;

/******************************************************************************/
/*                                                                            */
/*  SboxProc handles queries for posting subject & group                      */
/*                                                                            */
/******************************************************************************/
MRESULT EXPENTRY SboxProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
  static  HWND  pgroup, psubject;

  switch( msg )
  {
  case WM_INITDLG:  {
     pgroup = WinWindowFromID( hwnd, IDE_GROUP );
     psubject = WinWindowFromID( hwnd, IDE_SUBJ );
     WinSendMsg( pgroup, EM_SETTEXTLIMIT,
                 MPFROMLONG( 80 ), MPFROMLONG( 0 ) );
     WinSendMsg( psubject, EM_SETTEXTLIMIT,
                 MPFROMLONG( 80 ), MPFROMLONG( 0 ) );

     if ( gp != NULL )
        {
        strcpy( ngroup, gp->group );
        WinSetWindowText( pgroup, gp->group );
        }

     if ( artp != NULL )
        {
        strcpy( nsubj, artp->header );
        eat_gunk( nsubj );
        WinSetWindowText( psubject, nsubj );
        }

     WinSetFocus( HWND_DESKTOP, psubject );
     return (MRESULT)TRUE;
     }


    case WM_COMMAND: {
      USHORT cmd;

      cmd = SHORT1FROMMP( mp1 );

      if ( cmd == IDE_GSOK )
         {
         WinQueryWindowText( pgroup,
                             sizeof(ngroup), ngroup );


         WinQueryWindowText( psubject,
                             sizeof(nsubj), nsubj );
         WinSendMsg( hwndArt, WM_USER,
                     MPFROMLONG(POST_ARTICLE), MPFROMLONG(0L) );
         WinDestroyWindow( hwndSubj );
         }

      if ( cmd == IDE_GSCAN )
         {
         /* indicate to requestor that we cancelled */
         WinSendMsg( hwndArt, WM_USER,
                     MPFROMLONG(WE_CANCELLED), MPFROMLONG(0L) );
         WinDestroyWindow( hwndSubj );
         }

      break;
      }

  }
  return WinDefDlgProc( hwnd, msg, mp1, mp2 );
}


/******************************************************************************/
/*                                                                            */
/*  ArticleProc handles the detail display of text                            */
/*                                                                            */
/******************************************************************************/
MRESULT EXPENTRY ArticleProc( HWND hwnd, ULONG msg, MPARAM mp1, MPARAM mp2 )
{
static aLINE *topline;    /* top line in window */
static FONTMETRICS fm;
static HPS    hps;
static HWND   hwndScroll;
static int lcnt;  /* lines displayed */
static USHORT linehite;
static ULONG  currfunc = 0L;




/* article window size */
static SHORT  cyClient;

  switch( msg )
  {

    case WM_CREATE:
      {
      USHORT pos, itemx;
      HWND   hwndmenu;
      USHORT nxAttrib, nuAttrib, npAttrib;
      ART_ID *wart;

      /* set up to show curr_article */

      topline = tx->start;  /* 1st show line past header */


      hwndScroll = WinWindowFromID( WinQueryWindow( hwnd, QW_PARENT ),
                   FID_VERTSCROLL );
      /* set the scroll bar past header */
      pos = topline->index - 1;
      WinSendMsg( hwndScroll, SBM_SETSCROLLBAR, MPFROM2SHORT( pos, 0 ),
                  MPFROM2SHORT( 0, tx->lines - 1 ) );

      /* find the menu bar handle */
      hwndmenu = WinWindowFromID( WinQueryWindow( hwnd, QW_PARENT ),
                   FID_MENU );

      nxAttrib = SHORT1FROMMP( WinSendMsg( hwndmenu, MM_QUERYITEMATTR,
                               MPFROM2SHORT( IDM_NEXTART, TRUE ),
                               (MPARAM)MIA_DISABLED ) );
      nuAttrib = SHORT1FROMMP( WinSendMsg( hwndmenu, MM_QUERYITEMATTR,
                               MPFROM2SHORT( IDM_NEXTUNR, TRUE ),
                               (MPARAM)MIA_DISABLED ) );
      npAttrib = SHORT1FROMMP( WinSendMsg( hwndmenu, MM_QUERYITEMATTR,
                               MPFROM2SHORT( IDM_PREVART, TRUE ),
                               (MPARAM)MIA_DISABLED ) );


      /* if there is no 'prev' article, disable option */
      if ( curr_article->last_art == NULL )
         {
         npAttrib |= MIA_DISABLED;
         WinSendMsg( hwndmenu, MM_SETITEMATTR,
                     MPFROM2SHORT( IDM_PREVART, TRUE ),
                     MPFROM2SHORT( MIA_DISABLED, npAttrib ) );
         }


      /* if there is no 'next' article, disable options */
      if ( curr_article->next_art == NULL )
         {
         nxAttrib |= MIA_DISABLED;
         nuAttrib |= MIA_DISABLED;
         WinSendMsg( hwndmenu, MM_SETITEMATTR,
                     MPFROM2SHORT( IDM_NEXTART, TRUE ),
                     MPFROM2SHORT( MIA_DISABLED, nxAttrib ) );
         WinSendMsg( hwndmenu, MM_SETITEMATTR,
                     MPFROM2SHORT( IDM_NEXTUNR, TRUE ),
                     MPFROM2SHORT( MIA_DISABLED, nuAttrib ) );

         }
      else  /* scan for unread (slow) */
         {
         wart = curr_article->next_art;
         while (wart != NULL)
               {
               itemx = (int)(wart->id - gp->lo_num - 1);
               if ( *((gp->read_list)+itemx) == FALSE) break;
               wart = wart->next_art;
               }
         if ( wart == NULL )
            {
            nuAttrib |= MIA_DISABLED;
            WinSendMsg( hwndmenu, MM_SETITEMATTR,
                     MPFROM2SHORT( IDM_NEXTUNR, TRUE ),
                     MPFROM2SHORT( MIA_DISABLED, nuAttrib ) );
            }
         }


      WinSetFocus( HWND_DESKTOP, hwnd );
      return 0;
      }

    case WM_SIZE: {
      /* save the resize values */

      /* cxClient = SHORT1FROMMP(mp2); */
      cyClient = SHORT2FROMMP(mp2);


      break;  }


    case WM_COMMAND:
      {
      USHORT command;                   /* WM_COMMAND command value     */

      command = SHORT1FROMMP(mp1);      /* Extract the command value    */

      switch (command)
        {
        case IDM_RETHDRS: {
             WinSendMsg( hdrwin, WM_USER, (MPARAM) STOP_READING, NULL );
             return 0;
             }

        case IDM_NEXTUNR:  {
             WinSendMsg( hdrwin, WM_USER, (MPARAM) NEXT_UNREAD, NULL );
             return 0;
             }

        case IDM_NEXTART:  {
             WinSendMsg( hdrwin, WM_USER, (MPARAM) NEXT_ARTICLE, NULL );
             return 0;
             }

        case IDM_PREVART:  {
             WinSendMsg( hdrwin, WM_USER, (MPARAM) PREV_ARTICLE, NULL );
             return 0;
             }

        case IDM_MAILART: {
             currfunc = IDM_MAILART;
             /* open dialog box for addressee */
             hwndSubj = WinLoadDlg( HWND_DESKTOP, HWND_DESKTOP,
                        (PFNWP) MboxProc,
                        (HMODULE)0L, IDD_MAILBOX,
                         NULL );
             return 0;
             }

        case IDM_REPLY:  {
             currfunc = IDM_REPLY;
             DO_UTIL( REPART );
             return 0;
             }

        case IDM_PRINT:    {
             DO_UTIL( PRINT );
             return 0;
             }

        case IDM_SAVED:    {
             if ( FileProc( hwnd ) )
                DO_UTIL( SAVED );
             return 0;
             }

        case IDM_NEWPOST:  {
             currfunc = IDM_NEWPOST;
             /* open dialog box for subject, group */
             hwndSubj = WinLoadDlg( HWND_DESKTOP, HWND_DESKTOP,
                        (PFNWP) SboxProc,
                        (HMODULE)0L, IDD_SUBBOX,
                         NULL );
             return 0;
             }

        case IDM_FOLLOWUP:  {
             currfunc = IDM_FOLLOWUP;
             strcpy( ngroup, gp->group );
             strcpy( nsubj, artp->header );
             eat_gunk( nsubj );

             /* quote article? */
             quotans = message( "Quote Article?", "Which?", MB_YESNO );

             /* schedule  edit session */
             DO_UTIL( POST_EDIT );
             return 0;
             }

        default:
          return WinDefWindowProc( hwnd, msg, mp1, mp2 );
        }
      break;
      }


    /* send corresponding scroll bar messages */
    case WM_CHAR: {
      USHORT keyflag;
      USHORT vkey, ch;
      UCHAR  scan;

      keyflag = SHORT1FROMMP( mp1 );
      vkey    = SHORT2FROMMP( mp2 );
      scan    = CHAR1FROMMP( mp1 );
      ch      = SHORT1FROMMP( mp2 );

      if ( !( keyflag & KC_KEYUP ) )
         {
         if ( ( scan == 7 ) && ( ch == 0x0d ) )
            { /* far Enter key */
             WinSendMsg( hdrwin, WM_USER, (MPARAM) NEXT_ARTICLE, NULL );
             return 0;
            }

         if ( scan == 5 )
            {
            if ( ch == 0x2b )
               /* far plus key */
               WinSendMsg( hdrwin, WM_USER, (MPARAM) NEXT_UNREAD, NULL );
            else
            if ( ch == 0x2d )
               /* far minus key */
               WinSendMsg( hdrwin, WM_USER, (MPARAM) PREV_ARTICLE, NULL );
             return 0;
            }


         switch ( vkey )
           {
           case VK_PAGEUP:
              WinSendMsg( hwnd, WM_VSCROLL, 0L,
                 MPFROM2SHORT( 0, SB_PAGEUP ) );
              break;
           case VK_PAGEDOWN:
              WinSendMsg( hwnd, WM_VSCROLL, 0L,
                 MPFROM2SHORT( 0, SB_PAGEDOWN ) );
              break;
           case VK_UP:
              WinSendMsg( hwnd, WM_VSCROLL, 0L,
                 MPFROM2SHORT( 0, SB_LINEUP ) );
              break;
           case VK_DOWN:
              WinSendMsg( hwnd, WM_VSCROLL, 0L,
                 MPFROM2SHORT( 0, SB_LINEDOWN ) );
              break;

           /* keys for SNEWS emulation */
           case VK_ESC:
              WinSendMsg( hdrwin, WM_USER, (MPARAM) STOP_READING, NULL );
              break;
           case VK_TAB:
              WinSendMsg( hdrwin, WM_USER, (MPARAM) NEXT_UNREAD, NULL );
              break;
           case VK_BACKTAB:
              break;
           }
         }

      break;
      }


    /* handle requests from utility task */
    case WM_USER: {

      char  buf[80];

      switch ( (ULONG)mp1 )
        {
        /* put the focus here */
        case ART_FOCUS:
             WinSetFocus( HWND_DESKTOP, hwnd );
             break;

        case MAIL_VERIFY: {
          sprintf(buf, "Send message to %s? ", (char *)mp2 );
          queryans =
            message(buf, "Which?", MB_YESNO );
          DosPostEventSem( work_to_do );
          break;
          }
        case POST_VERIFY: {
          queryans =
            message("Post article?", "Which?", MB_YESNO );
          DosPostEventSem( work_to_do );
          break;
          }
        case MAIL_FILERR: {
          message("*** Cannot open temp file ***", "Sorry", MB_CANCEL );
          break;
          }
        case REPART_QUOTE: {
          queryans = message("Quote article? ", "Which?", MB_YESNO );
          DosPostEventSem( work_to_do );
          break;
          }

        case WE_CANCELLED: {
          break;
          }

        case MAIL_ARTICLE: {
         if ( strlen(recipient) > 1 )
            {
            DO_UTIL( MAIL );
            }
          break;
          }

        case POST_ARTICLE: {

          /* validate group */
          if ( !check_valid_post_group( ngroup ) )
             {
             message( "Invalid Newsgroup!", "Sorry", MB_CANCEL );
             break;
             }
          if ( tx != NULL )
             {
             if ( currfunc != IDM_NEWPOST )
                quotans = message( "Quote Article?", "Which?", MB_YESNO );
             else
                quotans = MBID_NO;
             DO_UTIL( POST_EDIT );
             }
          break;
          }
        }
      break;
      }


    case WM_VSCROLL: {
      BOOL paint;
      USHORT cntln, ix;
      aLINE  *orig;

      paint = TRUE;
      orig = topline;

      switch ( SHORT2FROMMP(mp2) )
         {
         case SB_LINEUP:
              if ( topline != tx->top )
                 topline = topline->last;
              break;

         case SB_PAGEUP:
              cntln = (cyClient / linehite) - 1;
              for ( ix = 0; ix < cntln; ix++ )
                  {
                  if ( topline != tx->top )
                     topline = topline->last;
                  else break;
                  }

              break;

         case SB_SLIDERTRACK:
              break;

         case SB_SLIDERPOSITION:
              cntln = SHORT1FROMMR( WinSendMsg( hwndScroll, SBM_QUERYPOS,
                                    NULL, NULL ) );
              topline = tx->top;
              for ( ix = 1; ix < cntln; ix++ )
                  if ( topline->next != NULL )
                     topline = topline->next;

              break;

         case SB_PAGEDOWN:
              cntln = (cyClient / linehite) - 1;
              for ( ix = 0; ix < cntln; ix++ )
                  {
                  if ( topline->next != NULL )
                     topline = topline->next;
                  else break;
                  }
              break;

         case SB_LINEDOWN:
              if ( topline->next != NULL )
                 topline = topline->next;
              break;

         case SB_ENDSCROLL:
              paint = FALSE;
              break;

         }

      if ( paint && topline != orig )
         {
         cntln = ( 100 * topline->index ) / tx->lines;
         WinSendMsg( hwndScroll, SBM_SETPOS, MPFROMSHORT( cntln ), NULL );
         WinInvalidateRect (hwnd, NULL, FALSE);
         }

      break;
      }

    case WM_ERASEBACKGROUND:
      /******************************************************************/
      /* Return TRUE to request PM to paint the window background       */
      /* in SYSCLR_WINDOW.                                              */
      /******************************************************************/
      return (MRESULT)( TRUE );

    case WM_PAINT:
      /******************************************************************/
      /* Window contents are drawn here in WM_PAINT processing.         */
      /******************************************************************/
      {
      RECTL  rc;                        /* Rectangle coordinates        */
      POINTL pt;                        /* String screen coordinates    */
      aLINE  *lnp;
      int    sLen;
      SWP  lastpos;

      hps = WinBeginPaint (hwnd, (HPS)0L, &rc);
      GpiQueryFontMetrics( hps, sizeof fm, &fm );
      linehite  =  fm.lMaxBaselineExt + (fm.lMaxDescender/2);
      WinSendMsg( hwndScroll, SBM_SETTHUMBSIZE,
                  MPFROM2SHORT( (cyClient / linehite), tx->lines ), 0L );

      GpiErase( hps );
      lnp = topline;

      /* write as many lines from topline as will fit in current window */
      pt.x = fm.lAveCharWidth;
      pt.y = cyClient;
      lcnt = 0;

      while ( ( pt.y -= linehite ) > 0
                && lnp != NULL )
            {
            sLen = strlen( lnp->data );
            if ( lnp->data[ sLen - 1 ] == '\n' ) sLen--;
            if ( sLen > 0 )
               {
               lcnt++;
               GpiCharStringAt( hps, &pt, sLen, lnp->data );
               }
            lnp = lnp->next;
            }

      WinEndPaint( hps );                      /* Drawing is complete   */
      if ( !WinQueryWindowPos( hwndArt, &lastpos ) )
         AbortSN( hwndFrame, hwndClient, 21 );
      artloc.artx = (USHORT)lastpos.x;
      artloc.arty = (USHORT)lastpos.y;
      artloc.artwidth  = (USHORT)lastpos.cx;
      artloc.artheight = (USHORT)lastpos.cy;
      break;
      }

    case WM_CLOSE: {
      /******************************************************************/
      /* This is the place to put your termination routines             */
      /******************************************************************/
      WinSendMsg( hdrwin, WM_USER, (MPARAM) STOP_READING, NULL );
      break;
      }

    case WM_DESTROY: {
      return WinDefWindowProc( hwnd, msg, mp1, mp2 );
      }


    default:
      /******************************************************************/
      /* Everything else comes here.  This call MUST exist              */
      /* in your window procedure.                                      */
      /******************************************************************/

      return WinDefWindowProc( hwnd, msg, mp1, mp2 );
  }

  return WinDefWindowProc( hwnd, msg, mp1, mp2 );
} /* End of ArticleProc */


/* utililty task for long-running functions, one at a time
   schedule only ONE function */

void util_task( void * parm )
{
ULONG postct;   /* post count, not used  */
APIRET rc;
FILE  *fp;      /* utility file */

  DosOpenEventSem( NULL, &work_to_do );

  do
    {

    /* wait on a work item */
    rc = DosWaitEventSem( work_to_do, -1L );

    /* clear semaphore */
    rc = DosResetEventSem( work_to_do, &postct );
    if ( rc )
       AbortSN( hwndFrame, hwndClient, 23 );

    switch ( util_func )
      {

      case MAIL: {
         mail_to_someone( tx, recipient );
         break;
         }

      case REPART: {
         reply_to_article( tx );
         break;
         }

      case PRINT: {
         /* print the current article */
         fp = fopen( "LPT1", "w" );
         if ( fp != NULL )
            copy_article_out( fp, tx );
         break;
         }

      case POST_EDIT: {
         post_art( quotans );
         fprintf(errf, "returned from post_edit\n" );
         break;
         }

      case SEARCH:
      case SENSEARCH:
      {
         ARTICLE *  rp;


         if ( util_func == SENSEARCH )
            {
            fprintf(errf,"case sensitive search\n");
            rp = search_group( gp, Srchstr, TRUE );
            }
         else
            rp = search_group( gp, Srchstr, FALSE );
         if ( rp != NULL )
            {
            /* post message to Glistproc to display the
               given threads */
            artlist = rp;
            WinPostMsg( hwndFrame, WM_USER, (MPARAM) DISP_LIST, MPFROMP( rp ) );
            }
         else
            {
            /* post message to say nothing found */
            WinPostMsg( hwndFrame, WM_USER, (MPARAM) MAIL_FILERR,
                        MPFROMP( "Unable to locate string" ) );
            }
         break;
         }

      case SAVED: {
         /* save the current article */
         fp = fopen( savepath, "w" );
         if ( fp != NULL )
            copy_article_out( fp, tx );
         break;
         }

      }

      if ( util_func != EOJ )
         util_func = TASK_FREE; /* mark available */

    }  while ( util_func != EOJ );

}


/*------------------------------- main --------------------------------*/

int  main(void)
{
  HMQ  hmq;                             /* Message queue handle         */
  QMSG qmsg;                            /* Message from message queue   */
  ULONG lsize;                          /* profile size query output    */
  SWP   wpos;                           /* group window position */


  /* open the debug file */
  errf = fopen( "pmnews.err" ,"w" );

  if ((hab = WinInitialize(0)) == 0L) /* Initialize PM     */
     AbortSN(hwndFrame, hwndClient, 1 ); /* Terminate the application    */




  if ((hmq = WinCreateMsgQueue( hab, 0 )) == 0L)/* Create a msg queue */
     AbortSN(hwndFrame, hwndClient, 2 ); /* Terminate the application    */

  if (!WinRegisterClass(                /* Register window class        */
     hab,                               /* Anchor block handle          */
     PMNEWS_CLASS,                      /* Window class name            */
     (PFNWP)ArticleProc,                /* Address of window procedure  */
     CS_SIZEREDRAW,                     /* Class style                  */
     0                                  /* No extra window words        */
     ))
     AbortSN(hwndFrame, hwndClient, 4 ); /* Terminate the application    */

  /*********************************/
  /* IPF Initialization Structure. */
  /*********************************/
  hmiHelpData.cb=sizeof(HELPINIT);
  hmiHelpData.ulReturnCode=0;
  hmiHelpData.pszTutorialName=NULL;
  hmiHelpData.phtHelpTable=(PVOID)(0xffff0000 | IDH_MAIN);
  hmiHelpData.hmodAccelActionBarModule=0;
  hmiHelpData.idAccelTable=0;
  hmiHelpData.idActionBar=0;
  hmiHelpData.pszHelpWindowTitle=(PSZ)"PMNews Help";
  hmiHelpData.hmodHelpTableModule=0;
  hmiHelpData.fShowPanelId=0;
  hmiHelpData.pszHelpLibraryName=(PSZ)"PMNEWS.HLP";

  /***************************/
  /* Create instance of IPF. */
  /***************************/
  hwndHelpInstance=WinCreateHelpInstance(hab, &hmiHelpData);
  if(!hwndHelpInstance)
    message( "Help Not Available", "Help Creation Error",
       MB_OK | MB_APPLMODAL | MB_MOVEABLE);
  else {
    if(hmiHelpData.ulReturnCode){
      message( "Help Terminated Due to Error", (PSZ)"Help Creation Error",
        MB_OK | MB_APPLMODAL | MB_MOVEABLE);
      WinDestroyHelpInstance(hwndHelpInstance);
      }
    }

  /* start the utility thread */
  DosCreateEventSem( NULL, &work_to_do, DC_SEM_SHARED, 0 );
  DosCreateEventSem( NULL, &eojsem, 0, 0 );

  util_thread = _beginthread( util_task, NULL, 16384, NULL );
  if ( util_thread < 0 )
     {
     AbortSN(hwndFrame, hwndClient, 11 ); /* Terminate the application    */
     }


  hwndFrame = WinLoadDlg( HWND_DESKTOP, HWND_DESKTOP,
                            (PFNWP) GListProc,
                            (HMODULE)0L, IDD_GLIST,
                            NULL );

/************************************************************************/
/* Get and dispatch messages from the application message queue         */
/* until WinGetMsg returns FALSE, indicating a WM_QUIT message.         */
/************************************************************************/


  if ( hwndFrame != (HWND)0L )
     {
     if ( ( init = PrfOpenProfile( hab, "pmnews.ini" ) )  == (HINI)0L )
        {
        message( "Unable to open pmnews.ini", "Sorry", MB_CANCEL );
        }
     else
        {
        /* load .ini data */
        PrfQueryProfileSize( init, "Article", "artloc", &lsize );
        if ( lsize == sizeof(artloc) )
           {
           PrfQueryProfileData( init, "Article", "artloc", &artloc, &lsize );
           if ( !( artloc.artx | artloc.arty | artloc.artheight | artloc.artwidth ) )
              {
              artloc.artx = ARTX;
              artloc.arty = ARTY;
              artloc.artwidth = ARTWIDTH;
              artloc.artheight = ARTHEIGHT;
              }
           WinSetWindowPos( hwndFrame, HWND_TOP,
                    artloc.grpx, artloc.grpy, 0, 0,
                    SWP_ZORDER | SWP_MOVE | SWP_SHOW );
           }

        /******************************/
        /* Associate instance of IPF. */
        /******************************/
        if(hwndHelpInstance)
          WinAssociateHelpInstance(hwndHelpInstance, hwndFrame);


        while( WinGetMsg( hab, &qmsg, 0L, 0, 0 ) )
            WinDispatchMsg( hab, &qmsg );

        if ( WinQueryWindowPos( hwndFrame, &wpos ) )
           {
           artloc.grpx = wpos.x;
           artloc.grpy  = wpos.y;
           }

        PrfWriteProfileData( init,
          "Article", "artloc", &artloc, sizeof(artloc) );
        PrfCloseProfile( init );
        }

     if (hwndHelpInstance)
        WinDestroyHelpInstance(hwndHelpInstance);
     WinDestroyWindow(hwndFrame);        /* Tidy up...                   */
     }
  else logWinErr();


  /* tell the utility task to go away */
  util_func = EOJ;
  DosPostEventSem( work_to_do );


  WinDestroyMsgQueue( hmq );             /* Tidy up...                   */
  WinTerminate( hab );                   /* Terminate the application    */


  fclose( errf );   /* close the debug file */

  return 0;

} /* End of main */

