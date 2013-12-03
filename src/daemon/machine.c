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
#include "machine.h"
#include "machines.h"
#include "utils.h"

/**
 * SECTION:machine
 * @title: Machine
 * @short_description: Implementation of #CockpitMachine
 *
 * This type provides an implementation of the #CockpitMachine interface.
 */

typedef struct _MachineClass MachineClass;

/**
 * Machine:
 *
 * The #Machine structure contains only private data and should
 * only be accessed using the provided API.
 */
struct _Machine
{
  CockpitMachineSkeleton parent_instance;

  Daemon *daemon;
  gchar *id;
};

struct _MachineClass
{
  CockpitMachineSkeletonClass parent_class;
};

enum
{
  PROP_0,
  PROP_DAEMON,
  PROP_ID
};

static void machine_iface_init (CockpitMachineIface *iface);

G_DEFINE_TYPE_WITH_CODE (Machine, machine, COCKPIT_TYPE_MACHINE_SKELETON,
                         G_IMPLEMENT_INTERFACE (COCKPIT_TYPE_MACHINE, machine_iface_init));

/* ---------------------------------------------------------------------------------------------------- */

void
machine_read (Machine *machine, GKeyFile *file)
{
  gs_free gchar *address = g_key_file_get_string (file, machine->id, "address", NULL);
  cockpit_machine_set_address (COCKPIT_MACHINE (machine), address? address : machine->id);
}

void
machine_write (Machine *machine, GKeyFile *file)
{
  const gchar *address = cockpit_machine_get_address (COCKPIT_MACHINE (machine));
  g_key_file_set_string (file, machine->id, "address", address);
}

void
machine_export (Machine *machine)
{
  if (g_dbus_interface_get_object (G_DBUS_INTERFACE (machine)) == NULL)
    {
      GDBusObjectManagerServer *object_manager;
      CockpitObjectSkeleton *object = NULL;
      gs_free gchar *object_path = NULL;

      object_manager = daemon_get_object_manager (machine->daemon);
      object_path = utils_generate_object_path ("/com/redhat/Cockpit/Machines", machine->id);
      object = cockpit_object_skeleton_new (object_path);
      cockpit_object_skeleton_set_machine (object, COCKPIT_MACHINE (machine));
      g_dbus_object_manager_server_export_uniquely (object_manager, G_DBUS_OBJECT_SKELETON (object));
      g_object_unref (object);
    }
}

void
machine_unexport (Machine *machine)
{
  GDBusObject *object = g_dbus_interface_get_object (G_DBUS_INTERFACE (machine));
  if (object)
    {
      GDBusObjectManagerServer *object_manager = daemon_get_object_manager (machine->daemon);
      g_dbus_object_manager_server_unexport (object_manager, g_dbus_object_get_object_path (object));
    }
}

static void
machine_finalize (GObject *object)
{
  G_OBJECT_CLASS (machine_parent_class)->finalize (object);
}

static void
machine_get_property (GObject *object,
                       guint prop_id,
                       GValue *value,
                       GParamSpec *pspec)
{
  Machine *machine = MACHINE (object);

  switch (prop_id)
    {
    case PROP_DAEMON:
      g_value_set_object (value, machine_get_daemon (machine));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
machine_set_property (GObject *object,
                       guint prop_id,
                       const GValue *value,
                       GParamSpec *pspec)
{
  Machine *machine = MACHINE (object);

  switch (prop_id)
    {
    case PROP_DAEMON:
      g_assert (machine->daemon == NULL);
      /* we don't take a reference to the daemon */
      machine->daemon = g_value_get_object (value);
      break;

    case PROP_ID:
      g_assert (machine->id == NULL);
      machine->id = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
machine_init (Machine *machine)
{
}

static void
machine_constructed (GObject *object)
{
  if (G_OBJECT_CLASS (machine_parent_class)->constructed != NULL)
    G_OBJECT_CLASS (machine_parent_class)->constructed (object);
}

static void
machine_class_init (MachineClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize     = machine_finalize;
  gobject_class->constructed  = machine_constructed;
  gobject_class->set_property = machine_set_property;
  gobject_class->get_property = machine_get_property;

  /**
   * Machine:daemon:
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

  /**
   * Machine:daemon:
   *
   * The #Daemon for the object.
   */
  g_object_class_install_property (gobject_class,
                                   PROP_ID,
                                   g_param_spec_string ("id",
                                                        NULL,
                                                        NULL,
                                                        NULL,
                                                        G_PARAM_READABLE |
                                                        G_PARAM_WRITABLE |
                                                        G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
}

/**
 * machine_new:
 * @daemon: A #Daemon.
 *
 * Creates a new #Machine instance.
 *
 * Returns: A new #Machine. Free with g_object_unref().
 */
CockpitMachine *
machine_new (Daemon *daemon,
             const gchar *id)
{
  g_return_val_if_fail (IS_DAEMON (daemon), NULL);
  return COCKPIT_MACHINE (g_object_new (COCKPIT_TYPE_DAEMON_MACHINE,
                                        "daemon", daemon,
                                        "id", id,
                                         NULL));
}

/**
 * machine_get_daemon:
 * @machine: A #Machine.
 *
 * Gets the daemon used by @machine.
 *
 * Returns: A #Daemon. Do not free, the object is owned by @machine.
 */
Daemon *
machine_get_daemon (Machine *machine)
{
  g_return_val_if_fail (COCKPIT_IS_DAEMON_MACHINE (machine), NULL);
  return machine->daemon;
}

/* ---------------------------------------------------------------------------------------------------- */

static gboolean
handle_remove (CockpitMachine *object,
               GDBusMethodInvocation *invocation)
{
  GError *error = NULL;
  Machine *machine = MACHINE (object);
  Machines *machines = daemon_get_machines (machine->daemon);

  if (!machines_remove_machine (machines, machine->id, &error))
    {
      g_dbus_method_invocation_take_error (invocation, error);
      return TRUE;
    }

  cockpit_machine_complete_remove (object, invocation);
  return TRUE;
}

/* ---------------------------------------------------------------------------------------------------- */

static void
machine_iface_init (CockpitMachineIface *iface)
{
  iface->handle_remove = handle_remove;
}
