#include <cryptopp/cryptlib.h>
#include <cryptopp/eccrypto.h>
#include <cryptopp/osrng.h>
#include <cryptopp/oids.h>
#include <cryptopp/files.h>
#include <cryptopp/queue.h>
#include <marlin/net/udp/UdpTransportFactory.hpp>
#include <map>
#include <spdlog/spdlog.h>
#include <spdlog/fmt/bin_to_hex.h>

#define cryptopp_SIGNBYTES 64

class MessageAttestation{
	typedef CryptoPP::ECP ECP;
	typedef CryptoPP::SHA256 SHA256;
	typedef CryptoPP::byte byte;

	private :
		// Prover : 1, Verifier : 2
		// uint8_t role=1;

		CryptoPP::ECDSA<ECP,SHA256>::PrivateKey priv_key;
		// can make it a hashmap of transport to public_key
		std::unordered_map<marlin::net::SocketAddress, CryptoPP::ECDSA<CryptoPP::ECP,SHA256>::PublicKey> pubkey_map;

		CryptoPP::AutoSeededRandomPool rnd;

/*
		void populate_pub_keys(std::string location){
			//getting from written location
			for(each line in location){
				(tranport, key) := extract(line);
				if(transport exists)
					continue;
				else
					key_map[transport] = key;
			}
		}
*/

		void Load(const std::string& filename, CryptoPP::BufferedTransformation& bt)
		{
        	CryptoPP::FileSource file(filename.data(), true);
 	        file.TransferTo(bt);
    	    bt.MessageEnd();
		}

		void LoadPrivateKey(const std::string& filename, CryptoPP::PrivateKey& key)
		{
		        CryptoPP::ByteQueue queue;
		        Load(filename, queue);
		        key.Load(queue);
		}

	public :

		MessageAttestation(){
			return;
			// priv_key.Initialize(rnd,ASN1::secp256k1());
			// can make it to load private key from a pariticular location
			// LoadPrivateKey(priv_key_filename,priv_key);
			//SPDLOG_INFO("{}",priv_key);
		}

		void set_private_key(std::string priv_key_filename){
			LoadPrivateKey(priv_key_filename,priv_key);
		}

		/*
		
		| Time Stamp | Message ID | Channel Length | Channel | Message Length | Message |
		
		*/

		//! Generating Attestaton Signature of Message being sent
		/*
		 \param time_stamp time of message attestation signature generation
		 \param msg_id unique identifier of message
		 \param channel identifier of broadcasting channel
		 \param msg_len size of message
		 \param msg communication information propagated on network
		 \param rs contains signature
		 */
		void generate_attst_signature(uint64_t &time_stamp, uint64_t msg_id, std::string channel, uint64_t msg_len, const char *msg, char* signature, uint64_t &signature_len){

			time_t t = time(NULL);
			time_stamp = (uintmax_t)t;

			CryptoPP::ECDSA<ECP,SHA256>::Signer s(priv_key);
			CryptoPP::PK_Signer &acc_signer(s);

			auto sign_accumulate = acc_signer.NewSignatureAccumulator(rnd);
			sign_accumulate->Update((byte *)&time_stamp,8);
			sign_accumulate->Update((byte *)&msg_id,8);
			uint64_t channel_len = channel.size();
			sign_accumulate->Update((byte *)&channel_len,8);
			sign_accumulate->Update((byte *)channel.data(),channel.size());
			sign_accumulate->Update((byte *)&msg_len,8);
			sign_accumulate->Update((byte *)msg,msg_len);

			signature_len = acc_signer.Sign(rnd,sign_accumulate,(byte *)signature);

			SPDLOG_INFO("ATTESTATION GENERATE_ATTST_CONCATE ### msg id: {}, channel size: {}, msg size: {}, time stamp: {}, msg: {}, channel: {}, signature_len: {}, signature: {}", msg_id, channel.size(), msg_len, time_stamp, std::string(msg,msg_len), channel, signature_len, std::string(signature,signature_len));

			return;
		}

		//! Verify Signature of Attested Message received
		/*
		 \param transport socket on which message is received 
		 \param time_stamp sender time of sending
		 \param msg_id unique identifier of message
		 \param channel_len size of sender attestation signature
		 \param channel cryptographic public key of sender attestation
		 \param msg_len size of message
		 \param msg communication information propagated on network
		 \param signature attesation signature sent by sender
		 */
		bool verify_signature(marlin::net::SocketAddress addr, uint64_t time_stamp, uint64_t msg_id, uint64_t channel_len, char *channel, uint64_t msg_len, char *msg, char* signature, uint64_t signature_len){
			
			if(addr.to_string() == "127.0.0.0:8043")
				return false;

			SPDLOG_INFO("ATTESTATION VERIFY_SIGNATURE ### msg id: {}, channel size: {}, msg size: {}, time stamp: {}, msg: {}, channel: {}", msg_id, channel_len, msg_len, time_stamp, std::string(msg,msg_len), std::string(channel,channel_len));

			// populate_pub_keys();

			CryptoPP::ECDSA<ECP,SHA256>::PublicKey pub_key;
			// pub_key = key_map.get(transport);
			priv_key.MakePublicKey(pub_key);
			CryptoPP::ECDSA<ECP, SHA256>::Verifier v(pub_key);
			CryptoPP::PK_Verifier &acc_verifier(v);

			auto sign_accumulate = acc_verifier.NewVerificationAccumulator();
			sign_accumulate->Update((byte *)&time_stamp,8);
			sign_accumulate->Update((byte *)&msg_id,8);
			sign_accumulate->Update((byte *)&channel_len,8);
			sign_accumulate->Update((byte *)channel,channel_len);
			sign_accumulate->Update((byte *)&msg_len,8);
			sign_accumulate->Update((byte *)msg,msg_len);

			acc_verifier.InputSignature(*sign_accumulate,(byte *)signature,signature_len);
			bool rslt = false;
			rslt = acc_verifier.Verify(sign_accumulate);

			return rslt;
		}

};
