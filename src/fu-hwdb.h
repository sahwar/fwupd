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

#ifndef __FU_HWDB_H
#define __FU_HWDB_H

G_BEGIN_DECLS

#include <glib-object.h>
#include <gusb.h>

#define FU_TYPE_HWDB (fu_hwdb_get_type ())
G_DECLARE_FINAL_TYPE (FuHwdb, fu_hwdb, FU, HWDB, GObject)

FuHwdb		*fu_hwdb_new				(void);
gboolean	 fu_hwdb_load				(FuHwdb		*self,
							 GError		**error);
const gchar	*fu_hwdb_lookup_by_id			(FuHwdb		*self,
							 const gchar	*prefix,
							 const gchar	*id);
const gchar	*fu_hwdb_lookup_by_usb_device		(FuHwdb		*self,
							 const gchar	*prefix,
							 GUsbDevice	*dev);

G_END_DECLS

#endif /* __FU_HWDB_H */

