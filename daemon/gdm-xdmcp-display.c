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

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib-object.h>

#include "gdm-display.h"
#include "gdm-xdmcp-display.h"

#include "gdm-common.h"
#include "gdm-address.h"

#define GDM_XDMCP_DISPLAY_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GDM_TYPE_XDMCP_DISPLAY, GdmXdmcpDisplayPrivate))

struct GdmXdmcpDisplayPrivate
{
        GdmAddress             *remote_address;
        gint32                  session_number;
};

enum {
        PROP_0,
        PROP_REMOTE_ADDRESS,
        PROP_SESSION_NUMBER,
};

static void     gdm_xdmcp_display_class_init    (GdmXdmcpDisplayClass *klass);
static void     gdm_xdmcp_display_init          (GdmXdmcpDisplay      *xdmcp_display);
static void     gdm_xdmcp_display_finalize      (GObject              *object);

G_DEFINE_ABSTRACT_TYPE (GdmXdmcpDisplay, gdm_xdmcp_display, GDM_TYPE_DISPLAY)

gint32
gdm_xdmcp_display_get_session_number (GdmXdmcpDisplay *display)
{
        g_return_val_if_fail (GDM_IS_XDMCP_DISPLAY (display), 0);

        return display->priv->session_number;
}

GdmAddress *
gdm_xdmcp_display_get_remote_address (GdmXdmcpDisplay *display)
{
        g_return_val_if_fail (GDM_IS_XDMCP_DISPLAY (display), NULL);

        return display->priv->remote_address;
}

static gboolean
gdm_xdmcp_display_create_authority (GdmDisplay *display)
{
        g_return_val_if_fail (GDM_IS_DISPLAY (display), FALSE);

        return GDM_DISPLAY_CLASS (gdm_xdmcp_display_parent_class)->create_authority (display);
}

static gboolean
gdm_xdmcp_display_add_user_authorization (GdmDisplay *display,
                                          const char *username,
                                          char      **filename,
                                          GError    **error)
{
        return GDM_DISPLAY_CLASS (gdm_xdmcp_display_parent_class)->add_user_authorization (display, username, filename, error);
}

static gboolean
gdm_xdmcp_display_remove_user_authorization (GdmDisplay *display,
                                             const char *username,
                                             GError    **error)
{
        return GDM_DISPLAY_CLASS (gdm_xdmcp_display_parent_class)->remove_user_authorization (display, username, error);
}

static gboolean
gdm_xdmcp_display_manage (GdmDisplay *display)
{
        g_return_val_if_fail (GDM_IS_DISPLAY (display), FALSE);

        GDM_DISPLAY_CLASS (gdm_xdmcp_display_parent_class)->manage (display);

        return TRUE;
}

static gboolean
gdm_xdmcp_display_unmanage (GdmDisplay *display)
{
        g_return_val_if_fail (GDM_IS_DISPLAY (display), FALSE);

        GDM_DISPLAY_CLASS (gdm_xdmcp_display_parent_class)->unmanage (display);
        return TRUE;
}

static void
_gdm_xdmcp_display_set_remote_address (GdmXdmcpDisplay *display,
                                       GdmAddress      *address)
{
        if (display->priv->remote_address != NULL) {
                gdm_address_free (display->priv->remote_address);
        }

        g_assert (address != NULL);

        gdm_address_debug (address);
        display->priv->remote_address = gdm_address_copy (address);
}

static void
gdm_xdmcp_display_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
        GdmXdmcpDisplay *self;

        self = GDM_XDMCP_DISPLAY (object);

        switch (prop_id) {
        case PROP_REMOTE_ADDRESS:
                _gdm_xdmcp_display_set_remote_address (self, g_value_get_boxed (value));
                break;
        case PROP_SESSION_NUMBER:
                self->priv->session_number = g_value_get_int (value);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gdm_xdmcp_display_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
        GdmXdmcpDisplay *self;

        self = GDM_XDMCP_DISPLAY (object);

        switch (prop_id) {
        case PROP_REMOTE_ADDRESS:
                g_value_set_boxed (value, self->priv->remote_address);
                break;
        case PROP_SESSION_NUMBER:
                g_value_set_int (value, self->priv->session_number);
                break;
        default:
                G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
                break;
        }
}

static void
gdm_xdmcp_display_get_timed_login_details (GdmDisplay *display,
                                           gboolean   *enabledp,
                                           char      **usernamep,
                                           int        *delayp)
{
        *enabledp = FALSE;
        *usernamep = g_strdup ("");
        *delayp = 0;
}

static void
gdm_xdmcp_display_class_init (GdmXdmcpDisplayClass *klass)
{
        GObjectClass    *object_class = G_OBJECT_CLASS (klass);
        GdmDisplayClass *display_class = GDM_DISPLAY_CLASS (klass);

        object_class->get_property = gdm_xdmcp_display_get_property;
        object_class->set_property = gdm_xdmcp_display_set_property;
        object_class->finalize = gdm_xdmcp_display_finalize;

        display_class->create_authority = gdm_xdmcp_display_create_authority;
        display_class->add_user_authorization = gdm_xdmcp_display_add_user_authorization;
        display_class->remove_user_authorization = gdm_xdmcp_display_remove_user_authorization;
        display_class->manage = gdm_xdmcp_display_manage;
        display_class->unmanage = gdm_xdmcp_display_unmanage;
        display_class->get_timed_login_details = gdm_xdmcp_display_get_timed_login_details;

        g_type_class_add_private (klass, sizeof (GdmXdmcpDisplayPrivate));

        g_object_class_install_property (object_class,
                                         PROP_REMOTE_ADDRESS,
                                         g_param_spec_boxed ("remote-address",
                                                             "Remote address",
                                                             "Remote address",
                                                             GDM_TYPE_ADDRESS,
                                                             G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

        g_object_class_install_property (object_class,
                                         PROP_SESSION_NUMBER,
                                         g_param_spec_int ("session-number",
                                                           "session-number",
                                                           "session-number",
                                                           G_MININT,
                                                           G_MAXINT,
                                                           0,
                                                           G_PARAM_READWRITE | G_PARAM_CONSTRUCT));

}

static void
gdm_xdmcp_display_init (GdmXdmcpDisplay *xdmcp_display)
{

        xdmcp_display->priv = GDM_XDMCP_DISPLAY_GET_PRIVATE (xdmcp_display);
}

static void
gdm_xdmcp_display_finalize (GObject *object)
{
        GdmXdmcpDisplay *xdmcp_display;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GDM_IS_XDMCP_DISPLAY (object));

        xdmcp_display = GDM_XDMCP_DISPLAY (object);

        g_return_if_fail (xdmcp_display->priv != NULL);

        G_OBJECT_CLASS (gdm_xdmcp_display_parent_class)->finalize (object);
}
