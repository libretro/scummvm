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

#ifndef LIBRETRO_STREAM_H
#define LIBRETRO_STREAM_H

#include "common/scummsys.h"
#include "common/noncopyable.h"
#include "common/stream.h"
#include "common/str.h"

/**
 * Stream based on libretro-common's filestream API, which routes file access
 * through the frontend VFS interface whenever one has been negotiated (see
 * retro_init_vfs()) and falls back to the local file system otherwise.
 *
 * It is the counterpart of StdioStream, which cannot be used here as it always
 * goes through stdio, bypassing the frontend.
 */
class LibRetroStream : public Common::SeekableReadStream, public Common::SeekableWriteStream, public Common::NonCopyable {
public:
	enum WriteMode {
		WriteMode_Read = 0,
		WriteMode_Write = 1,
		WriteMode_WriteAtomic = 2,
	};

	/**
	 * Opens the given path and wraps the result in a LibRetroStream instance.
	 *
	 * @param path the file to open.
	 * @param writeMode how the file is to be accessed. In atomic mode the data
	 * is written to a temporary file, which is renamed on stream destruction.
	 *
	 * @return the stream, or nullptr if the file could not be opened.
	 */
	static LibRetroStream *makeFromPath(const Common::String &path, WriteMode writeMode);

	LibRetroStream(struct RFILE *handle);
	~LibRetroStream() override;

	bool err() const override;
	void clearErr() override;
	bool eos() const override;

	uint32 write(const void *dataPtr, uint32 dataSize) override;
	bool flush() override;

	int64 pos() const override;
	int64 size() const override;
	bool seek(int64 offs, int whence = SEEK_SET) override;
	uint32 read(void *dataPtr, uint32 dataSize) override;

private:
	/** File handle to the actual file. */
	struct RFILE *_handle;
	/** Final path of a file opened in atomic mode, nullptr otherwise. */
	Common::String *_path;
	bool _err;
	bool _eos;
};

#endif // LIBRETRO_STREAM_H
