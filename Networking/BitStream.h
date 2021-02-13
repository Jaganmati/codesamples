#pragma once

#include <algorithm>
#include <atomic>
#include <string>
#include <type_traits>
#include <vector>

#include <Bullet/LinearMath/btVector3.h>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include "Util.h"

#define WRITE(stream, var) stream.write(var, BITSIZE(var))

#define READ_NUM(stream, var) (var = stream.get<decltype(var)>(BITSIZE(var)))
#define READ_OBJ(stream, var) stream.read(var, BITSIZE(var))
#define READ(stream, var) if constexpr (std::is_integral_v<decltype(var)>) READ_NUM(stream, var); else READ_OBJ(stream, var)

#define PEEK_NUM(stream, var) (var = stream.peekGet<decltype(var)>(BITSIZE(var)))
#define PEEK_OBJ(stream, var) stream.peek(var, BITSIZE(var))
#define PEEK(stream, var) if constexpr (std::is_integral_v<decltype(var)>) PEEK_NUM(stream, var); else PEEK_OBJ(stream, var)

namespace TechDemo
{
	namespace Util
	{
		struct Serializable;	//!< Forward declaration
	}

	namespace IO
	{
		class BitStream
		{
			public:
				BitStream();

				BitStream(char* data, unsigned bytes);

				BitStream(BitStream const& rhs);

				BitStream(BitStream&& rhs) noexcept(true);

				template <typename T>
				void write(T const& obj)
				{
					write(obj, Util::bitSize<T>());
				}

				template <>
				void write(BitStream const& obj)
				{
					write(obj, obj.size());
				}

				template <typename T>
				void write(T const& obj, unsigned const& size)
				{
					Writer<T, std::is_base_of_v<Util::Serializable, T>>::write(*this, obj, size);
				}

				void write(Util::Serializable const& obj, unsigned const& size);

				template <>
				void write(bool const& obj, unsigned const& size)
				{
					std::lock_guard lock(writeLock);
					size_t resizeAmount = (bits + 1) / Util::byteSize() + ((bits + 1) % Util::byteSize() ? 1 : 0);
					if (resizeAmount > buffer.capacity())
					{
						readLock.lock();
						buffer.reserve(resizeAmount);
						readLock.unlock();
					}

					if (writeBitOffset == Util::byteSize() || buffer.empty())
					{
						buffer.emplace_back(0);
						writeBitOffset = 0;
					}

					buffer[buffer.size() - 1] |= (obj ? 0x1 : 0x0) << (Util::byteSize() - ++writeBitOffset);

					++bits;
				}

				template <>
				void write(std::string const& obj, unsigned const& size)
				{
					std::lock_guard lock(writeLock);
					size_t resizeAmount = ((obj.size() * Util::byteSize()) + bits) / Util::byteSize() + (((obj.size() * Util::byteSize()) + bits) % Util::byteSize() ? 1 : 0);
					if (resizeAmount > buffer.capacity())
					{
						readLock.lock();
						buffer.reserve(resizeAmount);
						readLock.unlock();
					}

					write(obj.size(), 16);

					for (const char& c : obj)
						write(c);
				}

				template <>
				void write(char* const& obj, unsigned const& size)
				{
					std::lock_guard lock(writeLock);
					size_t resizeAmount = (size + bits) / Util::byteSize() + ((size + bits) % Util::byteSize() ? 1 : 0);
					if (resizeAmount > buffer.capacity())
					{
						readLock.lock();
						buffer.reserve(resizeAmount);
						readLock.unlock();
					}

					unsigned max = size - (size % Util::byteSize());

					constexpr unsigned bitSize = Util::bitSize<decltype(obj[0])>();

					for (unsigned i = 0, s = 0; s < max; s += Util::byteSize(), ++i)
						write(obj[i], bitSize);

					if (unsigned b = size - max)
					{
						for (unsigned bit = 0, objByte = size / Util::byteSize(); bit < b; ++bit, ++bits)
						{
							if (writeBitOffset == Util::byteSize() || buffer.empty())
							{
								buffer.emplace_back(0);
								writeBitOffset = 0;
							}

							buffer[buffer.size() - 1] |= ((obj[objByte] >> ((Util::byteSize() - bit) - 1)) & 0x1) << (Util::byteSize() - ++writeBitOffset);
						}
					}
				}

				template <>
				void write(btVector3 const& obj, unsigned const& size)
				{
					std::lock_guard lock(writeLock);
					constexpr unsigned len = 3 * Util::bitSize<decltype(obj[0])>();

					size_t resizeAmount = (len + bits) / Util::byteSize() + ((len + bits) % Util::byteSize() ? 1 : 0);
					if (resizeAmount > buffer.capacity())
					{
						readLock.lock();
						buffer.reserve(resizeAmount);
						readLock.unlock();
					}

					write(obj[0]);
					write(obj[1]);
					write(obj[2]);
				}

				template <>
				void write(glm::vec3 const& obj, unsigned const& size)
				{
					std::lock_guard lock(writeLock);
					constexpr unsigned len = 3 * Util::bitSize<decltype(obj[0])>();

					size_t resizeAmount = (len + bits) / Util::byteSize() + ((len + bits) % Util::byteSize() ? 1 : 0);
					if (resizeAmount > buffer.capacity())
					{
						readLock.lock();
						buffer.reserve(resizeAmount);
						readLock.unlock();
					}

					write(obj[0]);
					write(obj[1]);
					write(obj[2]);
				}

				template <>
				void write(glm::quat const& obj, unsigned const& size)
				{
					std::lock_guard lock(writeLock);
					constexpr unsigned len = 4 * Util::bitSize<decltype(obj[0])>();

					size_t resizeAmount = (len + bits) / Util::byteSize() + ((len + bits) % Util::byteSize() ? 1 : 0);
					if (resizeAmount > buffer.capacity())
					{
						readLock.lock();
						buffer.reserve(resizeAmount);
						readLock.unlock();
					}

					write(obj[0]);
					write(obj[1]);
					write(obj[2]);
					write(obj[3]);
					//write(obj[3] < 0.0f);
				}
				
				template <>
				void write(BitStream const& obj, unsigned const& size)
				{
					std::lock_guard lock(writeLock);
					size_t len = std::min(obj.remaining(), static_cast<size_t>(size)) + 16;

					size_t resizeAmount = (len + bits) / Util::byteSize() + ((len + bits) % Util::byteSize() ? 1 : 0);
					if (resizeAmount > buffer.capacity())
					{
						readLock.lock();
						buffer.reserve(resizeAmount);
						readLock.unlock();
					}

					write(len, 16);

					BitStream& str = const_cast<BitStream&>(obj);
					unsigned long offset = obj.readBitOffset;

					while (len > 0)
					{
						unsigned bits = static_cast<unsigned>(std::min(static_cast<size_t>(Util::byteSize()), len));

						char c = 0;
						str.read(c, bits);
						write(c, bits);

						len -= bits;
					}

					str.readBitOffset = offset;
				}

				template <typename T>
				void read(T const& obj)
				{
					readInternal(const_cast<T&>(obj), Util::bitSize<T>());
				}

				template <typename T>
				void read(T const& obj, unsigned const& size)
				{
					readInternal(const_cast<T&>(obj), size);
				}

				template <typename T>
				void peek(T const& obj)
				{
					unsigned long offset = readBitOffset;
					readInternal(const_cast<T&>(obj), Util::bitSize<T>());
					readBitOffset = offset;
				}

				template <typename T>
				void peek(T const& obj, unsigned const& size)
				{
					unsigned long offset = readBitOffset;
					readInternal(const_cast<T&>(obj), size);
					readBitOffset = offset;
				}

				template <typename T>
				T get()
				{
					return get(Util::bitSize<T>());
				}

				template <typename T>
				T get(unsigned const& size)
				{
					T obj = T();
					readInternal(obj, size);

					return obj;
				}

				template <typename T>
				T peekGet()
				{
					return peekGet(Util::bitSize<T>());
				}

				template <typename T>
				T peekGet(unsigned const& size)
				{
					T obj = T();
					peek(obj, size);

					return obj;
				}

				template <typename T>
				BitStream& operator<<(T const& obj)
				{
					write(obj);
					return *this;
				}

				template <>
				BitStream& operator<<(BitStream const& obj)
				{
					std::lock_guard lock(writeLock);
					constexpr unsigned size = Util::byteSize();

					size_t resizeAmount = (obj.remaining() + bits) / size + ((obj.remaining() + bits) % Util::byteSize() ? 1 : 0);
					if (resizeAmount > buffer.capacity())
					{
						readLock.lock();
						buffer.reserve(resizeAmount);
						readLock.unlock();
					}

					BitStream& str = const_cast<BitStream&>(obj);
					unsigned long offset = str.readBitOffset;

					while (str.remaining() > 0)
					{
						unsigned bits = static_cast<unsigned>(std::min(static_cast<size_t>(size), str.remaining()));

						char c = 0;
						str.read(c, bits);
						write(c, bits);
					}

					str.readBitOffset = offset;

					return *this;
				}

				template <typename T>
				BitStream& operator>>(T& obj)
				{
					readInternal(obj, Util::bitSize<T>());
					return *this;
				}

				template <>
				BitStream& operator>>(BitStream& obj)
				{
					std::lock_guard lock(obj.writeLock);
					constexpr unsigned size = Util::byteSize();

					size_t resizeAmount = (obj.bits + bits) / Util::byteSize() + ((obj.bits + bits) % Util::byteSize() ? 1 : 0);
					if (resizeAmount > buffer.capacity())
					{
						obj.readLock.lock();
						obj.buffer.reserve(resizeAmount);
						obj.readLock.unlock();
					}

					for (char& c : buffer)
						obj.write(c, size);

					return *this;
				}

				BitStream& operator=(BitStream&& rhs) noexcept(true);

				friend bool operator==(BitStream const& lhs, BitStream const& rhs);

				friend bool operator!=(BitStream const& lhs, BitStream const& rhs);

				const std::vector<char>& data() const;

				std::string data(size_t offset, size_t length) const;

				size_t size() const;

				size_t remaining() const;

				size_t readBit() const;

				void readBit(size_t readBit);

				void clear();

				void skip(size_t bits);

				void trim(size_t bits);

				static bool equals(BitStream const& lhs, BitStream const& rhs)
				{
					return lhs == rhs;
				}

			private:
				std::vector<char> buffer;
				std::atomic<size_t> bits = 0;
				unsigned char writeBitOffset = 0;
				size_t readBitOffset = 0;
				mutable std::recursive_mutex readLock;
				std::recursive_mutex writeLock;

				template <typename T, bool>
				struct Writer
				{
					static void write(BitStream& stream, T const& obj, unsigned const& size)
					{
						std::lock_guard lock(stream.writeLock);
						size_t resizeAmount = (size + stream.bits.load()) / Util::byteSize() + ((size + stream.bits) % Util::byteSize() ? 1 : 0);
						if (resizeAmount > stream.buffer.capacity())
						{
							stream.readLock.lock();
							stream.buffer.reserve(resizeAmount);
							stream.readLock.unlock();
						}

						const char* data = reinterpret_cast<const char*>(&obj);

						for (unsigned b = 0, objByte = 0, objBitOffset = (Util::byteSize() - (size % Util::byteSize())) % Util::byteSize(); b < size; ++b, ++stream.bits)
						{
							if (stream.writeBitOffset == Util::byteSize() || stream.buffer.empty())
							{
								stream.buffer.emplace_back(0);
								stream.writeBitOffset = 0;
							}

							stream.buffer[stream.buffer.size() - 1] |= ((data[objByte] >> ((Util::byteSize() - objBitOffset) - 1)) & 0x1) << (Util::byteSize() - ++stream.writeBitOffset);

							if (++objBitOffset == Util::byteSize())
							{
								++objByte;
								objBitOffset = 0;
							}
						}
					}
				};

				template <typename T>
				struct Writer<T, true>
				{
					static void write(BitStream& stream, T const& obj, unsigned const& size)
					{
						stream.write(dynamic_cast<Util::Serializable const&>(obj), size);
					}
				};

				template <typename T, bool>
				struct Reader
				{
					static void read(BitStream& stream, T& obj, unsigned const& size)
					{
						std::lock_guard lock(stream.readLock);
						if ((stream.bits.load() - stream.readBitOffset) < size)
							return;

						char* data = reinterpret_cast<char*>(&obj);

						data[0] = 0;

						unsigned byte = stream.readBitOffset / Util::byteSize();
						unsigned bitOffset = stream.readBitOffset % Util::byteSize();

						for (unsigned b = 0, objByte = 0, objBitOffset = (Util::byteSize() - (size % Util::byteSize())) % Util::byteSize(); b < size; ++b, ++stream.readBitOffset)
						{
							data[objByte] |= ((stream.buffer[byte] >> (Util::byteSize() - ++bitOffset)) & 0x1) << (Util::byteSize() - ++objBitOffset);

							if (bitOffset == Util::byteSize())
							{
								++byte;
								bitOffset = 0;
							}

							if (objBitOffset == Util::byteSize())
							{
								++objByte;
								if (b + 1 < size)
									data[objByte] = 0;
								objBitOffset = 0;
							}
						}
					}
				};

				template <typename T>
				struct Reader<T, true>
				{
					static void read(BitStream& stream, T& obj, unsigned const& size)
					{
						stream.readInternal(dynamic_cast<Util::Serializable&>(obj), size);
					}
				};

				template <typename T>
				void readInternal(T& obj, unsigned const& size)
				{
					Reader<T, std::is_base_of_v<Util::Serializable, T>>::read(*this, obj, size);
				}
				
				void readInternal(Util::Serializable& obj, unsigned const& size);

				template <>
				void readInternal(bool& obj, unsigned const& size)
				{
					std::lock_guard lock(readLock);
					if ((bits.load() - readBitOffset) < 1)
						return;

					unsigned byte = readBitOffset / Util::byteSize();
					unsigned bitOffset = readBitOffset % Util::byteSize();

					obj = (buffer[byte] >> ((Util::byteSize() - bitOffset) - 1)) & 0x1;

					++readBitOffset;
				}

				template <>
				void readInternal(std::string& obj, unsigned const& size)
				{
					std::lock_guard lock(readLock);
					unsigned short len = 0;
					peek(len, 16);

					if ((bits.load() - readBitOffset) < (16 + static_cast<unsigned>(len * Util::byteSize())))
						return;

					readBitOffset += 16;

					obj.assign(len, '\0');

					for (unsigned short i = 0; i < len; ++i)
						readInternal(obj[i], Util::bitSize<decltype(obj[i])>());
				}

				template <>
				void readInternal(char*& obj, unsigned const& size)
				{
					unsigned max = size - (size % Util::byteSize());

					std::lock_guard lock(readLock);
					if ((bits.load() - readBitOffset) < max)
						return;

					for (unsigned i = 0, s = 0; s < max; s += Util::bitSize<decltype(obj[i])>(), ++i)
						readInternal(obj[i], Util::bitSize<decltype(obj[i])>());

					if (unsigned b = size - max)
					{
						unsigned byte = readBitOffset / Util::byteSize();
						unsigned bitOffset = readBitOffset % Util::byteSize();

						unsigned objByte = size / Util::byteSize();

						obj[objByte] = 0;

						for (unsigned i = 0; i < b; ++readBitOffset)
						{
							obj[objByte] |= ((buffer[byte] >> (Util::byteSize() - ++bitOffset)) & 0x1) << (Util::byteSize() - ++i);

							if (bitOffset == Util::byteSize())
							{
								++byte;
								bitOffset = 0;
							}
						}
					}
				}

				template <>
				void readInternal(btVector3& obj, unsigned const& size)
				{
					std::lock_guard lock(readLock);
					constexpr unsigned len = 3 * Util::bitSize<decltype(obj[0])>();

					if ((bits.load() - readBitOffset) < len)
						return;

					read(obj[0]);
					read(obj[1]);
					read(obj[2]);
				}

				template <>
				void readInternal(glm::vec3& obj, unsigned const& size)
				{
					std::lock_guard lock(readLock);
					constexpr unsigned len = 3 * Util::bitSize<decltype(obj[0])>();

					if ((bits.load() - readBitOffset) < len)
						return;

					read(obj[0]);
					read(obj[1]);
					read(obj[2]);
				}

				template <>
				void readInternal(glm::quat& obj, unsigned const& size)
				{
					std::lock_guard lock(readLock);
					constexpr unsigned len = 4 * Util::bitSize<decltype(obj[0])>();

					if ((bits.load() - readBitOffset) < len)
						return;

					read(obj[0]);
					read(obj[1]);
					read(obj[2]);
					read(obj[3]);

					/*obj[3] = std::sqrtf(1.0f - obj[0] * obj[0] + obj[1] * obj[1] + obj[2] * obj[2]);

					bool negative = false;
					read(negative);

					if (negative)
						obj[3] *= -1.0f;*/
				}

				template <>
				void readInternal(BitStream& obj, unsigned const& size)
				{
					std::lock_guard lock(readLock);
					if ((bits.load() - readBitOffset) < 16)
						return;

					size_t len = 0U;
					peek(len, 16);

					if (remaining() - 16 >= len)
					{
						readBitOffset += 16;

						while (remaining() > 0)
						{
							if (size_t bits = std::min(len, Util::byteSize()))
							{
								char c = 0;
								read(c, bits);

								obj.write(c, bits);
								len -= bits;
							}
							else break;
						}
					}
				}
		};
	}
}