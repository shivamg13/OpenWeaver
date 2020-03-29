#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>
#include <cstring>
#include <algorithm>
#include <marlin/pubsub/PubSubNode.hpp>
#include <marlin/pubsub/witness/ChainWitnesser.hpp>
#include <marlin/pubsub/attestation/StakeAttester.hpp>
#include <marlin/pubsub/attestation/EmptyAttester.hpp>
#include <sodium.h>

using namespace marlin::net;
using namespace marlin::stream;
using namespace marlin::pubsub;
using namespace std;

using namespace CryptoPP;

class PubSubNodeDelegate {
private:
	using PubSubNodeType = PubSubNode<PubSubNodeDelegate, true, true, true, StakeAttester, ChainWitnesser>;

public:
	std::vector<uint16_t> channels = {100, 101};

	void did_unsubscribe(PubSubNodeType &, uint16_t channel) {
		SPDLOG_INFO("Did unsubscribe: {}", channel);
	}

	void did_subscribe(PubSubNodeType &ps, uint16_t channel) {
		ps.send_message_on_channel(channel, "hey", 3);
		SPDLOG_INFO("Did subscribe: {}", channel);
	}

	void did_recv_message(
		PubSubNodeType &,
		Buffer &&,
		typename PubSubNodeType::MessageHeaderType header,
		uint16_t channel,
		uint64_t message_id
	) {
		SPDLOG_INFO(
			"Received message {:spn} on channel {} with witness {}",
			spdlog::to_hex((uint8_t*)&message_id, ((uint8_t*)&message_id) + 8),
			channel,
			spdlog::to_hex(header.witness_data, header.witness_data+header.witness_size)
		);
	}

	void manage_subscriptions(
		size_t,
		typename PubSubNodeType::TransportSet&,
		typename PubSubNodeType::TransportSet&
	) {

	}
};

int main() {
	uint8_t static_sk[crypto_box_SECRETKEYBYTES];
	uint8_t static_pk[crypto_box_PUBLICKEYBYTES];

	crypto_box_keypair(static_pk, static_sk);

	PubSubNodeDelegate b_del;

	size_t max_sol_conn = 10;
	size_t max_unsol_conn = 1;

	auto addr = SocketAddress::from_string("127.0.0.1:8000");
	// ECDSA<ECP,Keccak_256>::PrivateKey priv_key1,priv_key2;
	// AutoSeededRandomPool rnd1,rnd2;
	// priv_key1.Initialize(rnd1,ASN1::secp256k1());
	// priv_key2.Initialize(rnd2,ASN1::secp256k1());

	ABCInterface abcIface;

	auto b = new PubSubNode<PubSubNodeDelegate, true, true, true, StakeAttester, ChainWitnesser>(addr, max_sol_conn, max_unsol_conn, static_sk, std::tie(abcIface), std::tie(static_sk));
	b->delegate = &b_del;

	auto addr2 = SocketAddress::from_string("127.0.0.1:8001");
	auto b2 = new PubSubNode<PubSubNodeDelegate, true, true, true, StakeAttester, ChainWitnesser>(addr2, max_sol_conn, max_unsol_conn, static_sk, std::tie(abcIface), std::tie(static_sk));
	b2->delegate = &b_del;

	SPDLOG_INFO("Start");

	b->dial(addr2, static_pk);

	return EventLoop::run();
}
