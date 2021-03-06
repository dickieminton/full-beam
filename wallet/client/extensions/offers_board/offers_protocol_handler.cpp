// Copyright 2019 The Beam Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "offers_protocol_handler.h"
#include "p2p/protocol_base.h"
#include "utility/logger.h"

namespace beam::wallet
{
    OfferBoardProtocolHandler::OfferBoardProtocolHandler(ECC::Key::IKdf::Ptr sbbsKdf, beam::wallet::IWalletDB::Ptr walletDB)
        : m_walletDB(walletDB),
          m_sbbsKdf(sbbsKdf)
    {}

    boost::optional<ByteBuffer> OfferBoardProtocolHandler::createMessage(const SwapOffer& content, const WalletID& wid) const
    {
        auto waddr = m_walletDB->getAddress(wid);

        if (waddr && waddr->isOwn())
        {
            // Get private key
            PrivateKey sk;
            PublicKey pk;
            m_sbbsKdf->DeriveKey(sk, ECC::Key::ID(waddr->m_OwnID, Key::Type::Bbs));
            pk.FromSk(sk);

            // Sign data with private key
            SwapOfferConfirmation confirmationBuilder;
            auto& contentRaw = confirmationBuilder.m_offerData;
            contentRaw = toByteBuffer(SwapOfferToken(content));
            confirmationBuilder.Sign(sk);
            auto signatureRaw = toByteBuffer(confirmationBuilder.m_Signature);

            // Create message header according to protocol
            size_t msgBodySize = contentRaw.size() + signatureRaw.size();
            assert(msgBodySize + MsgHeader::SIZE <= Bbs::s_MaxMsgSize);
            MsgHeader header(0, 0, m_protocolVersion, m_msgType, static_cast<uint32_t>(msgBodySize));

            // Combine all to final message
            ByteBuffer finalMessage(header.SIZE);
            header.write(finalMessage.data());  // copy header to finalMessage
            finalMessage.reserve(header.SIZE + header.size);
            std::copy(  std::begin(contentRaw),
                        std::end(contentRaw),
                        std::back_inserter(finalMessage));
            std::copy(  std::begin(signatureRaw),
                        std::end(signatureRaw),
                        std::back_inserter(finalMessage));
            
            return finalMessage;
        }
        return boost::none;
    };

    boost::optional<SwapOffer> OfferBoardProtocolHandler::parseMessage(const ByteBuffer& msg) const
    {        
        SwapOfferToken token;
        SwapOfferConfirmation confirmation;

        try
        {
            Deserializer d;
            d.reset(msg.data(), msg.size());
            d & token;
            d & confirmation.m_Signature;
        }
        catch(...)
        {
            LOG_WARNING() << "offer board message deserialization exception";
            return boost::none;
        }
        
        confirmation.m_offerData = toByteBuffer(token);
        if (token.getPublicKey() && !confirmation.IsValid(token.getPublicKey()->m_Pk))
        {
            LOG_WARNING() << "offer board message signature is invalid";
            return boost::none;
        }
        return token.Unpack();
    }

} // namespace beam::wallet