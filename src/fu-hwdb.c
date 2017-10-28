/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2017 Richard Hughes <richard@hughsie.com>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "config.h"

#include <glib-object.h>
#include <gio/gio.h>

#include "fu-hwdb.h"

#include "fwupd-error.h"
#include "fwupd-remote-private.h"

static void fu_hwdb_finalize	 (GObject *obj);

struct _FuHwdb
{
	GObject			 parent_instance;
	GPtrArray		*monitors;
	GHashTable		*hwdb_hash;	/* of prefix/id:string */
};

G_DEFINE_TYPE (FuHwdb, fu_hwdb, G_TYPE_OBJECT)

static void
fu_hwdb_monitor_changed_cb (GFileMonitor *monitor,
			      GFile *file,
			      GFile *other_file,
			      GFileMonitorEvent event_type,
			      gpointer user_data)
{
	FuHwdb *self = FU_HWDB (user_data);
	g_autoptr(GError) error = NULL;
	g_autofree gchar *filename = g_file_get_path (file);
	g_debug ("%s changed, reloading all configs", filename);
	if (!fu_hwdb_load (self, &error))
		g_warning ("failed to rescan hwdb: %s", error->message);
}

static gboolean
fu_hwdb_add_inotify (FuHwdb *self, const gchar *filename, GError **error)
{
	GFileMonitor *monitor;
	g_autoptr(GFile) file = g_file_new_for_path (filename);

	/* set up a notify watch */
	monitor = g_file_monitor (file, G_FILE_MONITOR_NONE, NULL, error);
	if (monitor == NULL)
		return FALSE;
	g_signal_connect (monitor, "changed",
			  G_CALLBACK (fu_hwdb_monitor_changed_cb), self);
	g_ptr_array_add (self->monitors, monitor);
	return TRUE;
}

/**
 * fu_hwdb_lookup_by_id:
 * @self: A #FuPlugin
 * @prefix: A string prefix that matches the hwdb file basename, e.g. "dfu-quirks"
 * @id: An ID to match the entry, e.g. "012345"
 *
 * Looks up an entry in the hardware database using a string value.
 *
 * Returns: (transfer none): values from the database, or %NULL if not found
 *
 * Since: 1.0.1
 **/
const gchar *
fu_hwdb_lookup_by_id (FuHwdb *self, const gchar *prefix, const gchar *id)
{
	g_autofree gchar *key = NULL;

	g_return_val_if_fail (FU_IS_HWDB (self), NULL);
	g_return_val_if_fail (id != NULL, NULL);
	g_return_val_if_fail (g_hash_table_size (self->hwdb_hash) != 0, NULL);

	key = g_strdup_printf ("%s/%s", prefix, id);
	return g_hash_table_lookup (self->hwdb_hash, key);
}

/**
 * fu_hwdb_lookup_by_usb_device:
 * @self: A #FuPlugin
 * @prefix: A string prefix that matches the hwdb file basename, e.g. "dfu-quirks"
 * @dev: A #GUsbDevice
 *
 * Looks up an entry in the hardware database using various keys generated
 * from @dev.
 *
 * Returns: (transfer none): values from the database, or %NULL if not found
 *
 * Since: 1.0.1
 **/
const gchar *
fu_hwdb_lookup_by_usb_device (FuHwdb *self, const gchar *prefix, GUsbDevice *dev)
{
	const gchar *tmp;
	g_autofree gchar *key1 = NULL;
	g_autofree gchar *key2 = NULL;
	g_autofree gchar *key3 = NULL;

	g_return_val_if_fail (FU_IS_HWDB (self), NULL);
	g_return_val_if_fail (prefix != NULL, NULL);
	g_return_val_if_fail (G_USB_IS_DEVICE (dev), NULL);

	/* prefer an exact match, VID:PID:REV */
	key1 = g_strdup_printf ("USB\\VID_%04X&PID_%04X&REV_%04X",
				g_usb_device_get_vid (dev),
				g_usb_device_get_pid (dev),
				g_usb_device_get_release (dev));
	tmp = fu_hwdb_lookup_by_id (self, prefix, key1);
	if (tmp != NULL)
		return tmp;

	/* VID:PID */
	key2 = g_strdup_printf ("USB\\VID_%04X&PID_%04X",
				g_usb_device_get_vid (dev),
				g_usb_device_get_pid (dev));
	tmp = fu_hwdb_lookup_by_id (self, prefix, key2);
	if (tmp != NULL)
		return tmp;

	/* VID */
	key3 = g_strdup_printf ("USB\\VID_%04X", g_usb_device_get_vid (dev));
	return fu_hwdb_lookup_by_id (self, prefix, key3);
}

static gboolean
fu_hwdb_add_hwdb_from_filename (FuHwdb *self, const gchar *filename, GError **error)
{
	g_autoptr(GKeyFile) kf = g_key_file_new ();
	g_auto(GStrv) groups = NULL;

	/* load keyfile */
	if (!g_key_file_load_from_file (kf, filename, G_KEY_FILE_NONE, error))
		return FALSE;

	/* add each set of groups and keys */
	groups = g_key_file_get_groups (kf, NULL);
	for (guint i = 0; groups[i] != NULL; i++) {
		g_auto(GStrv) keys = NULL;
		keys = g_key_file_get_keys (kf, groups[i], NULL, error);
		if (keys == NULL)
			return FALSE;
		for (guint j = 0; keys[j] != NULL; j++) {
			g_autofree gchar *tmp = NULL;
			tmp = g_key_file_get_string (kf, groups[i], keys[j], error);
			if (tmp == NULL)
				return FALSE;
			g_hash_table_insert (self->hwdb_hash,
					     g_strdup_printf ("%s/%s", groups[i], keys[j]),
					     g_steal_pointer (&tmp));
		}
	}
	g_debug ("now %u entries in the hwdb", g_hash_table_size (self->hwdb_hash));
	return TRUE;
}

static gint
fu_hwdb_filename_sort_cb (gconstpointer a, gconstpointer b)
{
	const gchar *stra = *((const gchar **) a);
	const gchar *strb = *((const gchar **) b);
	return g_strcmp0 (stra, strb);
}

static gboolean
fu_hwdb_add_hwdbs_for_path (FuHwdb *self, const gchar *path, GError **error)
{
	const gchar *tmp;
	g_autofree gchar *path_hw = NULL;
	g_autoptr(GDir) dir = NULL;
	g_autoptr(GPtrArray) filenames = g_ptr_array_new_with_free_func (g_free);

	/* add valid files to the array */
	path_hw = g_build_filename (path, "hardware.d", NULL);
	if (!g_file_test (path_hw, G_FILE_TEST_EXISTS))
		return TRUE;
	dir = g_dir_open (path_hw, 0, error);
	if (dir == NULL)
		return FALSE;
	while ((tmp = g_dir_read_name (dir)) != NULL) {
		if (!g_str_has_suffix (tmp, ".hwdb")) {
			g_debug ("skipping invalid file %s", tmp);
			continue;
		}
		g_ptr_array_add (filenames, g_build_filename (path_hw, tmp, NULL));
	}

	/* sort */
	g_ptr_array_sort (filenames, fu_hwdb_filename_sort_cb);

	/* process files */
	for (guint i = 0; i < filenames->len; i++) {
		const gchar *filename = g_ptr_array_index (filenames, i);

		/* load from keyfile */
		g_debug ("loading hwdb from %s", filename);
		if (!fu_hwdb_add_hwdb_from_filename (self, filename, error)) {
			g_prefix_error (error, "failed to load %s: ", filename);
			return FALSE;
		}

		/* watch the file for changes */
		if (!fu_hwdb_add_inotify (self, filename, error))
			return FALSE;
	}

	/* success */
	return TRUE;
}

gboolean
fu_hwdb_load (FuHwdb *self, GError **error)
{
	g_return_val_if_fail (FU_IS_HWDB (self), FALSE);

	/* ensure empty in case we're called from a monitor change */
	g_ptr_array_set_size (self->monitors, 0);
	g_hash_table_remove_all (self->hwdb_hash);

	/* system datadir */
	if (!fu_hwdb_add_hwdbs_for_path (self, FWUPDDATADIR, error))
		return FALSE;
	//FIXME: something we can write when using Fedora Atomic?

	/* success */
	return TRUE;
}

static void
fu_hwdb_class_init (FuHwdbClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = fu_hwdb_finalize;
}

static void
fu_hwdb_init (FuHwdb *self)
{
	self->monitors = g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
	self->hwdb_hash = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);
}

static void
fu_hwdb_finalize (GObject *obj)
{
	FuHwdb *self = FU_HWDB (obj);
	g_ptr_array_unref (self->monitors);
	g_hash_table_unref (self->hwdb_hash);
	G_OBJECT_CLASS (fu_hwdb_parent_class)->finalize (obj);
}

FuHwdb *
fu_hwdb_new (void)
{
	FuHwdb *self;
	self = g_object_new (FU_TYPE_HWDB, NULL);
	return FU_HWDB (self);
}
