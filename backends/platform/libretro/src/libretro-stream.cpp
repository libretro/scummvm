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

#include <streams/file_stream.h>

#include "backends/platform/libretro/include/libretro-stream.h"
#include "common/textconsole.h"

LibRetroStream::LibRetroStream(RFILE *handle) : _handle(handle), _path(nullptr), _err(false), _eos(false) {
	assert(handle);
}

LibRetroStream::~LibRetroStream() {
	filestream_close(_handle);

	if (!_path)
		return;

	// _path is set: recreate the temporary file name and rename the file to
	// its real name
	Common::String tmpPath(*_path);
	tmpPath += ".tmp";

	if (filestream_rename(tmpPath.c_str(), _path->c_str())) {
		// Error: try to delete the destination file first, as not all
		// implementations replace an existing file on rename
		filestream_delete(_path->c_str());

		if (filestream_rename(tmpPath.c_str(), _path->c_str()))
			warning("Couldn't save file %s", _path->c_str());
	}

	delete _path;
}

bool LibRetroStream::err() const {
	return _err || filestream_error(_handle);
}

void LibRetroStream::clearErr() {
	_err = false;
	_eos = false;

	if (!filestream_error(_handle))
		return;

	// filestream has no way to clear its own error flag, rewinding resets it
	// along with the file position, so the current position is restored.
	int64 oldPos = filestream_tell(_handle);
	filestream_rewind(_handle);
	if (oldPos > 0)
		filestream_seek(_handle, oldPos, RETRO_VFS_SEEK_POSITION_START);
}

bool LibRetroStream::eos() const {
	return _eos;
}

int64 LibRetroStream::pos() const {
	return filestream_tell(_handle);
}

int64 LibRetroStream::size() const {
	return filestream_get_size(_handle);
}

bool LibRetroStream::seek(int64 offs, int whence) {
	int position;

	switch (whence) {
	case SEEK_CUR:
		position = RETRO_VFS_SEEK_POSITION_CURRENT;
		break;
	case SEEK_END:
		position = RETRO_VFS_SEEK_POSITION_END;
		break;
	case SEEK_SET:
	default:
		position = RETRO_VFS_SEEK_POSITION_START;
		break;
	}

	if (filestream_seek(_handle, offs, position) < 0)
		return false;

	// As with stdio streams, seeking clears the end of stream condition
	_eos = false;

	return true;
}

uint32 LibRetroStream::read(void *dataPtr, uint32 dataSize) {
	int64 bytesRead = filestream_read(_handle, dataPtr, dataSize);

	if (bytesRead < 0) {
		_err = true;
		return 0;
	}

	if ((uint32)bytesRead < dataSize)
		_eos = true;

	return (uint32)bytesRead;
}

uint32 LibRetroStream::write(const void *dataPtr, uint32 dataSize) {
	int64 bytesWritten = filestream_write(_handle, dataPtr, dataSize);

	if (bytesWritten < 0) {
		_err = true;
		return 0;
	}

	if ((uint32)bytesWritten < dataSize)
		_err = true;

	return (uint32)bytesWritten;
}

bool LibRetroStream::flush() {
	return filestream_flush(_handle) == 0;
}

LibRetroStream *LibRetroStream::makeFromPath(const Common::String &path, WriteMode writeMode) {
	Common::String tmpPath(path);

	// In atomic mode a temporary file is created and renamed when the stream
	// is destroyed
	if (writeMode == WriteMode_WriteAtomic)
		tmpPath += ".tmp";

	RFILE *handle = filestream_open(tmpPath.c_str(),
	                                writeMode == WriteMode_Read ? RETRO_VFS_FILE_ACCESS_READ : RETRO_VFS_FILE_ACCESS_WRITE,
	                                RETRO_VFS_FILE_ACCESS_HINT_NONE);

	if (!handle)
		return nullptr;

	LibRetroStream *stream = new LibRetroStream(handle);

	// Store the final path alongside the stream: if _path is not nullptr, it
	// will be used to rename the file when closing it
	if (writeMode == WriteMode_WriteAtomic)
		stream->_path = new Common::String(path);

	return stream;
}
