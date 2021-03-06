//------------------------------------------------------------------------------
/*
    This file is part of rippled: https://github.com/ripple/rippled
    Copyright (c) 2012-2014 Ripple Labs Inc.

    Permission to use, copy, modify, and/or distribute this software for any
    purpose  with  or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL ,  DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include <BeastConfig.h>
#include <ripple/basics/strHex.h>
#include <ripple/crypto/KeyType.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/protocol/PublicKey.h>
#include <ripple/protocol/SecretKey.h>
#include <ripple/protocol/Seed.h>
#include <ripple/rpc/Context.h>
#include <ripple/rpc/impl/RPCHelpers.h>
#include <ripple/rpc/handlers/WalletPropose.h>
#include <ed25519-donna/ed25519.h>
#include <boost/optional.hpp>
#include <cmath>
#include <map>

namespace ripple {

double
estimate_entropy (std::string const& input)
{
    // First, we calculate the Shannon entropy. This gives
    // the average number of bits per symbol that we would
    // need to encode the input.
    std::map<int, double> freq;

    for (auto const& c : input)
        freq[c]++;

    double se = 0.0;

    for (auto const& f : freq)
    {
        auto x = f.second / input.length();
        se += (x) * log2(x);
    }

    // We multiply it by the length, to get an estimate of
    // the number of bits in the input. We floor because it
    // is better to be conservative.
    return std::floor (-se * input.length());
}

// {
//  passphrase: <string>
// }
Json::Value doWalletPropose (RPC::Context& context)
{
    return walletPropose (context.params);
}

Json::Value walletPropose (Json::Value const& params)
{
    boost::optional<Seed> seed;

    KeyType keyType = KeyType::unknown;
    if(nullptr == HardEncryptObj::getInstance())
        keyType = KeyType::secp256k1;
    else keyType = KeyType::gmalg;

    if (params.isMember (jss::key_type))
    {
        if (! params[jss::key_type].isString())
        {
            return RPC::expected_field_error (
                jss::key_type, "string");
        }

        keyType = keyTypeFromString (
            params[jss::key_type].asString());

        if (keyType == KeyType::invalid)
            return rpcError(rpcINVALID_PARAMS);
    }

    if (params.isMember (jss::passphrase) ||
        params.isMember (jss::seed) ||
        params.isMember (jss::seed_hex))
    {
        Json::Value err;
        seed = RPC::getSeedFromRPC (params, err);
        if (!seed)
            return err;
    }
    else
    {
        seed = randomSeed ();
    }

    auto const publicPrivatePair = generateKeyPair(keyType, *seed);
    //auto const publicKey = generateKeyPair (keyType, *seed).first;

    Json::Value obj (Json::objectValue);

    if (keyType == KeyType::gmalg)
    {
        obj[jss::private_key] = toBase58(TOKEN_ACCOUNT_SECRET, publicPrivatePair.second);
        //std::string private_keyDe58 = decodeBase58Token(private_keyStr, TOKEN_NODE_PRIVATE);
    }
    else
    {
        obj[jss::master_seed] = toBase58(*seed);
        obj[jss::master_seed_hex] = strHex(seed->data(), seed->size());
        obj[jss::master_key] = seedAs1751(*seed);
    }
    obj[jss::account_id] = toBase58(calcAccountID(publicPrivatePair.first));
    obj[jss::public_key] = toBase58(TOKEN_ACCOUNT_PUBLIC, publicPrivatePair.first);
    obj[jss::key_type] = to_string(keyType);
    obj[jss::public_key_hex] = strHex(publicPrivatePair.first.data(), publicPrivatePair.first.size());

    if (params.isMember (jss::passphrase))
    {
        auto const passphrase = params[jss::passphrase].asString();

        if (passphrase != seedAs1751 (*seed) &&
            passphrase != toBase58 (*seed) &&
            passphrase != strHex (seed->data(), seed->size()))
        {
            // 80 bits of entropy isn't bad, but it's better to
            // err on the side of caution and be conservative.
            if (estimate_entropy (passphrase) < 80.0)
                obj[jss::warning] =
                    "This wallet was generated using a user-supplied "
                    "passphrase that has low entropy and is vulnerable "
                    "to brute-force attacks.";
            else
                obj[jss::warning] =
                    "This wallet was generated using a user-supplied "
                    "passphrase. It may be vulnerable to brute-force "
                    "attacks.";
        }
    }

    return obj;
}

} // ripple
