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
#include <ripple/basics/Log.h>
#include <ripple/net/RPCErr.h>
#include <ripple/protocol/ErrorCodes.h>
#include <ripple/protocol/JsonFields.h>
#include <ripple/protocol/Seed.h>
#include <ripple/rpc/Context.h>
#include <dac/gmencrypt/hardencrypt/HardEncryptObj.h>

namespace ripple {

static
boost::optional<Seed>
validationSeed (Json::Value const& params)
{
    if (!params.isMember (jss::secret))
        return randomSeed ();

    return parseGenericSeed (params[jss::secret].asString ());
}

// {
//   secret: <string>   // optional
// }
//
// This command requires Role::ADMIN access because it makes
// no sense to ask an untrusted server for this.
Json::Value doValidationCreate (RPC::Context& context)
{
    Json::Value     obj(Json::objectValue);

    auto seed = validationSeed(context.params);
    if (!seed)
        return rpcError(rpcBAD_SEED);

#ifdef GM_ALG_PROCESS
    auto publicPrivatePair = generateKeyPair(KeyType::gmalg, *seed);

    obj[jss::validation_public_key] = toBase58(TokenType::TOKEN_NODE_PUBLIC, publicPrivatePair.first);
    obj[jss::validation_private_key] = toBase58(TokenType::TOKEN_NODE_PRIVATE, publicPrivatePair.second);

    return obj;
#else
    auto const private_key = generateSecretKey (KeyType::secp256k1, *seed);

    obj[jss::validation_public_key] = toBase58 (
        TokenType::TOKEN_NODE_PUBLIC,
        derivePublicKey (KeyType::secp256k1, private_key));

    obj[jss::validation_private_key] = toBase58 (
        TokenType::TOKEN_NODE_PRIVATE, private_key);

    obj[jss::validation_seed] = toBase58 (*seed);
    obj[jss::validation_key] = seedAs1751 (*seed);

    return obj;
    
#endif
}

} // ripple
