#include "id3v2.h"

// Synchsafe integer decode (7 bits per byte)
static uint32_t synchsafe_to_uint32(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 21) |
           ((uint32_t)buf[1] << 14) |
           ((uint32_t)buf[2] << 7) |
           ((uint32_t)buf[3]);
}


// Regular 32-bit integer decode (big-endian)
static uint32_t bytes_to_uint32(const uint8_t *buf) {
    return ((uint32_t)buf[0] << 24) |
           ((uint32_t)buf[1] << 16) |
           ((uint32_t)buf[2] << 8) |
           ((uint32_t)buf[3]);
}

// Initialize parser
void id3_parser_init(ID3Parser *parser, 
                     void (*callback)(const char*, const uint8_t*, uint32_t)) {
    memset(parser, 0, sizeof(ID3Parser));
    parser->state = STATE_FIND_HEADER;
    parser->frame_callback = callback;
}

// Free any allocated memory
void id3_parser_cleanup(ID3Parser *parser) {
    if (parser->current_frame.data) {
        free(parser->current_frame.data);
        parser->current_frame.data = NULL;
    }
}

// Process a chunk of data
int id3_parser_feed(ID3Parser *parser, const uint8_t *data, size_t len) {
    size_t i = 0;
    
    while (i < len && parser->state != STATE_DONE) {
        switch (parser->state) {
            case STATE_FIND_HEADER:
                // Look for "ID3" signature
                if (data[i] == 'I' && i + 2 < len && 
                    data[i+1] == 'D' && data[i+2] == '3') {
                    parser->buffer[0] = data[i];
                    parser->buffer[1] = data[i+1];
                    parser->buffer[2] = data[i+2];
                    parser->buf_pos = 3;
                    parser->state = STATE_READ_HEADER;
                    i += 3;
                } else {
                    i++;
                }
                break;
                
            case STATE_READ_HEADER:
                // Read 10-byte ID3v2 header
                while (parser->buf_pos < 10 && i < len) {
                    parser->buffer[parser->buf_pos++] = data[i++];
                }
                
                if (parser->buf_pos == 10) {
                    parser->version = parser->buffer[3];
                    parser->revision = parser->buffer[4];
                    parser->flags = parser->buffer[5];
                    parser->tag_size = synchsafe_to_uint32(&parser->buffer[6]);
                    parser->bytes_processed = 0;
                    
                    // Check for extended header (ID3v2.3+)
                    if ((parser->flags & 0x40) && parser->version >= 3) {
                        parser->state = STATE_READ_EXT_HEADER;
                        parser->buf_pos = 0;
                        parser->ext_bytes_read = 0;
                    } else {
                        parser->state = STATE_READ_FRAME_HEADER;
                        parser->buf_pos = 0;
                    }
                }
                break;
                
            case STATE_READ_EXT_HEADER:
                // Read extended header size first (4 bytes for v2.3, synchsafe for v2.4)
                while (parser->buf_pos < 4 && i < len && 
                       parser->bytes_processed < parser->tag_size) {
                    parser->buffer[parser->buf_pos++] = data[i++];
                    parser->bytes_processed++;
                }
                
                if (parser->buf_pos == 4) {
                    if (parser->version == 4) {
                        parser->ext_header_size = synchsafe_to_uint32(parser->buffer);
                    } else {
                        parser->ext_header_size = bytes_to_uint32(parser->buffer);
                    }
                    parser->ext_bytes_read = 4;
                }
                
                // Skip rest of extended header
                while (parser->ext_bytes_read < parser->ext_header_size && i < len &&
                       parser->bytes_processed < parser->tag_size) {
                    i++;
                    parser->ext_bytes_read++;
                    parser->bytes_processed++;
                }
                
                if (parser->ext_bytes_read >= parser->ext_header_size) {
                    parser->state = STATE_READ_FRAME_HEADER;
                    parser->buf_pos = 0;
                }
                break;
                
            case STATE_READ_FRAME_HEADER:
                // Check if we've processed all tag data
                if (parser->bytes_processed >= parser->tag_size) {
                    parser->state = STATE_DONE;
                    break;
                }
                
                // Read frame header (10 bytes for v2.3+, 6 bytes for v2.2)
                size_t header_size = (parser->version >= 3) ? 10 : 6;
                
                while (parser->buf_pos < header_size && i < len &&
                       parser->bytes_processed < parser->tag_size) {
                    parser->buffer[parser->buf_pos++] = data[i++];
                    parser->bytes_processed++;
                }
                
                if (parser->buf_pos == header_size) {
                    // Check for padding (all zeros)
                    if (parser->buffer[0] == 0) {
                        parser->state = STATE_DONE;
                        break;
                    }
                    
                    // Parse frame header
                    if (parser->version >= 3) {
                        memcpy(parser->current_frame.id, parser->buffer, 4);
                        parser->current_frame.id[4] = '\0';
                        
                        if (parser->version == 4) {
                            parser->current_frame.size = synchsafe_to_uint32(&parser->buffer[4]);
                        } else {
                            parser->current_frame.size = bytes_to_uint32(&parser->buffer[4]);
                        }
                        parser->current_frame.flags = (parser->buffer[8] << 8) | parser->buffer[9];
                    } else {
                        // ID3v2.2 uses 3-char frame IDs
                        memcpy(parser->current_frame.id, parser->buffer, 3);
                        parser->current_frame.id[3] = '\0';
                        parser->current_frame.size = (parser->buffer[3] << 16) |
                                                     (parser->buffer[4] << 8) |
                                                     parser->buffer[5];
                        parser->current_frame.flags = 0;
                    }
                    
                    // Allocate buffer for frame data
                    parser->current_frame.data = malloc(parser->current_frame.size);
                    if (!parser->current_frame.data) {
                        return -1; // Memory allocation failed
                    }
                    parser->current_frame.data_read = 0;
                    parser->frame_valid = 1;
                    parser->state = STATE_READ_FRAME_DATA;
                }
                break;
                
            case STATE_READ_FRAME_DATA:
                // Read frame data
                while (parser->current_frame.data_read < parser->current_frame.size &&
                       i < len && parser->bytes_processed < parser->tag_size) {
                    parser->current_frame.data[parser->current_frame.data_read++] = data[i++];
                    parser->bytes_processed++;
                }
                
                // Check if frame is complete
                if (parser->current_frame.data_read >= parser->current_frame.size) {
                    // Call callback with completed frame
                    if (parser->frame_callback) {
                        parser->frame_callback(
                            parser->current_frame.id,
                            parser->current_frame.data,
                            parser->current_frame.size
                        );
                    }
                    
                    // Free frame data and prepare for next frame
                    free(parser->current_frame.data);
                    parser->current_frame.data = NULL;
                    parser->frame_valid = 0;
                    parser->buf_pos = 0;
                    parser->state = STATE_READ_FRAME_HEADER;
                }
                break;
                
            case STATE_DONE:
                return 1; // Parsing complete
        }
    }
    
    return 0; // More data needed
}
