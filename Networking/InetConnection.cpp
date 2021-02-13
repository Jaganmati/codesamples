#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#include <iostream>
#include <memory>
#include <set>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "Clock.h"
#include "Engine.h"
#include "InetConnection.h"
#include "Packet.h"
#include "PacketHandshake.h"
#include "PacketNACK.h"
#include "PlayerConnection.h"

#define BUFFER_SIZE 1300

namespace TechDemo
{
	namespace IO
	{
		InetConnection::InetConnection() : Connection()
		{
		}

		InetConnection::InetConnection(std::string const& ipAddress, unsigned short port, unsigned socket) : Connection(), socket(socket), ipAddress(ipAddress), port(port), local(false)
		{
			connected = true;
		}

		InetConnection::~InetConnection()
		{
			std::lock_guard lock(sendLock);
			disconnect();

			if (listeningThread.joinable())
				listeningThread.join();
		}

		bool InetConnection::connect(std::string const& address)
		{
			// TODO: Check the protocol format of the address
			size_t index = address.find_last_of(':');

			if (index != std::string::npos)
			{
				std::string ipAddress(address.substr(0, index));
				unsigned short port = static_cast<unsigned short>(std::stoi(std::string(address.substr(index + 1))));

				return connect(ipAddress, port);
			}

			return false;
		}

		bool InetConnection::connect(std::string const& ipAddress, unsigned short port)
		{
			if (!connected && local)
			{
				sockaddr_storage addr = { 0 };

				sockaddr_in& inAddr = *reinterpret_cast<sockaddr_in*>(&addr);

				int result = inet_pton(AF_INET, ipAddress.c_str(), &inAddr.sin_addr);
				if (result == 0)
				{
					std::cerr << "An error occurred while parsing a provided IP address: Invalid IP address." << std::endl;
					return false;
				}
				else if (result == -1)
				{
					std::cerr << "An error occurred while parsing a provided IP address: " << getLastError().second;
					return false;
				}

				inAddr.sin_family = AF_INET;
				inAddr.sin_port = htons(port);

				if ((socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
				{
					std::cerr << "An error occurred while creating a new socket: " << getLastError().second;
					return false;
				}

				/*char opt = 1;
				if (setsockopt(socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == SOCKET_ERROR)
				{
					std::cerr << "An error occurred while setting a socket setting: " << getLastError();
					return false;
				}*/

				if ((::connect(socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr))) == SOCKET_ERROR)
				{
					std::cerr << "An error occurred while attempting to connect to the address \"" << ipAddress << ":" << std::to_string(port) << "\": " << getLastError().second;
					closesocket(socket);
					socket = 0;
					return false;
				}

				/*int bufferSize = 65535;
				if (setsockopt(socket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufferSize), sizeof(bufferSize)) == -1)
					std::cerr << getLastError().second;
				
				if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufferSize), sizeof(bufferSize)) == -1)
					std::cerr << getLastError().second;*/

				char address[INET6_ADDRSTRLEN] = {0};
				inet_ntop(AF_INET, getInetAddr(reinterpret_cast<sockaddr*>(&addr)), address, INET6_ADDRSTRLEN);

				this->ipAddress = address;
				this->port = port;

				int addrLen = sizeof(addr);
				getsockname(socket, reinterpret_cast<sockaddr*>(&addr), &addrLen);
				localPort = ntohs(reinterpret_cast<const sockaddr*>(&addr)->sa_family == AF_INET ? inAddr.sin_port : reinterpret_cast<const sockaddr_in6*>(&addr)->sin6_port);
				connecting = true;

				dataClock = Util::Clock::addClock(1, [this]()
				{
					if (isConnected())
					{
						Engine::clientSentHistory.push_back(bytesSent / 128.0f);
						Engine::clientRcvdHistory.push_back(bytesRcvd / 128.0f);
						Engine::clientPacketsSentHistory.push_back(static_cast<float>(packetsSent));
						Engine::clientPacketsResentHistory.push_back(static_cast<float>(packetsResent));
						Engine::clientPacketsLostHistory.push_back(static_cast<float>(packetsLost));
						Engine::clientPacketsDuplicatedHistory.push_back(static_cast<float>(packetsDuplicated));

						if (Engine::clientSentHistory.size() > 50)
							Engine::clientSentHistory.erase(Engine::clientSentHistory.begin());

						if (Engine::clientRcvdHistory.size() > 50)
							Engine::clientRcvdHistory.erase(Engine::clientRcvdHistory.begin());

						if (Engine::clientPacketsSentHistory.size() > 50)
							Engine::clientPacketsSentHistory.erase(Engine::clientPacketsSentHistory.begin());

						if (Engine::clientPacketsResentHistory.size() > 50)
							Engine::clientPacketsResentHistory.erase(Engine::clientPacketsResentHistory.begin());

						if (Engine::clientPacketsLostHistory.size() > 50)
							Engine::clientPacketsLostHistory.erase(Engine::clientPacketsLostHistory.begin());

						if (Engine::clientPacketsDuplicatedHistory.size() > 50)
							Engine::clientPacketsDuplicatedHistory.erase(Engine::clientPacketsDuplicatedHistory.begin());
					}

					bytesSent = 0;
					bytesRcvd = 0;
					packetsSent = 0;
					packetsResent = 0;
					packetsLost = 0;
					packetsDuplicated = 0;
				}, Util::Clock::getMainThreadId());

				send(new PacketHandshake);

				std::thread thread(&InetConnection::connectLoop, this);

				std::swap(listeningThread, thread);

				return true;
			}

			return false;
		}

		bool InetConnection::disconnect()
		{
			if (connected || connecting)
			{
				std::lock_guard lock(sendLock);
				if (local)
				{
					shutdown(socket, SD_SEND);

					Util::Clock::removeClock(dataClock);
					dataClock = 0;
				}
				socket = 0;
				connected = false;
				connecting = false;
				return true;
			}

			return false;
		}

		bool InetConnection::send(PacketBase* packet) const
		{
			std::lock_guard lock(sendLock);
			return packet && send(std::shared_ptr<PacketBase>(packet));
		}

		bool InetConnection::send(std::shared_ptr<PacketBase> const& packet) const
		{
			if (packet && (connected || connecting) && socket)
			{
				std::lock_guard lock(sendLock);
				if (connected || connecting)
				{
					IO::BitStream str, buff;

					str.write(packet->getId());
					packet->serialize(str);

					auto& data = str.data();
					uint32_t sequenceNumber = nextSequenceNumber++;
					buff.write(sequenceNumber);
					buff.write(static_cast<uint16_t>(str.size()), 16);

					for (size_t remaining = data.size(), size = std::min(remaining, static_cast<size_t>(BUFFER_SIZE) - buff.data().size()), offset = 0; remaining > 0; remaining -= size, offset += size, size = std::min(remaining, static_cast<size_t>(BUFFER_SIZE) - buff.data().size()))
					{
						std::string b = str.data(offset, size);
						buff.write(b.data(), static_cast<unsigned>(std::min(b.size() * Util::byteSize(), str.size() - (offset * Util::byteSize()))));

						std::string buffer(buff.data().data(), buff.data().size());

						if (packet->shouldRetransmit())
						{
							sentPacketLock.lock();
							sentPackets.insert(std::make_pair(sequenceNumber, buffer));
							sentPacketLock.unlock();
						}
						else
						{
							BitStream bs;
							bs.write(sequenceNumber);	// Sequence Number
							if (offset == 0)
								bs.write(0, 16);		// Size

							sentPacketLock.lock();
							sentPackets.insert(std::make_pair(sequenceNumber, std::string(bs.data().data(), bs.data().size())));
							sentPacketLock.unlock();
						}
						
						// TODO: Replace with priority order controlled buffer
						if (::send(socket, buffer.data(), buffer.size(), 0) == SOCKET_ERROR)
						{
							std::cerr << "An error occurred while sending a packet: " << getLastError().second;
							return false;
						}

						bytesSent += buffer.size();
						++packetsSent;
						buff.clear();

						if (remaining - size > 0)
							buff.write(sequenceNumber = nextSequenceNumber++);
					}

					return true;
				}
			}

			return false;
		}

		bool InetConnection::send(const char* data, unsigned short length) const
		{
			if (data && length && (connected || connecting) && socket)
			{
				std::lock_guard lock(sendLock);

				if (connected || connecting)
				{
					if (::send(socket, data, length, 0) == SOCKET_ERROR)
					{
						std::cerr << "An error occurred while sending a packet: " << getLastError().second;
						return false;
					}

					bytesSent += length;
					++packetsResent;

					return true;
				}
			}
			
			return false;
		}

		std::string const& InetConnection::getRemoteAddress() const
		{
			return ipAddress;
		}

		unsigned short InetConnection::getPort() const
		{
			return port;
		}

		unsigned short InetConnection::getLocalPort() const
		{
			return localPort;
		}

		bool InetConnection::isLocal() const
		{
			return local;
		}

		long InetConnection::getTimeOffset() const
		{
			return timeOffset;
		}

		void InetConnection::connectLoop()
		{
			fd_set fds, reads;
			FD_ZERO(&fds);
			FD_SET(socket, &fds);

			timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 5000;	// Five thousandths of a second.

			while (connected || connecting)
			{
				reads = fds;

				if (::select(socket + 1, &reads, nullptr, nullptr, &timeout) <= 0)
					continue;

				static bool verbose = false;

				int bytes = 0;
				char buffer[BUFFER_SIZE];

				if ((bytes = ::recv(socket, buffer, BUFFER_SIZE, 0)) == SOCKET_ERROR)
				{
					std::pair<int, std::string> error = getLastError();

					if (error.first != -1)
					{
						std::cerr << "An error occurred while looking for received packet data: " << error.second;
						disconnect();
					}

					continue;
				}

				/*if (packetDropChance)
				{
					verbose = true;
					__debugbreak();
				}*/

				if (verbose)
					std::cout << "Received bytes: " << bytes << std::endl;

				bytesRcvd += bytes;

				IO::BitStream data(buffer, bytes);

				uint32_t sequenceNumber = 0;
				data.read(sequenceNumber);

				if (verbose)
					std::cout << "Sequence number " << sequenceNumber << std::endl;

				if (!initialized)
				{
					if (data.remaining() >= 48)
					{
						static const uint32_t handshakeId = Util::CRC32::checksum(typeid(PacketHandshake).name());

						lastSequenceNumber = std::max(sequenceNumber, lastSequenceNumber);

						unsigned long readBit = data.readBit();

						data.skip(16);

						uint32_t typeId = 0;
						data.read(typeId);

						data.readBit(readBit);

						if (typeId != handshakeId)
						{
							receivedPackets.insert(std::make_pair(sequenceNumber, std::move(data)));
							continue;
						}
						else
						{
							expectedSequenceNumber = sequenceNumber;
							initialized = true;

							if (lastSequenceNumber != sequenceNumber)
							{
								auto next = receivedPackets.upper_bound(sequenceNumber);

								uint32_t difference = next->first - sequenceNumber;

								if (difference > 1)
								{
									packetsLost += difference - 1;

									missingPacketsLock.lock();
									for (uint32_t i = sequenceNumber + 1; i < next->first; missingPackets.emplace(i++));
									missingPacketsLock.unlock();

									send(new PacketNACK(sequenceNumber + 1, difference - 1));
								}
							}
						}
					}
					else
					{
						receivedPackets.insert(std::make_pair(sequenceNumber, std::move(data)));
						lastSequenceNumber = std::max(sequenceNumber, lastSequenceNumber);
						continue;
					}
				}

				if (!connecting && Util::Random::rand(0.0f, 1.0f) < packetDropChance)
				{
					++packetsLost;
					missingPacketsLock.lock();
					missingPackets.emplace(sequenceNumber);
					missingPacketsLock.unlock();
					lastSequenceNumber = std::max(lastSequenceNumber, sequenceNumber);
					send(new PacketNACK(sequenceNumber, 1));
					continue;
				}

				missingPacketsLock.lock();
				missingPackets.erase(sequenceNumber);
				missingPacketsLock.unlock();

				if (expectedSequenceNumber == sequenceNumber)
				{
					++expectedSequenceNumber;

					if (verbose)
						std::cout << "Expected the sequence number" << std::endl;

					//std::cout << "In: " << sequenceNumber << ", Want: " << expectedSequenceNumber << std::endl;

					if (!receivedPackets.empty())
					{
						uint16_t size = 0;

						bool useBoth = dataStream.remaining();
						BitStream* stream = useBoth ? &dataStream : &data;
						size_t readBit = stream->readBit();

						while (!size && stream->remaining() >= 16)
						{
							stream->read(size, 16);

							if (verbose)
								std::cout << "Read size " << size << std::endl;

							uint16_t skipped = std::min(size, static_cast<uint16_t>(stream->remaining()));
							stream->skip(skipped);
							size -= skipped;

							if (verbose)
								std::cout << "Size " << size << " with " << stream->remaining() << " remaining" << std::endl;

							if (size && useBoth)
							{
								stream->readBit(readBit);
								stream = &data;
								readBit = stream->readBit();
								stream->skip(size);
								size -= std::min(size, static_cast<uint16_t>(stream->remaining()));
								useBoth = false;
							}
						}

						if (verbose)
							std::cout << "Final size " << size << std::endl;

						if (stream->remaining())
							stream->trim(stream->remaining());

						stream->readBit(readBit);

						// Fit packet into stored packet data, and load following packets into buffer based on the packet length, incrementing expected number
						for (auto iter = receivedPackets.find(expectedSequenceNumber); iter != receivedPackets.end(); receivedPackets.erase(iter), iter = receivedPackets.find(++expectedSequenceNumber))
						{
							BitStream& packetData = iter->second;

							if (size == 0)
							{
								packetData.peek(size, 16);
								size += 16;
							}

							uint16_t len = std::min(size, static_cast<uint16_t>(packetData.remaining()));

							if (packetData.remaining() > len)
								packetData.trim(packetData.remaining() - len);

							data << packetData;

							size -= len;

							//std::cout << "Out (Late): " << iter->first << ", Want: " << (expectedSequenceNumber + 1) << std::endl;
						}
					}
					else
					{
						uint16_t size = 0;
						data.peek(size, 16);

						if (verbose)
							std::cout << "Packet size " << size << ", trimming " << ((data.remaining() - 16) - size) << " bits" << std::endl;

						if (data.remaining() - 16 > size)
							data.trim((data.remaining() - 16) - size);
					}
				}
				else if (expectedSequenceNumber < sequenceNumber)
				{
					if (verbose)
						std::cout << "Packet received early" << std::endl;

					if (lastSequenceNumber < sequenceNumber)
					{
						uint32_t newPackets = sequenceNumber - lastSequenceNumber;

						if (newPackets > 1)
						{
							packetsLost += newPackets - 1;

							missingPacketsLock.lock();
							for (uint32_t i = lastSequenceNumber + 1; i < sequenceNumber; missingPackets.emplace(i++));
							missingPacketsLock.unlock();

							send(new PacketNACK(lastSequenceNumber + 1, newPackets - 1));
						}
					}

					//std::cerr << "In (Early): " << sequenceNumber << ", Want: " << expectedSequenceNumber << std::endl;

					// Record packet data, and delay further processing
					receivedPackets.insert(std::make_pair(sequenceNumber, std::move(data)));
					lastSequenceNumber = std::max(sequenceNumber, lastSequenceNumber);
					continue;
				}
				else
				{
					if (verbose)
						std::cout << "Packet duplicated" << std::endl;
					
					++packetsDuplicated;
					//std::cerr << "In (Duplicate): " << sequenceNumber << ", Want: " << expectedSequenceNumber << std::endl;
					continue; // Drop duplicate packet
				}

				lastSequenceNumber = std::max(sequenceNumber, lastSequenceNumber);

				if (verbose)
					std::cout << "Adding data to stream (" << data.remaining() << " bits)" << std::endl;

				dataStream << data;

				handle:

				if (verbose)
					std::cout << "Stream length is now " << dataStream.remaining() << " bits" << std::endl;

				if (dataStream.remaining() >= 16)
				{
					uint16_t size = 0;
					dataStream.peek(size, 16);

					if (size)
					{
						unsigned length = dataStream.remaining() - 16 /* peeked size */;

						if (size <= length)
						{
							dataStream.skip(16);

							size_t targetReadBit = dataStream.readBit() + size;

							uint32_t id = 0;
							dataStream.read(id);

							std::shared_ptr<PacketBase> packet = PacketBase::getPacket(id);

							if (packet)
							{
								packet->deserialize(dataStream);
								packet->handle(*dynamic_cast<Connection const*>(this), Direction::Clientbound);
							}

							// Discard remaining packet data, and remaining bits in last byte to prepare for next packet
							//dataStream.skip((size - (dataStream.readBit() - readBit)) + ((Util::byteSize() - 1) - ((dataStream.readBit() - 1) % Util::byteSize())));

							if (dataStream.readBit() != targetReadBit)
							{
								std::cout << "Size: " << size << ", Length: " << length << ", Target: " << targetReadBit << ", Result: " << dataStream.readBit() << " on " << packet->getQualifiedName() << " with " << dataStream.remaining() << " bits remaining" << std::endl;

								dataStream.skip(targetReadBit - dataStream.readBit());

								__debugbreak();
							}
							
							if (dataStream.remaining() == 0)
								dataStream.clear();

							goto handle;
						}
						else
						{
							std::cout << "Size: " << size << ", Length: " << length << std::endl;

							__debugbreak();
						}
					}
					else
					{
						dataStream.skip(16);
						goto handle;
					}
				}
			}
		}

		std::pair<int, std::string> InetConnection::getLastError()
		{
			int error = WSAGetLastError();

			if (error > WSABASEERR && error != WSAEWOULDBLOCK)
			{
				constexpr int flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;

				char* s;
				FormatMessage(flags, nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s, 0, nullptr);
				std::string result(s);
				LocalFree(s);

				return std::make_pair(error, std::string("[") + std::to_string(error) + "] " + result);
			}

			return std::make_pair<int, std::string>(-1, "Unable to retrieve error.\n");
		}

		std::pair<int, std::string> InetConnection::getLastError(unsigned socket)
		{
			constexpr int flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
			int error;
			int errSize = sizeof(error);
			if (getsockopt(socket, SOL_SOCKET, SO_ERROR, reinterpret_cast<char*>(&error), &errSize) == SOCKET_ERROR && error > WSABASEERR && error != WSAEWOULDBLOCK)
			{
				char* s;
				FormatMessage(flags, nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&s, 0, nullptr);
				std::string result(s);
				LocalFree(s);

				return std::make_pair(error, std::string("[") + std::to_string(error) + "] " + result);
			}

			return std::make_pair<int, std::string>(-1, "Unable to retrieve error.\n");
		}

		void* InetConnection::getInetAddr(sockaddr* addr)
		{
			if (addr->sa_family == AF_INET)
				return &(reinterpret_cast<sockaddr_in*>(addr)->sin_addr);

			return &(reinterpret_cast<sockaddr_in6*>(addr)->sin6_addr);
		}

		void InetConnection::setDropChance(float dropChance)
		{
			packetDropChance = std::clamp(dropChance, 0.0f, 1.0f);
		}

		float InetConnection::getDropChance() const
		{
			return packetDropChance;
		}

		std::shared_ptr<InetConnection> InetConnection::getConnection(sockaddr_storage* address)
		{
			if (address)
			{
				char addr[INET6_ADDRSTRLEN] = {0};
				inet_ntop(address->ss_family, getInetAddr(reinterpret_cast<sockaddr*>(address)), addr, INET6_ADDRSTRLEN);

				std::string ipAddress(addr);
				unsigned short port = ntohs(static_cast<unsigned short>(address->ss_family == AF_INET6 ? reinterpret_cast<struct sockaddr_in6*>(address)->sin6_port : (address->ss_family == AF_INET ? reinterpret_cast<struct sockaddr_in*>(address)->sin_port : 0)));

				return std::dynamic_pointer_cast<InetConnection>(Connection::getConnection(ipAddress + ":" + std::to_string(port)));
			}

			return nullptr;
		}

		std::pair<std::shared_ptr<InetConnection>, bool> InetConnection::getConnection(sockaddr_storage* address, unsigned socket)
		{
			if (address)
			{
				char addr[INET6_ADDRSTRLEN] = { 0 };
				inet_ntop(address->ss_family, getInetAddr(reinterpret_cast<sockaddr*>(address)), addr, INET6_ADDRSTRLEN);

				std::string ipAddress(addr);
				unsigned short port = ntohs(static_cast<unsigned short>(address->ss_family == AF_INET6 ? reinterpret_cast<struct sockaddr_in6*>(address)->sin6_port : (address->ss_family == AF_INET ? reinterpret_cast<struct sockaddr_in*>(address)->sin_port : 0)));

				std::string combined(ipAddress + ":" + std::to_string(port));

				std::lock_guard<std::recursive_mutex> lock(connectionsLock);
				std::shared_ptr<InetConnection> result = std::dynamic_pointer_cast<InetConnection>(Connection::getConnection(combined));

				bool created = false;

				if (socket && !result)
				{
					result = std::shared_ptr<InetConnection>(new PlayerConnection(ipAddress, port, socket));
					connections.emplace(combined, std::dynamic_pointer_cast<Connection>(result));
					created = true;
				}

				return std::make_pair(result, created);
			}

			return std::pair<std::shared_ptr<InetConnection>, bool>(nullptr, false);
		}
	}
}