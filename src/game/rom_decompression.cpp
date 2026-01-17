#include <cassert>
#include <cstring>
#include <fstream>

#include "zelda_game.h"

uint8_t read_bit_array(std::span<const uint8_t> arr, size_t index) {
    uint8_t byte = arr[index / 8];
    uint8_t bit_index = index % 8;
    return (byte >> (7 - bit_index)) & 1;
}

void naive_copy(std::span<uint8_t> dst, std::span<const uint8_t> src) {
    for (size_t i = 0; i < src.size(); i++) {
        dst[i] = src[i];
    }
}

void mio0_decompress(std::span<const uint8_t> input, std::span<uint8_t> output, uint32_t compressed_stream_offset, uint32_t uncompressed_stream_offset) {
    std::span<const uint8_t> layout_bit_data = input.subspan(0x10); // Advance past the MIO0 header.
    std::span<const uint8_t> compressed_data = input.subspan(compressed_stream_offset);
    std::span<const uint8_t> uncompressed_data = input.subspan(uncompressed_stream_offset);
    int32_t layout_bit_index = 0;

    size_t compressed_input_pos = 0;
    size_t uncompressed_input_pos = 0;
    size_t output_pos = 0;

    size_t input_size = input.size();
    size_t output_size = output.size();

    while (output_pos < output_size) {
        uint8_t layout_bit = read_bit_array(layout_bit_data, layout_bit_index);
        layout_bit_index++;

        // Layout bit set means uncompressed byte.
        if (layout_bit) { 
            output[output_pos++] = uncompressed_data[uncompressed_input_pos++];
        }
        // Layout bit unset means compressed block.
        else {
            int32_t first_byte = compressed_data[compressed_input_pos++];
            int32_t second_byte = compressed_data[compressed_input_pos++];
            uint32_t bytes = first_byte << 8 | second_byte;
            uint32_t offset = (bytes & 0x0FFF) + 1;
            uint32_t length = ((bytes & 0xF000) >> 12) + 3;

            naive_copy(output.subspan(output_pos, length), output.subspan(output_pos - offset, length));
            output_pos += length;
        }
    }
}

#ifdef _MSC_VER
inline uint32_t byteswap(uint32_t val) {
    return _byteswap_ulong(val);
}
#else
constexpr uint32_t byteswap(uint32_t val) {
    return __builtin_bswap32(val);
}
#endif

// Produces a decompressed SF64 rom. This is only needed because the game has compressed code.
// For other recomps using this repo as an example, you can omit the decompression routine and
// set the corresponding fields in the GameEntry if the game doesn't have compressed code,
// even if it does have compressed data.
std::vector<uint8_t> zelda64::decompress_sf64(std::span<const uint8_t> compressed_rom) {
    // Sanity check the rom size and header. These should already be correct from the runtime's check,
    // but it should prevent this file from accidentally being copied to another recomp.
    if (compressed_rom.size() != 0xC00000) {
        assert(false);
        return {};
    }

    if (compressed_rom[0x3B] != 'N' || compressed_rom[0x3C] != 'F' || compressed_rom[0x3D] != 'X' || compressed_rom[0x3E] != 'E') {
        assert(false);
        return {};
    }

    struct DmaDataEntry {
        uint32_t vrom_start;
        uint32_t rom_start;
        uint32_t rom_end;
        uint32_t is_compressed;

        void bswap() {
            vrom_start = byteswap(vrom_start);
            rom_start = byteswap(rom_start);
            rom_end = byteswap(rom_end);
            is_compressed = byteswap(is_compressed);
        }
    };

    DmaDataEntry cur_entry{};
    size_t cur_entry_index = 0;

    constexpr size_t dma_data_rom_addr = 0xDE480;

    std::vector<uint8_t> ret{};
    ret.resize(0xEFFAE0);

    size_t content_end = 0;

    do {
        // Read the entry from the compressed rom.
        size_t cur_entry_rom_address = dma_data_rom_addr + (cur_entry_index++) * sizeof(DmaDataEntry);
        memcpy(&cur_entry, compressed_rom.data() + cur_entry_rom_address, sizeof(DmaDataEntry));
        // Swap the entry to native endianness after reading from the big endian data.
        cur_entry.bswap();

        // Rom end being 0 means the data is already uncompressed, so copy it as-is to vrom start.
        uint32_t is_compressed = cur_entry.is_compressed;
        if (!is_compressed) {
            uint32_t entry_size = cur_entry.rom_end - cur_entry.rom_start;
            memcpy(ret.data() + cur_entry.vrom_start, compressed_rom.data() + cur_entry.rom_start, cur_entry.rom_end - cur_entry.rom_start);

            // Edit the entry to account for it being in a new location now.
            cur_entry.rom_start = cur_entry.vrom_start;
            cur_entry.rom_end = cur_entry.rom_start + entry_size;
        }
        // Otherwise, decompress the input data into the output data.
        else {
            if (cur_entry.rom_end != cur_entry.rom_start) {
                // Validate the presence of the yaz0 header.
                if (compressed_rom[cur_entry.rom_start + 0] != 'M' ||
                    compressed_rom[cur_entry.rom_start + 1] != 'I' ||
                    compressed_rom[cur_entry.rom_start + 2] != 'O' ||
                    compressed_rom[cur_entry.rom_start + 3] != '0')
                {
                    assert(false);
                    return {};
                }
                // Get the fields from the MIO0 header.
                uint32_t entry_decompressed_size = 
                    (compressed_rom[cur_entry.rom_start + 0x4] << 24) |
                    (compressed_rom[cur_entry.rom_start + 0x5] << 16) |
                    (compressed_rom[cur_entry.rom_start + 0x6] <<  8) |
                    (compressed_rom[cur_entry.rom_start + 0x7] <<  0);
                uint32_t compressed_stream_offset = 
                    (compressed_rom[cur_entry.rom_start + 0x8] << 24) |
                    (compressed_rom[cur_entry.rom_start + 0x9] << 16) |
                    (compressed_rom[cur_entry.rom_start + 0xA] <<  8) |
                    (compressed_rom[cur_entry.rom_start + 0xB] <<  0);
                uint32_t uncompressed_stream_offset = 
                    (compressed_rom[cur_entry.rom_start + 0xC] << 24) |
                    (compressed_rom[cur_entry.rom_start + 0xD] << 16) |
                    (compressed_rom[cur_entry.rom_start + 0xE] <<  8) |
                    (compressed_rom[cur_entry.rom_start + 0xF] <<  0);

                size_t compressed_data_rom_start = cur_entry.rom_start;
                size_t entry_compressed_size = cur_entry.rom_end - compressed_data_rom_start;

                std::span input_span = std::span{ compressed_rom }.subspan(compressed_data_rom_start, entry_compressed_size);
                std::span output_span = std::span{ ret }.subspan(cur_entry.vrom_start, entry_decompressed_size);
                mio0_decompress(input_span, output_span, compressed_stream_offset, uncompressed_stream_offset);

                // Edit the entry to account for it being decompressed now.
                cur_entry.rom_start = cur_entry.vrom_start;
                cur_entry.rom_end = cur_entry.rom_start + entry_decompressed_size;
                cur_entry.is_compressed = 0;
            }
        }

        // Swap the entry back to big endian for writing.
        cur_entry.bswap();
        // Write the modified entry to the decompressed rom.
        memcpy(ret.data() + cur_entry_rom_address, &cur_entry, sizeof(DmaDataEntry));
    } while (cur_entry.rom_end != 0);

    return ret;
}
