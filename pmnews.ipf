.******************************************************
.* This file contains the help panel definitions for  *
.* the PMNews program.                                *
.******************************************************
:userdoc.
.*
:h1 res=1.  Extended Help for PMNews
:hp2.PMNews Overview:ehp2.
.*
:p.PMNews is a Presentation Manager implementation of the SNEWS program
provided to support UUPC news delivery.  Aside from some installation
differences, it should be 'plug compatible' with SNEWS.
The impetus for using SNEWS/PMNews as your news reader is primarily the
requirement to save disk space when the FAT file system is being used with
OS/2 (HPFS is also supported). In future,
PMNews may be converted to support the 'normal',
hierarchical UUPC news delivery (a one-to-one article-to-file relationship).
.*
:p.PMNews supports&colon.
:ol.
:li.news reading&colon.
:ol.
:li.subject-line threading
:li.group text search
:li.marking of articles-read
:eol.
:ol.mailing
:li.forwarding article
:li.replying to an article via e-mail
:eol.
:ol.posting
:li.followups
:li.original posts
:eol.
:eol.
.*
.*
:p.To get more information about any one of these features, double click on
one of the following highlighted words (or press the Tab key until the
highlighted word is selected and then press Enter).
.*
:p.
:link reftype=hd res=10.
    Installation
:elink.
:link reftype=hd res=11.
    Keys
:elink.
:link reftype=hd res=12.
    Searching a Group
:elink.
:link reftype=hd res=13.
    Marking Groups/Threads/Articles
:elink.
:link reftype=hd res=14.
    Threading
:elink.
:link reftype=hd res=15.
    Printing/Saving an Article
:elink.
:link reftype=hd res=16.
    Posting
:elink.
:link reftype=hd res=17.
    Mailing
:elink.
.*
:h1 res=2.   Thread Header List
:p.This window allows you to select via double-click a thread
within the selected new group.  All articels within a thread
have a similar subject line.
.*
:h1 res=3.   Article Window
:p.This window displays the text of each article.
PMNews will remember the position of this window between
sessions.  The window is initially displayed with the first actual
text line at the top of the window.  If you wish to view the
contents of the header, if present, scroll up using the scroller
or use the page-up key.
.*
:h1 res=10.  Installation
:hp2.PMNews Installation:ehp2.
:i1.installation
:p.The best procedure for PMNews installation is to install SNEWS
completely, setting up all required environment variables, as well as
all the required entries in either UUPC.RC or the specified PERSONAL.RC.
The required parameters for PMNews are:
:dl tsize=5 break=all.
:dt.mailserv
:dd.The entry name for your mail server in the SYSTEMS file.
:dt.domain
:dd.Your mail domain address
:dt.nodename
:dd.Usually the first qualifier of your domain (host name).
:dt.newsdir
:dd.The target directory of RNEWS (the intermediate directory for SNEWS).
:dt.tempdir
:dd.Any temporary subdirectory
:dt.mailbox
:dd.Where your mail is saved after reading (.spb)
:dt.Signature
:dd.Your ugly sig file.
:dt.name
:dd.Actual, or pseudonymous, name
:dt.Organization
:dd.Company, University, etc.
:dt.Editor
:dd.Command string for your fave
:dt.Home
:dd.Home directory for mail
:dt.Aliases
:dd.The alias file for your mailer.
:edl.
.*
:p.There are no DLLs associated with PMNews.  Create a program object
which points to the subdirectory containing PMNEWS.EXE.  Specify this
directory as the default directory for the object.  Find a nice icon
or use the one provided (ripped-off from someplace else). Double-click
on the object and PMNews will be invoked.  Two files will be written
into the default directory (PMNEWS.INI and PMNEWS.ERR) with each
invocation.  The .ERR file will either be empty or contain error codes
from PM.  The .INI file will retain the window position of news group
dialog window across invocations.
.********************
.* Key Assignments. *
.********************
:h1 res=11.  Keys
:p.:hp2.PMNews Keys:ehp2.
:dl tsize=5 break=all.
:dt.Enter
:dd.Next Article
:dt.Keypad Plus
:dd.Next Unread
:dt.Esc
:dd.Exit Window
:dt.Arrow Keys
:dd.Listbox Select
:dt.Keypad Minus
:dd.Previous Article
:dt.MouseButton1
:dd.Select Entry (double-click)
:dt.MouseButton2
:dd.Next Unread
:edl.
.*
:p.:hp2.HELP KEYS:ehp2.
:dl tsize=5 break=all.
:dt.F1
:dd.Get help
:dt.F2
:dd.Get extended help (from within any help window)
:dt.Alt+F4
:dd.End help
:dt.F9
:dd.Go to a list of keys (from within any help window)
:dt.F11
:dd.Go to the help index (from within any help window)
:dt.Esc
:dd.Previous Help Panel, or End help if only one panel
:dt.Alt+F6
:dd.Go to/from help and programs
:dt.Shift+F10
:dd.Get help for help
:edl.
.*
:p.:hp2.SYSTEM KEYS:ehp2.
:dl tsize=5 break=all.
:dt.Alt+Esc
:dd.Switch to the next program
:dt.Ctrl+Esc
:dd.Switch to the Task List
:edl.
.*
:p.:hp2.WINDOW KEYS:ehp2.
:dl tsize=5 break=all.
:dt.F10
:dd.Go to/from the action bar
:dt.Arrow keys
:dd.Move among choices
:dt.End
:dd.Go to the last choice in a pull-down
:dt.Esc
:dd.Cancel a pull-down or the system menu
:dt.Home
:dd.Go to the first choice in a pull-down
:dt.Underlined letter
:dd.Move among the choices on the action bar and pull-downs
:dt.Alt+F4
:dd.Close the window
:dt.Alt+F5
:dd.Restore the window
:dt.Alt+F7
:dd.Move the window
:dt.Alt+F8
:dd.Size the window
:dt.Alt+F9
:dd.Minimize the window
:dt.Alt+F10
:dd.Maximize the window
:dt.Shift+Esc or Alt+Spacebar
:dd.Go to/from the system menu
:dt.Shift+Esc or Alt
:dd.Go to/from the system menu of a text window
:edl.
.*
.********************
.* Searching a Group*
.********************
:h1 res=12.  Searching
:p.First, from the News Group Dialog, select (highlight) a
News Group. Then, click on
the :hp2.Search:ehp2. button.  This will pop a single line entry field
for the entry of a string search argument.  Currently, this must be the
exact string.  After entering this, click on 'OK'.  The utility task will
then searhc the body of all articles within the group searching for that
string.  If the string isn't found a message box will pop-up to inform
you.  If the string is found, a thread header list box will be presented
consisting of all the articles which contain this string.
.*
.*
.********************
.* Marking          *
.********************
:h1 res=13.  Marking
:p.Marking articles as read may occur at the Group or Thread Header
level.  When an article is opened, it is automatically marked as read.
When the group or thread is high-lighted, simply click on the 'Mark'
button.
.*
.*
.********************
.* Threading        *
.********************
:h1 res=14.  Threading
:p.SNEWS does not use the reference field of the article header to
relate articles to threads.  Instead, the subject line is scanned and
tokenized, then compared other articles within the grou[ containing
the same tokens.  This is an inexact science.  Someday, an 'offline'
threading program can build the structure statically overnight.
Also distributed with the program is 'pmnthrd.c', which is the
original algorithm for subject-line threading.
.*
.*
.********************
.* Printing/Saving  *
.********************
:h1 res=15.  Printing/Saving
:p.When within an article window, it's possible to print or save
that article to disk; simply click on the corresponding button.
The :hp2.Print:ehp2. request writes the article to LPT1 immediately.
The spooler will do its thing if allowed.
:p.To save an article to disk, click on the :hp2.Save:ehp2. button.
The standard file dialog will be called to assist you in selecting a
target filename and directory.
.*
.*
.********************
.* Posting          *
.********************
:h1 res=16.  Posting
:p.When within an article window, it's possible to post a follow-up
to the currently selected article to the originator via e-mail, or
to add a completely new post to the current news group.
It's somewhat awkward to be required to enter an article in order to
compose a new post.  Click on :hp2.Post:ehp2. and then click on
:hp2.New Post:ehp2. in the secondary menu.
You specified editor will be called to compose the article.  When you've
completed the editor session, you'll be prompted to accept the new post.
:p.Follow-ups are handled in a similar manner, except that the subject
line of the current article is used as your subject line.
.*
.*
.********************
.* Posting          *
.********************
:h1 res=17.  Mailing
:p.When within an article window, it's possible to mail the current article
to someone else (forward it).  Click on :hp2.Mail:ehp2., then select
:hp2.Mail Article:ehp2..  A listbox window containing your mail aliases will
be displayed, along with an entry field.  Enter the addressee for this
mailing by double-clicking in the listbox or entering the full address.
:p.To reply to this article, select :hp2.Reply:ehp2. from the
:hp2.Mail:ehp2. submenu.  You will have the opportunity to 'quote'
the current article in your mailing.
:index.
:euserdoc.
