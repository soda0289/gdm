/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2007 William Jon McCann <mccann@jhu.edu>
 * Copyright (C) 2008 Red Hat, Inc.
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 *
 * Written by: William Jon McCann <mccann@jhu.edu>
 *             Ray Strode <rstrode@redhat.com>
 */

#include "config.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <dirent.h>
#include <sys/stat.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>

#include "gdm-clock-widget.h"

#define GDM_CLOCK_WIDGET_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), GDM_TYPE_CLOCK_WIDGET, GdmClockWidgetPrivate))

struct GdmClockWidgetPrivate
{
        GtkWidget *label;
        char      *time_format;
        guint     update_clock_id;
        guint     should_show_seconds : 1;
};

static void     gdm_clock_widget_class_init  (GdmClockWidgetClass *klass);
static void     gdm_clock_widget_init        (GdmClockWidget      *clock_widget);
static void     gdm_clock_widget_finalize    (GObject             *object);
static gboolean update_timeout_cb            (GdmClockWidget *clock);

G_DEFINE_TYPE (GdmClockWidget, gdm_clock_widget, GTK_TYPE_ALIGNMENT)

static char *
get_time_format (GdmClockWidget *clock)
{
        const char *time_format;
        const char *date_format;
        char       *clock_format;
        char       *result;

        time_format = clock->priv->should_show_seconds ? _("%l:%M:%S %p") : _("%l:%M %p");
        /* translators: replace %e with %d if, when the day of the
         *              month as a decimal number is a single digit, it
         *              should begin with a 0 in your locale (e.g. "May
         *              01" instead of "May  1").
         */
        date_format = _("%a %b %e");
        /* translators: reverse the order of these arguments
         *              if the time should come before the
         *              date on a clock in your locale.
         */
        clock_format = g_strdup_printf (_("%1$s, %2$s"),
                                        date_format,
                                        time_format);

        result = g_locale_from_utf8 (clock_format, -1, NULL, NULL, NULL);
        g_free (clock_format);

        return result;
}

static void
update_clock (GtkLabel   *label,
              const char *format)
{
        time_t     t;
        struct tm *tm;
        char       buf[256];
        char      *utf8;

        time (&t);
        tm = localtime (&t);
        if (tm == NULL) {
                g_warning ("Unable to get broken down local time");
                return;
        }
        if (strftime (buf, sizeof (buf), format, tm) == 0) {
                g_warning ("Couldn't format time: %s", format);
                strcpy (buf, "???");
        }
        utf8 = g_locale_to_utf8 (buf, -1, NULL, NULL, NULL);
        gtk_label_set_text (label, utf8);
        g_free (utf8);
}

static void
set_clock_timeout (GdmClockWidget *clock,
                   time_t          now)
{
        GTimeVal       tv;
        int            timeouttime;

        if (clock->priv->update_clock_id > 0) {
                g_source_remove (clock->priv->update_clock_id);
                clock->priv->update_clock_id = 0;
        }

        g_get_current_time (&tv);
        timeouttime = (G_USEC_PER_SEC - tv.tv_usec) / 1000 + 1;

        /* timeout of one minute if we don't care about the seconds */
        if (! clock->priv->should_show_seconds) {
                timeouttime += 1000 * (59 - now % 60);
        }

        clock->priv->update_clock_id = g_timeout_add (timeouttime,
                                                      (GSourceFunc)update_timeout_cb,
                                                      clock);
}

static gboolean
update_timeout_cb (GdmClockWidget *clock)
{
        time_t     new_time;

        time (&new_time);

        if (clock->priv->label != NULL) {
                update_clock (GTK_LABEL (clock->priv->label),
                              clock->priv->time_format);
        }

        set_clock_timeout (clock, new_time);

        return FALSE;
}

static void
remove_timeout (GdmClockWidget *clock)
{
        if (clock->priv->update_clock_id > 0) {
                g_source_remove (clock->priv->update_clock_id);
                clock->priv->update_clock_id = 0;
        }
}

static void
gdm_clock_widget_class_init (GdmClockWidgetClass *klass)
{
        GObjectClass *object_class;

        object_class = G_OBJECT_CLASS (klass);

        object_class->finalize = gdm_clock_widget_finalize;

        g_type_class_add_private (klass, sizeof (GdmClockWidgetPrivate));
}

static void
gdm_clock_widget_init (GdmClockWidget *widget)
{
        widget->priv = GDM_CLOCK_WIDGET_GET_PRIVATE (widget);

        widget->priv->label = gtk_label_new ("");
        gtk_widget_show (widget->priv->label);

        gtk_container_add (GTK_CONTAINER (widget), widget->priv->label);

        widget->priv->time_format = get_time_format (widget);
        update_timeout_cb (widget);
}

static void
gdm_clock_widget_finalize (GObject *object)
{
        GdmClockWidget *clock_widget;

        g_return_if_fail (object != NULL);
        g_return_if_fail (GDM_IS_CLOCK_WIDGET (object));

        clock_widget = GDM_CLOCK_WIDGET (object);

        g_return_if_fail (clock_widget->priv != NULL);

        remove_timeout (clock_widget);

        G_OBJECT_CLASS (gdm_clock_widget_parent_class)->finalize (object);
}

GtkWidget *
gdm_clock_widget_new (void)
{
        GObject *object;

        object = g_object_new (GDM_TYPE_CLOCK_WIDGET,
                               NULL);

        return GTK_WIDGET (object);
}