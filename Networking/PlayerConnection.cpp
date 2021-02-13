#include <chrono>
#include <iostream>
#include <sstream>

#include <winsock2.h>
#include <ws2tcpip.h>

#include "Clock.h"
#include "GameObject.h"
#include "History.h"
#include "Packet.h"
#include "PacketNACK.h"
#include "PacketPing.h"
#include "PacketUpdateComponent.h"
#include "PacketUpdateComponents.h"
#include "PacketUpdateRigidBody.h"
#include "PacketUpdateTransform.h"
#include "PlayerConnection.h"
#include "PlayerController.h"
#include "Serializable.h"
#include "ServerConnection.h"

#include "Engine.h"

#define BUFFER_SIZE 1300

namespace TechDemo
{
	namespace IO
	{
		PlayerConnection::PlayerConnection() : InetConnection()
		{
		}

		PlayerConnection::PlayerConnection(std::string const& ipAddress, unsigned short port, unsigned socket) : InetConnection(ipAddress, port, socket)
		{
			connected = true;
			rttClock = Util::Clock::addClock(40, [this]()
			{
				if (isConnected())
				{
					rttHistory.push_back(static_cast<float>(rtt.load()));
					if (rttHistory.size() > 150)
						rttHistory.erase(rttHistory.begin());

					unsigned long long timestamp = Engine::getTimestamp();

					PacketPing* ping = new PacketPing(Direction::Clientbound);
					ping->setTimestamp(timestamp);
					send(std::shared_ptr<PacketBase>(ping));

					//auto changes = World::History::getChanges<World::Transform>(Engine::getScene().getUUID(), lastUpdate, timestamp);
					
					//if (!changes.empty())
					//	send(new PacketUpdateComponents(changes));

					//for (auto const& pair : World::History::getChanges(Engine::getScene().getUUID(), lastUpdate, timestamp))
					//	send(new PacketUpdateComponent(pair.first, pair.second));

					missingPacketsLock.lock();
					if (!missingPackets.empty())
						send(new PacketNACK(missingPackets));
					missingPacketsLock.unlock();

					lastUpdate = timestamp;
				}
			}, Util::Clock::getMainThreadId());
		}

		PlayerConnection::~PlayerConnection()
		{
			Util::Clock::removeClock(rttClock);
		}

		bool PlayerConnection::connect(std::string const& address)
		{
			return InetConnection::connect(address);
		}

		bool PlayerConnection::connect(std::string const& ipAddress, unsigned short port)
		{
			if (InetConnection::connect(ipAddress, port))
			{
				rttClock = Util::Clock::addClock(40, [this]()
				{
					if (isConnected())
					{
						rttHistory.push_back(static_cast<float>(rtt.load()));
						if (rttHistory.size() > 150)
							rttHistory.erase(rttHistory.begin());

						unsigned long long timestamp = Engine::getTimestamp();

						PacketPing* ping = new PacketPing(Direction::Serverbound);
						ping->setTimestamp(timestamp);
						send(std::shared_ptr<PacketBase>(ping));

						for (auto const& pair : World::History::getChanges(Engine::getScene().getUUID(), lastUpdate, timestamp))
							send(new PacketUpdateComponent(pair.first, pair.second));

						missingPacketsLock.lock();
						if (!missingPackets.empty())
							send(new PacketNACK(missingPackets));
						missingPacketsLock.unlock();

						lastUpdate = timestamp;
					}
				}, Util::Clock::getMainThreadId());

				return true;
			}

			return false;
		}

		bool PlayerConnection::disconnect()
		{
			if (InetConnection::disconnect())
			{
				Util::Clock::removeClock(rttClock);
				rttClock = 0;
				rtt = 0;

				return true;
			}

			return false;
		}

		bool PlayerConnection::send(PacketBase* packet) const
		{
			return InetConnection::send(packet);
		}

		bool PlayerConnection::send(std::shared_ptr<PacketBase> const& packet) const
		{
			if (!local)
			{
				if (packet && connected && socket)
				{
					std::lock_guard lock(sendLock);

					if (connected)
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
								IO::BitStream bs;
								bs.write(sequenceNumber);	// Sequence Number
								if (offset == 0)
									bs.write(0, 16);		// Size

								sentPacketLock.lock();
								sentPackets.insert(std::make_pair(sequenceNumber, std::string(bs.data().data(), bs.data().size())));
								sentPacketLock.unlock();
							}

							sockaddr_storage addr = { 0 };

							sockaddr_in& inAddr = *reinterpret_cast<sockaddr_in*>(&addr);
							inAddr.sin_family = AF_INET;
							inAddr.sin_port = htons(port);
							inet_pton(AF_INET, ipAddress.data(), &inAddr.sin_addr);

							// TODO: Replace with priority order controlled buffer
							if (::sendto(socket, buffer.data(), buffer.size(), 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
							{
								std::cerr << "An error occurred while sending a packet to " << ipAddress << ":" << port << ": " << getLastError().second;
								return false;
							}

							Engine::server->bytesSent += buffer.size();
							++Engine::server->packetsSent;
							buff.clear();

							if (remaining - size > 0)
								buff.write(sequenceNumber = nextSequenceNumber++);
						}

						return true;
					}
				}

				return false;
			}
			
			return InetConnection::send(packet);
		}

		bool PlayerConnection::send(const char* data, unsigned short length) const
		{
			if (data && length && connected && socket)
			{
				std::lock_guard lock(sendLock);
				if (connected)
				{
					if (!local)
					{
						sockaddr_storage addr = { 0 };

						sockaddr_in& inAddr = *reinterpret_cast<sockaddr_in*>(&addr);
						inAddr.sin_family = AF_INET;
						inAddr.sin_port = htons(port);
						inet_pton(AF_INET, ipAddress.data(), &inAddr.sin_addr);

						if (::sendto(socket, data, length, 0, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR)
						{
							std::cerr << "An error occurred while sending a packet to " << ipAddress << ":" << port << ": " << getLastError().second;
							return false;
						}
					}
					else if (::send(socket, data, length, 0) == SOCKET_ERROR)
					{
						std::cerr << "An error occurred while sending a packet: " << getLastError().second;
						return false;
					}

					(local ? bytesSent : Engine::server->bytesSent) += length;
					local ? ++packetsResent : ++Engine::server->packetsResent;

					return true;
				}
			}

			return false;
		}

		int PlayerConnection::getRTT() const
		{
			return rtt;
		}

		const std::vector<float>& PlayerConnection::getRTTHistory() const
		{
			return rttHistory;
		}
	}
}