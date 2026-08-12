/* Copyright (C) 2023 Giovanni Cascione <ing.cascione@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include <file/file_path.h>
#include <retro_dirent.h>
#include <streams/file_stream.h>

#include "backends/platform/libretro/include/libretro-vfs.h"
#include "backends/platform/libretro/include/libretro-core.h"

/* Directory listing and stat/mkdir wrappers need VFS v3, streams need v2 only. */
#define RETRO_VFS_WANTED_VERSION 3

static retro_environment_t vfs_environ_cb = NULL;
static bool vfs_enabled = false;

void retro_init_vfs(retro_environment_t cb) {
	struct retro_vfs_interface_info vfs_iface_info;

	vfs_environ_cb = cb;
	vfs_enabled = false;

	if (!cb)
		return;

	vfs_iface_info.required_interface_version = RETRO_VFS_WANTED_VERSION;
	vfs_iface_info.iface = NULL;

	if (!cb(RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &vfs_iface_info) || !vfs_iface_info.iface) {
		if (retro_log_cb)
			retro_log_cb(RETRO_LOG_INFO, "Frontend provides no VFS interface, using local file system.\n");
		return;
	}

	/* The frontend reports back the version it actually implements: the
	   init helpers below silently keep their built-in implementation if it
	   is lower than what they need. */
	filestream_vfs_init(&vfs_iface_info);
	path_vfs_init(&vfs_iface_info);
	dirent_vfs_init(&vfs_iface_info);

	/* Streams are already routed through the frontend with v2, but the file
	   system backend also needs the v3 stat/mkdir/directory calls to be able
	   to browse frontend specific locations. */
	vfs_enabled = vfs_iface_info.required_interface_version >= RETRO_VFS_WANTED_VERSION;

	if (retro_log_cb)
		retro_log_cb(vfs_enabled ? RETRO_LOG_INFO : RETRO_LOG_WARN,
		             "Frontend VFS interface version %u found, VFS file system %s (version %d needed).\n",
		             vfs_iface_info.required_interface_version, vfs_enabled ? "enabled" : "disabled", RETRO_VFS_WANTED_VERSION);
}

bool retro_vfs_enabled(void) {
	return vfs_enabled;
}

Common::Array<LibRetroVfsLocation> retro_get_authorized_locations(void) {
	Common::Array<LibRetroVfsLocation> retval;
	struct retro_vfs_authorized_locations locations;

	if (!vfs_enabled || !vfs_environ_cb)
		return retval;

	locations.locations = NULL;
	locations.count = 0;

	if (!vfs_environ_cb(RETRO_ENVIRONMENT_GET_VFS_AUTHORIZED_LOCATIONS, &locations) || !locations.locations)
		return retval;

	for (size_t i = 0; i < locations.count; i++) {
		LibRetroVfsLocation location;

		if (!locations.locations[i].path)
			continue;

		/* The frontend owns the strings, hence the copies. */
		location.path = locations.locations[i].path;
		location.label = locations.locations[i].label ? locations.locations[i].label : locations.locations[i].path;
		retval.push_back(location);
	}

	return retval;
}
