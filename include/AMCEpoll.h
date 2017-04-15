/*******************************************************************************
	Copyright (C) 2017 by Andrew Chang <laplacezhang@126.com>
	Licensed under the LGPL v2.1, see the file COPYING in base directory.
	
	File name: 	AMCEpoll.h
	
	Description: 	
	    This file declares main interfaces of AMCEpoll tool.
			
	History:
		2017-04-09: File created as "AMCEpoll.h"

	------------------------------------------------------------------------

	    This library is free software; you can redistribute it and/or modify it 
	under the terms of the GNU Lesser General Public License as published by the 
	Free Software Foundation, version 2.1 of the License. 
	    This library is distributed in the hope that it will be useful, but WITHOUT
	ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
	FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
	details. 
	    You should have received a copy of the GNU Lesser General Public License 
	along with this library; if not, see <http://www.gnu.org/licenses/>.
		
********************************************************************************/

#ifndef __AMC_EPOLL_H__
#define __AMC_EPOLL_H__

/********/
/* headers */
#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <errno.h>

/********/
/* data types */
struct AMCEpoll;
struct AMCEpollEvent;

#ifndef NULL
#ifndef _DO_NOT_DEFINE_NULL
#define NULL	((void*)0)
#endif
#endif

#ifndef BOOL
#ifndef _DO_NOT_DEFINE_BOOL
#define BOOL	int
#define FALSE	0
#define TRUE	(!(FALSE))
#endif
#endif

/* for uint16_t "events" */
enum {
	EP_EVENT_READ    = (1 << 0),
	EP_EVENT_WRITE   = (1 << 1),
	EP_EVENT_ERROR   = (1 << 2),
	EP_EVENT_FREE    = (1 << 3),
	EP_EVENT_TIMEOUT = (1 << 4),

	EP_MODE_PERSIST  = (1 << 8),	/* only used when adding events */
	EP_MODE_EDGE     = (1 << 9),	/* only used when adding events */
};

/* callback */
typedef void (*ev_callback)(int fd, uint16_t events, void *arg);

/********/
/* functions */
struct AMCEpoll *
	AMCEpoll_New(size_t buffSize);
int
	AMCEpoll_Free(struct AMCEpoll *obj);
int 
	AMCEpoll_AddEvent(struct AMCEpoll *obj, int fd, uint16_t events, int timeout, ev_callback callback, void *userData, struct AMCEpollEvent **eventOut);
int 
	AMCEpoll_DelEvent(struct AMCEpoll *obj, struct AMCEpollEvent *event);
int 
	AMCEpoll_DelEventByFd(struct AMCEpoll *obj, int fd);
int 
	AMCEpoll_GetFdByEvent(struct AMCEpollEvent *event);
int 
	AMCEpoll_Dispatch(struct AMCEpoll *obj);
int 
	AMCEpoll_LoopExit(struct AMCEpoll *obj);
int 
	AMCFd_MakeNonBlock(int fd);
int 
	AMCFd_MakeCloseOnExec(int fd);
ssize_t 
	AMCFd_Read(int fd, void *buff, size_t nbyte);


#endif
/* EOF */

