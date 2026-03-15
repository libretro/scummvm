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

/*
 * Libretro MIDI output driver
 * Based on the ScummVM CoreMIDI and STMIDI drivers
 * Implements RETRO_ENVIRONMENT_GET_MIDI_INTERFACE for MIDI Out support
 */

// Disable symbol overrides so that we can use system headers.
#define FORBIDDEN_SYMBOL_ALLOW_ALL

#include "common/scummsys.h"

#if defined(__LIBRETRO__)

#include <libretro.h>
#include "audio/mpu401.h"
#include "common/error.h"
#include "common/util.h"
#include "audio/musicplugin.h"
#include "backends/platform/libretro/include/libretro-defs.h"

// External libretro MIDI interface
extern struct retro_midi_interface *retro_midi_interface;

class MidiDriver_Libretro : public MidiDriver_MPU401 {
public:
	MidiDriver_Libretro() : _isOpen(false) { }
	int open();
	bool isOpen() const { return _isOpen && (retro_midi_interface != nullptr) && retro_midi_interface->output_enabled(); }
	void close();
	void send(uint32 b) override;
	void sysEx(const byte *msg, uint16 length) override;

private:
	bool _isOpen;
};

int MidiDriver_Libretro::open() {
	if (_isOpen)
		return MERR_ALREADY_OPEN;

	if (!retro_midi_interface) {
		return MERR_DEVICE_NOT_AVAILABLE;
	}

	if (!retro_midi_interface->output_enabled()) {
		return MERR_DEVICE_NOT_AVAILABLE;
	}

	_isOpen = true;
	return 0;
}

void MidiDriver_Libretro::close() {
	MidiDriver_MPU401::close();
	_isOpen = false;
}

void MidiDriver_Libretro::send(uint32 b) {
	midiDriverCommonSend(b);

	if (!retro_midi_interface || !retro_midi_interface->output_enabled())
		return;

	byte status_byte = (b & 0x000000FF);
	byte first_byte = (b & 0x0000FF00) >> 8;
	byte second_byte = (b & 0x00FF0000) >> 16;

	// Calculate delta time (in microseconds since last write)
	// For simplicity, we use 0 delta time for all messages
	// This is acceptable for most MIDI output scenarios
	retro_midi_interface->write(status_byte, 0);

	switch (b & 0xF0) {
	case 0x80:	// Note Off
	case 0x90:	// Note On
	case 0xA0:	// Polyphonic Key Pressure
	case 0xB0:	// Controller
	case 0xE0:	// Pitch Bend
		retro_midi_interface->write(first_byte, 0);
		retro_midi_interface->write(second_byte, 0);
		break;
	case 0xC0:	// Program Change
	case 0xD0:	// Aftertouch
		retro_midi_interface->write(first_byte, 0);
		break;
	default:
		// Unknown message type
		break;
	}
}

void MidiDriver_Libretro::sysEx(const byte *msg, uint16 length) {
	midiDriverCommonSysEx(msg, length);

	if (!retro_midi_interface || !retro_midi_interface->output_enabled())
		return;

	// Send SysEx start
	retro_midi_interface->write(0xF0, 0);

	// Send SysEx data (excluding F0 start and F7 end)
	for (uint16 i = 0; i < length; i++) {
		retro_midi_interface->write(msg[i], 0);
	}

	// Send SysEx end
	retro_midi_interface->write(0xF7, 0);

	// Flush to ensure all data is sent
	retro_midi_interface->flush();
}

// Plugin interface

class LibretroMusicPlugin : public MusicPluginObject {
public:
	const char *getName() const {
		return "Libretro";
	}

	const char *getId() const {
		return "libretro";
	}

	MusicDevices getDevices() const;
	Common::Error createInstance(MidiDriver **mididriver, MidiDriver::DeviceHandle = 0) const;
};

MusicDevices LibretroMusicPlugin::getDevices() const {
	MusicDevices devices;
	// Return GM device as default
	devices.push_back(MusicDevice(this, "", MT_GM));
	return devices;
}

Common::Error LibretroMusicPlugin::createInstance(MidiDriver **mididriver, MidiDriver::DeviceHandle) const {
	*mididriver = new MidiDriver_Libretro();

	return Common::kNoError;
}

REGISTER_PLUGIN_STATIC(LIBRETRO, PLUGIN_TYPE_MUSIC, LibretroMusicPlugin);

#endif
