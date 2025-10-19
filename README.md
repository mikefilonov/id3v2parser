# ID3v2 Streaming Parser

A stateful C library for parsing ID3v2 tags from MP3 files with support for streaming/chunked read. I use it in my Arduino projects, but it is generic enough to work in other contexts.

## Features

- **Streaming Support**: Parse files in arbitrary-sized chunks without loading entire file into memory
- **Stateful Design**: Maintains parsing state across multiple buffer feeds
- **Multi-Version Support**: Compatible with ID3v2.2, ID3v2.3, and ID3v2.4
- **Frame Spanning**: Handles frames that span multiple buffer boundaries
- **Callback-Based**: Non-blocking design with user-defined callbacks
- **Extended Headers**: Properly handles optional ID3v2.3+ extended headers
- **Synchsafe Integers**: Correct handling of ID3v2.4 synchsafe encoding

## Quick Start

### Basic Usage

```c
ID3Parser parser;
id3_parser_init(&parser, your_callback);

// Feed data in any chunk size
while (reading_file) {
    id3_parser_feed(&parser, buffer, bytes_read);
}

id3_parser_cleanup(&parser);
```

## API Reference

### Initialization

```c
void id3_parser_init(ID3Parser *parser, 
                     void (*frame_callback)(const char*, const uint8_t*, uint32_t, void*));
```

Initialize the parser with callback functions.

**Parameters:**
- `parser`: Pointer to ID3Parser struct to initialize
- `frame_callback`: Called when a complete frame is parsed (can be NULL)

### Feeding Data

```c
int id3_parser_feed(ID3Parser *parser, const uint8_t *data, size_t len);
```

Feed a chunk of data to the parser.

**Parameters:**
- `parser`: Initialized parser instance
- `data`: Buffer containing ID3v2 tag data
- `len`: Length of data buffer

**Returns:**
- `0`: Need more data (continue feeding)
- `1`: Parsing complete (all ID3v2 tags processed)
- `-1`: Error (memory allocation failure)

### Cleanup

```c
void id3_parser_cleanup(ID3Parser *parser);
```

Free any allocated memory. Always call this when done parsing.

## Callbacks

### Frame Callback

```c
void frame_callback(const char *frame_id, const uint8_t *data, 
                   uint32_t size);
```

Called for every parsed frame.

**Parameters:**
- `frame_id`: 4-character frame ID (e.g., "TIT2", "APIC")
- `data`: Frame data (including encoding byte for text frames)
- `size`: Size of frame data in bytes

**Common Frame IDs:**
- `TIT2`: Title
- `TPE1`: Artist
- `TALB`: Album
- `APIC`: Attached picture
- `COMM`: Comment

## How It Works

### State Machine

The parser operates as a state machine with the following states:

1. **FIND_HEADER**: Scans buffer for "ID3" signature
2. **READ_HEADER**: Reads 10-byte ID3v2 header
3. **READ_EXT_HEADER**: Reads extended header (if present)
4. **READ_FRAME_HEADER**: Reads frame header (10 bytes for v2.3+)
5. **READ_FRAME_DATA**: Accumulates frame data
6. **DONE**: All tags processed

### Streaming Design

The parser maintains internal buffers for partial reads:

- **Header buffer**: Accumulates header bytes across chunks
- **Frame buffer**: Dynamically allocated for each frame
- **Position tracking**: Remembers position within current structure

This allows parsing frames that span multiple `id3_parser_feed()` calls.

### APIC Parsing

When an APIC frame is detected:

1. Parse text encoding byte
2. Extract MIME type (null-terminated string)
3. Read picture type byte
4. Skip description (encoding-dependent null terminator)
5. Verify PNG signature (0x89504E47...)
6. Call `png_callback` with PNG data

## License

This is example/educational code. Use and modify freely.

## References

- [ID3v2.4 Specification](https://id3.org/id3v2.4.0-structure)
- [ID3v2.3 Specification](https://id3.org/id3v2.3.0)
- [Frame List](https://id3.org/id3v2.4.0-frames)
