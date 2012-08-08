/** \file io.c

Utilities for io redirection.
	
*/
#include "config.h"


#include <stdlib.h>
#include <stdio.h>
#include <wchar.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>

#ifdef HAVE_SYS_TERMIOS_H
#include <sys/termios.h>
#endif

#ifdef HAVE_SYS_IOCTL_H
#include <sys/ioctl.h>
#endif

#include <unistd.h>
#include <fcntl.h>

#if HAVE_NCURSES_H
#include <ncurses.h>
#else
#include <curses.h>
#endif

#if HAVE_TERMIO_H
#include <termio.h>
#endif

#if HAVE_TERM_H
#include <term.h>
#elif HAVE_NCURSES_TERM_H
#include <ncurses/term.h>
#endif

#include "fallback.h"
#include "util.h"

#include "wutil.h"
#include "exec.h"
#include "common.h"
#include "io.h"


void io_buffer_read( io_data_t *d )
{
	exec_close(d->param1.pipe_fd[1] );

	if( d->io_mode == IO_BUFFER )
	{		
/*		if( fcntl( d->param1.pipe_fd[0], F_SETFL, 0 ) )
		{
			wperror( L"fcntl" );
			return;
			}	*/
		debug( 4, L"io_buffer_read: blocking read on fd %d", d->param1.pipe_fd[0] );
		while(1)
		{
			char b[4096];
			long l;
			l=read_blocked( d->param1.pipe_fd[0], b, 4096 );
			if( l==0 )
			{
				break;
			}
			else if( l<0 )
			{
				/*
				  exec_read_io_buffer is only called on jobs that have
				  exited, and will therefore never block. But a broken
				  pipe seems to cause some flags to reset, causing the
				  EOF flag to not be set. Therefore, EAGAIN is ignored
				  and we exit anyway.
				*/
				if( errno != EAGAIN )
				{
					debug( 1, 
						   _(L"An error occured while reading output from code block on file descriptor %d"), 
						   d->param1.pipe_fd[0] );
					wperror( L"io_buffer_read" );				
				}
				
				break;				
			}
			else
			{				
				d->out_buffer_append( b, l );				
			}
		}
	}
}


io_data_t *io_buffer_create( int is_input )
{
    bool success = true;
	io_data_t *buffer_redirect = new io_data_t;
	buffer_redirect->out_buffer_create();
	buffer_redirect->io_mode=IO_BUFFER;
	buffer_redirect->is_input = is_input;
	buffer_redirect->fd=is_input?0:1;
	
	if( exec_pipe( buffer_redirect->param1.pipe_fd ) == -1 )
	{
		debug( 1, PIPE_ERROR );
		wperror (L"pipe");
		success = false;
	}
	else if( fcntl( buffer_redirect->param1.pipe_fd[0],
					F_SETFL,
					O_NONBLOCK ) )
	{
		debug( 1, PIPE_ERROR );
		wperror( L"fcntl" );
		success = false;
	}
    
    if (! success)
    {
        delete buffer_redirect;
        buffer_redirect = NULL;
    }
    
	return buffer_redirect;
}

void io_buffer_destroy( io_data_t *io_buffer )
{

	/**
	   If this is an input buffer, then io_read_buffer will not have
	   been called, and we need to close the output fd as well.
	*/
	if( io_buffer->is_input )
	{
		exec_close(io_buffer->param1.pipe_fd[1] );
	}

	exec_close( io_buffer->param1.pipe_fd[0] );

	/*
	  Dont free fd for writing. This should already be free'd before
	  calling exec_read_io_buffer on the buffer
	*/
	delete io_buffer;
}

void io_remove(io_chain_t &list, const io_data_t *element)
{
    io_chain_t::iterator where = find(list.begin(), list.end(), element);
    if (where != list.end())
    {
        list.erase(where);
    }
}

io_chain_t io_duplicate(const io_chain_t &chain)
{
    io_chain_t result;
    result.reserve(chain.size());
    for (io_chain_t::const_iterator iter = chain.begin(); iter != chain.end(); iter++)
    {
        const io_data_t *io = *iter;
        result.push_back(new io_data_t(*io));
    }
    return result;
}

void io_duplicate_append( const io_chain_t &src, io_chain_t &dst )
{
    dst.reserve(dst.size() + src.size());
    for (io_chain_t::const_iterator iter = src.begin(); iter != src.end(); iter++) {
        const io_data_t *src_data = *iter;
        dst.push_back(new io_data_t(*src_data));
    }
}

void io_chain_destroy(io_chain_t &chain) 
{
    for (io_chain_t::iterator iter = chain.begin(); iter != chain.end(); iter++) {
        delete *iter;
    }
    chain.clear();
}

/* The old function returned the last match, so we mimic that. */
const io_data_t *io_chain_get(const io_chain_t &src, int fd) {
    for (io_chain_t::const_reverse_iterator iter = src.rbegin(); iter != src.rend(); iter++) {
        const io_data_t *data = *iter;
        if (data->fd == fd) {
            return data;
        }
    }
    return NULL;
}

io_data_t *io_chain_get(io_chain_t &src, int fd) {
    for (io_chain_t::reverse_iterator iter = src.rbegin(); iter != src.rend(); iter++) {
        io_data_t *data = *iter;
        if (data->fd == fd) {
            return data;
        }
    }
    return NULL;
}


void io_print( const io_chain_t &chain )
{
    
    for (io_chain_t::const_iterator iter = chain.begin(); iter != chain.end(); iter++) 
    {
        const io_data_t *io = *iter;
        debug( 1, L"IO fd %d, type ", io->fd );
        switch( io->io_mode )
        {
            case IO_PIPE:
                debug( 1, L"PIPE, data %d", io->param1.pipe_fd[io->fd?1:0] );
                break;
            
            case IO_FD:
                debug( 1, L"FD, copy %d", io->param1.old_fd );
                break;

            case IO_BUFFER:
                debug( 1, L"BUFFER" );
                break;
                
            default:
                debug( 1, L"OTHER" );
        }
    }
}
