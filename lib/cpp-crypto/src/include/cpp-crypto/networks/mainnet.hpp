/**
 * This file is part of Ark Cpp Crypto.
 *
 * (c) Ark Ecosystem <info@ark.io>
 *
 * For the full copyright and license information, please view the LICENSE
 * file that was distributed with this source code.
 **/

#ifndef ARK_NETWORKS_MAINNET_HPP
#define ARK_NETWORKS_MAINNET_HPP

#include "common/network.hpp"

namespace Ark {
namespace Crypto {

////////////////////////////////////////////////////////////////////////////////
// Mainnet
// SXP Public Network
const Network Mainnet {
    "16db20c30c52d53638ca537ad0ed113408da3ae686e2c4bfa7e315d4347196dc",  // nethash
    33,                                                                 // slip44
    0xFC,                                                                // wif
    0x3F,                                                                // version
    "2022-03-28T18:00:00.000Z"                                           // epoch
};

}  // namespace Crypto
}  // namespace Ark

#endif
