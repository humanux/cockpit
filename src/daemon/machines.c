/*
 * This file is part of Cockpit.
 *
 * Copyright (C) 2013 Red Hat, Inc.
 *
 * Cockpit is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * Cockpit is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Cockpit; If not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

#include <string.h>
#include <stdio.h>
#include <math.h>

#include <glib.h>
#include <glib/gi18n-lib.h>

#include "libgsystem.h"

#include "daemon.h"
#include "auth.h"
#include "machines.h"
#include "machine.h"
#include "utils.h"

/**
 * SECTION:machines
 * @title: Machines
 * @short_description: Implementation of #CockpitMachines
 *
 * This type provides an implementation of the #CockpitMachines interface.
 */

typedef struct _MachinesClass MachinesClass;

/**
 * Machines:
 *
 * The #Machines structure contains only private data and should
 * only be accessed using the provided API.
 */
struct _Machines
{
  CockpitMachinesSkeleton parent_instance;

  Daemon *daemon;

  GHashTable *machine_ifaces;
  GKeyFile *file;
};

struct _MachinesClass
{
  CockpitMachinesSkeletonClass parent_class;
};

enum
{
  PROP_0,
  PROP_DAEMON
};

static void machines_iface_init (CockpitMachinesIface *iface);

G_DEFINE_TYPE_WITH_CODE (Machines, machines, COCKPIT_TYPE_MACHINES_SKELETON,
                         G_IMPLEMENT_INTERFACE (COCKPIT_TYPE_MACHINES, machines_iface_init));

/* ---------------------------------------------------------------------------------------------------- */

#define MACHINES_FILE PACKAGE_LOCALSTATE_DIR "/lib/cockpit/machines"

static gboolean
machines_write (Machines *machines, GError **error)
{
  guint n = g_hash_table_size (machines->machine_ifaces);
  gs_free const gchar **ids = g_new0 (const gchar *, n+1);

  GKeyFile *file = g_key_file_new ();

  // This puts [main] at the top, which is slightly nicer.
  g_key_file_set_string_list (file, "main", "machines", NULL, 0);

  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init (&iter, machines->machine_ifaces);
  int i = 0;
  while (g_hash_table_iter_next (&iter, &key, &value))
    {
      ids[i++] = (const gchar *)key;
      machine_write (MACHINE (value), file);
    }
  ids[i] = NULL;

  g_key_file_set_string_list (file, "main", "machines", ids, n);

  gs_free gchar *data = g_key_file_to_data (file, NULL, NULL);
  g_key_file_free (file);

  return g_file_set_contents (MACHINES_FILE, data, -1, error);
}

static void
machines_read (Machines *machines)
{
  GError *error = NULL;
  GKeyFile *file = NULL;
  gchar **ids = NULL;

  file = g_key_file_new();
  if (!g_key_file_load_from_file (file, MACHINES_FILE, 0, &error))
    {
      if (g_error_matches (error, G_FILE_ERROR, G_FILE_ERROR_NOENT))
        {
          const gchar *address = "localhost";
          Machine *machine = MACHINE (machine_new (machines->daemon, address));
          g_hash_table_insert (machines->machine_ifaces, g_strdup (address), machine);
          cockpit_machine_set_address (COCKPIT_MACHINE (machine), address);
          machine_export (machine);
          machines_write (machines, NULL);
        }
      else
        g_warning ("Can't read %s: %s", MACHINES_FILE, error->message);
      goto out;
    }

  ids = g_key_file_get_string_list (file, "main", "machines", NULL, &error);
  if (ids == NULL)
    {
      g_warning ("Can't read %s: %s", MACHINES_FILE, error->message);
      goto out;
    }

  for (int i = 0; ids[i]; i++)
    {
      Machine *machine = g_hash_table_lookup (machines->machine_ifaces, ids[i]);
      if (machine == NULL)
        {
          machine = MACHINE (machine_new (machines->daemon, ids[i]));
          g_hash_table_insert (machines->machine_ifaces, g_strdup (ids[i]), machine);
        }

      machine_read (machine, file);
      machine_export (machine);
    }

 out:
  g_key_file_free (file);
  g_strfreev (ids);
  g_clear_error (&error);
  return;
}

static void
machines_finalize (GObject *object)
{
  Machines *machines = MACHINES (object);

  g_key_file_free (machines->file);

  G_OBJECT_CLASS (machines_parent_class)->finalize (object);
}

static void
machines_get_property (GObject *object,
                       guint prop_id,
                       GValue *value,
                       GParamSpec *pspec)
{
  Machines *machines = MACHINES (object);

  switch (prop_id)
    {
    case PROP_DAEMON:
      g_value_set_object (value, machines_get_daemon (machines));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
machines_set_property (GObject *object,
                       guint prop_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
  Machines *machines = MACHINES (object);

  switch (prop_id)
    {
    case PROP_DAEMON:
      g_assert (machines->daemon == NULL);
      /* we don't take a reference to the daemon */
      machines->daemon = g_value_get_object (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
machines_init (Machines *machines)
{
}

static void
machines_constructed (GObject *object)
{
  Machines *machines = MACHINES (object);

  machines->machine_ifaces = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_object_unref);
  machines->file = g_key_file_new ();

  machines_read (machines);

  if (G_OBJECT_CLASS (machines_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (machines_parent_class)->constructed (object);
}

static void
machines_class_init (MachinesClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize     = machines_finalize;
  gobject_class->constructed  = machines_constructed;
  gobject_class->set_property = machines_set_property;
  gobject_class->get_property = machines_get_property;

  /**
   * Machines:daemon:
   *
   * The #Daemon for the object.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_DAEMON,
                                   g_param_spec_object ("daemon",
                                                        NULL,
                                                        NULL,
                                                        TYPE_DAEMON,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

/**
 * machines_new:
 * @daemon: A #Daemon.
 *
 * Creates a new #Machines instance.
 *
 * Returns: A new #Machines. Free with g_object_unref().
 */
CockpitMachines *
machines_new (Daemon *daemon)
{
  g_return_val_if_fail (IS_DAEMON (daemon), NULL);
  return COCKPIT_MACHINES (g_object_new (COCKPIT_TYPE_DAEMON_MACHINES,
                                        "daemon", daemon,
                                         NULL));
}

/**
 * machines_get_daemon:
 * @machines: A #Machines.
 *
 * Gets the daemon used by @machines.
 *
 * Returns: A #Daemon. Do not free, the object is owned by @machines.
 */
Daemon *
machines_get_daemon (Machines *machines)
{
  g_return_val_if_fail (COCKPIT_IS_DAEMON_MACHINES (machines), NULL);
  return machines->daemon;
}

/* ---------------------------------------------------------------------------------------------------- */

gboolean
machines_remove_machine (Machines *machines,
                         const gchar *id,
                         GError **error)
{
  Machine *machine = g_hash_table_lookup (machines->machine_ifaces, id);
  if (machine == NULL)
    return TRUE;

  machine_unexport (machine);
  g_hash_table_remove (machines->machine_ifaces, id);
  return machines_write (machines, error);
}

static gboolean
handle_add (CockpitMachines *object,
            GDBusMethodInvocation *invocation,
            const gchar *arg_address)
{
  GError *error = NULL;
  Machines *machines = MACHINES (object);

  /* XXX - sanitize address or use something else as the id.
   */
  const gchar *id = arg_address;

  if (g_hash_table_lookup (machines->machine_ifaces, id))
    {
      cockpit_machines_complete_add (object, invocation);
      return TRUE;
    }

  Machine *machine = MACHINE (machine_new (machines->daemon, id));
  cockpit_machine_set_address (COCKPIT_MACHINE (machine), arg_address);
  machine_export (machine);
  g_hash_table_insert (machines->machine_ifaces, g_strdup (id), machine);

  if (!machines_write (machines, &error))
    {
      g_dbus_method_invocation_take_error (invocation, error);
      return TRUE;
    }

  cockpit_machines_complete_add (object, invocation);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
machines_iface_init (CockpitMachinesIface *iface)
{
  iface->handle_add = handle_add;
}
