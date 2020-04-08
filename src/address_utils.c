/*******************************************************************************
* Fantom Ledger App
* (c) 2020 Fantom Foundation
*
* Some parts of the code are derived from Ledger Ethereum App distributed under
* Apache License, Version 2.0. You may obtain a copy of the License at
*
*      http://www.apache.org/licenses/LICENSE-2.0
*
********************************************************************************/
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "os.h"
#include "cx.h"
#include "utils.h"
#include "derive_key.h"
#include "address_utils.h"

// HEXDIGITS defines textual glyphs usable for address generation
static const uint8_t const HEXDIGITS[] = "0123456789abcdef";

// deriveAddress implements address derivation for given BIP44 path.
size_t deriveAddress(bip44_path_t *path, cx_sha3_t *sha3Context, uint8_t *out, size_t outputSize) {
    // make sanity check, the buffer may never exceed this number
    ASSERT(outputSize < MAX_BUFFER_SIZE);

    // prep containers for private key
    private_key_t privateKey;
    chain_code_t chainCode;
    size_t addressSize = 0;

    BEGIN_TRY
    {
        TRY
        {
            // get the private code for the path
            derivePrivateKey(
                    path,
                    &chainCode,
                    &privateKey
            );

            // prep container for public key
            cx_ecfp_public_key_t publicKey;

            // derive the public key from the private one
            deriveRawPublicKey(&privateKey, &publicKey);

            // get raw address for the public key
            addressSize = getRawAddress(&publicKey, sha3Context, out, outputSize);
        }
        FINALLY
        {
            // clear the private key storage so we don't leak it after this call
            os_memset(&privateKey, 0, SIZEOF(privateKey));
        }
    }
    END_TRY;

    return addressSize;
}

// getRawAddress implements wallet address calculation for given public key.
size_t getRawAddress(cx_ecfp_public_key_t *publicKey, cx_sha3_t *sha3Context, uint8_t *out, size_t outputSize) {
    // make sanity check, the buffer may never exceed this number
    ASSERT(outputSize < MAX_BUFFER_SIZE);

    // make a buffer
    uint8_t hashAddress[32];

    // init context for SHA3 calculation
    cx_keccak_init(sha3Context, 256);

    // get the public key hash
    cx_hash((cx_hash_t *) sha3Context, CX_LAST, publicKey->W + 1, 64, hashAddress, 32);

    // move the last 20 bytes of the hash to output buffer
    os_memmove(out, hashAddress + 12, 20);

    return 20;
}


// formatAddressStr implements formatting of a raw address into a human readable textual form.
void formatAddressStr(uint8_t *address, cx_sha3_t *sha3Context, char *out, size_t outputSize) {
    // make sanity check, the buffer may never exceed this number
    ASSERT(outputSize < MAX_BUFFER_SIZE);

    // make sure the address will fit inside the buffer
    VALIDATE(outputSize >= MIN_ADDRESS_STR_BUFFER_SIZE, ERR_ASSERT);

    // prep checksum buffer
    uint8_t hashChecksum[32];
    uint8_t tmp[40];
    uint8_t i;

    // prep base textual address representation (convert BYTE to HEX)
    // so we can calculate SHA3 hash of the address and add folding checksum
    for (i = 0; i < 20; i++) {
        uint8_t digit = address[i];
        tmp[2 * i] = HEXDIGITS[(digit >> 4) & 0x0f];
        tmp[2 * i + 1] = HEXDIGITS[digit & 0x0f];
    }

    // calculate SHA3 hash of the address
    cx_keccak_init(sha3Context, 256);
    cx_hash((cx_hash_t *) sha3Context, CX_LAST, tmp, 40, hashChecksum, 32);

    // parse address digits
    for (i = 0; i < 40; i++) {
        uint8_t digit = address[i / 2];
        if ((i % 2) == 0) {
            digit = (digit >> 4) & 0x0f;
        } else {
            digit = digit & 0x0f;
        }

        // do we need to fold?
        if (digit >= 10) {
            int v = (hashChecksum[i / 2] >> (4 * (1 - i % 2))) & 0x0f;
            if (v >= 8) {
                // fold the digit and continue with the loop
                out[i + 2] = HEXDIGITS[digit] - 'a' + 'A';
                continue;
            }
        }

        // simply copy the digit
        out[i + 2] = HEXDIGITS[digit];
    }

    // add address prefix and terminator
    out[0] = '0';
    out[1] = 'x';
    out[43] = '\0';
}
