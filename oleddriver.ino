// DOCS:
// https://cdn.velleman.eu/downloads/29/infosheets/sh1106_datasheet.pdf
// https://www.ti.com/lit/an/slva704/slva704.pdf?ts=1701950009996&ref_url=https%253A%252F%252Fwww.google.com%252F
#define I2C_SCL_PIN                 32
#define I2C_SDA_PIN                 33
#define I2C_BAUD_RATE               100000
#define I2C_WRITE_MODE              0
#define I2C_READ_MODE               1
#define I2C_SCREEN_ADDR             0x3C
#define OLED_WIDTH                  128
#define OLED_HEIGHT                 64
#define OLED_COLS                   128
#define OLED_PAGES                  8
// For some godforsaken reason, my display has an "inset"
// in memory, which causes it to skip the first 2 pages of memory
// when drawing, because the driver is designed for 132 columns
// of display, while my display has 128
#define OLED_COL_OFFSET             2
// vram size = cols * pages
#define OLED_SIZE                   1024
#define OLED_COMMAND                0x00
#define OLED_DATA                   0x40
// Same as the above but with the continuation bit set (meaning
// there are more control bytes to come)
#define OLED_NOCON_COMMAND          0x80
#define OLED_NOCON_DATA             0xC0
// This is meant to be OR'd with the column address bits (4 LSB)
#define OLED_SET_COL_LOW            0x00
// This is meant to be OR'd with the column address bits (4 LSB)
#define OLED_SET_COL_HIGH           0x10
// This is meant to be OR'd with the pump voltage output (2 LSB)
#define OLED_SET_PUMP_OUT           0x30
// This is meant to be OR'd with the line start value (6 LSB)
#define OLED_SET_DISPLAY_START_LINE 0x40
#define OLED_SET_DISPLAY_CONTRAST   0x81
#define OLED_SET_SEG_REMAP_RIGHT    0xA0
#define OLED_SET_SEG_REMAP_LEFT     0xA1
#define OLED_SET_DISPLAY_RAM        0xA4
#define OLED_SET_DISPLAY_ALL        0xA5
#define OLED_SET_DISPLAY_NORMAL     0xA6
#define OLED_SET_DISPLAY_INVERSE    0xA7
#define OLED_SET_MUX_RATIO          0xA8
#define OLED_SET_DC_DC_CONVERTER    0xAD
#define OLED_DC_DC_CONVERTER_ON     0x8B
#define OLED_DC_DVD_CONVERTER_OFF   0x8A
#define OLED_DISPLAY_ON             0xAF
#define OLED_DISPLAY_OFF            0xAE
// This is meant to be OR'd with the page address bits (4 LSB)
#define OLED_SET_PAGE_ADDR          0xB0
#define OLED_COM_SCAN_DIR_INC       0xC0
#define OLED_COM_SCAN_DIR_DEC       0xC8
#define OLED_SET_COM_OFFSET         0xD3
#define OLED_SET_DIV_OSC_RATE       0xD5
#define OLED_SET_CHARGE_PERIODS     0xD9
#define OLED_SET_COM_CONFIG         0xDA
#define OLED_COM_SEQUENTIAL         0x02
#define OLED_COM_ALTERNATIVE        0x12
#define OLED_SET_VCOM_DESLCT_LVL    0xDB
#define OLED_RMW_START              0xE0
#define OLED_RMW_END                0xEE
#define OLED_NOP                    0xE3
#define SPRITE_VER                  0
#define SPRITE_HOR                  1

// The amount of time in microseconds we should wait after
// every pinMode() operation
// Defined as a third of the time it takes to transmit 1 bit
// because each transfer of a bit contains 3 pinMode() operations
unsigned short bus_delay = static_cast<unsigned short>(std::floor(333333 / I2C_BAUD_RATE));
byte OLED_INIT_COMMANDS[] = {
    OLED_COMMAND
   ,OLED_DISPLAY_OFF
   ,OLED_SET_DC_DC_CONVERTER
   ,OLED_DC_DC_CONVERTER_ON
   ,OLED_SET_PUMP_OUT | 0x03
   ,OLED_SET_SEG_REMAP_LEFT
   ,OLED_SET_MUX_RATIO
   ,0x3F
   ,OLED_SET_DIV_OSC_RATE
   ,0x80
   ,OLED_SET_CHARGE_PERIODS
   ,0x1F
   ,OLED_SET_COM_CONFIG
   ,OLED_COM_ALTERNATIVE
   ,OLED_SET_VCOM_DESLCT_LVL
   ,0x40
   ,OLED_COM_SCAN_DIR_DEC
   ,OLED_SET_COM_OFFSET
   ,0x00
   ,OLED_SET_DISPLAY_START_LINE
   ,0x40
   ,OLED_SET_DISPLAY_CONTRAST
   ,0xFF
   ,OLED_SET_DISPLAY_RAM
   ,OLED_SET_DISPLAY_NORMAL
   ,OLED_DISPLAY_ON
};
// Has 8 rows of 128 bytes each, each byte covers a slit of 1 x 8 pixels vertically,
// just like the PAGE/COL/SEG format of the OLED in horizontal addressing mode
// In cartesian coordinates, it would look like this:
// y=0  +----------+
//      |          |
//      |          |
//      |          |
// y=63 +----------+
//     x=0        x=127
// where the index grows by first going right, then
// shifting down by 1 once it reaches the end of a row
byte* oled_vram;
// Divides the VRAM into 8x8 pixel sections, so that when the screen is
// updated, most unchanged pixels are not rewritten
// Given a position in the VRAM i, the corresponding entry in this array is i >> 3
byte* oled_areas;
byte test_spr[] = {
    0b00111100
   ,0b01000010                               
   ,0b01000010
   ,0b00111100
   ,0b00011000
   ,0b11111111
   ,0b00011000
   ,0b00100100
};

// Note:
// pinMode(OUTPUT) = hard LOW (drain line)
// pinMode(INPUT)  = soft HIGH (leave line untouched)

void i2c_init(){
    // First we initialize both lines' output register to LOW
    pinMode(I2C_SCL_PIN, OUTPUT);
    pinMode(I2C_SDA_PIN, OUTPUT);
    digitalWrite(I2C_SCL_PIN, LOW);
    digitalWrite(I2C_SDA_PIN, LOW);
    delayMicroseconds(bus_delay);
    // Now we set both lines to HIGH
    pinMode(I2C_SCL_PIN, INPUT);
    pinMode(I2C_SDA_PIN, INPUT);
    delayMicroseconds(bus_delay);
}

// Returns 1 if we detected an already existing I2C communication
// on the bus (one of the liens was already being drained)
byte i2c_start(){
    if (!digitalRead(I2C_SCL_PIN)) return 1;
    if (!digitalRead(I2C_SDA_PIN)) return 1;
    // Start condition is SDA -> LOW before SCL -> LOW
    pinMode(I2C_SDA_PIN, OUTPUT);
    delayMicroseconds(bus_delay);
    // We can set the SCL pin to LOW afterwards
    pinMode(I2C_SCL_PIN, OUTPUT);
    delayMicroseconds(bus_delay);
    return 0;
}

void i2c_stop(){
    // Stop condition is SDA -> HIGH before SCL -> LOW
    // We need to set SDA to LOW to that we can actually transition
    // it to HIGH while SCL is HIGH
    pinMode(I2C_SDA_PIN, OUTPUT);
    delayMicroseconds(bus_delay);
    pinMode(I2C_SCL_PIN, INPUT);
    delayMicroseconds(bus_delay);
    pinMode(I2C_SDA_PIN, INPUT);
    delayMicroseconds(bus_delay);
    // We don't need to touch SCL afterwards, as it's already at HIGH
}

void i2c_write_addr(byte addr){
    // Addresses are only 7 bits
    for (byte i = 0; i < 7; i++){
        // We need to read bit 6, which corresponds to 0b0100_000 = 0x40
        if (addr & 0x40) pinMode(I2C_SDA_PIN, INPUT);
        else             pinMode(I2C_SDA_PIN, OUTPUT);
        delayMicroseconds(bus_delay);
        pinMode(I2C_SCL_PIN, INPUT);
        delayMicroseconds(bus_delay);
        pinMode(I2C_SCL_PIN, OUTPUT);
        delayMicroseconds(bus_delay);
        // We continously modify and shift the data
        // we are meant to save for better performance
        addr <<= 1;
    }
}

// 0x00      = WRITE
// 0x01-0xFF = READ
void i2c_write_mode(byte mode){
    // Basically the same as set_addr, but just 1 bit
    if (mode) pinMode(I2C_SDA_PIN, INPUT);
    else      pinMode(I2C_SDA_PIN, OUTPUT);
    delayMicroseconds(bus_delay);
    pinMode(I2C_SCL_PIN, INPUT);
    delayMicroseconds(bus_delay);
    pinMode(I2C_SCL_PIN, OUTPUT);
    delayMicroseconds(bus_delay);
}

// Returns 0 if ACK was received, 1 if NACK was received
byte i2c_read_ack(){
    // SCL is already HIGH from the clock stretching
    // We need to set SDA to INPUT because we need to read
    // signal we are getting back
    pinMode(I2C_SDA_PIN, INPUT);
    delayMicroseconds(bus_delay);
    byte temp = digitalRead(I2C_SDA_PIN);
    delayMicroseconds(bus_delay);
    // We set SCL back down to LOW once we have our reading
    pinMode(I2C_SCL_PIN, OUTPUT);
    delayMicroseconds(bus_delay);
    return temp;
}

// Blocks until the SCL line is released to HIGH
void i2c_clock_stretch(){
    // We need to release SCL to actually read it
    pinMode(I2C_SCL_PIN, INPUT);
    delayMicroseconds(bus_delay);
    while (!digitalRead(I2C_SCL_PIN)){
        delayMicroseconds(bus_delay);
    }
}

void i2c_write_byte(byte data){
    //Serial.println(data, HEX);
    // Basically the same as set_addr but with 8 bits
    for (byte i = 0; i < 8; i++){
        if (data & 0x80) pinMode(I2C_SDA_PIN, INPUT);
        else             pinMode(I2C_SDA_PIN, OUTPUT);
        delayMicroseconds(bus_delay);
        pinMode(I2C_SCL_PIN, INPUT);
        delayMicroseconds(bus_delay);
        pinMode(I2C_SCL_PIN, OUTPUT);
        delayMicroseconds(bus_delay);
        data <<= 1;
    }
}

// Returns 0 if everything went well, 1 if NACK was
// received when writing address and mode, 2 if there was
// already an I2C transmission on the bus
byte i2c_start_transmission(byte addr, byte mode){
    // If there is an ongoing transmission on the bus, we should leave
    if (i2c_start()) return 2;
    i2c_write_addr(addr);
    i2c_write_mode(mode);
    i2c_clock_stretch();
    // If NACK was received, notify through serial and retur
    if (i2c_read_ack()){
        Serial.print("NACK received on writing slave address and mode: addr=0x");
        Serial.print(addr, HEX);
        Serial.print(", mode=WRITE\n\0");
        return 1;
    }
    return 0;
}

// Returns 0 if everything went well, 1 if NACK was
// received when sending a data frame
byte i2c_write_buffer(byte* buf, short size){
    for (short i = 0; i < size; i++){
        i2c_write_byte(*(buf+i));
        i2c_clock_stretch();
        // If NACK was received, notify through serial and return
        if (i2c_read_ack()){
            Serial.print("NACK received on writing data to slave: data=0x");
            Serial.print(*(buf+i), HEX);
            Serial.print(", index=");
            Serial.print(i, DEC);
            Serial.print(", size=");
            Serial.print(size, DEC);
            Serial.print("\n\0");
            return 1;
        }
    }
    return 0;
}

// Returns 0 if everything went well, 1 if NACK
// was received when sending the data frame
byte i2c_write_frame(byte frame){
    i2c_write_byte(frame);
    i2c_clock_stretch();
    if (i2c_read_ack()){
        Serial.print("NACK received on writing data to slave: data=0x");
        Serial.print(frame, HEX);
        Serial.print("\n\0");
        return 1;
    }
    return 0;
}

// Returns 0 if eveything went well, 1 if NACK was received
// when sending data, 2 if NACK was received when setting slave
// address and mode, 3 if there was an already existing I2C
// transmission on the bus
// This function is blocking, so it could take a while for larger messages
byte i2c_write_msg(byte addr, byte* data, short size){
    byte start_status = i2c_start_transmission(addr, I2C_WRITE_MODE);
    if (start_status == 1){
        return 2;
    }
    if (start_status == 2){
        i2c_stop();
        return 3;
    }
    byte buffer_write_status = i2c_write_buffer(data, size);
    if (buffer_write_status == 1){
        i2c_stop();
        return 1;
    }
    i2c_stop();
    return 0;
}


void oled_init(){
    oled_vram  = (byte*)malloc(OLED_SIZE);
    oled_areas = (byte*)malloc(OLED_SIZE >> 3);
    oled_clear();
    i2c_init();
    if (!i2c_write_msg(I2C_SCREEN_ADDR, OLED_INIT_COMMANDS, sizeof(OLED_INIT_COMMANDS))){
        Serial.print("OLED init successful\n\0");
    }
    else{
        Serial.print("OLED init failed\n\0");
    }
}

// Updates only an 8x8 area of pixels
void oled_update_area(byte page, byte col){
    short vram_pos = (page * OLED_COLS) + col;
    byte new_pos_coms[] = {
        // Pedantic compiler forces me to put all of these casts
        OLED_NOCON_COMMAND, (byte)(OLED_SET_PAGE_ADDR | page) 
       ,OLED_NOCON_COMMAND, (byte)(OLED_SET_COL_LOW   |   ((col + OLED_COL_OFFSET) & 0x0F))
       ,OLED_NOCON_COMMAND, (byte)(OLED_SET_COL_HIGH  |  (((col + OLED_COL_OFFSET) & 0xF0) >> 4))
    };
    i2c_write_buffer(new_pos_coms, sizeof(new_pos_coms));
    for (byte i = 0; i < 8; i++){
        i2c_write_frame(OLED_NOCON_DATA);
        i2c_write_frame(oled_vram[ + vram_pos + i]);
    }
    *(oled_areas + (vram_pos >> 3)) = 0x00;
}

// Updates only the areas which have been registered have changed in oled_areas
// I have found that it is vastly faster to perform every area update in just 1
// I2C transmission, despite the cost of always having to send control bytes
void oled_display(){
    i2c_start_transmission(I2C_SCREEN_ADDR, I2C_WRITE_MODE);
    for (short i = 0; i < (OLED_SIZE >> 3); i++){
        if (!oled_areas[i]) continue;
        // This is a bit of bitwise magic with the knowledge that the amount
        // of pages are 128 (2^7) and our index represents jumps of 8 (2^3)
        // pixels per pass through the loop
        oled_update_area(i >> 4, (i & 0x0F) << 3);
    }
    i2c_stop();
}

// Updates the whole screen, no exceptions
void oled_display_full(){
    i2c_start_transmission(I2C_SCREEN_ADDR, I2C_WRITE_MODE);
    // Saves a bit of performance by not having to multiply
    // i by OLED_COLS when accessing the VRAM
    for (short i = 0; i < OLED_SIZE; i += OLED_COLS){
        byte new_pos_coms[] = {
            // Tiny bit of bitwise magic, shifting back by the bits taken up by OLED_COLS (2^7)
            // Pedantic compiler forces me to put this cast
            OLED_NOCON_COMMAND, (byte)(OLED_SET_PAGE_ADDR | (i >> 7))
           ,OLED_NOCON_COMMAND, OLED_SET_COL_LOW   |  (OLED_COL_OFFSET & 0x0F)
           ,OLED_NOCON_COMMAND, OLED_SET_COL_HIGH  | ((OLED_COL_OFFSET & 0xF0) >> 4)
        };
        i2c_write_buffer(new_pos_coms, sizeof(new_pos_coms));
        for (byte j = 0; j < OLED_COLS; j++){
            i2c_write_frame(OLED_NOCON_DATA);
            i2c_write_frame(*(oled_vram + i + j));
        }
    }
    i2c_stop();
}

void oled_clear(){
    for (short i = 0; i < OLED_SIZE; i++){
        // We don't need to do anything if the section is already clear (0x00)
        if (!oled_vram[i]) continue;
        oled_vram[i] = 0x00;
        oled_areas[i >> 3] = 0x01;
    }
}

// This function is mostly for testing/debugging purposes
void oled_noise(){
    for (short i = 0; i < OLED_SIZE; i++){
        byte new_byte = random(0x00, 0x100);
        // Despite this ultimately being almost always useless, it's
        // still nice having the "optimization" of only registering a change
        // in the registry when there is an actual change in the VRAM
        if (new_byte == oled_vram[i]) continue;
        oled_vram[i] = new_byte;
        oled_areas[i >> 3] = 0x01;
    }
}

// Sets just 1 bit on/off in the VRAM given by X and Y coordinates, as layed out above
void oled_set_pixel(byte x, byte y, byte color){
    // Take into account only the page component of Y
    short vram_pos = ((y >> 3) * OLED_COLS) + x;
    byte cur_byte = oled_vram[vram_pos];
    // Shift in the bit by the sub-page component of Y
    // It's important to know that within an 8-bit strip of image
    // represented by a byte in the VRAM, each bit corresponds to the
    // following part of the resulting image:
    //   X -> LSB
    //   X
    //   X
    //   X
    //   X
    //   X
    //   X
    //   X -> MSB
    byte new_bit = 0x01 << (y & 0x07);
    // We don't need to do anything if the pixel we want to change already has
    // the color we want to override it to
    // We need to place a ! operator on both sides of the equation because the
    // masked bit we get isn't strictly a 0 or 1, like color is
    if (!(cur_byte & new_bit) == (!color)) return;
    // Override the bit in memory depending on what color we have been given
    if (color) oled_vram[vram_pos] = cur_byte |   new_bit;
    else       oled_vram[vram_pos] = cur_byte & (~new_bit);
    // Update the area registry
    oled_areas[vram_pos >> 3] = 0x01;
}

// Puts an 8x8 bitmap composed of 8 bytes into the VRAM in any coordinates
// The mode indicates if each byte of the 8x8 sprite represents a vertical
// or horizontal strip of it
// The wrap mode is slightly more complicated, as it uses 4 bits of information,
// following this format:
// UUUF YSXZ
// |||| ||||
// |||| |||+-- 0: Draw only the part in the X component before the wrapping
// |||| |||    1: Draw only the part in the X component after the wrapping
// |||| ||+--- 0: Take into account the before bit  1: Always draw the X component
// |||| |+---- 0: Draw only the part in the Y component before the wrapping
// |||| |      1: Draw only the part in the Y component after the wrapping
// |||| +----- 0: Take into account the before bit  1: Always draw the Y component
// |||+------- Indicates that this sprite can be sent to the fast algorithm if possible
// |||        (used in the wrapper function which decides between the fast/slow algorithm)
// +++-------- Unused
// These settings for wrapping make it easy to implement all types of environments,
// such as cyclical maps and transisitions between different stages/rooms
void oled_put_spr_slow(byte x, byte y, byte* bmp, byte mode, byte wrap){
    // The VRAM pointer has accuracy up to the page level
    // in the Y coordinate, which means we need to ignore the
    // 3 LSB and take the bit-level accuracy into account when actually
    // rendering the sprite into memory
    short vram_pos = ((y >> 3) * OLED_COLS) + x;
    for (byte i = 0; i < 8; i++){
        short cur_pos = vram_pos + i;
        byte hor_wrapped = 0x00;
        // Calculate horizontal wrapping
        if ((x + i) >= OLED_COLS){
            cur_pos -= OLED_COLS;
            hor_wrapped = 0x01;
        }
        // Skip this strip depending on the wrapping mode
        if (!(wrap & 0x02)){
            if (((!hor_wrapped) && (wrap & 0x01)) || (hor_wrapped && (!(wrap & 0x01)))) continue;
        }
        // Calculate the byte we should place into the VRAM
        byte new_byte = 0x00;
        // Vertical sprite mode is trivial
        if      (mode == SPRITE_VER) new_byte = bmp[i];
        // Horizontal sprite mode is harder to compute
        else if (mode == SPRITE_HOR){
            // We need to generate the byte by "vertically scanning"
            // each bit column of the bitmap
            for (byte j = 0; j < 8; j++){
                // Choose the pixel that correspods with the column we are writing
                byte cur_bit = 0x01 << i;
                byte masked_bit = bmp[j] & cur_bit;
                // Shift the buffer right to progressively fill it up with the corresponding bits
                new_byte >>= 1;
                // Set the MSB to 1 if the masked bit is set
                if (masked_bit) new_byte |= 0x80;
            }
        }
        // Calculate what bits we should mask in to the
        // top and bottom byte
        // This is very bitwise magic-y, so I recommend scribbling
        // down a little drawing of what should happen and going backwards
        // from there, but basically, we start off with a full bitmask and the
        // remove all the pixels up to the fine part of Y by creating another
        // bitmask full of 1s by subtracting 1 from 2^FINE_Y, which is the same
        // as taking the bitwise complement of that expression with fine Y
        byte top_bits    = ~((0x01 << (y & 0x07)) - 1);
        // Whatever bits we didn't fit onto the top byte,
        // we have to put into the bottom byte
        byte bottom_bits = ~top_bits;
        // Put top byte if the value of the VRAM isn't the one we want to put and the wrap
        // mode allows us to (we know we can never have wrapped vertically on the top byte)
        if ((wrap & 0x08) || (!(wrap & 0x04))){
            byte top_byte = (new_byte << (y & 0x07)) & top_bits;
            if ((oled_vram[cur_pos] & top_bits) != top_byte){
                oled_vram[cur_pos] = (oled_vram[cur_pos] & (~top_bits)) | top_byte;
                oled_areas[cur_pos >> 3] = 0x01;
            }
        }
        
        // Same for the bottom byte, taking into account vertical wrapping
        cur_pos += OLED_COLS;
        byte ver_wrapped = 0x00;
        if (cur_pos >= OLED_SIZE){
            // If wrap is on, we send the pointer back to the top page
            cur_pos -= OLED_SIZE;
            ver_wrapped = 0x01;
        }
        // Now we do the exact same thing as above, except we shift
        // bottom_byte in the other direction
        if ((wrap & 0x08) || ((!(wrap & 0x04)) && (!ver_wrapped)) || ((wrap & 0x04) && ver_wrapped)){
            byte bottom_byte = (new_byte >> (0x08 - (y & 0x07))) & bottom_bits;
            if ((oled_vram[cur_pos] & bottom_bits) != bottom_byte){
                oled_vram[cur_pos] = (oled_vram[cur_pos] & (~bottom_bits)) | bottom_byte;
                oled_areas[cur_pos >> 3] = 0x01;
            }
        }
    }
}

// Same as the above function, Y can only be a multiple of 8, and the X coordinate
// can't cause the sprite too overflow off the screen (X + 7 <= 127)
// This operation is faster than the normal sprite placement operation, because
// it barely has to do any bitwise operations (specially when mode is SPRITE_VER)
void oled_put_spr_fast(byte x, byte y, byte* bmp, byte mode){
    short vram_pos = ((y >> 3) * OLED_COLS) + x;
    for (byte i = 0; i < 8; i++){
        // Same byte calculations as above
        byte new_byte = 0x00;
        if      (mode == SPRITE_VER) new_byte = bmp[i];
        else if (mode == SPRITE_HOR){
            for (byte j = 0; j < 8; j++){
                byte cur_bit = 0x01 << i;
                byte masked_bit = bmp[j] & cur_bit;
                new_byte >>= 1;
                if (masked_bit) new_byte |= 0x80;
            }
        }
        // We don't have to do anything if there value there already is
        // in the VRAM is the same as the one we want to write
        if (new_byte == oled_vram[vram_pos + i]) continue;
        oled_vram[vram_pos + i] = new_byte;
        oled_areas[vram_pos >> 3] = 0x01;
    }
}

// Uses the optimized function if the parameters are within its domain
void oled_put_spr(byte x, byte y, byte* bmp, byte mode, byte wrap){
    if (((y & 0xF8) == y) && (y <= 55) && (x <= 120) && (wrap & 0x10)){
        oled_put_spr_fast(x, y, bmp, mode);
    }
    else{
        oled_put_spr_slow(x, y, bmp, mode, wrap);
    }
}

void setup(){
    Serial.begin(9600);
    randomSeed(time(NULL));
    oled_init();
    byte c = 0;
    while (1){
        oled_clear();
        oled_put_spr(c & 0x7F, (c + 8) & 0x3F, test_spr, SPRITE_HOR, 0x0A);
        oled_display();
        c += 1;
    }
}

void loop(){
    
}
