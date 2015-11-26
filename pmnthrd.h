/*
 *  decls for thread algorithm support for PMNews
 */


/* threading algorithm must return a pointer which it wants
   passed to it with each new subject line processed by pmnews1 */
void * thread_init(void);

/* called at completion of thread pass for the newsgroup */
void   thread_end( void * ancp );



/* called for each new subject line.
   returns TRUE if it found a subject (ARTICLE) to thread-to,
   FALSE if it had to allocate an ARTICLE.
   'subjline' is mixed-case with some extraneous gunk removed
   'ancp' is ptr returned by thread_init.
   'targ' is to be stored in all cases */
BOOL   thread_compare( char * subjline, void * ancp, ARTICLE **targ );
