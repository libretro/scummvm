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

#ifndef LIBRETRO_VFS_H
#define LIBRETRO_VFS_H

#include <libretro.h>

#include "common/array.h"
#include "common/str.h"

/**
 * Locations the frontend has been granted access to (e.g. SAF trees on
 * Android), as returned by RETRO_ENVIRONMENT_GET_VFS_AUTHORIZED_LOCATIONS.
 * Defined here as long as the bundled libretro.h predates that environment
 * call.
 */
#ifndef RETRO_ENVIRONMENT_GET_VFS_AUTHORIZED_LOCATIONS
#define RETRO_ENVIRONMENT_GET_VFS_AUTHORIZED_LOCATIONS (93 | RETRO_ENVIRONMENT_EXPERIMENTAL)

struct retro_vfs_authorized_location {
	const char *path;
	const char *label;
	unsigned flags;
};

struct retro_vfs_authorized_locations {
	const struct retro_vfs_authorized_location *locations;
	size_t count;
};
#endif

struct LibRetroVfsLocation {
	Common::String path;
	Common::String label;
};

/**
 * Requests the frontend file system interface and hooks it into the
 * libretro-common filestream/dirent/path wrappers used by the file system
 * backend. If the frontend provides no (or a too old) interface, those
 * wrappers keep using their built-in local file system implementation.
 *
 * Must be called from retro_set_environment(), i.e. before the ScummVM
 * file system factory is instantiated.
 */
void retro_init_vfs(retro_environment_t cb);

/**
 * @return Whether file access is routed through the frontend VFS interface.
 */
bool retro_vfs_enabled(void);

/**
 * Retrieves the locations the frontend granted access to. The list is queried
 * on each call, as the user may authorize further locations while the core is
 * running.
 *
 * @return The authorized locations, empty if the frontend has none or does not
 * support the related environment call.
 */
Common::Array<LibRetroVfsLocation> retro_get_authorized_locations(void);

#endif // LIBRETRO_VFS_H
