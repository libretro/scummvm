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

#ifndef WINTERMUTE_RECT32_H
#define WINTERMUTE_RECT32_H

#include "common/system.h"
#include "engines/wintermute/math/floatpoint.h"
#include "common/rect.h"

namespace Wintermute {

struct Point32 {
	int32 x;
	int32 y;
	Point32() : x(0), y(0) {}
	Point32(int32 x1, int32 y1) : x(x1), y(y1) {}
	bool operator==(const Point32 &p) const { return x == p.x && y == p.y; }
	bool operator!=(const Point32 &p) const { return x != p.x || y != p.y; }
	Point32 operator+(const Point32 &delta) const { return Point32(x + delta.x, y + delta.y); }
	Point32 operator-(const Point32 &delta) const { return Point32(x - delta.x, y - delta.y); }

	Point32 &operator+=(const Point32 &delta) {
		x += delta.x;
		y += delta.y;
		return *this;
	}

	Point32 &operator-=(const Point32 &delta) {
		x -= delta.x;
		y -= delta.y;
		return *this;
	}

	operator FloatPoint() {
		return FloatPoint(x,y);
	}


};

struct Rect32 {
	int32 top, left;        ///< The point at the top left of the rectangle (part of the rect).
	int32 bottom, right;    ///< The point at the bottom right of the rectangle (not part of the rect).

	Rect32() : top(0), left(0), bottom(0), right(0) {}
	Rect32(int32 w, int32 h) : top(0), left(0), bottom(h), right(w) {}
	Rect32(const Common::Rect &rect) : top(rect.top), left(rect.left), bottom(rect.bottom), right(rect.right) {}
	Rect32(int32 x1, int32 y1, int32 x2, int32 y2) : top(y1), left(x1), bottom(y2), right(x2) {
		assert(isValidRect());
	}
	bool operator==(const Rect32 &rhs) const {
		return equals(rhs);
	}
	bool operator!=(const Rect32 &rhs) const {
		return !equals(rhs);
	}

	int32 width() const {
		return right - left;
	}
	int32 height() const {
		return bottom - top;
	}

	void setWidth(int32 aWidth) {
		right = left + aWidth;
	}

	void setHeight(int32 aHeight) {
		bottom = top + aHeight;
	}

	void setEmpty() {
		left = right = top = bottom = 0;
	}

	bool isRectEmpty() const {
		return (left >= right) || (top >= bottom);
	}

	void offsetRect(int dx, int dy) {
		left   += dx;
		top    += dy;
		right  += dx;
		bottom += dy;
	}

	void setRect(int32 newLeft, int32 newTop, int32 newRight, int32 newBottom) {
		this->left   = newLeft;
		this->top    = newTop;
		this->right  = newRight;
		this->bottom = newBottom;
	}

	/**
	 * Check if the given rect is equal to this one.
	 *
	 * @param r The rectangle to check
	 *
	 * @return true if the given rect is equal, false otherwise
	 */
	bool equals(const Rect32 &r) const {
		return (left == r.left) && (right == r.right) && (top == r.top) && (bottom == r.bottom);
	}

	bool isValidRect() const {
		return (left <= right && top <= bottom);
	}
};

} // End of namespace Wintermute

#endif
