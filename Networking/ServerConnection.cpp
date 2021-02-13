#include <algorithm>
#include <iostream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#define BUFFER_SIZE 1300

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>

#include "Clock.h"
#include "Engine.h"
#include "Messenger.h"
#include "NetworkManager.h"
#include "Packet.h"
#include "PacketNACK.h"
#include "PlayerConnection.h"
#include "ServerConnection.h"

namespace TechDemo
{
	namespace IO
	{
		ServerConnection::ServerConnection() : InetConnection()
		{
			dataClock = Util::Clock::addClock(1, [this]()
			{
				Engine::serverSentHistory.push_back(bytesSent / 128.0f);
				Engine::serverRcvdHistory.push_back(bytesRcvd / 128.0f);
				Engine::serverPacketsSentHistory.push_back(static_cast<float>(packetsSent));
				Engine::serverPacketsResentHistory.push_back(static_cast<float>(packetsResent));

				if (Engine::serverSentHistory.size() > 50)
					Engine::serverSentHistory.erase(Engine::serverSentHistory.begin());

				if (Engine::serverRcvdHistory.size() > 50)
					Engine::serverRcvdHistory.erase(Engine::serverRcvdHistory.begin());

				if (Engine::serverPacketsSentHistory.size() > 50)
					Engine::serverPacketsSentHistory.erase(Engine::serverPacketsSentHistory.begin());

				if (Engine::serverPacketsResentHistory.size() > 50)
					Engine::serverPacketsResentHistory.erase(Engine::serverPacketsResentHistory.begin());

				bytesSent = 0;
				bytesRcvd = 0;
				packetsSent = 0;
				packetsResent = 0;
			}, Util::Clock::getMainThreadId());
		}

		bool ServerConnection::listen(unsigned short port)
		{
			if (!connected && local)
			{
				if ((socket = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == SOCKET_ERROR)
				{
					std::cerr << "An error occurred while creating a new socket: " << getLastError().second;
					return false;
				}

				sockaddr_storage addr = {0};

				sockaddr_in& inAddr = *reinterpret_cast<sockaddr_in*>(&addr);
				inAddr.sin_family = AF_INET;
				inAddr.sin_port = htons(port);
				inAddr.sin_addr.S_un.S_addr = INADDR_ANY;

				if (::bind(socket, reinterpret_cast<const sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
				{
					std::cerr << "An error occurred while binding a socket: " << getLastError().second;
					closesocket(socket);
					socket = 0;
					return false;
				}

				/*int bufferSize = 65535;
				if (setsockopt(socket, SOL_SOCKET, SO_SNDBUF, reinterpret_cast<const char*>(&bufferSize), sizeof(bufferSize)) == -1)
					std::cerr << getLastError().second;

				if (setsockopt(socket, SOL_SOCKET, SO_RCVBUF, reinterpret_cast<const char*>(&bufferSize), sizeof(bufferSize)) == -1)
					std::cerr << getLastError().second;*/

				this->ipAddress = "127.0.0.1";
				this->port = port;
				connected = true;

				std::thread lThread(&ServerConnection::listenLoop, this);
				std::swap(listeningThread, lThread);

				return true;
			}

			return false;
		}

		bool ServerConnection::connect(std::string const& ipAddress, unsigned short port)
		{
			return false;
		}

		bool ServerConnection::disconnect()
		{
			if (connected)
			{
				std::lock_guard<std::recursive_mutex> lock(connectionsLock);
				for (std::pair<const std::string, std::shared_ptr<Connection>>& conn : connections)
					conn.second->disconnect();
				connections.clear();
			}

			return InetConnection::disconnect();
		}

		bool ServerConnection::send(PacketBase* packet) const
		{
			return packet && send(std::shared_ptr<PacketBase>(packet));
		}

		bool ServerConnection::send(std::shared_ptr<PacketBase> const& packet) const
		{
			bool sent = false;

			if (packet)
			{
				sent = true;

				std::lock_guard lock(connectionsLock);
				for (auto& client : connections)
					sent = client.second->send(packet) && sent;
			}

			return sent;
		}

		std::unordered_set<std::shared_ptr<PlayerConnection>> ServerConnection::getClients() const
		{
			std::unordered_set<std::shared_ptr<PlayerConnection>> clients;

			std::lock_guard lock(connectionsLock);
			for (auto& pair : connections)
			{
				std::shared_ptr<Connection>& conn = pair.second;

				if (std::shared_ptr<PlayerConnection> player = std::dynamic_pointer_cast<PlayerConnection>(conn))
				{
					clients.emplace(player);
				}
			}

			return clients;
		}

		void ServerConnection::listenLoop()
		{
			fd_set fds, reads;
			FD_ZERO(&fds);
			FD_SET(socket, &fds);

			timeval timeout;
			timeout.tv_sec = 0;
			timeout.tv_usec = 1000;	// One thousandth of a second.

			Util::Messenger::send(new ServerStartListening);

			while (connected)
			{
				reads = fds;

				int result = ::select(socket + 1, &reads, nullptr, nullptr, &timeout);
				if (result > 0)
				{
					int bytes;
					char buffer[BUFFER_SIZE] = {0};
					sockaddr_storage addr;
					int addrSize = sizeof(addr);

					if ((bytes = ::recvfrom(socket, buffer, BUFFER_SIZE, 0, reinterpret_cast<sockaddr*>(&addr), &addrSize)) == SOCKET_ERROR)
					{
						char ip[INET6_ADDRSTRLEN] = {0};
						inet_ntop(AF_INET, getInetAddr(reinterpret_cast<sockaddr*>(&addr)), ip, INET6_ADDRSTRLEN);
						unsigned short port = ntohs(static_cast<unsigned short>(addr.ss_family == AF_INET6 ? reinterpret_cast<struct sockaddr_in6*>(&addr)->sin6_port : (addr.ss_family == AF_INET ? reinterpret_cast<struct sockaddr_in*>(&addr)->sin_port : 0)));
						
						std::pair<int, std::string> error = getLastError();

						if (error.first != -1)
							std::cerr << "An error occurred while retrieving received packet data from " << std::string(ip) << ":" << std::to_string(port) << ": " << error.second;

						std::lock_guard<std::recursive_mutex> lock(connectionsLock);
						auto iter = connections.find(std::string(ip) + ":" + std::to_string(port));
						
						if (iter != connections.end())
						{
							std::cout << "Client " << ip << ":" << std::to_string(port) << " has disconnected." << std::endl;

							Util::Messenger::send(new ServerClientDisconnect{ std::dynamic_pointer_cast<PlayerConnection>(iter->second) });
							iter->second->disconnect();
							connections.erase(iter);
						}

						continue;
					}

					bytesRcvd += bytes;

					std::pair<std::shared_ptr<InetConnection>, bool> conn = getConnection(&addr, socket);

					if (conn.first)
					{
						BitStream data(buffer, bytes);
						
						uint32_t sequenceNumber = 0;
						data.read(sequenceNumber);

						if (conn.second)
						{
							std::cout << "Client " << conn.first->getRemoteAddress() << ":" << conn.first->getPort() << " has connected." << std::endl;

							conn.first->lastSequenceNumber = sequenceNumber;
							conn.first->expectedSequenceNumber = sequenceNumber + 1;
						}
						else
						{
							if (Util::Random::rand(0.0f, 1.0f) < packetDropChance)
							{
								++packetsLost;
								conn.first->missingPacketsLock.lock();
								conn.first->missingPackets.emplace(sequenceNumber);
								conn.first->missingPacketsLock.unlock();
								conn.first->lastSequenceNumber = std::max(conn.first->lastSequenceNumber, sequenceNumber);
								conn.first->send(new PacketNACK(sequenceNumber, 1));
								continue;
							}

							conn.first->missingPacketsLock.lock();
							conn.first->missingPackets.erase(sequenceNumber);
							conn.first->missingPacketsLock.unlock();

							if (conn.first->expectedSequenceNumber == sequenceNumber)
							{
								++conn.first->expectedSequenceNumber;

								if (!conn.first->receivedPackets.empty())
								{
									uint16_t size = 0;

									bool useBoth = conn.first->dataStream.remaining();
									BitStream* stream = useBoth ? &conn.first->dataStream : &data;
									size_t readBit = stream->readBit();

									while (!size && stream->remaining() >= 16)
									{
										stream->read(size, 16);

										uint16_t skipped = std::min(size, static_cast<uint16_t>(stream->remaining()));
										stream->skip(skipped);
										size -= skipped;

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

									if (stream->remaining())
										stream->trim(stream->remaining());

									stream->readBit(readBit);

									// Fit packet into stored packet data, and load following packets into buffer based on the packet length, incrementing expected number
									for (auto iter = conn.first->receivedPackets.find(conn.first->expectedSequenceNumber); iter != conn.first->receivedPackets.end(); conn.first->receivedPackets.erase(iter), iter = conn.first->receivedPackets.find(++conn.first->expectedSequenceNumber))
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
									}
								}
								else
								{
									uint16_t size = 0;
									data.peek(size, 16);

									if (data.remaining() - 16 > size)
										data.trim((data.remaining() - 16) - size);
								}
							}
							else if (conn.first->expectedSequenceNumber < sequenceNumber)
							{
								if (conn.first->lastSequenceNumber < sequenceNumber)
								{
									uint32_t newPackets = sequenceNumber - conn.first->lastSequenceNumber;

									if (newPackets > 1)
									{
										packetsLost += newPackets - 1;

										conn.first->missingPacketsLock.lock();
										for (uint32_t i = conn.first->lastSequenceNumber + 1; i < sequenceNumber; conn.first->missingPackets.emplace(i++));
										conn.first->missingPacketsLock.unlock();

										conn.first->send(new PacketNACK(conn.first->lastSequenceNumber + 1, newPackets - 1));
									}
								}

								// Record packet data, and delay further processing
								conn.first->receivedPackets.insert(std::make_pair(sequenceNumber, std::move(data)));
								conn.first->lastSequenceNumber = std::max(sequenceNumber, conn.first->lastSequenceNumber);
								continue;
							}
							else
							{
								++packetsDuplicated;
								continue; // Drop duplicate packet
							}

							conn.first->lastSequenceNumber = std::max(sequenceNumber, conn.first->lastSequenceNumber);
						}

						conn.first->dataStream << data;

						handle:

						if (conn.first->dataStream.remaining() >= 16)
						{
							uint16_t size = 0;
							conn.first->dataStream.peek(size, 16);

							if (size)
							{
								unsigned length = conn.first->dataStream.remaining() - 16 /* peek size */;

								if (size <= length)
								{
									conn.first->dataStream.skip(16);

									unsigned long readBit = conn.first->dataStream.readBit();

									uint32_t id = 0;
									conn.first->dataStream.read(id);

									std::shared_ptr<IO::PacketBase> packet = IO::PacketBase::getPacket(id);

									if (packet)
									{
										packet->deserialize(conn.first->dataStream);
										packet->handle(*std::dynamic_pointer_cast<Connection>(conn.first), Direction::Serverbound);
									}

									// Discard remaining packet data, and remaining bits in last byte to prepare for next packet
									//conn.first->dataStream.skip((size - (conn.first->dataStream.readBit() - readBit)) + ((Util::byteSize() - 1) - ((conn.first->dataStream.readBit() - 1) % Util::byteSize())));

									if (conn.first->dataStream.remaining() == 0)
										conn.first->dataStream.clear();

									goto handle;
								}
							}
							else
							{
								conn.first->dataStream.skip(16);
								goto handle;
							}
						}

						if (conn.second)
							Util::Messenger::send(new ServerIncomingConnect{ std::dynamic_pointer_cast<PlayerConnection>(conn.first) });
					}
				}
				else if (result == SOCKET_ERROR)
				{
					std::cerr << "An error occurred while polling for available packets: " << getLastError().second;
					continue;
				}
			}

			Util::Messenger::send(new ServerStop);
		}
	}
}