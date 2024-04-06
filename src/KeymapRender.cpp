#include <string.h>

#include <string>
#include <vector>
#include <array>

#include "KeymapRender.hpp"

using namespace std::literals;

static constexpr auto fret_height = 4;
static constexpr auto cell_padding = 1;
static constexpr auto char_padding = 1;

static constexpr auto num_belt_rows = 1;
static constexpr auto num_rows = 3;
static constexpr auto num_cols = 10;

static const auto symkey_alpha_table
	= std::vector<std::vector<std::pair<int, int>>>
	{ { {16, 'Q'}, {17, 'W'}, {18, 'E'}, {19, 'R'}, {20, 'T'}, {21, 'Y'}
	  , {22, 'U'}, {23, 'I'}, {24, 'O'}, {25, 'P'} }
	, { {30, 'A'}, {31, 'S'}, {32, 'D'}, {33, 'F'}, {34, 'G'}, {35, 'H'}
	  , {36, 'J'}, {37, 'K'}, {38, 'L'} }
	, { { 0,'\0'}, {44, 'Z'}, {45, 'X'}, {46, 'C'}, {47, 'V'}, {48, 'B'}
	  , {49, 'N'}, {50, 'M'}, {113, '$'} }
};

static const auto belt_labels
	= std::vector<std::pair<char const*, char const*>>
	{ {"Ctrl", "Call"}, {"Meta", "Berry"}
	, {"", ""}, { "Esc", "Back"}
	, {"Tmux", "End"}
};

template <typename RenderFunc>
static void render_map(char const* map_label, unsigned char* pix, size_t width, size_t height, size_t cell_width, size_t cell_height,
	PSF& psf, RenderFunc&& render)
{
	// Set to white background
	::memset(pix, 0xff, width * height);

	// Render belt labels
	auto belt_cell_width = width / 5;
	{ auto row = 0;

		// Render fret
		::memset(&pix[(row * cell_height) * width], 0,
			width * fret_height);

		for (size_t col = 0; col < 5; col++) {

			// Render cell padding
			for (size_t y = fret_height; y < cell_height; y++) {
				for (size_t x = 0; x < cell_padding; x++) {
					auto dx = (col * belt_cell_width) + x;
					auto dy = (row * cell_height) + y;
					pix[dy * width + dx] = 0;
				}
			}

			auto label = (col == 2)
				? map_label
				: belt_labels[col].first;
			auto label_len = ::strlen(label);
			auto key = belt_labels[col].second;
			auto key_len = ::strlen(key);

			// Draw key
			for (auto i = 0; i < key_len; i++) {
				auto x = (col * belt_cell_width) + char_padding + (i * psf.getWidth());
				if (col < 2) {
					x += cell_padding + char_padding;
				} else {
					x += (belt_cell_width - (cell_padding + char_padding + key_len * psf.getWidth()));
				}
				psf.drawUtf16(key[i],
					pix, width, height, x,
					(row * cell_height) + fret_height + char_padding,
					1);
			}

			// Draw label
			auto centered = (belt_cell_width - (label_len * psf.getWidth())) / 2;
			for (auto i = 0; label[i] != '\0'; i++) {
				psf.drawUtf16(label[i],
					pix, width, height,
					(col * belt_cell_width) + char_padding + (i * psf.getWidth()) + centered,
					(row * cell_height) + fret_height + char_padding + psf.getHeight(),
					1);
			}
		}
	}

	// Render rows
	for (size_t row = num_belt_rows; row < num_belt_rows + num_rows; row++) {

		auto key_row = row - num_belt_rows;

		// Render fret
		::memset(&pix[(row * cell_height) * width], 0,
			width * fret_height);

		// Render key contents
		for (size_t col = 0; col < 10; col++) {

			// Render cell padding
			for (size_t y = fret_height; y < cell_height; y++) {
				for (size_t x = 0; x < cell_padding; x++) {
					auto dx = (col * cell_width) + x;
					auto dy = (row * cell_height) + y;
					pix[dy * width + dx] = 0;
				}
			}

			// Get alpha / symbol keys
			if ((symkey_alpha_table.size() <= key_row)
			 || (symkey_alpha_table[key_row].size() <= col)) {
				continue;
			}
			auto const& [symkey, alpha_utf16] = symkey_alpha_table[key_row][col];
			if (symkey == 0) {
				continue;
			}

			// Render mapped key, don't render alpha key if no mapped key
			if (!render(row, col, symkey)) {
				continue;
			}

			// Render alpha key
			psf.drawUtf16(alpha_utf16,
				pix, width, height,
				// Left-align on left half, right-align on right half
				(col * cell_width) + ((col < 5)
					? cell_padding + char_padding
					: cell_width - (char_padding + psf.getWidth())),
				(row * cell_height) + fret_height + char_padding,
				1);
		}
	}
}

// Common initialization
KeymapRender::KeymapRender(unsigned char const* psf_data, size_t psf_size)
	: m_psf{psf_data, psf_size}
	, m_cellWidth{40}
	, m_cellHeight{fret_height + char_padding + 2 * m_psf.getHeight()}
	, m_width{400}
	, m_height{(num_belt_rows + num_rows) * m_cellHeight}
	, m_pix{new unsigned char[m_width * m_height]}
{}

KeymapRender::KeymapRender(unsigned char const* psf_data, size_t psf_size, Keymap const& keymap)
	: KeymapRender(psf_data, psf_size)
{
	render_map("SYMBOLS", m_pix.get(), m_width, m_height, m_cellWidth, m_cellHeight, m_psf,
		[this, &keymap](size_t row, size_t col, int symkey) {

			// Look up symbol
			auto symkeyUtf16 = keymap.find(symkey);
			if (symkeyUtf16 == keymap.end()) {
				return false;
			}

			// Render mapped key
			m_psf.drawUtf16(symkeyUtf16->second,
				m_pix.get(), m_width, m_height,
				// Centered, 2x scale
				(col * m_cellWidth) + (m_cellWidth / 2 - m_psf.getWidth()),
				(row * m_cellHeight) + fret_height
					+ (m_cellHeight / 2 - m_psf.getHeight()),
				2);

			return true;
		}
	);
}

KeymapRender::KeymapRender(unsigned char const* psf_data, size_t psf_size, KeymapRender::ThreeKeymap const& threeKeymap)
	: KeymapRender(psf_data, psf_size)
{
	render_map("META", m_pix.get(), m_width, m_height, m_cellWidth, m_cellHeight, m_psf,
		[this, &threeKeymap](size_t row, size_t col, int symkey) {

			// Look up symbol
			auto symkeyUtf16Pair = threeKeymap.find(symkey);
			if (symkeyUtf16Pair == threeKeymap.end()) {
				return false;
			}

			// Render mapped keys
			auto&& [utf16_1, utf16_2, utf16_3] = symkeyUtf16Pair->second;

			// Exit if all empty
			if ((utf16_1 == '\0') && (utf16_2 == '\0') && (utf16_3 == '\0')) {
				return false;

			// No second character renders first character large
			} else if (utf16_2 == '\0') {

				m_psf.drawUtf16(utf16_1,
					m_pix.get(), m_width, m_height,
					// Centered, 2x scale
					(col * m_cellWidth) + (m_cellWidth / 2 - m_psf.getWidth()),
					(row * m_cellHeight) + fret_height
						+ (m_cellHeight / 2 - m_psf.getHeight()),
					2);

			// Render all
			} else {

				auto start_at_x = (col * m_cellWidth) + ((col < 5)
					? cell_padding + m_psf.getWidth() + char_padding
					: m_cellWidth - (4 * m_psf.getWidth())
				);
				auto y = (row * m_cellHeight) + fret_height
						+ (m_cellHeight / 2) - (m_psf.getHeight() / 2);

				m_psf.drawUtf16(utf16_1,
					m_pix.get(), m_width, m_height,
					start_at_x + (0 * m_psf.getWidth()),
					y,
					1);
				m_psf.drawUtf16(utf16_2,
					m_pix.get(), m_width, m_height,
					start_at_x + (1 * m_psf.getWidth()),
					y,
					1);
				m_psf.drawUtf16(utf16_3,
					m_pix.get(), m_width, m_height,
					start_at_x + (2 * m_psf.getWidth()),
					y,
					1);
			}

			return true;
		}
	);
}
