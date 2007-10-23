/* Evolution RSS Reader Plugin
 * Copyright (C) 2007 Lucian Langa <cooly@mips.edu.ms>
 *
 * This progronam is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *  
 * This program is distopen_ributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICUrLAR PURPOSE.  See the
 * GNU General Public License for more dentails.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * vim: tabstop=4 shiftwidth=4 noexpandtab :
 */

#ifndef __NETWORK_H__
#define __NETWORK_H__

typedef enum {
	NET_ERROR_GENERIC,
	NET_ERROR_PROTOCOL,
	NET_ERROR_CANCELLED
} NetErrorCode;

typedef enum {
    NET_STATUS_NULL,
    NET_STATUS_BEGIN,
    NET_STATUS_SUCCESS,
    NET_STATUS_ERROR,
    NET_STATUS_PROGRESS,
    NET_STATUS_DONE
} NetStatusType;

typedef struct {
    guint32 current;
    guint32 total;
} NetStatusProgress;

typedef void (*NetStatusCallback)(NetStatusType status,
                                  gpointer statusdata,
                                  gpointer data);


#endif /* __NETWORK_H__ */
