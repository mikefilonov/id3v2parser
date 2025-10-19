#pragma once


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>



// ID3v2 parser state
typedef enum {
    STATE_FIND_HEADER,
    STATE_READ_HEADER,
    STATE_READ_EXT_HEADER,
    STATE_READ_FRAME_HEADER,
    STATE_READ_FRAME_DATA,
    STATE_DONE
} ParserState;

typedef struct {
    char id[5];
    uint32_t size;
    uint16_t flags;
    uint8_t *data;
    uint32_t data_read;
} ID3Frame;

typedef struct {
    ParserState state;
    uint8_t buffer[10];        // Temp buffer for headers
    size_t buf_pos;            // Position in temp buffer
    
    // ID3v2 header info
    uint8_t version;
    uint8_t revision;
    uint8_t flags;
    uint32_t tag_size;
    uint32_t bytes_processed;
    
    // Extended header
    uint32_t ext_header_size;
    uint32_t ext_bytes_read;
    
    // Current frame being parsed
    ID3Frame current_frame;
    int frame_valid;
    
    // Callback for completed frames
    void (*frame_callback)(const char *id, const uint8_t *data, uint32_t size);
} ID3Parser;


int id3_parser_feed(ID3Parser *parser, const uint8_t *data, size_t len);
void id3_parser_init(ID3Parser *parser, void (*callback)(const char*, const uint8_t*, uint32_t));