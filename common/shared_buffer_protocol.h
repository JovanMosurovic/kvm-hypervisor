#ifndef SHARED_BUFFER_PROTOCOL_H
#define SHARED_BUFFER_PROTOCOL_H

#define SHARED_BUFFER_PORT 0x0510u        /* Mode, byte count and data */
#define SHARED_BUFFER_STATUS_PORT 0x0520u /* Final transferred byte count */
#define BUFFER_SIZE 256u

#define VM_MODE_READER 0u
#define VM_MODE_WRITER 1u

#endif /* SHARED_BUFFER_PROTOCOL_H */
