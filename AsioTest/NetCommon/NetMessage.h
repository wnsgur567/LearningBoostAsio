#pragma once

#include "NetCommon.h"

namespace olc
{
	namespace net
	{
		// Message Header is sent at start of all messages.
		// The template allows us to use "enum class" to ensure that the messages are valid at complie time
		template <typename T>
		struct message_header
		{
			T id{};
			uint32_t size = 0;
		};

		template <typename T>
		struct message
		{
			message_header<T> header{};
			std::vector<uint8_t> body;

			// return size of entire message packet in bytes
			size_t size() const
			{
				return sizeof(message_header<T>) + body.size();
			}

			friend std::ostream& operator << (std::ostream& os, const message<T>& msg)
			{
				os << "ID:" << int(msg.header.id) << " Size:" << msg.header.size;
				return os;
			}

			// Convenience Operator overloads - These allow us to add and remove stuff from
			// the body vector as if it were a stack, so First in, Last Out. These are a 
			// template in itself, because we dont know what data type the user is pushing or 
			// popping, so lets allow them all. NOTE: It assumes the data type is fundamentally
			// Plain Old Data (POD). TLDR: Serialise & Deserialise into/from a vector
			// POD - plain old data (생성 소멸 및 가상 멤버함수가 없는 클래스)

			// pushes any POD-like data into the message buffer
			template<typename DataType>
			friend message<T>& operator << (message<T>& msg, const DataType& data)
			{
				// Check that the type of the data being pushed is trivially copyable
				static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector");

				// Cache current size of vector, as this will be the point we insert the data
				size_t i = msg.body.size();
				// Resize the vector by the size
				msg.body.resize(msg.body.size() + sizeof(DataType));
				// Physically copy the data into the newly allocated vector space
				std::memcpy(msg.body.data() + i, &data, sizeof(DataType));
				// Recalculate the message size
				msg.header.size = static_cast<uint32_t>(msg.size());

				// Return the targe message so it can be "chained"
				return msg;
			}

			// Pulls any POD-like data form the message buffer
			template<typename DataType>
			friend message<T>& operator >> (message<T>& msg, DataType& data)
			{
				// Check that the type of the data being pushed is trivially copyable
				static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pulled from vector");

				// Cache the location towards the end of the vector where the pulled data starts
				size_t i = msg.body.size() - sizeof(DataType);
				// Physically copy the data from the vector into the user variable
				std::memcpy(&data, msg.body.data() + i, sizeof(DataType));
				// Shrink the vector to remove read bytes, and reset end position
				msg.body.resize(i);
				// Recaculate the message size
				msg.header.size = msg.size();

				// Return the targe message so it can be "chained"
				return msg;
			}
		};


		// owned_message 도 일반 message와 동일한데, 헌재 connection 과 연관되어있을 뿐(소유자 표시)
		// server 시스템에서 message의 주인은 message를 보낸 client 이고
		// client 시스템에서 message의 주인은 server이다

		// Forward declare the connection
		template <typename T>
		class connection;


		template <typename T>
		struct owned_message
		{
			std::shared_ptr<connection<T>> remote = nullptr;
			message<T> msg;

			friend std::ostream& operator << (std::ostream& os, const owned_message<T>& msg)
			{
				os << msg.msg;
				return os;
			}
		};
	}
}