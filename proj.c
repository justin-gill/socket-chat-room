
#include <stdio.h>
#include <string.h>

#include "proj.h"

/**
 * Read stdin until the newline character or at most buf_len-1 characters, and save read characters in buf.
 *
 * The newline character, if any, is retained. If any characters are read, a `\0` in appended to end the string.
 *
 * @param buf Result parameter. On successful read of stdin, this is filled with read input (at most buf_len)
 * @param bul_len Length of buffer. Read at most these many characters.
 * @param more Result parameter. Contains 1 if there is more data on stdin that can be read, 0 otherwise
 * @return length of characters read from stdin
 */
int read_stdin(char *buf, int buf_len, int *more) {
    char * input = fgets(buf, buf_len, stdin);
    *more = (strchr(buf, '\n') == NULL);
    return strlen(input);
}

/**
 * Helper function that prints binary representation of the given int.
 * This can be useful for debugging when implementing header parsing/creation functions.
 *
 * You may want to write your own print_u32_as_bits(uint32_t x) function.
 *
 * @param x
 */
void print_u32_as_bits(uint32_t x) {
    printf("%u: ", x);
    for (int i = 31; i >= 0; i--) {
        printf("%u ", (x & (0x0001 << i)) >> i);
    }
    printf("\n");
}

/**
 * Helper function that prints binary representation of the given int.
 * This can be useful for debugging when implementing header parsing/creation functions.
 *
 * You may want to write your own print_u32_as_bits(uint32_t x) function.
 *
 * @param x
 */
void print_u16_as_bits(uint16_t x) {
    printf("%u: ", x);
    for (int i = 15; i >= 0; i--) {
        printf("%u ", (x & (0x0001 << i)) >> i);
    }
    printf("\n");
}

/**
 * Parse the MT bit in the given first byte of the header.
 *
 * @param header_byte1
 * @return -1 on failure (e.g., header_byte1 is NULL). Otherwise MT bit value (0 or 1)
 */
int parse_mt_bit(uint8_t *header_byte1) {
    if (header_byte1 == NULL) return -1;
    uint8_t mt_mask = 0x80;
    uint8_t mt_bit = *header_byte1 & mt_mask; 
    return (mt_bit ? 1 : 0);
}


/**
 * Parse 2 byte control header.
 *
 * This function simply parses (or unpacks) all the fields fields in a control header. It shouldn't do any validation for the field values. For example, as per
 * specifications ulen should not be greater than 11. But this function shouldn't do that validation. Validation should be done elsewhere.
 *
 * @param header: pointer to a 2-byte header buffer
 * @param mt: Result parameter that'll be filled with Message type field value in the header
 * @param code: Result parameter that'll be filled with CODE field value in the header
 * @param unc: Result parameter that'll be filled with UNC field value in the header
 * @param ulen: Result parameter that'll be filled with ULEN field value in the header
 *
 * @return 1 on success, -1 on failure (e.g., header is NULL).
 */
int parse_control_header(uint16_t *header, uint8_t *mt, uint8_t *code, uint8_t *unc, uint8_t *ulen) {
    if (header == NULL) return -1;
    uint16_t mt_mask = 0x8000;
    uint16_t code_mask = 0x7800;
    uint16_t unc_mask = 0x00F0;
    uint16_t ulen_mask = 0x000F;
    *mt = (*header & mt_mask) >> 15; 
    *code = (*header & code_mask) >> 11;
    *unc = (*header & unc_mask) >> 4;
    *ulen = (*header & ulen_mask);
    return 1;
}

/**
 * Create control message header
 *
 * This function simply packs all the given fields to create a control header. It shouldn't do any validation for the field values. For example, as per
 * specifications ulen should not be greater than 11. But this function shouldn't do that validation. Validation should be done elsewhere.
 *
 * @param header: Pointer to an allocated 2-byte header buffer. This will be filled with the header bytes.
 * @param mt: Message type field value
 * @param code: CODE field value
 * @param unc: UNC field value
 * @param ulen: Username length field value
 *
 * @return 1 on success, -1 on failure (e.g., header is NULL).
 */
int create_control_header(uint16_t *_header, uint8_t mt, uint8_t code, uint8_t unc, uint8_t ulen) {
    if (_header == NULL) return -1;
    uint16_t mt_mask = 0x1;
    uint16_t code_mask = 0xF;
    uint16_t unc_mask = 0xF;
    uint16_t ulen_mask = 0xF;
    mt_mask = (mt & mt_mask) << 15;
    code_mask = (code & code_mask) << 11;
    unc_mask = (unc_mask & unc) << 4;
    ulen_mask = (ulen & ulen_mask);
    *_header = mt_mask | code_mask | unc_mask | ulen_mask;
    return 1;
}


/**
 * Parse chat message header
 *
 * This function simply parses (or unpacks) all the fields fields in a chat header. It shouldn't do any validation for the field values. For example, as per
 * specifications ulen should not be greater than 11. But this function shouldn't do that validation. Validation should be done elsewhere.
 *
 * @return 1 on success, 0 on failure.
 */
int parse_chat_header(uint32_t *header, uint8_t *mt, uint8_t *pub, uint8_t *prv, uint8_t *frg, uint8_t *lst, uint8_t *ulen, uint16_t *length) {
    if (header == NULL) return -1;
    uint32_t mt_mask = 0x80000000;
    uint32_t pub_mask = 0x40000000;
    uint32_t prv_mask = 0x20000000;
    uint32_t frg_mask = 0x10000000;
    uint32_t lst_mask = 0x8000000;
    uint32_t ulen_mask = 0xF000;
    uint32_t length_mask = 0xFFF;
    *mt = (*header & mt_mask) >> 31; 
    *pub = (*header & pub_mask) >> 30;
    *prv = (*header & prv_mask) >> 29; 
    *frg = (*header & frg_mask) >> 28;
    *lst = (*header & lst_mask) >> 27; 
    *ulen = (*header & ulen_mask) >> 12;
    *length = (*header & length_mask);
    return 1;
}


/**
 * Create chat message header.
 *
 * This function simply packs all the given fields to create a chat header. It shouldn't do any validation for the field values. For example, as per
 * specifications ulen should not be greater than 11. But this function shouldn't do that validation. Validation should be done elsewhere.
 *
 * @return 1 on success, -1 on failure (e.g., header is NULL).
 */
int create_chat_header(uint32_t *header, uint8_t mt, uint8_t pub, uint8_t prv, uint8_t frg, uint8_t lst, uint8_t ulen, uint16_t length) {
    if (header == NULL) return -1;
    uint32_t mt_mask = 0x1;
    uint32_t pub_mask = 0x1;
    uint32_t prv_mask = 0x1;
    uint32_t frg_mask = 0x1;
    uint32_t lst_mask = 0x1;
    uint32_t ulen_mask = 0xF;
    uint32_t length_mask = 0xFFF; 
    mt_mask = (mt & mt_mask) << 31;
    pub_mask = (pub & pub_mask) << 30;
    prv_mask = (prv & prv_mask) << 29;
    frg_mask = (frg & frg_mask) << 28;
    lst_mask = (lst & lst_mask) << 27;
    ulen_mask = (ulen & ulen_mask) << 12;
    length_mask = (length & length_mask);
    *header = mt_mask | pub_mask | prv_mask | frg_mask | lst_mask | ulen_mask | length_mask;
    return 1;
}
