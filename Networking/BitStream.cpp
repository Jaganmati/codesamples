#include <utility>

#include "BitStream.h"
#include "Serializable.h"

#define STARTING_SIZE 64

namespace TechDemo
{
	namespace IO
	{
		BitStream::BitStream()
		{
			buffer.reserve(STARTING_SIZE);
		}

		BitStream::BitStream(char* data, unsigned bytes) : buffer(data, data + bytes), bits(bytes * Util::byteSize()), writeBitOffset(Util::byteSize())
		{
			buffer.reserve(STARTING_SIZE);
		}

		BitStream::BitStream(BitStream const& rhs) : buffer(rhs.buffer), bits(rhs.bits.load()), writeBitOffset(rhs.writeBitOffset), readBitOffset(rhs.readBitOffset)
		{
		}

		BitStream::BitStream(BitStream&& rhs) noexcept(true) : buffer(std::move(rhs.buffer)), bits(rhs.bits.load()), writeBitOffset(std::move(rhs.writeBitOffset)), readBitOffset(std::move(rhs.readBitOffset))
		{
		}

		void BitStream::write(Util::Serializable const& obj, unsigned const& size)
		{
			const_cast<Util::Serializable&>(obj).serialize(*this);
		}

		void BitStream::readInternal(Util::Serializable& obj, unsigned const& size)
		{
			obj.deserialize(*this);
		}

		BitStream& BitStream::operator=(BitStream&& rhs) noexcept(true)
		{
			buffer = std::move(rhs.buffer);
			bits = rhs.bits.load();
			writeBitOffset = std::move(rhs.writeBitOffset);
			readBitOffset = std::move(rhs.readBitOffset);

			return *this;
		}

		bool operator==(BitStream const& lhs, BitStream const& rhs)
		{
			if (lhs.bits == rhs.bits)
			{
				for (size_t byte = 0; byte < lhs.buffer.size() && byte < rhs.buffer.size(); ++byte)
				{
					if (lhs.buffer[byte] != rhs.buffer[byte])
						return false;
				}

				return true;
			}

			return false;
		}

		bool operator!=(BitStream const& lhs, BitStream const& rhs)
		{
			return !operator==(lhs, rhs);
		}

		const std::vector<char>& BitStream::data() const
		{
			return buffer;
		}

		std::string BitStream::data(size_t offset, size_t length) const
		{
			return offset < buffer.size() ? std::string(buffer.data() + offset, std::min(length, buffer.size() - offset)) : std::string();
		}

		size_t BitStream::size() const
		{
			return bits;
		}

		size_t BitStream::remaining() const
		{
			std::lock_guard lock(readLock);
			return bits.load() - readBitOffset;
		}

		size_t BitStream::readBit() const
		{
			std::lock_guard lock(readLock);
			return readBitOffset;
		}

		void BitStream::readBit(size_t readBit)
		{
			std::lock_guard lock(readLock);
			readBitOffset = readBit;
		}

		void BitStream::clear()
		{
			writeLock.lock();
			readLock.lock();
			buffer.clear();
			readBitOffset = bits = 0;
			readLock.unlock();
			writeBitOffset = 0;
			writeLock.unlock();
		}

		void BitStream::skip(size_t bits)
		{
			std::lock_guard lock(readLock);
			readBitOffset += std::min(bits, this->bits.load() - readBitOffset);
		}

		void BitStream::trim(size_t bits)
		{
			readLock.lock();
			writeLock.lock();
			this->bits -= (bits = std::min(this->bits.load(), bits));
			writeBitOffset = static_cast<unsigned char>((writeBitOffset + (Util::byteSize() - bits)) % Util::byteSize());
			readBitOffset = std::min(readBitOffset, this->bits.load());
			writeLock.unlock();
			readLock.unlock();
		}
	}
}