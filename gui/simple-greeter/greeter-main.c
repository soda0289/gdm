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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "config.h"

#include <stdlib.h>
#include <locale.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#include "gdm-log.h"
#include "gdm-common.h"
#include "gdm-signal-handler.h"
#include "gdm-settings-client.h"
#include "gdm-settings-keys.h"
#include "gdm-profile.h"

#include "gdm-greeter-session.h"

#define ACCESSIBILITY_KEY         "/desktop/gnome/interface/accessibility"
#define DEBUG_KEY                 "/apps/gdm/simple-greeter/debug"

static Atom AT_SPI_IOR;

static gboolean
assistive_registry_launch (void)
{
        GPid        pid;
        GError     *error;
        const char *command;
        char      **argv;
        gboolean    res;
        gboolean    ret;

        ret = FALSE;

        gdm_profile_start (NULL);

        command = AT_SPI_REGISTRYD_DIR "/at-spi-registryd";

        argv = NULL;
        error = NULL;
        res = g_shell_parse_argv (command, NULL, &argv, &error);
        if (! res) {
                g_warning ("Unable to parse command: %s", error->message);
                goto out;
        }

        error = NULL;
        res = g_spawn_async (NULL,
                             argv,
                             NULL,
                             G_SPAWN_SEARCH_PATH
                             | G_SPAWN_STDOUT_TO_DEV_NULL
                             | G_SPAWN_STDERR_TO_DEV_NULL,
                             NULL,
                             NULL,
                             &pid,
                             &error);
        g_strfreev (argv);

        if (! res) {
                g_warning ("Unable to run command %s: %s", command, error->message);
                goto out;
        }

        if (kill (pid, 0) < 0) {
                g_warning ("at-spi-registryd not running");
                goto out;
        }

        ret = TRUE;
 out:
        gdm_profile_end (NULL);

        return ret;
}

static GdkFilterReturn
filter_watch (GdkXEvent *xevent,
              GdkEvent  *event,
              GMainLoop *loop)
{
        XEvent *xev = (XEvent *)xevent;

        if (xev->xany.type == PropertyNotify
            && xev->xproperty.atom == AT_SPI_IOR) {
                g_debug ("a11y registry started");
                g_main_loop_quit (loop);

                return GDK_FILTER_REMOVE;
        }

        return GDK_FILTER_CONTINUE;
}

static gboolean
filter_timeout (GMainLoop *loop)
{
        g_warning ("The accessibility registry was not found.");

        g_main_loop_quit (loop);

        return FALSE;
}

static void
assistive_registry_start (void)
{
        GdkWindow *root;
        guint      tid;
        GMainLoop *loop;

        gdm_profile_start (NULL);

        g_debug ("Starting a11y registry");

        root = gdk_get_default_root_window ();

        if ( ! AT_SPI_IOR) {
                AT_SPI_IOR = XInternAtom (GDK_DISPLAY (), "AT_SPI_IOR", False);
        }

        gdk_window_set_events (root, GDK_PROPERTY_CHANGE_MASK);

        if ( ! assistive_registry_launch ()) {
                g_warning ("The accessibility registry could not be started.");
                return;
        }

        loop = g_main_loop_new (NULL, FALSE);
        gdk_window_add_filter (root, (GdkFilterFunc)filter_watch, loop);
        tid = g_timeout_add_seconds (5, (GSourceFunc)filter_timeout, loop);

        g_main_loop_run (loop);

        gdk_window_remove_filter (root, (GdkFilterFunc)filter_watch, loop);
        g_source_remove (tid);

        g_main_loop_unref (loop);

        gdm_profile_end (NULL);
}

static void
at_set_gtk_modules (void)
{
        GSList     *modules_list;
        GSList     *l;
        const char *old;
        char      **modules;
        gboolean    found_gail;
        gboolean    found_atk_bridge;
        int         n;

        gdm_profile_start (NULL);

        n = 0;
        modules_list = NULL;
        found_gail = FALSE;
        found_atk_bridge = FALSE;

        if ((old = g_getenv ("GTK_MODULES")) != NULL) {
                modules = g_strsplit (old, ":", -1);
                for (n = 0; modules[n]; n++) {
                        if (strcmp (modules[n], "gail") == 0) {
                                found_gail = TRUE;
                        } else if (strcmp (modules[n], "atk-bridge") == 0) {
                                found_atk_bridge = TRUE;
                        }

                        modules_list = g_slist_prepend (modules_list, modules[n]);
                        modules[n] = NULL;
                }
                g_free (modules);
        }

        if (!found_gail) {
                modules_list = g_slist_prepend (modules_list, "gail");
                ++n;
        }

        if (!found_atk_bridge) {
                modules_list = g_slist_prepend (modules_list, "atk-bridge");
                ++n;
        }

        modules = g_new (char *, n + 1);
        modules[n--] = NULL;
        for (l = modules_list; l; l = l->next) {
                modules[n--] = g_strdup (l->data);
        }

        g_setenv ("GTK_MODULES", g_strjoinv (":", modules), TRUE);
        g_strfreev (modules);
        g_slist_free (modules_list);

        gdm_profile_end (NULL);
}

static void
load_a11y (void)
{
        const char        *env_a_t_support;
        gboolean           a_t_support;
        GConfClient       *gconf_client;

        gdm_profile_start (NULL);

        gconf_client = gconf_client_get_default ();

        env_a_t_support = g_getenv ("GNOME_ACCESSIBILITY");
        if (env_a_t_support) {
                a_t_support = atoi (env_a_t_support);
        } else {
                GConfValue *val;

                a_t_support = TRUE;

                val = gconf_client_get_without_default (gconf_client, ACCESSIBILITY_KEY, NULL);
                if (val != NULL) {
                        a_t_support = gconf_value_get_bool (val);
                        gconf_value_free (val);
                }
        }

        if (a_t_support) {
                assistive_registry_start ();
                at_set_gtk_modules ();
        }

        g_object_unref (gconf_client);

        gdm_profile_end (NULL);
}

static gboolean
is_debug_set (void)
{
        GConfClient *client;
        gboolean     is;

        /* enable debugging for unstable builds */
        if (gdm_is_version_unstable ()) {
                return TRUE;
        }

        client = gconf_client_get_default ();
        is = gconf_client_get_bool (client, DEBUG_KEY, NULL);
        g_object_unref (client);

        return is;
}


static gboolean
signal_cb (int      signo,
           gpointer data)
{
        int ret;

        g_debug ("Got callback for signal %d", signo);

        ret = TRUE;

        switch (signo) {
        case SIGFPE:
        case SIGPIPE:
                /* let the fatal signals interrupt us */
                g_debug ("Caught signal %d, shutting down abnormally.", signo);
                ret = FALSE;

                break;

        case SIGINT:
        case SIGTERM:
                /* let the fatal signals interrupt us */
                g_debug ("Caught signal %d, shutting down normally.", signo);
                ret = FALSE;

                break;

        case SIGHUP:
                g_debug ("Got HUP signal");
                /* FIXME:
                 * Reread config stuff like system config files, VPN service files, etc
                 */
                ret = TRUE;

                break;

        case SIGUSR1:
                g_debug ("Got USR1 signal");
                /* FIXME:
                 * Play with log levels or something
                 */
                ret = TRUE;

                gdm_log_toggle_debug ();

                break;

        default:
                g_debug ("Caught unhandled signal %d", signo);
                ret = TRUE;

                break;
        }

        return ret;
}

int
main (int argc, char *argv[])
{
        GError            *error;
        GdmGreeterSession *session;
        gboolean           res;
        GdmSignalHandler  *signal_handler;

        bindtextdomain (GETTEXT_PACKAGE, GNOMELOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);

        setlocale (LC_ALL, "");

        gdm_set_fatal_warnings_if_unstable ();

        g_type_init ();

        gdm_profile_start ("Initializing settings client");
        if (! gdm_settings_client_init (GDMCONFDIR "/gdm.schemas", "/")) {
                g_critical ("Unable to initialize settings client");
                exit (1);
        }
        gdm_profile_end ("Initializing settings client");

        g_debug ("Greeter session pid=%d display=%s xauthority=%s",
                 (int)getpid (),
                 g_getenv ("DISPLAY"),
                 g_getenv ("XAUTHORITY"));

        /* FIXME: For testing to make it easier to attach gdb */
        /*sleep (15);*/

        gdm_log_init ();
        gdm_log_set_debug (is_debug_set ());

        gdk_init (&argc, &argv);

        load_a11y ();

        gtk_init (&argc, &argv);

        signal_handler = gdm_signal_handler_new ();
        gdm_signal_handler_add_fatal (signal_handler);
        gdm_signal_handler_add (signal_handler, SIGTERM, signal_cb, NULL);
        gdm_signal_handler_add (signal_handler, SIGINT, signal_cb, NULL);
        gdm_signal_handler_add (signal_handler, SIGFPE, signal_cb, NULL);
        gdm_signal_handler_add (signal_handler, SIGHUP, signal_cb, NULL);
        gdm_signal_handler_add (signal_handler, SIGUSR1, signal_cb, NULL);

        gdm_profile_start ("Creating new greeter session");
        session = gdm_greeter_session_new ();
        if (session == NULL) {
                g_critical ("Unable to create greeter session");
                exit (1);
        }
        gdm_profile_end ("Creating new greeter session");

        error = NULL;
        res = gdm_greeter_session_start (session, &error);
        if (! res) {
                g_warning ("Unable to start greeter session: %s", error->message);
                g_error_free (error);
                exit (1);
        }

        gtk_main ();

        if (session != NULL) {
                g_object_unref (session);
        }

        return 0;
}