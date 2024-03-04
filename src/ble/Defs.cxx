/** @copyright
 * Copyright (c) 2024, Stuart Baker
 * All rights reserved
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are strictly prohibited without written consent.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * @file Defs.cxx
 *
 * BLE specific definitions.
 *
 * @author Stuart Baker
 * @date 2 March 2024
 */

#include "ble/Defs.hxx"

namespace ble
{

const uint8_t Defs::PRIMARY_SERVICE_UUID[2] = {0x00, 0x28};
const uint8_t Defs::SECONDARY_SERVICE_UUID[2] = {0x01, 0x28};
const uint8_t Defs::CHAR_DECLARATOIN_UUID[2] = {0x03, 0x28};
const uint8_t Defs::CHAR_CLIENT_CONFIG_UUID[2] = {0x02, 0x29};
const uint8_t Defs::CHAR_PROP_READ_WRITE_NOTIFY[1] =
{
    (1 << 1) | // read
    (1 << 3) | // write
    (1 << 4)   // notify
};

//
// Defs::adv_find_data()
//
ssize_t Defs::adv_find_data(
    std::vector<uint8_t> &adv, AdvType type, uint8_t *size, unsigned instance)
{
    char t = static_cast<char>(type);

    for (size_t idx = 1; instance && idx < adv.size(); ++idx)
    {
        if (adv[idx] == t)
        {
            uint8_t len = adv[idx - 1];
            if (size)
            {
                *size = len;
            }
            if (--instance == 0)
            {
                return idx;
            }
            // one additional added by the four loop to skip over next length
            idx += len;
        }
    }

    return -1;
}

} // namespace ble
