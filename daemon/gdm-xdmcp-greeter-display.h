/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 */

#ifndef __GDM_XDMCP_GREETER_DISPLAY_H
#define __GDM_XDMCP_GREETER_DISPLAY_H

#include <sys/types.h>
#include <sys/socket.h>
#include <glib-object.h>

#include "gdm-xdmcp-display.h"
#include "gdm-address.h"

G_BEGIN_DECLS

#define GDM_TYPE_XDMCP_GREETER_DISPLAY         (gdm_xdmcp_greeter_display_get_type ())
#define GDM_XDMCP_GREETER_DISPLAY(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GDM_TYPE_XDMCP_GREETER_DISPLAY, GdmXdmcpGreeterDisplay))
#define GDM_XDMCP_GREETER_DISPLAY_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), GDM_TYPE_XDMCP_GREETER_DISPLAY, GdmXdmcpGreeterDisplayClass))
#define GDM_IS_XDMCP_GREETER_DISPLAY(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GDM_TYPE_XDMCP_GREETER_DISPLAY))
#define GDM_IS_XDMCP_GREETER_DISPLAY_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GDM_TYPE_XDMCP_GREETER_DISPLAY))
#define GDM_XDMCP_GREETER_DISPLAY_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GDM_TYPE_XDMCP_GREETER_DISPLAY, GdmXdmcpGreeterDisplayClass))

typedef struct GdmXdmcpGreeterDisplayPrivate GdmXdmcpGreeterDisplayPrivate;

typedef struct
{
        GdmXdmcpDisplay                parent;
        GdmXdmcpGreeterDisplayPrivate *priv;
} GdmXdmcpGreeterDisplay;

typedef struct
{
        GdmXdmcpDisplayClass           parent_class;

} GdmXdmcpGreeterDisplayClass;

GType                     gdm_xdmcp_greeter_display_get_type         (void);


GdmDisplay *              gdm_xdmcp_greeter_display_new              (const char              *hostname,
                                                                      int                      number,
                                                                      GdmAddress              *addr,
                                                                      gint32                   serial_number);

G_END_DECLS

#endif /* __GDM_XDMCP_GREETER_DISPLAY_H */
