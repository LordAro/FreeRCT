/*
 * This file is part of FreeRCT.
 * FreeRCT is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, version 2.
 * FreeRCT is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details. You should have received a copy of the GNU General Public License along with FreeRCT. If not, see <http://www.gnu.org/licenses/>.
 */

/** @file sprite_data.cpp Code for sprite data. */

#include "stdafx.h"
#include "palette.h"
#include "sprite_data.h"
#include "fileio.h"
#include "bitmath.h"
#include "video.h"

#include <memory>
#include <vector>

static const int MAX_IMAGE_COUNT = 5000; ///< Maximum number of images that can be loaded (arbitrary number).

static std::vector<std::unique_ptr<ImageData>> _sprites;  ///< Available sprites to the program.

ImageData::ImageData() : flags(0), width(0), height(0), xoffset(0), yoffset(0)
{
}

ImageData::~ImageData()
{
}

/**
 * Load the sizes of the image.
 * @param rcd_file RCD file to load from.
 * @param length Remaining length of RCD file.
 * @return True iff loaded sizes are reasonable.
 */
bool ImageData::LoadSizes(RcdFileReader *rcd_file, size_t length)
{
	if (length < 8) return false; // 2 bytes width, 2 bytes height, 2 bytes x-offset, and 2 bytes y-offset
	this->width  = rcd_file->GetUInt16();
	this->height = rcd_file->GetUInt16();
	this->xoffset = rcd_file->GetInt16();
	this->yoffset = rcd_file->GetInt16();

	/* Check against some arbitrary limits that look sufficient at this time. */
	if (this->width == 0 || this->width > 300 || this->height == 0 || this->height > 500) return false;

	length -= 8;
	return length <= 100 * 1024; // Another arbitrary limit.
}

/**
 * @fn ImageData::GetPixel(uint16 xoffset, uint16 yoffset, const Recolouring *recolour, GradientShift shift) const
 * Return the pixel-value of the provided position.
 * @param xoffset Horizontal offset in the sprite.
 * @param yoffset Vertical offset in the sprite.
 * @param recolour Recolouring to apply to the retrieved pixel. Use \c nullptr for disabling recolouring.
 * @param shift Gradient shift to apply to the retrieved pixel. Use #GS_NORMAL for not shifting the colour.
 * @return Pixel value at the given position (\c 0 if transparent).
 */

/**
 * @fn ImageData::BlitImages(const ClippedRectangle &cr, int32 x_base, int32 y_base, uint16 numx, uint16 numy, const Recolouring &recolour, GradientShift shift) const
 * Blit images to the screen.
 * @param cr Clipped rectangle to draw to.
 * @param x_base Base X coordinate of the sprite data.
 * @param y_base Base Y coordinate of the sprite data.
 * @param numx Number of sprites to draw in horizontal direction.
 * @param numy Number of sprites to draw in vertical direction.
 * @param recolour Sprite recolouring definition.
 * @param shift Gradient shift.
 */


/**
 * 32bpp images.
 * @ingroup sprites_group
 */
class ImageData32bpp : public ImageData {
public:
	ImageData32bpp();
	ImageData32bpp(const ImageData32bpp &) = delete;

	~ImageData32bpp();

	bool LoadData(RcdFileReader *rcd_file, size_t length) override;
	uint32 GetPixel(uint16 xoffset, uint16 yoffset, const Recolouring *recolour = nullptr, GradientShift shift = GS_NORMAL) const override;
	void BlitImages(const ClippedRectangle &cr, int32 x_base, int32 y_base, uint16 numx, uint16 numy, const Recolouring &recolour, GradientShift shift) const override;

	uint8 *data; ///< The image data itself.
};

/**
 * 8bpp images.
 * @ingroup sprites_group
 */
class ImageData8bpp : public ImageData {
public:
	ImageData8bpp();
	ImageData8bpp(const ImageData8bpp &) = delete;

	~ImageData8bpp();

	bool LoadData(RcdFileReader *rcd_file, size_t length) override;
	uint32 GetPixel(uint16 xoffset, uint16 yoffset, const Recolouring *recolour = nullptr, GradientShift shift = GS_NORMAL) const override;
	void BlitImages(const ClippedRectangle &cr, int32 x_base, int32 y_base, uint16 numx, uint16 numy, const Recolouring &recolour, GradientShift shift) const override;

	uint32 *table; ///< The jump table. For missing entries, #INVALID_JUMP is used.
	uint8 *data;   ///< The image data itself.
};

/**
 * Blit a pixel to an area of \a numx times \a numy sprites.
 * @param cr Clipped rectangle.
 * @param scr_base Base address of the screen array.
 * @param xmin Minimal x position.
 * @param ymin Minimal y position.
 * @param numx Number of horizontal count.
 * @param numy Number of vertical count.
 * @param width Width of an image.
 * @param height Height of an image.
 * @param colour Pixel value to blit.
 * @note Function does not handle alpha blending of the new pixel with the background.
 */
static void BlitPixel(const ClippedRectangle &cr, uint32 *scr_base,
		int32 xmin, int32 ymin, uint16 numx, uint16 numy, uint16 width, uint16 height, uint32 colour)
{
	const int32 xend = xmin + numx * width;
	const int32 yend = ymin + numy * height;
	while (ymin < yend) {
		if (ymin >= cr.height) return;

		if (ymin >= 0) {
			uint32 *scr = scr_base;
			int32 x = xmin;
			while (x < xend) {
				if (x >= cr.width) break;
				if (x >= 0) *scr = colour;

				x += width;
				scr += width;
			}
		}
		ymin += height;
		scr_base += height * cr.pitch;
	}
}

/**
 * Blend new pixel (\a r, \a g, \a b) with \a old_pixel.
 * @param r Red channel of the new pixel.
 * @param g Green channel of the new pixel.
 * @param b Blue channel of the new pixel.
 * @param old_pixel Previous plotted pixel.
 * @param opacity Opacity of the new pixel.
 * @return The resulting pixel colour (always fully opaque).
 */
static uint32 BlendPixels(uint r, uint g, uint b, uint32 old_pixel, uint opacity)
{
	r = r * opacity + GetR(old_pixel) * (256 - opacity);
	g = g * opacity + GetG(old_pixel) * (256 - opacity);
	b = b * opacity + GetB(old_pixel) * (256 - opacity);

	/* Opaque, but colour adjusted depending on the old pixel. */
	return MakeRGBA(r >> 8, g >> 8, b >> 8, OPAQUE);
}

ImageData8bpp::ImageData8bpp() : ImageData(), table(nullptr), data(nullptr)
{
}

ImageData8bpp::~ImageData8bpp()
{
	delete[] table;
	delete[] data;
}

/**
 * Load image data from the RCD file.
 * @param rcd_file File to load from.
 * @param length Length of the image data block.
 * @return Load was successful.
 * @pre File pointer is at first byte of the block.
 */
bool ImageData8bpp::LoadData(RcdFileReader *rcd_file, size_t length)
{
	size_t jmp_table = 4 * this->height;
	if (length <= jmp_table) return false; // You need at least place for the jump table.
	length -= jmp_table;

	this->table = new uint32[jmp_table / 4];
	this->data  = new uint8[length];
	if (this->table == nullptr || this->data == nullptr) return false;

	/* Load jump table, adjusting the entries while loading. */
	for (uint i = 0; i < this->height; i++) {
		uint32 dest = rcd_file->GetUInt32();
		if (dest == 0) {
			this->table[i] = INVALID_JUMP;
			continue;
		}
		dest -= jmp_table;
		if (dest >= length) return false;
		this->table[i] = dest;
	}

	rcd_file->GetBlob(this->data, length); // Load the image data.

	/* Verify the image data. */
	for (uint i = 0; i < this->height; i++) {
		uint32 offset = this->table[i];
		if (offset == INVALID_JUMP) continue;

		uint32 xpos = 0;
		for (;;) {
			if (offset + 2 >= length) return false;
			uint8 rel_pos = this->data[offset];
			uint8 count = this->data[offset + 1];
			xpos += (rel_pos & 127) + count;
			offset += 2 + count;
			if ((rel_pos & 128) == 0) {
				if (xpos >= this->width || offset >= length) return false;
			} else {
				if (xpos > this->width || offset > length) return false;
				break;
			}
		}
	}
	return true;
}

uint32 ImageData8bpp::GetPixel(uint16 xoffset, uint16 yoffset, const Recolouring *recolour, GradientShift shift) const
{
	if (xoffset >= this->width) return _palette[0];
	if (yoffset >= this->height) return _palette[0];

	uint32 offset = this->table[yoffset];
	if (offset == INVALID_JUMP) return _palette[0];

	uint16 xpos = 0;
	while (xpos <= xoffset) {
		uint8 rel_pos = this->data[offset];
		uint8 count = this->data[offset + 1];
		xpos += (rel_pos & 127);
		if (xpos > xoffset) return _palette[0];
		if (xoffset - xpos < count) {
			uint8 pixel = this->data[offset + 2 + xoffset - xpos];
			if (recolour != nullptr) {
				const uint8 *recolour_table = recolour->GetPalette(shift);
				pixel = recolour_table[pixel];
			}
			return _palette[pixel];
		}
		xpos += count;
		offset += 2 + count;
		if ((rel_pos & 128) != 0) break;
	}
	return _palette[0];
}

void ImageData8bpp::BlitImages(const ClippedRectangle &cr, int32 x_base, int32 y_base, uint16 numx, uint16 numy, const Recolouring &recolour, GradientShift shift) const
{
	const uint8 *recoloured = recolour.GetPalette(shift);
	uint32 *line_base = cr.address + x_base + cr.pitch * y_base;
	int32 ypos = y_base;
	for (int yoff = 0; yoff < this->height; yoff++) {
		uint32 offset = this->table[yoff];
		if (offset != INVALID_JUMP) {
			int32 xpos = x_base;
			uint32 *src_base = line_base;
			for (;;) {
				uint8 rel_off = this->data[offset];
				uint8 count   = this->data[offset + 1];
				uint8 *pixels = &this->data[offset + 2];
				offset += 2 + count;

				xpos += rel_off & 127;
				src_base += rel_off & 127;
				while (count > 0) {
					uint32 colour = _palette[recoloured[*pixels]];
					if (GetA(colour) != OPAQUE) {
						colour =  BlendPixels(GetR(colour), GetG(colour), GetB(colour), *src_base, GetA(colour));
					}
					BlitPixel(cr, src_base, xpos, ypos, numx, numy, this->width, this->height, colour);
					pixels++;
					xpos++;
					src_base++;
					count--;
				}
				if ((rel_off & 128) != 0) break;
			}
		}
		line_base += cr.pitch;
		ypos++;
	}
}


ImageData32bpp::ImageData32bpp() : ImageData(), data(nullptr)
{
}

ImageData32bpp::~ImageData32bpp()
{
	delete[] this->data;
}

/**
 * Load a 32bpp image.
 * @param rcd_file Input stream to read from.
 * @param length Length of the 32bpp block.
 * @return Exit code, \0 means ok, every other number indicates an error.
 */
bool ImageData32bpp::LoadData(RcdFileReader *rcd_file, size_t length)
{
	/* Allocate and load the image data. */
	this->data = new uint8[length];
	if (this->data == nullptr) return false;
	rcd_file->GetBlob(this->data, length);

	/* Verify the data. */
	uint8 *abs_end = this->data + length;
	int line_count = 0;
	const uint8 *ptr = this->data;
	bool finished = false;
	while (ptr < abs_end && !finished) {
		line_count++;

		/* Find end of this line. */
		uint16 line_length = ptr[0] | (ptr[1] << 8);
		const uint8 *end;
		if (line_length == 0) {
			finished = true;
			end = abs_end;
		} else {
			end = ptr + line_length;
			if (end > abs_end) return false;
		}
		ptr += 2;

		/* Read line. */
		bool finished_line = false;
		uint xpos = 0;
		while (ptr < end && !finished_line) {
			uint8 mode = *ptr++;
			if (mode == 0) {
				finished_line = true;
				break;
			}
			xpos += mode & 0x3F;
			switch (mode >> 6) {
				case 0: ptr += 3 * (mode & 0x3F); break;
				case 1: ptr += 1 + 3 * (mode & 0x3F); break;
				case 2: break;
				case 3: ptr += 1 + 1 + (mode & 0x3F); break;
			}
		}
		if (xpos > this->width) return false;
		if (!finished_line) return false;
		if (ptr != end) return false;
	}
	if (line_count != this->height) return false;
	if (ptr != abs_end) return false;
	return true;
}

uint32 ImageData32bpp::GetPixel(uint16 xoffset, uint16 yoffset, const Recolouring *recolour, GradientShift shift) const
{
	if (xoffset >= this->width) return _palette[0];
	if (yoffset >= this->height) return _palette[0];

	const uint8 *ptr = this->data;
	while (yoffset > 0) {
		uint16 length = ptr[0] | (ptr[1] << 8);
		ptr += length;
		yoffset--;
	}
	ptr += 2;
	while (xoffset > 0) {
		uint8 mode = *ptr++;
		if (mode == 0) break;
		if ((mode & 0x3F) < xoffset) {
			xoffset -= mode & 0x3F;
			switch (mode >> 6) {
				case 0: ptr += 3 * (mode & 0x3F); break;
				case 1: ptr += 1 + 3 * (mode & 0x3F); break;
				case 2: ptr++; break;
				case 3: ptr += 1 + 1 + (mode & 0x3F); break;
			}
		} else {
			ShiftFunc sf = GetGradientShiftFunc(shift);
			switch (mode >> 6) {
				case 0:
					ptr += 3 * xoffset;
					return MakeRGBA(sf(ptr[0]), sf(ptr[1]), sf(ptr[2]), OPAQUE);
				case 1: {
					uint8 opacity = *ptr;
					ptr += 1 + 3 * xoffset;
					return MakeRGBA(sf(ptr[0]), sf(ptr[1]), sf(ptr[2]), opacity);
				}
				case 2:
					return _palette[0]; // Arbitrary fully transparent.
				case 3: {
					uint8 opacity = ptr[1];
					if (recolour == nullptr) return MakeRGBA(0, 0, 0, opacity); // Arbitrary colour with the correct opacity.
					const uint32 *table = recolour->GetRecolourTable(ptr[0] - 1);
					ptr += 2 + xoffset;
					uint32 recoloured = table[*ptr];
					return MakeRGBA(sf(GetR(recoloured)), sf(GetG(recoloured)), sf(GetB(recoloured)), opacity);
				}
			}
		}
	}
	return _palette[0]; // Arbitrary fully transparent.
}

/**
 * Blit images to the screen.
 * @param cr Clipped rectangle to draw to.
 * @param x_base Base X coordinate of the sprite data.
 * @param y_base Base Y coordinate of the sprite data.
 * @param numx Number of sprites to draw in horizontal direction.
 * @param numy Number of sprites to draw in vertical direction.
 * @param recolour Sprite recolouring definition.
 * @param shift Gradient shift.
 */
void ImageData32bpp::BlitImages(const ClippedRectangle &cr, int32 x_base, int32 y_base, uint16 numx, uint16 numy, const Recolouring &recolour, GradientShift shift) const
{
	uint32 *line_base = cr.address + x_base + cr.pitch * y_base;
	ShiftFunc sf = GetGradientShiftFunc(shift);
	int32 ypos = y_base;
	const uint8 *src = this->data + 2; // Skip the length word.
	for (int yoff = 0; yoff < this->height; yoff++) {
		int32 xpos = x_base;
		uint32 *src_base = line_base;
		for (;;) {
			uint8 mode = *src++;
			if (mode == 0) break;
			switch (mode >> 6) {
				case 0: // Fully opaque pixels.
					mode &= 0x3F;
					if (shift == GS_SEMI_TRANSPARENT) {
						src += 3 * mode;
						for (; mode > 0; mode--) {
							uint32 ndest = BlendPixels(255, 255, 255, *src_base, OPACITY_SEMI_TRANSPARENT);
							BlitPixel(cr, src_base, xpos, ypos, numx, numy, this->width, this->height, ndest);
							xpos++;
							src_base++;
						}
					} else {
						for (; mode > 0; mode--) {
							uint32 colour = MakeRGBA(sf(src[0]), sf(src[1]), sf(src[2]), OPAQUE);
							BlitPixel(cr, src_base, xpos, ypos, numx, numy, this->width, this->height, colour);
							xpos++;
							src_base++;
							src += 3;
						}
					}
					break;

				case 1: { // Partial opaque pixels.
					uint8 opacity = *src++;
					if (shift == GS_SEMI_TRANSPARENT && opacity > OPACITY_SEMI_TRANSPARENT) opacity = OPACITY_SEMI_TRANSPARENT;
					mode &= 0x3F;
					for (; mode > 0; mode--) {
						uint32 ndest = BlendPixels(sf(src[0]), sf(src[1]), sf(src[2]), *src_base, opacity);
						BlitPixel(cr, src_base, xpos, ypos, numx, numy, this->width, this->height, ndest);
						xpos++;
						src_base++;
						src += 3;
					}
					break;
				}
				case 2: // Fully transparent pixels.
					xpos += mode & 0x3F;
					src_base += mode & 0x3F;
					break;

				case 3: { // Recoloured pixels.
					uint8 layer = *src++;
					const uint32 *table = recolour.GetRecolourTable(layer - 1);
					uint8 opacity = *src++;
					if (shift == GS_SEMI_TRANSPARENT && opacity > OPACITY_SEMI_TRANSPARENT) opacity = OPACITY_SEMI_TRANSPARENT;
					mode &= 0x3F;
					for (; mode > 0; mode--) {
						uint32 colour = table[*src++];
						colour = BlendPixels(sf(GetR(colour)), sf(GetG(colour)), sf(GetB(colour)), *src_base, opacity);
						BlitPixel(cr, src_base, xpos, ypos, numx, numy, this->width, this->height, colour);
						xpos++;
						src_base++;
					}
					break;
				}
			}
		}
		line_base += cr.pitch;
		ypos++;
		src += 2; // Skip the length word.
	}
}

/**
 * Load 8bpp or 32bpp sprite block from the \a rcd_file.
 * @param rcd_file File being loaded.
 * @return Loaded sprite, if loading was successful, else \c nullptr.
 */
ImageData *LoadImage(RcdFileReader *rcd_file)
{
	bool is_8bpp = strcmp(rcd_file->name, "8PXL") == 0;
	if (rcd_file->version != (is_8bpp ? 2 : 1)) return nullptr;

	/* Sad ternary operator */
	std::unique_ptr<ImageData> imd(is_8bpp ?
			std::unique_ptr<ImageData>(new ImageData8bpp()) : std::unique_ptr<ImageData>(new ImageData32bpp()));
	imd->flags = is_8bpp ? (1 << IFG_IS_8BPP) : 0;
	if (imd->LoadSizes(rcd_file, rcd_file->size) && imd->LoadData(rcd_file, rcd_file->size - 8)) {
		_sprites.push_back(std::move(imd));
		return _sprites.back().get();
	}
	return nullptr;
}

/** Initialize image storage. */
void InitImageStorage()
{
	_sprites.reserve(MAX_IMAGE_COUNT);
}

/** Clear all memory. */
void DestroyImageStorage()
{
	_sprites.clear();
}
