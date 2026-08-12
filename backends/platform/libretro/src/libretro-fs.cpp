/* ScummVM - Graphic Adventure Engine
 *
 * ScummVM is the legal property of its developers, whose names
 * are too numerous to list here. Please refer to the COPYRIGHT
 * file distributed with this source distribution.
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

// Re-enable some forbidden symbols to avoid clashes with stat.h and unistd.h.
// Also with clock() in sys/time.h in some Mac OS X SDKs.
#define FORBIDDEN_SYMBOL_EXCEPTION_time_h
#define FORBIDDEN_SYMBOL_EXCEPTION_unistd_h
#define FORBIDDEN_SYMBOL_EXCEPTION_mkdir
#define FORBIDDEN_SYMBOL_EXCEPTION_getenv
#define FORBIDDEN_SYMBOL_EXCEPTION_strcat
#define FORBIDDEN_SYMBOL_EXCEPTION_strcpy
#define FORBIDDEN_SYMBOL_EXCEPTION_exit // Needed for IRIX's unistd.h

#include <file/file_path.h>
#include <retro_dirent.h>
#include <retro_stat.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>

#include "backends/platform/libretro/include/libretro-fs.h"
#include "backends/platform/libretro/include/libretro-stream.h"
#include "backends/platform/libretro/include/libretro-vfs.h"
#include "common/algorithm.h"
#include "common/util.h"

/**
 * Frontend VFS paths may be URIs (e.g. saf://<tree>/dir/file on Android)
 * rather than plain file system paths. Their scheme must be kept out of any
 * path manipulation, as normalizing it would collapse the "//" separator.
 *
 * @return the length of the "scheme://" prefix, 0 if the path has none.
 */
static uint uriSchemeLength(const Common::String &path) {
	uint i;

	for (i = 0; i < path.size(); i++) {
		const char c = path[i];

		if (Common::isAlnum(c) || c == '+' || c == '-' || c == '.')
			continue;

		// A scheme must not be empty and is followed by "://"
		return (i > 0 && !strncmp(path.c_str() + i, "://", 3)) ? i + 3 : 0;
	}

	return 0;
}

/**
 * Normalizes a path, leaving any URI scheme prefix untouched.
 */
static Common::String normalizeRetroPath(const Common::String &path) {
	const uint schemeLength = uriSchemeLength(path);

	if (!schemeLength)
		return Common::normalizePath(path, '/');

	return Common::String(path.c_str(), schemeLength) + Common::normalizePath(Common::String(path.c_str() + schemeLength), '/');
}

/**
 * Returns the offset of the first path component below the root of the given
 * path, i.e. the point up to which getParent() may strip components. For URIs
 * the root is the scheme along with the location it refers to (e.g. the SAF
 * tree), which cannot be split any further.
 */
static uint rootLength(const Common::String &path) {
	uint i = uriSchemeLength(path);

	if (!i)
		return 1; // "/"

	while (i < path.size() && path[i] != '/')
		i++;

	return i;
}

void LibRetroFilesystemNode::setFlags() {
	const char *fspath = _path.c_str();

	_isValid = path_is_valid(fspath);
	_isDirectory = path_is_directory(fspath);

	if (retro_vfs_enabled()) {
		// The VFS interface reports no permissions, assume whatever the
		// frontend hands out can be both read and written.
		_isReadable = _isValid;
		_isWritable = _isValid;
	} else {
		_isReadable = access(fspath, R_OK) == 0;
		_isWritable = access(fspath, W_OK) == 0;
	}
}

LibRetroFilesystemNode::LibRetroFilesystemNode(const Common::String &p) {
	assert(p.size() > 0);

	// Expand "~/" to the value of the HOME env variable
	if (p.hasPrefix("~/") || p.hasPrefix("~\\")) {
		Common::String homeDir = getHomeDir();
		if (homeDir.empty())
			homeDir = ".";

		// Skip over the tilda.  We know that p contains at least
		// two chars, so this is safe:
		_path = homeDir + (p.c_str() + 1);

	} else
		_path = p;

	char portable_path[_path.size() + 1];
	strcpy(portable_path, _path.c_str());
	pathname_make_slashes_portable(portable_path);

	// Normalize the path (that is, remove unneeded slashes etc.), keeping any
	// URI scheme of a frontend VFS path intact
	_path = normalizeRetroPath(Common::String(portable_path));
	_displayName = Common::lastPathComponent(_path, '/');

	setFlags();
}

AbstractFSNode *LibRetroFilesystemNode::getChild(const Common::String &n) const {
	assert(!_path.empty());
	assert(_isDirectory);

	// Make sure the string contains no slashes
	assert(!n.contains('/'));

	// We assume here that _path is already normalized (hence don't bother to call
	//  Common::normalizePath on the final path).
	Common::String newPath(_path);
	if (_path.lastChar() != '/')
		newPath += '/';
	newPath += n;

	return makeNode(newPath);
}

bool LibRetroFilesystemNode::getChildren(AbstractFSList &myList, ListMode mode, bool hidden) const {
	assert(_isDirectory);

	struct RDIR *dirp = retro_opendir(_path.c_str());

	if (dirp == NULL)
		return false;

	// loop over dir entries using readdir
	while ((retro_readdir(dirp))) {
		const char *d_name = retro_dirent_get_name(dirp);

		// Skip 'invisible' files if necessary
		if (d_name[0] == '.' && !hidden) {
			continue;
		}
		// Skip '.' and '..' to avoid cycles
		if ((d_name[0] == '.' && d_name[1] == 0) || (d_name[0] == '.' && d_name[1] == '.')) {
			continue;
		}

		// Start with a clone of this node, with the correct path set
		LibRetroFilesystemNode entry(*this);
		entry._displayName = d_name;
		if (_path.lastChar() != '/')
			entry._path += '/';
		entry._path += entry._displayName;

		entry._isValid = true;
		entry._isDirectory = retro_dirent_is_dir(dirp, entry._path.c_str());

		// Skip files that are invalid for some reason (e.g. because we couldn't
		// properly stat them).
		if (!entry._isValid)
			continue;

		// Honor the chosen mode
		if ((mode == Common::FSNode::kListFilesOnly && entry._isDirectory) || (mode == Common::FSNode::kListDirectoriesOnly && !entry._isDirectory))
			continue;

		myList.push_back(new LibRetroFilesystemNode(entry));
	}
	retro_closedir(dirp);

	if (mode != Common::FSNode::kListFilesOnly && _path == "/")
		addAuthorizedLocations(myList);

	return true;
}

void LibRetroFilesystemNode::addAuthorizedLocations(AbstractFSList &myList) const {
	// Locations the frontend granted access to (e.g. SAF trees on Android)
	// live outside of the local file system hierarchy, hence they are exposed
	// as additional children of the root node to make them browsable.
	Common::Array<LibRetroVfsLocation> locations = retro_get_authorized_locations();
	Common::StringArray usedNames;

	for (uint i = 0; i < locations.size(); i++) {
		LibRetroFilesystemNode entry(locations[i].path);

		if (!entry._isValid || !entry._isDirectory)
			continue;

		if (!locations[i].label.empty())
			entry._displayName = locations[i].label;

		// Frontends may label several locations the same way (e.g. all SAF
		// trees on Android), make sure each entry is distinguishable
		Common::String name(entry._displayName);
		for (uint n = 2; Common::find(usedNames.begin(), usedNames.end(), entry._displayName) != usedNames.end(); n++)
			entry._displayName = Common::String::format("%s (%u)", name.c_str(), n);

		usedNames.push_back(entry._displayName);
		myList.push_back(new LibRetroFilesystemNode(entry));
	}
}

AbstractFSNode *LibRetroFilesystemNode::getParent() const {
	if (_path == "/")
		return 0; // The filesystem root has no parent

	const uint root = rootLength(_path);

	// A frontend VFS location cannot be split any further: its parent is the
	// root node, which lists all of them along with the local file system
	if (_path.size() <= root)
		return makeNode("/");

	const char *start = _path.c_str();
	const char *end = start + _path.size();

	// Strip of the last component. We make use of the fact that at this
	// point, _path is guaranteed to be normalized
	while (end > start && *(end - 1) != '/')
		end--;

	if (end == start) {
		return 0;
	}

	if ((uint)(end - start) <= root)
		return makeNode(Common::String(start, root));

	AbstractFSNode *parent = makeNode(Common::String(start, end));

	if (parent->isDirectory() == false)
		return 0;

	return parent;
}

Common::SeekableReadStream *LibRetroFilesystemNode::createReadStream() {
	return LibRetroStream::makeFromPath(getPath(), LibRetroStream::WriteMode_Read);
}

Common::SeekableWriteStream *LibRetroFilesystemNode::createWriteStream(bool atomic) {
	return LibRetroStream::makeFromPath(getPath(), atomic ? LibRetroStream::WriteMode_WriteAtomic : LibRetroStream::WriteMode_Write);
}

bool LibRetroFilesystemNode::createDirectory() {
	if (path_mkdir(_path.c_str()))
		setFlags();

	return _isValid && _isDirectory;
}

namespace Posix {

bool assureDirectoryExists(const Common::String &dir, const char *prefix) {
	// Check whether the prefix exists if one is supplied.
	if (prefix) {
		if (!path_is_valid(prefix)) {
			return false;
		} else if (!path_is_directory(prefix)) {
			return false;
		}
	}

	// Obtain absolute path.
	Common::String path;
	if (prefix) {
		path = prefix;
		path += '/';
		path += dir;
	} else {
		path = dir;
	}

	path = normalizeRetroPath(path);

	const Common::String::iterator end = path.end();
	// Skip the root: neither "/" nor a frontend VFS location (whose scheme
	// would be mangled by the loop below) can be created
	Common::String::iterator cur = path.begin() + rootLength(path);
	if (cur < end && *cur == '/')
		++cur;

	do {
		if (cur + 1 != end) {
			if (*cur != '/') {
				continue;
			}

			// It is kind of ugly and against the purpose of Common::String to
			// insert 0s inside, but this is just for a local string and
			// simplifies the code a lot.
			*cur = '\0';
		}

		if (!path_mkdir(path.c_str())) {
			if (errno == EEXIST) {
				if (!path_is_valid(path.c_str())) {
					return false;
				} else if (!path_is_directory(path.c_str())) {
					return false;
				}
			} else {
				return false;
			}
		}

		*cur = '/';
	} while (cur++ != end);

	return true;
}

} // End of namespace Posix

Common::String LibRetroFilesystemNode::getHomeDir(void) {
	Common::String path;
	const char *home = nullptr;

#ifdef _WIN32
	const char *drv = getenv("HOMEDRIVE");
	const char *pth = getenv("HOMEPATH");
	if (drv && *drv && pth && *pth) {
		Common::String s = Common::String(drv);
		s += pth;
		return s;
	}
#else
	home = getenv("HOME");
#endif

	if (home && *home)
		path = home;

	return path;
}
