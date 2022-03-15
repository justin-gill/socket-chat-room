/*********** DO NOT MODIFY this file *******************/

#include <stdint.h>

#ifdef DEBUG_FLAG
#define DEBUG_wARG(fmt, ...) fprintf(stderr, fmt, __VA_ARGS__)
#define DEBUG_MSG(msg) fprintf(stderr, "%s", msg);
#else
#define DEBUG_wARG(fmt, ...)
#define DEBUG_MSG(msg)
#endif

#define PRINT_wARG(fmt, ...) printf(fmt, __VA_ARGS__); fflush(stdout)
#define PRINT_MSG(msg) printf("%s", msg); fflush(stdout)

int read_stdin(char *buf, int buf_len, int *more);
int parse_mt_bit(uint8_t *header);
int parse_control_header(uint16_t *header, uint8_t *mt, uint8_t *code, uint8_t *unc, uint8_t *ulen);
int create_control_header(uint16_t *_header, uint8_t mt, uint8_t code, uint8_t unc, uint8_t ulen);
int parse_chat_header(uint32_t *header, uint8_t *mt, uint8_t *pub, uint8_t *prv, uint8_t *frg, uint8_t *lst, uint8_t *ulen, uint16_t *length);
int create_chat_header(uint32_t *header, uint8_t mt, uint8_t pub, uint8_t prv, uint8_t frg, uint8_t lst, uint8_t ulen, uint16_t length);
void print_u8_as_bits(uint8_t x);
void print_u16_as_bits(uint16_t x);
void print_u32_as_bits(uint32_t x);


/* Macros for printing info on client side */
#define PRINT_SERVER_FULL PRINT_MSG("Server is full\n");
#define PRINT_USER_PROMPT(n) PRINT_wARG("Choose a username (should be less than %d): ", (n) + 1); // n is max_ulen
#define PRINT_USER_JOINED(user) PRINT_wARG("%% User <%s> has joined\n", user);
#define PRINT_USER_LEFT(user) PRINT_wARG("%% User <%s> has left\n", user);
#define PRINT_INVALID_RECIPIENT(user) PRINT_wARG("%% Warning: user %s doesn't exist\n", user);
#define PRINT_CHAT_PROMPT PRINT_MSG("> ");
#define PRINT_PUBLIC_MSG(uname, msg) PRINT_wARG("[%s] %s\n", uname, msg);
#define PRINT_PRIVATE_MSG(from, to, msg) PRINT_wARG("[%s->%s] %s\n", from, to, msg);
