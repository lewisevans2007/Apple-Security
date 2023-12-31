/*
 * Copyright (c) 2006,2011,2014 Apple Inc. All Rights Reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

/*      $NetBSD: xdr_mem.c,v 1.15 2000/01/22 22:19:18 mycroft Exp $     */

/*
 * Sun RPC is a product of Sun Microsystems, Inc. and is provided for
 * unrestricted use provided that this legend is included on all tape
 * media and as a part of the software program in whole or part.  Users
 * may copy or modify Sun RPC without charge, but are not authorized
 * to license or distribute it to anyone else except as part of a product or
 * program developed by the user.
 *
 * SUN RPC IS PROVIDED AS IS WITH NO WARRANTIES OF ANY KIND INCLUDING THE
 * WARRANTIES OF DESIGN, MERCHANTIBILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE, OR ARISING FROM A COURSE OF DEALING, USAGE OR TRADE PRACTICE.
 *
 * Sun RPC is provided with no support and without any obligation on the
 * part of Sun Microsystems, Inc. to assist in its use, correction,
 * modification or enhancement.
 *
 * SUN MICROSYSTEMS, INC. SHALL HAVE NO LIABILITY WITH RESPECT TO THE
 * INFRINGEMENT OF COPYRIGHTS, TRADE SECRETS OR ANY PATENTS BY SUN RPC
 * OR ANY PART THEREOF.
 *
 * In no event will Sun Microsystems, Inc. be liable for any lost revenue
 * or profits or other special, indirect and consequential damages, even if
 * Sun has been advised of the possibility of such damages.
 *
 * Sun Microsystems, Inc.
 * 2550 Garcia Avenue
 * Mountain View, California  94043
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char *sccsid = "@(#)xdr_mem.c 1.19 87/08/11 Copyr 1984 Sun Micro";
static char *sccsid = "@(#)xdr_mem.c    2.1 88/07/29 4.0 RPCSRC";
#endif
#include <sys/cdefs.h>

/*
 * xdr_mem.h, XDR implementation using memory buffers.
 *
 * Copyright (C) 1984, Sun Microsystems, Inc.
 *
 * If you have some data to be interpreted as external data representation
 * or to be converted to external data representation in a memory buffer,
 * then this is the package for you.
 *
 */

#include <sys/types.h>

#include <netinet/in.h>

#include <string.h>

#include "sec_xdr.h"

static void sec_xdrmem_destroy(XDR *);

#ifdef __LP64__
#define long_callback_ptr_t int *
#else
#define long_callback_ptr_t long *
#endif
static bool_t sec_xdrmem_getlong_aligned(XDR *, long_callback_ptr_t);
static bool_t sec_xdrmem_putlong_aligned(XDR *, const long_callback_ptr_t);
static bool_t sec_xdrmem_getlong_unaligned(XDR *, long_callback_ptr_t);
static bool_t sec_xdrmem_putlong_unaligned(XDR *, const long_callback_ptr_t);

static bool_t sec_xdrmem_getbytes(XDR *, char *, u_int);
static bool_t sec_xdrmem_putbytes(XDR *, const char *, u_int);
/* XXX: w/64-bit pointers, u_int not enough! */
static u_int sec_xdrmem_getpos(XDR *);
static bool_t sec_xdrmem_setpos(XDR *, u_int);
static int32_t *sec_xdrmem_inline_aligned(XDR *, u_int);
static int32_t *sec_xdrmem_inline_unaligned(XDR *, u_int);

static const struct     xdr_ops sec_xdrmem_ops_aligned = {
    sec_xdrmem_getlong_aligned,
    sec_xdrmem_putlong_aligned,
    sec_xdrmem_getbytes,
    sec_xdrmem_putbytes,
    sec_xdrmem_getpos,
    sec_xdrmem_setpos,
    sec_xdrmem_inline_aligned,
    sec_xdrmem_destroy
};

static const struct     xdr_ops sec_xdrmem_ops_unaligned = {
    sec_xdrmem_getlong_unaligned,
    sec_xdrmem_putlong_unaligned,
    sec_xdrmem_getbytes,
    sec_xdrmem_putbytes,
    sec_xdrmem_getpos,
    sec_xdrmem_setpos,
    sec_xdrmem_inline_unaligned,
    sec_xdrmem_destroy
};

/*
 * The procedure sec_xdrmem_create initializes a stream descriptor for a
 * memory buffer.
 */
void
sec_xdrmem_create(XDR *xdrs, char *addr, u_int size, enum xdr_op op)
{
    xdrs->x_op = op;
    xdrs->x_ops = ((unsigned long)addr & (sizeof(int32_t) - 1))
        ? &sec_xdrmem_ops_unaligned : &sec_xdrmem_ops_aligned;
    xdrs->x_private = xdrs->x_base = addr;
    xdrs->x_public = NULL;
    xdrs->x_handy = size;
}

/*ARGSUSED*/
static void
sec_xdrmem_destroy(XDR *xdrs)
{

}

static bool_t
sec_xdrmem_getlong_aligned(XDR *xdrs, long_callback_ptr_t lp)
{
    if (xdrs->x_handy < sizeof(int32_t))
        return (FALSE);
    xdrs->x_handy -= sizeof(int32_t);
    if (lp) *lp = ntohl(*(u_int32_t *)xdrs->x_private);
    xdrs->x_private = (char *)xdrs->x_private + sizeof(int32_t);
    return (TRUE);
}

static bool_t
sec_xdrmem_putlong_aligned(XDR *xdrs, const long_callback_ptr_t lp)
{
    if (xdrs->x_handy < sizeof(int32_t))
        return (FALSE);
    xdrs->x_handy -= sizeof(int32_t);
    if (lp) *(u_int32_t *)xdrs->x_private = htonl((u_int32_t)*lp);
    xdrs->x_private = (char *)xdrs->x_private + sizeof(int32_t);
    return (TRUE);
}

static bool_t
sec_xdrmem_getlong_unaligned(XDR *xdrs, long_callback_ptr_t lp)
{
    u_int32_t l;

    if (xdrs->x_handy < sizeof(int32_t))
        return (FALSE);
    xdrs->x_handy -= sizeof(int32_t);
    memmove(&l, xdrs->x_private, sizeof(int32_t));
    if (lp) *lp = ntohl(l);
    xdrs->x_private = (char *)xdrs->x_private + sizeof(int32_t);
    return (TRUE);
}

static bool_t
sec_xdrmem_putlong_unaligned(XDR *xdrs, const long_callback_ptr_t lp)
{
    u_int32_t l = 0;

    if (xdrs->x_handy < sizeof(int32_t))
        return (FALSE);
    xdrs->x_handy -= sizeof(int32_t);
    if (lp) l = htonl((u_int32_t)*lp);
    memmove(xdrs->x_private, &l, sizeof(int32_t));
    xdrs->x_private = (char *)xdrs->x_private + sizeof(int32_t);
    return (TRUE);
}

static bool_t
sec_xdrmem_getbytes(XDR *xdrs, char *addr, u_int len)
{
    if (xdrs->x_handy < len)
        return (FALSE);
    xdrs->x_handy -= len;
    if (addr) memmove(addr, xdrs->x_private, len);
    xdrs->x_private = (char *)xdrs->x_private + len;
    return (TRUE);
}

static bool_t
sec_xdrmem_putbytes(XDR *xdrs, const char *addr, u_int len)
{
    if (xdrs->x_handy < len)
        return (FALSE);
    xdrs->x_handy -= len;
    if (addr) memmove(xdrs->x_private, addr, len);
    xdrs->x_private = (char *)xdrs->x_private + len;
    return (TRUE);
}

static u_int
sec_xdrmem_getpos(XDR *xdrs)
{
    /* XXX w/64-bit pointers, u_int not enough! */
    return (u_int)((u_long)xdrs->x_private - (u_long)xdrs->x_base);
}

static bool_t
sec_xdrmem_setpos(XDR *xdrs, u_int pos)
{
    char *newaddr = xdrs->x_base + pos;
    char *lastaddr = (char *)xdrs->x_private + xdrs->x_handy;

    if (newaddr > lastaddr)
        return (FALSE);
    xdrs->x_private = newaddr;
    xdrs->x_handy = (u_int)(lastaddr - newaddr); /* XXX sizeof(u_int) <? sizeof(ptrdiff_t) */
    return (TRUE);
}

static int32_t *
sec_xdrmem_inline_aligned(XDR *xdrs, u_int len)
{
    int32_t *buf = 0;

    if (xdrs->x_handy >= len) {
        xdrs->x_handy -= len;
        buf = (int32_t *)xdrs->x_private;
        xdrs->x_private = (char *)xdrs->x_private + len;
    }
    return (buf);
}

/* ARGSUSED */
static int32_t *
sec_xdrmem_inline_unaligned(XDR *xdrs, u_int len)
{
    return (0);
}

/**
 * This is almost a straight copy of the standard implementation, except
 * that all calls made that move memory don't do so if passed a NULL dest.
 * getbytes in particular when called from xdr_bytes during sizing of a decode
 * needs this.
 */
