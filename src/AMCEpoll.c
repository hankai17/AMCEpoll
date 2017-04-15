/*******************************************************************************
	Copyright (C) 2017 by Andrew Chang <laplacezhang@126.com>
	Licensed under the LGPL v2.1, see the file COPYING in base directory.
	
	File name: 	AMCEpoll.c
	
	Description: 	
	    This file contains mail logic of AMCEpoll.h.
			
	History:
		2017-04-08: File created as "AMCEpoll.c"

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

/********/
#define __HEADERS
#ifdef __HEADERS

#include "utilLog.h"
#include "AMCEpoll.h"
#include "cAssocArray.h"

#include <sys/epoll.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdint.h>

#endif


/********/
#define __DATA_DEFINITIONS
#ifdef __DATA_DEFINITIONS

/* for uint32_t "base_status" in AMCEpoll */
enum {
	EP_STAT_SHOULD_EXIT = (1 << 0),
	EP_STAT_EPOLL_ERROR = (1 << 1),
	/* for future use */
};

#define MAX_FD_STR_LEN		(16)
#define EP_EVENT_ALL_MASK	(\
	EP_MODE_PERSIST | EP_MODE_EDGE | EP_EVENT_READ | \
	EP_EVENT_WRITE | EP_EVENT_ERROR | EP_EVENT_FREE | \
	EP_EVENT_TIMEOUT\
	)
typedef struct epoll_event epoll_event_st;

struct AMCEpollEvent {
	int            fd;
	ev_callback    callback;
	void          *user_data;
	uint16_t       events;
};


struct AMCEpoll {
	int             epoll_fd;
	uint32_t        base_status;
	cAssocArray    *all_events;
	size_t          epoll_buff_size;
	epoll_event_st  epoll_buff[0];
};

#define _RETURN_ERRNO()	\
	do{\
		int err = errno;\
		if ((err) > 0) {\
			return (0 - (err));\
		} else {\
			return -1;\
		}\
	}while(0)

#define _RETURN_ERR(err)	\
	do{\
		if (err > 0) {\
			errno = err;\
			return (0 - err);\
		} else if (err < 0) {\
			errno = 0 - err;\
			return err;\
		} else {\
			return -1;\
		}\
	}while(0)


#endif

/********/
#define __EPOLL_OPERATIONS
#ifdef __EPOLL_OPERATIONS

/* --------------------_epoll_code_from_amc_code----------------------- */
static int _epoll_code_from_amc_code(uint16_t amcEv)
{
	int ret = 0;
	if (0 == (amcEv & EP_MODE_PERSIST)) {
		ret |= EPOLLONESHOT;
	}
	if (amcEv & EP_MODE_EDGE) {
		ret |= EPOLLET;
	}
	if (amcEv & EP_EVENT_READ) {
		ret |= EPOLLIN | EPOLLPRI;
	}
	if (amcEv & EP_EVENT_WRITE) {
		ret |= EPOLLOUT;
	}
	if (amcEv & EP_EVENT_ERROR) {
		ret |= EPOLLERR | EPOLLHUP;
	}
#if 0
	if (amcEv & EP_EVENT_FREE) {
		/* nothing */
	}
	if (amcEv & EP_EVENT_TIMEOUT) {
		/* nothing */
	}
#endif
	return ret;
}


/* --------------------_epoll_add----------------------- */
static int _epoll_add(struct AMCEpoll *base, struct AMCEpollEvent *amcEvent)
{
	int callStat = 0;
	struct epoll_event epollEvent;
	
	epollEvent.events = _epoll_code_from_amc_code(amcEvent->events);
	epollEvent.data.ptr = amcEvent;

	callStat = epoll_ctl(base->epoll_fd, EPOLL_CTL_ADD, amcEvent->fd, &epollEvent);
	if (0 == callStat) {
		return 0;
	} else {
		int err = errno;
		ERROR("Failed in epoll_add(): %s", strerror(err));
		_RETURN_ERR(err);
	}
}


/* --------------------_epoll_del----------------------- */
static int _epoll_del(struct AMCEpoll *base, struct AMCEpollEvent *amcEvent)
{
	int callStat = epoll_ctl(base->epoll_fd, EPOLL_CTL_DEL, amcEvent->fd, NULL);
	if (0 == callStat) {
		return 0;
	} else {
		int err = errno;
		ERROR("Failed in epoll_del(): %s", strerror(err));
		_RETURN_ERR(err);
	}
}


/* --------------------_epoll_mod----------------------- */
static int _epoll_mod(struct AMCEpoll *base, struct AMCEpollEvent *amcEvent)
{
	int callStat = 0;
	struct epoll_event epollEvent;

	epollEvent.events = _epoll_code_from_amc_code(amcEvent->events);
	epollEvent.data.ptr = amcEvent;

	callStat = epoll_ctl(base->epoll_fd, EPOLL_CTL_MOD, amcEvent->fd, &epollEvent);
	if (0 == callStat) {
		return 0;
	} else {
		int err = errno;
		ERROR("Failed in epoll_mod(): %s", strerror(err));
		_RETURN_ERR(err);
	}
}


#endif


/********/
#define __CALLBACK_INVOKES
#ifdef __CALLBACK_INVOKES

/* --------------------_invoke_callback----------------------- */
static inline void _invoke_callback(struct AMCEpollEvent *evObj, uint16_t evType)
{
	(evObj->callback)(evObj->fd, evType, evObj->user_data);
	return;
}


/* --------------------_invoke_callback_if_necessary----------------------- */
static void _invoke_callback_if_necessary(struct AMCEpollEvent *evObj, const int epoll_events)
{
	uint16_t amcEvents = 0;

	if (epoll_events & EPOLLERR) {
		DEBUG("Fd %d got error: EPOLLERR (0x%x)", evObj->fd, EPOLLERR);
		amcEvents |= EP_EVENT_ERROR;
	}
	if (epoll_events & EPOLLHUP) {
		DEBUG("Fd %d got error: EPOLLHUP (0x%x)", evObj->fd, EPOLLHUP);
		amcEvents |= EP_EVENT_ERROR;
	}
	if ((epoll_events & EPOLLIN) || (epoll_events & EPOLLPRI)) {
		DEBUG("Fd %d can read", evObj->fd);
		amcEvents |= EP_EVENT_READ;
	}
	if (epoll_events & EPOLLOUT) {
		DEBUG("Fd %d can write", evObj->fd);
		amcEvents |= EP_EVENT_WRITE;
	}

	if (0 == amcEvents) {
		DEBUG("Fd %d got no events", evObj->fd);
		// nothing to do
	}
	else if (amcEvents != (evObj->events & ~amcEvents)) {
		DEBUG("Invoke event 0x%p", evObj);
		_invoke_callback(evObj, amcEvents);
	}
	else {
		// event not observed
	}

	return;
}

#endif


/********/
#define __EVENT_OPERATIONS
#ifdef __EVENT_OPERATIONS

/* --------------------_get_event_for_fd----------------------- */
static struct AMCEpollEvent *_get_event_for_fd(struct AMCEpoll *base, int fd)
{
	struct AMCEpollEvent *ret = NULL;
	char fdStr[MAX_FD_STR_LEN];

	snprintf(fdStr, sizeof(fdStr), "%d", fd);
	ret = (struct AMCEpollEvent *)cAssocArray_GetValue(base->all_events, fdStr);
	return ret;
}


/* --------------------_free_event_for_fd----------------------- */
static int _free_event_for_fd(struct AMCEpoll *base, int fd)
{
	struct AMCEpollEvent *event = NULL;
	char fdStr[MAX_FD_STR_LEN];
	snprintf(fdStr, sizeof(fdStr), "%d", fd);
	int callStat;
	int epollErr = 0;

	event = cAssocArray_GetValue(base->all_events, fdStr);
	if (NULL == event) {
		NOTICE("Fd %d was not added before.", fd);
		_RETURN_ERR(ENOENT);
	}

	DEBUG("Now delete %s in 0x%p", fdStr, event);
	cAssocArray_DumpToStdout(base->all_events);
	epollErr = _epoll_del(base, event);
	
	if (event->events & EP_EVENT_FREE) {
		_invoke_callback(event, EP_EVENT_FREE);
	}

	callStat = cAssocArray_RemoveValue(base->all_events, fdStr, TRUE);
	cAssocArray_DumpToStdout(base->all_events);
	if (epollErr) {
		_RETURN_ERR(epollErr);
	} else if (callStat < 0) {
		int err = errno;
		ERROR("Failed in removing event: %s", strerror(err));
		_RETURN_ERR(err);
	} else {
		return 0;
	}
}


/* --------------------_free_event----------------------- */
static int _free_event(struct AMCEpoll *base, struct AMCEpollEvent *event)
{
	struct AMCEpollEvent *eventDummy = _get_event_for_fd(base, event->fd);

	if (eventDummy != event) {
		NOTICE("Cannot find event %p", event);
		_RETURN_ERR(ENOENT);
	}

	return _free_event_for_fd(base, event->fd);
}


/* --------------------_new_event----------------------- */
static struct AMCEpollEvent *_new_event(int fd, ev_callback callback, void *userData, uint16_t events)
{
	struct AMCEpollEvent *ret = malloc(sizeof(*ret));
	if (ret) {
		ret->fd = fd;
		ret->callback = callback;
		ret->user_data = userData;
		ret->events = events & EP_EVENT_ALL_MASK;
	}
	else {
		ERROR("Failed to alloc new event: %s", strerror(errno));
	}
	return ret;
}


/* --------------------_new_event----------------------- */
static int _add_event(struct AMCEpoll *base, struct AMCEpollEvent *event)
{
	int callStat = 0;
	char fdStr[MAX_FD_STR_LEN];
	snprintf(fdStr, sizeof(fdStr), "%d", event->fd);
	DEBUG("Add New event %s 0x%p", fdStr, event);

	callStat = cAssocArray_AddValue(base->all_events, fdStr, event);
	if (callStat < 0) {
		int err = errno;
		ERROR("Failed to add new event: %s", strerror(err));
		_RETURN_ERR(err);
	}

	callStat = _epoll_add(base, event);
	if (0 == callStat) {
		return 0;
	}
	else {
		int err = errno;
		cAssocArray_RemoveValue(base->all_events, fdStr, FALSE);
		_RETURN_ERR(err);
	}
}


/* --------------------_mod_event----------------------- */
static int _mod_event(struct AMCEpoll *base, struct AMCEpollEvent *evObj, ev_callback callback, void *userData, uint16_t events)
{
	evObj->callback = callback;
	evObj->user_data = userData;

	if (evObj->events != (events & EP_EVENT_ALL_MASK))
	{
		evObj->events = events & EP_EVENT_ALL_MASK;
		return _epoll_mod(base, evObj);
	}
	else {
		return 0;
	}
}

#endif

/********/
#define __AMC_EPOLL_MAIN_LOOP
#ifdef __AMC_EPOLL_MAIN_LOOP

/* --------------------_dispatch_main_loop----------------------- */
static int _dispatch_main_loop(struct AMCEpoll *base)
{
	struct epoll_event *evBuff = base->epoll_buff;
	int evFd = base->epoll_fd;
	int evSize = base->epoll_buff_size;
	int nTotal = 0;
	int nIndex = 0;
	int errCpy = 0;
	BOOL shouldExit = FALSE;

	base->base_status = 0;

	/* This is actually a thread-like process */
	do {
		nTotal = epoll_wait(evFd, evBuff, evSize, 1000);		// TODO: implement timeout
		if (nTotal < 0) {
			errCpy = errno;
			ERROR("Failed in epoll_wait(): %s", strerror(errCpy));
			base->base_status |= EP_STAT_EPOLL_ERROR;
			shouldExit = TRUE;
		}
		else if (0 == nTotal) {
			// TODO: Add support
			DEBUG("Enjoy your peace...");
		}
		else {
			DEBUG("%d event(s) active", nTotal);
			int epoll_events = 0;
			struct AMCEpollEvent *amcEvent = NULL;
			for (nIndex = 0; nIndex < nTotal; nIndex ++)
			{
				epoll_events = evBuff[nIndex].events;
				amcEvent = (struct AMCEpollEvent *)(evBuff[nIndex].data.ptr);

				if (amcEvent)
				{
					_invoke_callback_if_necessary(amcEvent, epoll_events);

					if (0 == (amcEvent->events & EP_MODE_PERSIST)) {
						_free_event(base, amcEvent);
						amcEvent = NULL;
					}
				}
			}
		}
		// end of "else (nTotal < 0) {..."

	} while (FALSE == shouldExit);
	// end of "do - while (FALSE == shouldExit)"

	/* clean status */
	base->base_status &= ~EP_STAT_SHOULD_EXIT;

	/* return */
	if (base->base_status & EP_STAT_EPOLL_ERROR) {
		_RETURN_ERR(errCpy);
	}
	else {
		return 0;
	}
}


#endif


/********/
#define __PUBLIC_FUNCTIONS
#ifdef __PUBLIC_FUNCTIONS

/* --------------------AMCEpoll_New----------------------- */
struct AMCEpoll *AMCEpoll_New(size_t buffSize)
{
	if (buffSize <= 0) {
		ERROR("Invalid size %d", buffSize);
		errno = EINVAL;
		return NULL;
	}
	else {
		struct AMCEpoll *ret;
		size_t objLen = sizeof(*ret) + sizeof(epoll_event_st) * buffSize;
		BOOL isOK = TRUE;

		/* malloc */
		ret = malloc(objLen);
		if (NULL == ret) {
			isOK = FALSE;
		} else {
			memset(ret, 0, objLen);
			ret->epoll_buff_size = buffSize;
		}

		/* epoll_create */
		if (isOK) {
			ret->epoll_fd = epoll_create(buffSize);
			if (ret->epoll_fd < 0) {
				isOK = FALSE;
			}
		}

		/* aAssocArray */
		if (isOK) {
			ret->all_events = cAssocArray_Create(FALSE);
			if (NULL == ret->all_events) {
				isOK = FALSE;
			}
		}

		/* return */
		if (FALSE == isOK) {
			if (ret) {
				AMCEpoll_Free(ret);
				ret = NULL;
			}
		}
		return ret;
	}
}


/* --------------------AMCEpoll_Free----------------------- */
int AMCEpoll_Free(struct AMCEpoll *obj)
{
	if (NULL == obj) {
		ERROR("Nil parameter");
		_RETURN_ERR(EINVAL);
	}
	else
	{
		// TODO: free with cAssocArray callback
		if (obj->all_events) {
			cArrayKeys *allKeys = cAssocArray_GetKeys(obj->all_events);
			cArrayKeys *eachKey = allKeys;
			while (eachKey)
			{
				int fd = strtol(eachKey->key, NULL, 10);
				_free_event_for_fd(obj, fd);
				eachKey = eachKey->next;
			}
			cAssocArray_Delete(obj->all_events, TRUE);
			obj->all_events = NULL;

			if (allKeys) {
				cArrayKeys_Free(allKeys);
				allKeys = NULL;
			}
		}

		if (obj->epoll_fd > 0) {
			close(obj->epoll_fd);
			obj->epoll_fd = -1;
		}

		free(obj);
		obj = NULL;

		return 0;
	}
}


/* --------------------AMCEpoll_AddEvent----------------------- */
int AMCEpoll_AddEvent(struct AMCEpoll *obj, int fd, uint16_t events, int timeout, ev_callback callback, void *userData, struct AMCEpollEvent **eventOut)
{
	int ret = 0;
	struct AMCEpollEvent *anEvent = NULL;

	if (NULL == obj) {
		ERROR("Nil parameter");
		ret = EINVAL;
		goto ERR_END;
	}
	else if (fd < 0) {
		ERROR("Negative file descriptor");
		ret = EINVAL;
		goto ERR_END;
	}
	else if (0 == (events & (EP_EVENT_READ | EP_EVENT_WRITE))) {
		ERROR("Invalid event types: 0x%x", events);
		ret = EINVAL;
		goto ERR_END;
	}

	/* para check OK */
	anEvent = _get_event_for_fd(obj, fd);
	if (NULL == anEvent)
	{
		/* This is an new event */
		anEvent = _new_event(fd, callback, userData, events);
		if (NULL == anEvent) {
			ret = errno;
			goto ERR_END;
		}
		else {
			int ret = _add_event(obj, anEvent);
			if (0 == ret) {
				if (eventOut) {
					*eventOut = anEvent;
				}
				return 0;
			}
			else {
				ret = errno;
				free(anEvent);
				anEvent = NULL;
				goto ERR_END;
			}
		}
		// end of "else (NULL == anEvent) {..."
	}
	else
	{
		/* This is an existed event */
		ret = _mod_event(obj, anEvent, callback, userData, events);
		if (ret < 0) {
			goto ERR_END;
		}
		else {
			if (eventOut) {
				*eventOut = anEvent;
			}
			return 0;
		}
	}
	// end of: "else (NULL == anEvent) {..."


ERR_END:
	if (eventOut) {
		*eventOut = NULL;
	}
	_RETURN_ERR(ret);
}


/* --------------------AMCEpoll_DelEvent----------------------- */
int AMCEpoll_DelEvent(struct AMCEpoll *obj, struct AMCEpollEvent *event)
{
	if (NULL == obj) {
		ERROR("Nil parameter");
		_RETURN_ERR(EINVAL);
	}
	else if (NULL == event) {
		ERROR("Nil parameter");
		_RETURN_ERR(EINVAL);
	}
	else {
		return _free_event(obj, event);
	}
}


/* --------------------AMCEpoll_DelEventByFd----------------------- */
int AMCEpoll_DelEventByFd(struct AMCEpoll *obj, int fd)
{
	if (NULL == obj) {
		ERROR("Nil parameter");
		_RETURN_ERR(EINVAL);
	}
	else if (fd < 0) {
		ERROR("Negative file descriptor");
		_RETURN_ERR(EINVAL);
	}
	else {
		return _free_event_for_fd(obj, fd);
	}
}


/* --------------------AMCEpoll_GetFdByEvent----------------------- */
int AMCEpoll_GetFdByEvent(struct AMCEpollEvent *event)
{
	if (NULL == event) {
		ERROR("Nil parameter");
		_RETURN_ERR(EINVAL);
	}
	else {
		return event->fd;
	}
}


/* --------------------AMCEpoll_DelEvent----------------------- */
int AMCEpoll_Dispatch(struct AMCEpoll *obj)
{
	if (NULL == obj) {
		ERROR("Nil parameter");
		_RETURN_ERR(EINVAL);
	} 
	else if (0 == cAssocArray_Size(obj->all_events)) {
		return 0;
	}
	else {
		return _dispatch_main_loop(obj);
	}
}


/* --------------------AMCEpoll_LoopExit----------------------- */
int AMCEpoll_LoopExit(struct AMCEpoll *obj)
{
	if (NULL == obj) {
		ERROR("Nil parameter");
		_RETURN_ERR(EINVAL);
	} else {
		obj->base_status |= EP_STAT_SHOULD_EXIT;
		return 0;
	}
}


/* --------------------AMCFd_MakeNonBlock----------------------- */
int AMCFd_MakeNonBlock(int fd)
{
	if (fd < 0) {
		ERROR("Invalid file descriptor");
		_RETURN_ERR(EINVAL);
	}
	else {
		int flags = fcntl(fd, F_GETFL, NULL);
		flags = fcntl(fd, F_SETFL, (flags | O_NONBLOCK));
		if (0 == flags) {
			return 0;
		} else {
			int err = errno;
			ERROR("Failed to set O_NONBLOCK for fd %d: %s", fd, strerror(err));
			_RETURN_ERR(err);
		}
	}
}


/* --------------------AMCFd_MakeCloseOnExec----------------------- */
int AMCFd_MakeCloseOnExec(int fd)
{
	if (fd < 0) {
		ERROR("Invalid file descriptor");
		_RETURN_ERR(EINVAL);
	}
	else {
		int flags = fcntl(fd, F_GETFD, NULL);
		flags = fcntl(fd, F_SETFD, (flags | FD_CLOEXEC));
		if (0 == flags) {
			return 0;
		} else {
			int err = errno;
			ERROR("Failed to set FD_CLOEXEC for fd %d: %s", fd, strerror(err));
			_RETURN_ERR(err);
		}
	}
}


/* --------------------AMCFd_Read----------------------- */
ssize_t AMCFd_Read(int fd, void *rawBuf, size_t nbyte)
{
	int err = 0;
	int callStat = 0;
	ssize_t ret = 0;
	uint8_t *buff = (uint8_t *)rawBuf;
	BOOL isDone = FALSE;

	if (fd < 0) {
		_RETURN_ERR(EBADF);
	}
	if (NULL == buff) {
		_RETURN_ERR(EINVAL);
	}
	if (0 == nbyte) {
		return 0;
	}

	/* loop read */
	while (FALSE == isDone)
	{
		callStat = read(fd, buff + ret, nbyte - ret);
		err = errno;

		if (0 == callStat) {
			/* EOF */
			ret = 0;
			isDone = TRUE;
		}
		else if (callStat < 0)
		{
			if (EINTR == err) {
				DEBUG("Fd %d EINTR", fd);
				isDone = TRUE;
			}
			else if (EAGAIN == err) {
				DEBUG("Fd %d EAGAIN", fd);
				isDone = TRUE;
			}
			else {
				ret = -1;
				isDone = TRUE;
			}
		}
		else
		{
			if (ret < nbyte) {
				ret += callStat;
			}
			else {
				isDone = TRUE;
			}
		}
	}
	// end of "while (FALSE == isDone)"

	return ret;
}



#endif


/* EOF */

