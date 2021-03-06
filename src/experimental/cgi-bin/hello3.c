/*
 * FILE:
 * hello3.c
 *
 * FUNCTION:
 * experimental gnucash server
 * written as a demo, not real code.
 * this file is here mostly as a simple intro to what
 * the server is doing.
 */
/********************************************************************\
 * This program is free software; you can redistribute it and/or    *
 * modify it under the terms of the GNU General Public License as   *
 * published by the Free Software Foundation; either version 2 of   *
 * the License, or (at your option) any later version.              *
 *                                                                  *
 * This program is distributed in the hope that it will be useful,  *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of   *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the    *
 * GNU General Public License for more details.                     *
 *                                                                  *
 * You should have received a copy of the GNU General Public License*
 * along with this program; if not, contact:                        *
 *                                                                  *
 * Free Software Foundation           Voice:  +1-617-542-5942       *
 * 51 Franklin Street, Fifth Floor    Fax:    +1-617-542-2652       *
 * Boston, MA  02110-1301,  USA       gnu@gnu.org                   *
 *                                                                  *
\********************************************************************/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "gnc-engine.h"
#include "io-gncxml.h"

#include <fcgi_stdio.h>


int
main (int argc, char *argv[])
{
    int err, fake_argc = 1;
    char * fake_argv[] = {"hello", 0};
    QofBook *book;
    Account *root;
    char *bufp;
    int rc, sz;

    /* intitialize the engine */
    gnc_engine_init (fake_argc, fake_argv);

    /* contact the database, which is a flat file for this demo */
    book = qof_book_new ();

    rc = gnc_book_begin (book, "file:/tmp/demo.gnucash", FALSE);
    if (!rc) goto bookerrexit;

    rc = gnc_book_load (book);
    if (!rc) goto bookerrexit;

    /* the root pointer points to our local cache of the data */
    root = gnc_book_get_root_account (book);

    /* --------------------------------------------------- */
    /* done with initialization, go into event loop */

    while (FCGI_Accept() >= 0)
    {
        GList *split_list;
        Query *q = NULL;
        char *request_method;
        int read_len = 0;

        /* get the request method */
        request_method = getenv ("REQUEST_METHOD");

        /* Lets pretend that method=get means user has logged
         * in.  Send the user the accounts and currencies,
         * but not the transactions/splits. */
        if (!strcmp ("GET", request_method))
        {
            gncxml_write_account_tree_to_buf(root, &bufp, &sz);

            /* print the HTTP header */
            printf("Content-type: text/gnc-xml\r\n"
                   "Content-Length: %d\r\n"
                   "\r\n", sz);

            /* send the xml to the client */
            printf ("%s", bufp);
            free (bufp);

            /* wait for the next request */
            continue;
        }


        if (!strcmp ("POST", request_method))
        {
            char * content_length = getenv("CONTENT_LENGTH");
            read_len = atoi (content_length);

            /* read 'read_len' bytes from stdin ... */
            bufp = (char *) malloc (read_len);
            fread (bufp, read_len, 1, stdin);

            /* conver the xml input into a gnucash query structure... */
            q = gncxml_read_query (bufp, read_len);
            xaccQuerySetGroup (q, root);

            /* hack -- limit to 30 splits ... */
            qof_query_set_max_results (q, 30);
            split_list = qof_query_run (q);

            qof_query_destroy (q);

            /* wait for the next request */
            continue;
        }

        /* if we got to here, an error -- unknown method */
        printf("Content-type: text/plain\r\n"
               "\r\n"
               "unknown request type \n");


    }

bookerrexit:

    err = gnc_book_get_error (book);

    /* 500 Server Error */
    FCGI_SetExitStatus (500);

    printf("Content-type: text/plain\r\n\r\n"
           "error was %s\n", strerror (err));

    FCGI_Finish();

    /* close the book */
    qof_book_destroy (book);

    /* shut down the engine */
    gnc_engine_shutdown ();

    sleep (1);

    return 0;
}

