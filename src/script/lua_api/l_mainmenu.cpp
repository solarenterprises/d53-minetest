/*
Minetest
Copyright (C) 2013 sapier

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation; either version 2.1 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/

#include "lua_api/l_mainmenu.h"
#include "lua_api/l_internal.h"
#include "common/c_content.h"
#include "cpp_api/s_async.h"
#include "scripting_mainmenu.h"
#include "gui/guiEngine.h"
#include "gui/guiMainMenu.h"
#include "gui/guiKeyChangeMenu.h"
#include "gui/guiPathSelectMenu.h"
#include "version.h"
#include "porting.h"
#include "filesys.h"
#include "convert_json.h"
#include "content/content.h"
#include "content/subgames.h"
#include "serverlist.h"
#include "mapgen/mapgen.h"
#include "settings.h"
#include "client/client.h"
#include "client/renderingengine.h"
#include "network/networkprotocol.h"
#include "crypto/message.hpp"
#include "common/configuration.hpp"
#include "identities/address.hpp"
#include "identities/keys.hpp"
#include "networks/mainnet.hpp"
#include "bip39/bip39.h"
#include <cstdint>
#include <iostream>
#include "util/base64.h"
#include <iterator>
#include <sstream>

/* ssl stuff */

#include <sstream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <cstring>

#include <openssl/aes.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/evperr.h>
#include <openssl/aes.h>
#include <openssl/crypto.h>

#include <random>

/* ssl stuff */

#define DECL_OPENSSL_PTR(tname, free_func) \
    struct openssl_##tname##_dtor {            \
        void operator()(tname* v) {        \
            free_func(v);              \
        }                              \
    };                                 \
    typedef std::unique_ptr<tname, openssl_##tname##_dtor> tname##_t


DECL_OPENSSL_PTR(EVP_CIPHER_CTX, ::EVP_CIPHER_CTX_free);

struct error : public std::exception {
private:
    std::string m_msg;

public:
    error(const std::string& message)
        : m_msg(message) {
    }

    error(const char* msg)
        : m_msg(msg, msg + strlen(msg)) {
    }

    virtual const char* what() const noexcept override {
        return m_msg.c_str();
    }
};

struct openssl_error: public virtual error {
private:
    int m_code = -1;
    std::string m_msg;

public:
    openssl_error(int code, const std::string& message)
        : error(message),
          m_code(code) {
        std::stringstream ss;
        ss << "[" << m_code << "]: " << message;
        m_msg = ss.str();

    }

    openssl_error(int code, const char* msg)
        : error(msg),
          m_code(code) {
        std::stringstream ss;
        ss << "[" << m_code << "]: " << msg;
        m_msg = ss.str();
    }

    const char* what() const noexcept override {
        return m_msg.c_str();
    }
};

static void throw_if_error(int res = 1, const char* file = nullptr, uint64_t line = 0) {

    unsigned long errc = ERR_get_error();
    if (res <= 0 || errc != 0) {
        if (errc == 0) {
            return;
        }
        std::vector<std::string> errors;
        while (errc != 0) {
            std::vector<std::uint8_t> buf(256);
            ERR_error_string(errc, (char*) buf.data());
            errors.push_back(std::string(buf.begin(), buf.end()));
            errc = ERR_get_error();
        }

        std::stringstream ss;
        ss << "\n";
        for (auto&& err : errors) {
            if (file != nullptr) {
                ss << file << ":" << (line - 1) << " ";
            }
            ss << err << "\n";
        }
        const std::string err_all = ss.str();
        throw openssl_error(errc, err_all);
    }
}

/* Encryption stuff */

class aes256_cbc {
private:
    std::vector<std::uint8_t> m_iv;

public:
    explicit aes256_cbc(std::vector<std::uint8_t> iv)
        : m_iv(std::move(iv)) {
    }

    void encrypt(const std::vector<std::uint8_t>& key, const std::vector<std::uint8_t>& message, std::vector<std::uint8_t>& output) const {
        output.resize(message.size() * AES_BLOCK_SIZE);
        int inlen = message.size();
        int outlen = 0;
        std::size_t total_out = 0;

        EVP_CIPHER_CTX_t ctx(EVP_CIPHER_CTX_new());
        throw_if_error(1, __FILE__, __LINE__);

        // todo: sha256 function
        // const std::vector<uint8_t> enc_key = key.size() != 32 ? sha256(key) : key;

        const std::vector<std::uint8_t> enc_key = key;

        int res;
        res = EVP_EncryptInit(ctx.get(), EVP_aes_256_cbc(), enc_key.data(), m_iv.data());
        throw_if_error(res, __FILE__, __LINE__);
        res = EVP_EncryptUpdate(ctx.get(), output.data(), &outlen, message.data(), inlen);
        throw_if_error(res, __FILE__, __LINE__);
        total_out += outlen;
        res = EVP_EncryptFinal(ctx.get(), output.data()+total_out, &outlen);
        throw_if_error(res, __FILE__, __LINE__);
        total_out += outlen;

        output.resize(total_out);
    }

    void decrypt(const std::vector<std::uint8_t>& key, const std::vector<std::uint8_t>& message, std::vector<std::uint8_t>& output) const {
        output.resize(message.size() * 3);
        int outlen = 0;
        std::size_t total_out = 0;

        EVP_CIPHER_CTX_t ctx(EVP_CIPHER_CTX_new());
        throw_if_error();

        // todo: sha256 function const std::vector<uint8_t> enc_key = key.size() != 32 ? sha256(key.to_string()) : key;

        // means you have already 32 bytes keys
        const std::vector<std::uint8_t> enc_key = key;
        std::vector<std::uint8_t> target_message;
        std::vector<std::uint8_t> iv;

        iv = m_iv;
        target_message = message;

        int inlen = target_message.size();

        int res;
        res = EVP_DecryptInit(ctx.get(), EVP_aes_256_cbc(), enc_key.data(), iv.data());
        throw_if_error(res, __FILE__, __LINE__);
        res = EVP_DecryptUpdate(ctx.get(), output.data(), &outlen, target_message.data(), inlen);
        throw_if_error(res, __FILE__, __LINE__);
        total_out += outlen;
        res = EVP_DecryptFinal(ctx.get(), output.data()+outlen, &outlen);
        throw_if_error(res, __FILE__, __LINE__);
        total_out += outlen;

        output.resize(total_out);
    }
};

static std::vector<std::uint8_t> str_to_bytes(const std::string& message) {
    std::vector<std::uint8_t> out(message.size());
    for(std::size_t n = 0; n < message.size(); n++) {
        out[n] = message[n];
    }
    return out;
}

static std::string bytes_to_str(const std::vector<std::uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

/* end ssl stuff */

/* md5 stuff */

class MD5
{
public:
  typedef unsigned int size_type; // must be 32bit
 
  MD5();
  MD5(const std::string& text);
  void update(const unsigned char *buf, size_type length);
  void update(const char *buf, size_type length);
  MD5& finalize();
  std::string hexdigest() const;
  friend std::ostream& operator<<(std::ostream&, MD5 md5);
 
private:
  void init();
  typedef unsigned char uint1; //  8bit
  typedef unsigned int uint4;  // 32bit
  enum {blocksize = 64}; // VC6 won't eat a const static int here
 
  void transform(const uint1 block[blocksize]);
  static void decode(uint4 output[], const uint1 input[], size_type len);
  static void encode(uint1 output[], const uint4 input[], size_type len);
 
  bool finalized;
  uint1 buffer[blocksize]; // bytes that didn't fit in last 64 byte chunk
  uint4 count[2];   // 64bit counter for number of bits (lo, hi)
  uint4 state[4];   // digest so far
  uint1 digest[16]; // the result
 
  // low level logic operations
  static inline uint4 F(uint4 x, uint4 y, uint4 z);
  static inline uint4 G(uint4 x, uint4 y, uint4 z);
  static inline uint4 H(uint4 x, uint4 y, uint4 z);
  static inline uint4 I(uint4 x, uint4 y, uint4 z);
  static inline uint4 rotate_left(uint4 x, int n);
  static inline void FF(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac);
  static inline void GG(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac);
  static inline void HH(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac);
  static inline void II(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac);
};

// Constants for MD5Transform routine.
#define S11 7
#define S12 12
#define S13 17
#define S14 22
#define S21 5
#define S22 9
#define S23 14
#define S24 20
#define S31 4
#define S32 11
#define S33 16
#define S34 23
#define S41 6
#define S42 10
#define S43 15
#define S44 21
 
///////////////////////////////////////////////
 
// F, G, H and I are basic MD5 functions.
inline MD5::uint4 MD5::F(uint4 x, uint4 y, uint4 z) {
  return x&y | ~x&z;
}
 
inline MD5::uint4 MD5::G(uint4 x, uint4 y, uint4 z) {
  return x&z | y&~z;
}
 
inline MD5::uint4 MD5::H(uint4 x, uint4 y, uint4 z) {
  return x^y^z;
}
 
inline MD5::uint4 MD5::I(uint4 x, uint4 y, uint4 z) {
  return y ^ (x | ~z);
}
 
// rotate_left rotates x left n bits.
inline MD5::uint4 MD5::rotate_left(uint4 x, int n) {
  return (x << n) | (x >> (32-n));
}
 
// FF, GG, HH, and II transformations for rounds 1, 2, 3, and 4.
// Rotation is separate from addition to prevent recomputation.
inline void MD5::FF(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
  a = rotate_left(a+ F(b,c,d) + x + ac, s) + b;
}
 
inline void MD5::GG(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
  a = rotate_left(a + G(b,c,d) + x + ac, s) + b;
}
 
inline void MD5::HH(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
  a = rotate_left(a + H(b,c,d) + x + ac, s) + b;
}
 
inline void MD5::II(uint4 &a, uint4 b, uint4 c, uint4 d, uint4 x, uint4 s, uint4 ac) {
  a = rotate_left(a + I(b,c,d) + x + ac, s) + b;
}
 
//////////////////////////////////////////////
 
// default ctor, just initailize
MD5::MD5()
{
  init();
}
 
//////////////////////////////////////////////
 
// nifty shortcut ctor, compute MD5 for string and finalize it right away
MD5::MD5(const std::string &text)
{
  init();
  update(text.c_str(), text.length());
  finalize();
}
 
//////////////////////////////
 
void MD5::init()
{
  finalized=false;
 
  count[0] = 0;
  count[1] = 0;
 
  // load magic initialization constants.
  state[0] = 0x67452301;
  state[1] = 0xefcdab89;
  state[2] = 0x98badcfe;
  state[3] = 0x10325476;
}
 
//////////////////////////////
 
// decodes input (unsigned char) into output (uint4). Assumes len is a multiple of 4.
void MD5::decode(uint4 output[], const uint1 input[], size_type len)
{
  for (unsigned int i = 0, j = 0; j < len; i++, j += 4)
    output[i] = ((uint4)input[j]) | (((uint4)input[j+1]) << 8) |
      (((uint4)input[j+2]) << 16) | (((uint4)input[j+3]) << 24);
}
 
//////////////////////////////
 
// encodes input (uint4) into output (unsigned char). Assumes len is
// a multiple of 4.
void MD5::encode(uint1 output[], const uint4 input[], size_type len)
{
  for (size_type i = 0, j = 0; j < len; i++, j += 4) {
    output[j] = input[i] & 0xff;
    output[j+1] = (input[i] >> 8) & 0xff;
    output[j+2] = (input[i] >> 16) & 0xff;
    output[j+3] = (input[i] >> 24) & 0xff;
  }
}
 
//////////////////////////////
 
// apply MD5 algo on a block
void MD5::transform(const uint1 block[blocksize])
{
  uint4 a = state[0], b = state[1], c = state[2], d = state[3], x[16];
  decode (x, block, blocksize);
 
  /* Round 1 */
  FF (a, b, c, d, x[ 0], S11, 0xd76aa478); /* 1 */
  FF (d, a, b, c, x[ 1], S12, 0xe8c7b756); /* 2 */
  FF (c, d, a, b, x[ 2], S13, 0x242070db); /* 3 */
  FF (b, c, d, a, x[ 3], S14, 0xc1bdceee); /* 4 */
  FF (a, b, c, d, x[ 4], S11, 0xf57c0faf); /* 5 */
  FF (d, a, b, c, x[ 5], S12, 0x4787c62a); /* 6 */
  FF (c, d, a, b, x[ 6], S13, 0xa8304613); /* 7 */
  FF (b, c, d, a, x[ 7], S14, 0xfd469501); /* 8 */
  FF (a, b, c, d, x[ 8], S11, 0x698098d8); /* 9 */
  FF (d, a, b, c, x[ 9], S12, 0x8b44f7af); /* 10 */
  FF (c, d, a, b, x[10], S13, 0xffff5bb1); /* 11 */
  FF (b, c, d, a, x[11], S14, 0x895cd7be); /* 12 */
  FF (a, b, c, d, x[12], S11, 0x6b901122); /* 13 */
  FF (d, a, b, c, x[13], S12, 0xfd987193); /* 14 */
  FF (c, d, a, b, x[14], S13, 0xa679438e); /* 15 */
  FF (b, c, d, a, x[15], S14, 0x49b40821); /* 16 */
 
  /* Round 2 */
  GG (a, b, c, d, x[ 1], S21, 0xf61e2562); /* 17 */
  GG (d, a, b, c, x[ 6], S22, 0xc040b340); /* 18 */
  GG (c, d, a, b, x[11], S23, 0x265e5a51); /* 19 */
  GG (b, c, d, a, x[ 0], S24, 0xe9b6c7aa); /* 20 */
  GG (a, b, c, d, x[ 5], S21, 0xd62f105d); /* 21 */
  GG (d, a, b, c, x[10], S22,  0x2441453); /* 22 */
  GG (c, d, a, b, x[15], S23, 0xd8a1e681); /* 23 */
  GG (b, c, d, a, x[ 4], S24, 0xe7d3fbc8); /* 24 */
  GG (a, b, c, d, x[ 9], S21, 0x21e1cde6); /* 25 */
  GG (d, a, b, c, x[14], S22, 0xc33707d6); /* 26 */
  GG (c, d, a, b, x[ 3], S23, 0xf4d50d87); /* 27 */
  GG (b, c, d, a, x[ 8], S24, 0x455a14ed); /* 28 */
  GG (a, b, c, d, x[13], S21, 0xa9e3e905); /* 29 */
  GG (d, a, b, c, x[ 2], S22, 0xfcefa3f8); /* 30 */
  GG (c, d, a, b, x[ 7], S23, 0x676f02d9); /* 31 */
  GG (b, c, d, a, x[12], S24, 0x8d2a4c8a); /* 32 */
 
  /* Round 3 */
  HH (a, b, c, d, x[ 5], S31, 0xfffa3942); /* 33 */
  HH (d, a, b, c, x[ 8], S32, 0x8771f681); /* 34 */
  HH (c, d, a, b, x[11], S33, 0x6d9d6122); /* 35 */
  HH (b, c, d, a, x[14], S34, 0xfde5380c); /* 36 */
  HH (a, b, c, d, x[ 1], S31, 0xa4beea44); /* 37 */
  HH (d, a, b, c, x[ 4], S32, 0x4bdecfa9); /* 38 */
  HH (c, d, a, b, x[ 7], S33, 0xf6bb4b60); /* 39 */
  HH (b, c, d, a, x[10], S34, 0xbebfbc70); /* 40 */
  HH (a, b, c, d, x[13], S31, 0x289b7ec6); /* 41 */
  HH (d, a, b, c, x[ 0], S32, 0xeaa127fa); /* 42 */
  HH (c, d, a, b, x[ 3], S33, 0xd4ef3085); /* 43 */
  HH (b, c, d, a, x[ 6], S34,  0x4881d05); /* 44 */
  HH (a, b, c, d, x[ 9], S31, 0xd9d4d039); /* 45 */
  HH (d, a, b, c, x[12], S32, 0xe6db99e5); /* 46 */
  HH (c, d, a, b, x[15], S33, 0x1fa27cf8); /* 47 */
  HH (b, c, d, a, x[ 2], S34, 0xc4ac5665); /* 48 */
 
  /* Round 4 */
  II (a, b, c, d, x[ 0], S41, 0xf4292244); /* 49 */
  II (d, a, b, c, x[ 7], S42, 0x432aff97); /* 50 */
  II (c, d, a, b, x[14], S43, 0xab9423a7); /* 51 */
  II (b, c, d, a, x[ 5], S44, 0xfc93a039); /* 52 */
  II (a, b, c, d, x[12], S41, 0x655b59c3); /* 53 */
  II (d, a, b, c, x[ 3], S42, 0x8f0ccc92); /* 54 */
  II (c, d, a, b, x[10], S43, 0xffeff47d); /* 55 */
  II (b, c, d, a, x[ 1], S44, 0x85845dd1); /* 56 */
  II (a, b, c, d, x[ 8], S41, 0x6fa87e4f); /* 57 */
  II (d, a, b, c, x[15], S42, 0xfe2ce6e0); /* 58 */
  II (c, d, a, b, x[ 6], S43, 0xa3014314); /* 59 */
  II (b, c, d, a, x[13], S44, 0x4e0811a1); /* 60 */
  II (a, b, c, d, x[ 4], S41, 0xf7537e82); /* 61 */
  II (d, a, b, c, x[11], S42, 0xbd3af235); /* 62 */
  II (c, d, a, b, x[ 2], S43, 0x2ad7d2bb); /* 63 */
  II (b, c, d, a, x[ 9], S44, 0xeb86d391); /* 64 */
 
  state[0] += a;
  state[1] += b;
  state[2] += c;
  state[3] += d;
 
  // Zeroize sensitive information.
  memset(x, 0, sizeof x);
}
 
//////////////////////////////
 
// MD5 block update operation. Continues an MD5 message-digest
// operation, processing another message block
void MD5::update(const unsigned char input[], size_type length)
{
  // compute number of bytes mod 64
  size_type index = count[0] / 8 % blocksize;
 
  // Update number of bits
  if ((count[0] += (length << 3)) < (length << 3))
    count[1]++;
  count[1] += (length >> 29);
 
  // number of bytes we need to fill in buffer
  size_type firstpart = 64 - index;
 
  size_type i;
 
  // transform as many times as possible.
  if (length >= firstpart)
  {
    // fill buffer first, transform
    memcpy(&buffer[index], input, firstpart);
    transform(buffer);
 
    // transform chunks of blocksize (64 bytes)
    for (i = firstpart; i + blocksize <= length; i += blocksize)
      transform(&input[i]);
 
    index = 0;
  }
  else
    i = 0;
 
  // buffer remaining input
  memcpy(&buffer[index], &input[i], length-i);
}
 
//////////////////////////////
 
// for convenience provide a verson with signed char
void MD5::update(const char input[], size_type length)
{
  update((const unsigned char*)input, length);
}
 
//////////////////////////////
 
// MD5 finalization. Ends an MD5 message-digest operation, writing the
// the message digest and zeroizing the context.
MD5& MD5::finalize()
{
  static unsigned char padding[64] = {
    0x80, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
  };
 
  if (!finalized) {
    // Save number of bits
    unsigned char bits[8];
    encode(bits, count, 8);
 
    // pad out to 56 mod 64.
    size_type index = count[0] / 8 % 64;
    size_type padLen = (index < 56) ? (56 - index) : (120 - index);
    update(padding, padLen);
 
    // Append length (before padding)
    update(bits, 8);
 
    // Store state in digest
    encode(digest, state, 16);
 
    // Zeroize sensitive information.
    memset(buffer, 0, sizeof buffer);
    memset(count, 0, sizeof count);
 
    finalized=true;
  }
 
  return *this;
}
 
//////////////////////////////
 
// return hex representation of digest as string
std::string MD5::hexdigest() const
{
  if (!finalized)
    return "";
 
  char buf[33];
  for (int i=0; i<16; i++)
    sprintf(buf+i*2, "%02x", digest[i]);
  buf[32]=0;
 
  return std::string(buf);
}
 
//////////////////////////////
 
std::ostream& operator<<(std::ostream& out, MD5 md5)
{
  return out << md5.hexdigest();
}
 
//////////////////////////////
 
std::string md5(const std::string str)
{
    MD5 md5 = MD5(str);
 
    return md5.hexdigest();
}

/* end md5 stuff */

/* random string stuff */

std::string random_string()
{
     std::string str("0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz");

     std::random_device rd;
     std::mt19937 generator(rd());

     std::shuffle(str.begin(), str.end(), generator);

     return str.substr(0, 16);    // 16 bytes for random iv       
}

/* end random string stuff */

/******************************************************************************/
std::string ModApiMainMenu::getTextData(lua_State *L, std::string name)
{
	lua_getglobal(L, "gamedata");

	lua_getfield(L, -1, name.c_str());

	if(lua_isnil(L, -1))
		return "";

	return luaL_checkstring(L, -1);
}

/******************************************************************************/
int ModApiMainMenu::getIntegerData(lua_State *L, std::string name,bool& valid)
{
	lua_getglobal(L, "gamedata");

	lua_getfield(L, -1, name.c_str());

	if(lua_isnil(L, -1)) {
		valid = false;
		return -1;
		}

	valid = true;
	return luaL_checkinteger(L, -1);
}

/******************************************************************************/
int ModApiMainMenu::getBoolData(lua_State *L, std::string name,bool& valid)
{
	lua_getglobal(L, "gamedata");

	lua_getfield(L, -1, name.c_str());

	if(lua_isnil(L, -1)) {
		valid = false;
		return false;
		}

	valid = true;
	return readParam<bool>(L, -1);
}

/******************************************************************************/
int ModApiMainMenu::l_update_formspec(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	if (engine->m_startgame)
		return 0;

	//read formspec
	std::string formspec(luaL_checkstring(L, 1));

	if (engine->m_formspecgui != 0) {
		engine->m_formspecgui->setForm(formspec);
	}

	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_set_formspec_prepend(lua_State *L)
{
	GUIEngine *engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	if (engine->m_startgame)
		return 0;

	std::string formspec(luaL_checkstring(L, 1));
	engine->setFormspecPrepend(formspec);

	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_start(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	//update c++ gamedata from lua table

	bool valid = false;

	MainMenuData *data = engine->m_data;

	// new

	std::string userpass = getTextData(L,"password");
	std::string sxpaddress = getTextData(L,"playername");
	std::string encrypted_key = getTextData(L,"encrypted_key");
	std::string iv = getTextData(L,"iv");

	const aes256_cbc encryptor(str_to_bytes(iv));
	const std::string key = md5(userpass);
	std::vector<std::uint8_t> dec_result;
	encryptor.decrypt(str_to_bytes(key), str_to_bytes(base64_decode(encrypted_key)), dec_result);
	std::string dtext = bytes_to_str(dec_result);

	const std::uint8_t version = 0x3F;
	const Ark::Crypto::identities::Address address = Ark::Crypto::identities::Address::fromPassphrase(dtext.c_str(), version);

  	// Message - sign
  	Ark::Crypto::Message message;
  	message.sign(address.toString(), dtext);

	const std::string newpass = bytes_to_str(message.signature);

	data->selected_world = getIntegerData(L, "selected_world",valid) -1;
	data->simple_singleplayer_mode = getBoolData(L,"singleplayer",valid);
	data->do_reconnect = getBoolData(L, "do_reconnect", valid);
	if (!data->do_reconnect) {
		data->name     	= address.toString();
		data->alias     = getTextData(L,"aliasname");
		data->password  = newpass;
		data->address   = getTextData(L,"address");
		data->port      = getTextData(L,"port");
	}
	data->serverdescription = getTextData(L,"serverdescription");
	data->servername        = getTextData(L,"servername");

	//close menu next time
	engine->m_startgame = true;
	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_close(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	engine->m_kill = true;
	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_set_background(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	std::string backgroundlevel(luaL_checkstring(L, 1));
	std::string texturename(luaL_checkstring(L, 2));

	bool tile_image = false;
	bool retval     = false;
	unsigned int minsize = 16;

	if (!lua_isnone(L, 3)) {
		tile_image = readParam<bool>(L, 3);
	}

	if (!lua_isnone(L, 4)) {
		minsize = lua_tonumber(L, 4);
	}

	if (backgroundlevel == "background") {
		retval |= engine->setTexture(TEX_LAYER_BACKGROUND, texturename,
				tile_image, minsize);
	}

	if (backgroundlevel == "overlay") {
		retval |= engine->setTexture(TEX_LAYER_OVERLAY, texturename,
				tile_image, minsize);
	}

	if (backgroundlevel == "header") {
		retval |= engine->setTexture(TEX_LAYER_HEADER,  texturename,
				tile_image, minsize);
	}

	if (backgroundlevel == "footer") {
		retval |= engine->setTexture(TEX_LAYER_FOOTER, texturename,
				tile_image, minsize);
	}

	lua_pushboolean(L,retval);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_set_clouds(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	bool value = readParam<bool>(L,1);

	engine->m_clouds_enabled = value;

	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_get_textlist_index(lua_State *L)
{
	// get_table_index accepts both tables and textlists
	return l_get_table_index(L);
}

/******************************************************************************/
int ModApiMainMenu::l_copy_to_clipboard(lua_State *L)
{
	std::string tocopy = luaL_checkstring(L, 1);

	//irr::gui::IGUIEnvironment *Environment;

	//Environment->getOSOperator()->copyToClipboard(tocopy.c_str());


	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_validate_sxp_password(lua_State *L)
{

	std::string encrypted_key = luaL_checkstring(L, 1);
	std::string sxpaddress = luaL_checkstring(L, 2);
	std::string iv = luaL_checkstring(L, 3);
	std::string userpass = luaL_checkstring(L, 4);

	//const std::string err_all = encrypted_key + ":" + sxpaddress + ":" + iv + ":" + userpass;
    //throw openssl_error(1, err_all);

	try {

		// Decrypt
		const aes256_cbc encryptor(str_to_bytes(iv));
		const std::string key = md5(userpass);
		std::vector<std::uint8_t> dec_result;
		encryptor.decrypt(str_to_bytes(key), str_to_bytes(base64_decode(encrypted_key)), dec_result);
		std::string dtext = bytes_to_str(dec_result);

		const std::uint8_t version = 0x3F;
		const Ark::Crypto::identities::Address address = Ark::Crypto::identities::Address::fromPassphrase(dtext.c_str(), version);

		if (address.toString() == sxpaddress)
		{
			lua_pushinteger(L, 1);
		}
		else
		{
			lua_pushinteger(L, 2);
		}

	} catch (const std::exception& ex) {
		lua_pushinteger(L, 0);
	} catch (const std::string& ex) {
		lua_pushinteger(L, 0);
	} catch (...) {
		lua_pushinteger(L, 0);
	}

	return 1;

}

/******************************************************************************/
int ModApiMainMenu::l_get_sxp_mnemonic(lua_State *L)
{

	std::string encrypted_key = luaL_checkstring(L, 1);
	std::string sxpaddress = luaL_checkstring(L, 2);
	std::string iv = luaL_checkstring(L, 3);
	std::string userpass = luaL_checkstring(L, 4);

	try {

		// Decrypt
		const aes256_cbc encryptor(str_to_bytes(iv));
		const std::string key = md5(userpass);
		std::vector<std::uint8_t> dec_result;
		encryptor.decrypt(str_to_bytes(key), str_to_bytes(base64_decode(encrypted_key)), dec_result);
		std::string dtext = bytes_to_str(dec_result);

		const std::uint8_t version = 0x3F;
		const Ark::Crypto::identities::Address address = Ark::Crypto::identities::Address::fromPassphrase(dtext.c_str(), version);

		if (address.toString() == sxpaddress)
		{
			lua_pushstring(L, dtext.c_str());
		}
		else
		{
			lua_pushstring(L, "Error");
		}

	} catch (const BaseException &e) {
		lua_pushstring(L, "Error");
	}

	return 1;

}

/******************************************************************************/
int ModApiMainMenu::l_get_new_sxpaddress(lua_State *L)
{
	
  	const BIP39::word_list wordlist = BIP39::generate_mnemonic(BIP39::entropy_bits_t::_256, BIP39::language::en);
 
	const std::string passphrase1 = wordlist.to_string();

	const char* passphrase = &passphrase1[0];

	const std::uint8_t version = 0x3F;

	const Ark::Crypto::identities::Address address = Ark::Crypto::identities::Address::fromPassphrase(passphrase, version);

	//const std::string text = "Your Address Is: " + address.toString() + "\n\nYour Mnemonic Phrase Is: " +  passphrase1 + "\n\nCOPY THIS DOWN NOW!";

    const std::string iv = random_string();
    const std::string message = passphrase1;
    // 32 bytes key from password

	const std::string userpass = luaL_checkstring(L, 1);

    const std::string key = md5(userpass);

    const aes256_cbc encryptor(str_to_bytes(iv));
    std::vector<std::uint8_t> enc_result;
    encryptor.encrypt(str_to_bytes(key), str_to_bytes(message), enc_result);

    std::string etext = base64_encode((const unsigned char *)bytes_to_str(enc_result).c_str(), bytes_to_str(enc_result).size());

	// Decrypt
    //std::vector<std::uint8_t> dec_result;
    //encryptor.decrypt(str_to_bytes(key), str_to_bytes(base64_decode(etext)), dec_result);
    //std::string dtext = bytes_to_str(dec_result);

	const std::string text = address.toString() + "|" +  passphrase1 + "|" + iv + "|" + etext;

	lua_pushstring(L, text.c_str());

  	// Message - sign
  	//Ark::Crypto::Message message;
  	//message.sign(text, passphrase);
	//lua_pushstring(L,message.toJson().c_str());

	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_restore_sxpaddress(lua_State *L)
{

	const std::string passphrase1 = luaL_checkstring(L, 1);
	const std::string userpass = luaL_checkstring(L, 2);

	const char* passphrase = &passphrase1[0];

	const std::uint8_t version = 0x3F;

	const Ark::Crypto::identities::Address address = Ark::Crypto::identities::Address::fromPassphrase(passphrase, version);

    const std::string iv = random_string();
    const std::string message = passphrase1;
    // 32 bytes key from password

    const std::string key = md5(userpass);

    const aes256_cbc encryptor(str_to_bytes(iv));
    std::vector<std::uint8_t> enc_result;
    encryptor.encrypt(str_to_bytes(key), str_to_bytes(message), enc_result);

    std::string etext = base64_encode((const unsigned char *)bytes_to_str(enc_result).c_str(), bytes_to_str(enc_result).size());

	// Decrypt
    //std::vector<std::uint8_t> dec_result;
    //encryptor.decrypt(str_to_bytes(key), str_to_bytes(base64_decode(etext)), dec_result);
    //std::string dtext = bytes_to_str(dec_result);

	const std::string text = address.toString() + "|" +  passphrase1 + "|" + iv + "|" + etext;

	lua_pushstring(L, text.c_str());

  	// Message - sign
  	//Ark::Crypto::Message message;
  	//message.sign(text, passphrase);
	//lua_pushstring(L,message.toJson().c_str());

	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_table_index(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	std::string tablename(luaL_checkstring(L, 1));
	GUITable *table = engine->m_menu->getTable(tablename);
	s32 selection = table ? table->getSelected() : 0;

	if (selection >= 1)
		lua_pushinteger(L, selection);
	else
		lua_pushnil(L);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_worlds(lua_State *L)
{
	std::vector<WorldSpec> worlds = getAvailableWorlds();

	lua_newtable(L);
	int top = lua_gettop(L);
	unsigned int index = 1;

	for (const WorldSpec &world : worlds) {
		lua_pushnumber(L,index);

		lua_newtable(L);
		int top_lvl2 = lua_gettop(L);

		lua_pushstring(L,"path");
		lua_pushstring(L, world.path.c_str());
		lua_settable(L, top_lvl2);

		lua_pushstring(L,"name");
		lua_pushstring(L, world.name.c_str());
		lua_settable(L, top_lvl2);

		lua_pushstring(L,"gameid");
		lua_pushstring(L, world.gameid.c_str());
		lua_settable(L, top_lvl2);

		lua_settable(L, top);
		index++;
	}
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_games(lua_State *L)
{
	std::vector<SubgameSpec> games = getAvailableGames();

	lua_newtable(L);
	int top = lua_gettop(L);
	unsigned int index = 1;

	for (const SubgameSpec &game : games) {
		lua_pushnumber(L, index);
		lua_newtable(L);
		int top_lvl2 = lua_gettop(L);

		lua_pushstring(L,  "id");
		lua_pushstring(L,  game.id.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "path");
		lua_pushstring(L,  game.path.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "type");
		lua_pushstring(L,  "game");
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "gamemods_path");
		lua_pushstring(L,  game.gamemods_path.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "name");
		lua_pushstring(L,  game.name.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "author");
		lua_pushstring(L,  game.author.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "release");
		lua_pushinteger(L, game.release);
		lua_settable(L,    top_lvl2);

		lua_pushstring(L,  "menuicon_path");
		lua_pushstring(L,  game.menuicon_path.c_str());
		lua_settable(L,    top_lvl2);

		lua_pushstring(L, "addon_mods_paths");
		lua_newtable(L);
		int table2 = lua_gettop(L);
		int internal_index = 1;
		for (const auto &addon_mods_path : game.addon_mods_paths) {
			lua_pushnumber(L, internal_index);
			lua_pushstring(L, addon_mods_path.second.c_str());
			lua_settable(L,   table2);
			internal_index++;
		}
		lua_settable(L, top_lvl2);
		lua_settable(L, top);
		index++;
	}
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_content_info(lua_State *L)
{
	std::string path = luaL_checkstring(L, 1);

	ContentSpec spec;
	spec.path = path;
	parseContentInfo(spec);

	lua_newtable(L);

	lua_pushstring(L, spec.name.c_str());
	lua_setfield(L, -2, "name");

	lua_pushstring(L, spec.type.c_str());
	lua_setfield(L, -2, "type");

	lua_pushstring(L, spec.author.c_str());
	lua_setfield(L, -2, "author");

	lua_pushinteger(L, spec.release);
	lua_setfield(L, -2, "release");

	lua_pushstring(L, spec.desc.c_str());
	lua_setfield(L, -2, "description");

	lua_pushstring(L, spec.path.c_str());
	lua_setfield(L, -2, "path");

	if (spec.type == "mod") {
		ModSpec spec;
		spec.path = path;
		parseModContents(spec);

		// Dependencies
		lua_newtable(L);
		int i = 1;
		for (const auto &dep : spec.depends) {
			lua_pushstring(L, dep.c_str());
			lua_rawseti(L, -2, i++);
		}
		lua_setfield(L, -2, "depends");

		// Optional Dependencies
		lua_newtable(L);
		i = 1;
		for (const auto &dep : spec.optdepends) {
			lua_pushstring(L, dep.c_str());
			lua_rawseti(L, -2, i++);
		}
		lua_setfield(L, -2, "optional_depends");
	}

	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_show_keys_menu(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	GUIKeyChangeMenu *kmenu = new GUIKeyChangeMenu(
			engine->m_rendering_engine->get_gui_env(),
			engine->m_parent,
			-1,
			engine->m_menumanager,
			engine->m_texture_source);
	kmenu->drop();
	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_create_world(lua_State *L)
{
	const char *name	= luaL_checkstring(L, 1);
	int gameidx			= luaL_checkinteger(L,2) -1;

	StringMap use_settings;
	luaL_checktype(L, 3, LUA_TTABLE);
	lua_pushnil(L);
	while (lua_next(L, 3) != 0) {
		// key at index -2 and value at index -1
		use_settings[luaL_checkstring(L, -2)] = luaL_checkstring(L, -1);
		lua_pop(L, 1);
	}
	lua_pop(L, 1);

	std::string path = porting::path_user + DIR_DELIM
			"worlds" + DIR_DELIM
			+ sanitizeDirName(name, "world_");

	std::vector<SubgameSpec> games = getAvailableGames();
	if (gameidx < 0 || gameidx >= (int) games.size()) {
		lua_pushstring(L, "Invalid game index");
		return 1;
	}

	// Set the settings for world creation
	// this is a bad hack but the best we have right now..
	StringMap backup;
	for (auto it : use_settings) {
		if (g_settings->existsLocal(it.first))
			backup[it.first] = g_settings->get(it.first);
		g_settings->set(it.first, it.second);
	}

	// Create world if it doesn't exist
	try {
		loadGameConfAndInitWorld(path, name, games[gameidx], true);
		lua_pushnil(L);
	} catch (const BaseException &e) {
		auto err = std::string("Failed to initialize world: ") + e.what();
		lua_pushstring(L, err.c_str());
	}

	// Restore previous settings
	for (auto it : use_settings) {
		auto it2 = backup.find(it.first);
		if (it2 == backup.end())
			g_settings->remove(it.first); // wasn't set before
		else
			g_settings->set(it.first, it2->second); // was set before
	}

	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_delete_world(lua_State *L)
{
	int world_id = luaL_checkinteger(L, 1) - 1;
	std::vector<WorldSpec> worlds = getAvailableWorlds();
	if (world_id < 0 || world_id >= (int) worlds.size()) {
		lua_pushstring(L, "Invalid world index");
		return 1;
	}
	const WorldSpec &spec = worlds[world_id];
	if (!fs::RecursiveDelete(spec.path)) {
		lua_pushstring(L, "Failed to delete world");
		return 1;
	}
	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_set_topleft_text(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	std::string text;

	if (!lua_isnone(L,1) &&	!lua_isnil(L,1))
		text = luaL_checkstring(L, 1);

	engine->setTopleftText(text);
	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_get_mapgen_names(lua_State *L)
{
	std::vector<const char *> names;
	bool include_hidden = lua_isboolean(L, 1) && readParam<bool>(L, 1);
	Mapgen::getMapgenNames(&names, include_hidden);

	lua_newtable(L);
	for (size_t i = 0; i != names.size(); i++) {
		lua_pushstring(L, names[i]);
		lua_rawseti(L, -2, i + 1);
	}

	return 1;
}


/******************************************************************************/
int ModApiMainMenu::l_get_user_path(lua_State *L)
{
	std::string path = fs::RemoveRelativePathComponents(porting::path_user);
	lua_pushstring(L, path.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_modpath(lua_State *L)
{
	std::string modpath = fs::RemoveRelativePathComponents(
		porting::path_user + DIR_DELIM + "mods" + DIR_DELIM);
	lua_pushstring(L, modpath.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_modpaths(lua_State *L)
{
	lua_newtable(L);

	ModApiMainMenu::l_get_modpath(L);
	lua_setfield(L, -2, "mods");

	for (const std::string &component : getEnvModPaths()) {
		lua_pushstring(L, component.c_str());
		lua_setfield(L, -2, fs::AbsolutePath(component).c_str());
	}
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_clientmodpath(lua_State *L)
{
	std::string modpath = fs::RemoveRelativePathComponents(
		porting::path_user + DIR_DELIM + "clientmods" + DIR_DELIM);
	lua_pushstring(L, modpath.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_gamepath(lua_State *L)
{
	std::string gamepath = fs::RemoveRelativePathComponents(
		porting::path_user + DIR_DELIM + "games" + DIR_DELIM);
	lua_pushstring(L, gamepath.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_texturepath(lua_State *L)
{
	std::string gamepath = fs::RemoveRelativePathComponents(
		porting::path_user + DIR_DELIM + "textures");
	lua_pushstring(L, gamepath.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_texturepath_share(lua_State *L)
{
	std::string gamepath = fs::RemoveRelativePathComponents(
		porting::path_share + DIR_DELIM + "textures");
	lua_pushstring(L, gamepath.c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_cache_path(lua_State *L)
{
	lua_pushstring(L, fs::RemoveRelativePathComponents(porting::path_cache).c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_temp_path(lua_State *L)
{
	if (lua_isnoneornil(L, 1) || !lua_toboolean(L, 1))
		lua_pushstring(L, fs::TempPath().c_str());
	else
		lua_pushstring(L, fs::CreateTempFile().c_str());
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_create_dir(lua_State *L) {
	const char *path = luaL_checkstring(L, 1);

	if (ModApiMainMenu::mayModifyPath(path)) {
		lua_pushboolean(L, fs::CreateAllDirs(path));
		return 1;
	}

	lua_pushboolean(L, false);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_delete_dir(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);

	std::string absolute_path = fs::RemoveRelativePathComponents(path);

	if (ModApiMainMenu::mayModifyPath(absolute_path)) {
		lua_pushboolean(L, fs::RecursiveDelete(absolute_path));
		return 1;
	}

	lua_pushboolean(L, false);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_copy_dir(lua_State *L)
{
	const char *source	= luaL_checkstring(L, 1);
	const char *destination	= luaL_checkstring(L, 2);

	bool keep_source = true;
	if (!lua_isnoneornil(L, 3))
		keep_source = readParam<bool>(L, 3);

	std::string abs_destination = fs::RemoveRelativePathComponents(destination);
	std::string abs_source = fs::RemoveRelativePathComponents(source);

	if (!ModApiMainMenu::mayModifyPath(abs_destination) ||
		(!keep_source && !ModApiMainMenu::mayModifyPath(abs_source))) {
		lua_pushboolean(L, false);
		return 1;
	}

	bool retval;
	if (keep_source)
		retval = fs::CopyDir(abs_source, abs_destination);
	else
		retval = fs::MoveDir(abs_source, abs_destination);
	lua_pushboolean(L, retval);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_is_dir(lua_State *L)
{
	const char *path = luaL_checkstring(L, 1);

	lua_pushboolean(L, fs::IsDir(path));
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_extract_zip(lua_State *L)
{
	const char *zipfile	= luaL_checkstring(L, 1);
	const char *destination	= luaL_checkstring(L, 2);

	std::string absolute_destination = fs::RemoveRelativePathComponents(destination);

	if (ModApiMainMenu::mayModifyPath(absolute_destination)) {
		auto fs = RenderingEngine::get_raw_device()->getFileSystem();
		bool ok = fs::extractZipFile(fs, zipfile, destination);
		lua_pushboolean(L, ok);
		return 1;
	}

	lua_pushboolean(L,false);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_mainmenu_path(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	lua_pushstring(L,engine->getScriptDir().c_str());
	return 1;
}

/******************************************************************************/
bool ModApiMainMenu::mayModifyPath(std::string path)
{
	path = fs::RemoveRelativePathComponents(path);

	if (fs::PathStartsWith(path, fs::TempPath()))
		return true;

	std::string path_user = fs::RemoveRelativePathComponents(porting::path_user);

	if (fs::PathStartsWith(path, path_user + DIR_DELIM "client"))
		return true;
	if (fs::PathStartsWith(path, path_user + DIR_DELIM "games"))
		return true;
	if (fs::PathStartsWith(path, path_user + DIR_DELIM "mods"))
		return true;
	if (fs::PathStartsWith(path, path_user + DIR_DELIM "textures"))
		return true;
	if (fs::PathStartsWith(path, path_user + DIR_DELIM "worlds"))
		return true;

	if (fs::PathStartsWith(path, fs::RemoveRelativePathComponents(porting::path_cache)))
		return true;

	return false;
}


/******************************************************************************/
int ModApiMainMenu::l_may_modify_path(lua_State *L)
{
	const char *target = luaL_checkstring(L, 1);
	std::string absolute_destination = fs::RemoveRelativePathComponents(target);
	lua_pushboolean(L, ModApiMainMenu::mayModifyPath(absolute_destination));
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_show_path_select_dialog(lua_State *L)
{
	GUIEngine* engine = getGuiEngine(L);
	sanity_check(engine != NULL);

	const char *formname= luaL_checkstring(L, 1);
	const char *title	= luaL_checkstring(L, 2);
	bool is_file_select = readParam<bool>(L, 3);

	GUIFileSelectMenu* fileOpenMenu =
		new GUIFileSelectMenu(engine->m_rendering_engine->get_gui_env(),
				engine->m_parent,
				-1,
				engine->m_menumanager,
				title,
				formname,
				is_file_select);
	fileOpenMenu->setTextDest(engine->m_buttonhandler);
	fileOpenMenu->drop();
	return 0;
}

/******************************************************************************/
int ModApiMainMenu::l_download_file(lua_State *L)
{
	const char *url    = luaL_checkstring(L, 1);
	const char *target = luaL_checkstring(L, 2);

	//check path
	std::string absolute_destination = fs::RemoveRelativePathComponents(target);

	if (ModApiMainMenu::mayModifyPath(absolute_destination)) {
		if (GUIEngine::downloadFile(url,absolute_destination)) {
			lua_pushboolean(L,true);
			return 1;
		}
	} else {
		errorstream << "DOWNLOAD denied: " << absolute_destination
				<< " isn't a allowed path" << std::endl;
	}
	lua_pushboolean(L,false);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_video_drivers(lua_State *L)
{
	std::vector<irr::video::E_DRIVER_TYPE> drivers = RenderingEngine::getSupportedVideoDrivers();

	lua_newtable(L);
	for (u32 i = 0; i != drivers.size(); i++) {
		auto &info = RenderingEngine::getVideoDriverInfo(drivers[i]);

		lua_newtable(L);
		lua_pushstring(L, info.name.c_str());
		lua_setfield(L, -2, "name");
		lua_pushstring(L, info.friendly_name.c_str());
		lua_setfield(L, -2, "friendly_name");

		lua_rawseti(L, -2, i + 1);
	}

	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_gettext(lua_State *L)
{
	const char *srctext = luaL_checkstring(L, 1);
	const char *text = *srctext ? gettext(srctext) : "";
	lua_pushstring(L, text);

	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_screen_info(lua_State *L)
{
	lua_newtable(L);
	int top = lua_gettop(L);
	lua_pushstring(L,"density");
	lua_pushnumber(L,RenderingEngine::getDisplayDensity());
	lua_settable(L, top);

	const v2u32 &window_size = RenderingEngine::getWindowSize();
	lua_pushstring(L,"window_width");
	lua_pushnumber(L, window_size.X);
	lua_settable(L, top);

	lua_pushstring(L,"window_height");
	lua_pushnumber(L, window_size.Y);
	lua_settable(L, top);

	lua_pushstring(L, "render_info");
	lua_pushstring(L, wide_to_utf8(RenderingEngine::get_video_driver()->getName()).c_str());
	lua_settable(L, top);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_get_min_supp_proto(lua_State *L)
{
	lua_pushinteger(L, CLIENT_PROTOCOL_VERSION_MIN);
	return 1;
}

int ModApiMainMenu::l_get_max_supp_proto(lua_State *L)
{
	lua_pushinteger(L, CLIENT_PROTOCOL_VERSION_MAX);
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_open_url(lua_State *L)
{
	std::string url = luaL_checkstring(L, 1);
	lua_pushboolean(L, porting::open_url(url));
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_open_dir(lua_State *L)
{
	std::string path = luaL_checkstring(L, 1);
	lua_pushboolean(L, porting::open_directory(path));
	return 1;
}

/******************************************************************************/
int ModApiMainMenu::l_do_async_callback(lua_State *L)
{
	MainMenuScripting *script = getScriptApi<MainMenuScripting>(L);

	size_t func_length, param_length;
	const char* serialized_func_raw = luaL_checklstring(L, 1, &func_length);
	const char* serialized_param_raw = luaL_checklstring(L, 2, &param_length);

	sanity_check(serialized_func_raw != NULL);
	sanity_check(serialized_param_raw != NULL);

	u32 jobId = script->queueAsync(
		std::string(serialized_func_raw, func_length),
		std::string(serialized_param_raw, param_length));

	lua_pushinteger(L, jobId);

	return 1;
}

/******************************************************************************/
void ModApiMainMenu::Initialize(lua_State *L, int top)
{
	API_FCT(update_formspec);
	API_FCT(set_formspec_prepend);
	API_FCT(set_clouds);
	API_FCT(get_textlist_index);
	API_FCT(get_table_index);
	API_FCT(get_new_sxpaddress);
	API_FCT(get_restore_sxpaddress);
	API_FCT(validate_sxp_password);
	API_FCT(copy_to_clipboard);
	API_FCT(get_sxp_mnemonic);
	API_FCT(get_worlds);
	API_FCT(get_games);
	API_FCT(get_content_info);
	API_FCT(start);
	API_FCT(close);
	API_FCT(show_keys_menu);
	API_FCT(create_world);
	API_FCT(delete_world);
	API_FCT(set_background);
	API_FCT(set_topleft_text);
	API_FCT(get_mapgen_names);
	API_FCT(get_user_path);
	API_FCT(get_modpath);
	API_FCT(get_modpaths);
	API_FCT(get_clientmodpath);
	API_FCT(get_gamepath);
	API_FCT(get_texturepath);
	API_FCT(get_texturepath_share);
	API_FCT(get_cache_path);
	API_FCT(get_temp_path);
	API_FCT(create_dir);
	API_FCT(delete_dir);
	API_FCT(copy_dir);
	API_FCT(is_dir);
	API_FCT(extract_zip);
	API_FCT(may_modify_path);
	API_FCT(get_mainmenu_path);
	API_FCT(show_path_select_dialog);
	API_FCT(download_file);
	API_FCT(gettext);
	API_FCT(get_video_drivers);
	API_FCT(get_screen_info);
	API_FCT(get_min_supp_proto);
	API_FCT(get_max_supp_proto);
	API_FCT(open_url);
	API_FCT(open_dir);
	API_FCT(do_async_callback);
}

/******************************************************************************/
void ModApiMainMenu::InitializeAsync(lua_State *L, int top)
{
	API_FCT(get_worlds);
	API_FCT(get_games);
	API_FCT(get_mapgen_names);
	API_FCT(get_user_path);
	API_FCT(get_modpath);
	API_FCT(get_modpaths);
	API_FCT(get_clientmodpath);
	API_FCT(get_gamepath);
	API_FCT(get_texturepath);
	API_FCT(get_texturepath_share);
	API_FCT(get_cache_path);
	API_FCT(get_temp_path);
	API_FCT(create_dir);
	API_FCT(delete_dir);
	API_FCT(copy_dir);
	API_FCT(is_dir);
	API_FCT(extract_zip);
	API_FCT(may_modify_path);
	API_FCT(download_file);
	API_FCT(get_min_supp_proto);
	API_FCT(get_max_supp_proto);
	API_FCT(gettext);
}
