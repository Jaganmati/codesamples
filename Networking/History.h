#pragma once

#include <atomic>
#include <list>
#include <map>
#include <mutex>
#include <tuple>
#include <unordered_map>

#include "RTTI.h"
#include "UUID.h"

namespace TechDemo
{
	namespace IO
	{
		class Connection;	//!< Forward declaration
	}

	namespace World
	{
		class ComponentBase;	//!< Forward declaration

		class History
		{
			public:
				static std::atomic_bool logEverything;

				static std::list<std::shared_ptr<ComponentBase>> getState(const std::shared_ptr<IO::Connection>& conn, const Util::UUID& sceneId, unsigned long long timestamp);

				static IO::BitStream getState(const Util::UUID& sceneId, unsigned long long timestamp, const Util::UUID& componentId, const std::string& variable);

				template <typename T>
				static std::list<std::shared_ptr<T>> getState(const std::shared_ptr<IO::Connection>& conn, const Util::UUID& sceneId, unsigned long long timestamp)
				{
					static_assert(std::is_base_of_v<ComponentBase, T>, "Invalid component type provided to History::getState.");

					static const uint32_t targetHash = Util::CRC32::checksum(Util::getQualifiedName<T>());

					std::list<std::shared_ptr<T>> result;

					dataLock.lock();

					auto sceneIter = data.find(sceneId);
					if (sceneIter != data.end())
					{
						sceneIter->second.first.lock();

						auto& frames = sceneIter->second.second;
						auto frameIter = frames.lower_bound(timestamp);
						bool isBetween = frameIter != frames.end() && frameIter->first != timestamp;
						frameIter = isBetween ? frameIter != frames.begin() ? std::prev(frameIter) : frames.end() : frameIter;

						if (frameIter != frames.end())
						{
							frameIter->second.first->lock();

							auto& components = frameIter->second.second;
							for (auto& componentPair : components)
							{
								auto& uniqueId = componentPair.first;
								auto& typeHash = std::get<1>(componentPair.second);

								if (typeHash == targetHash)
								{
									auto type = Util::RTTI::getType(typeHash);

									if (type != nullptr)
									{
										auto component = ComponentBase::create(typeHash);

										if (component != nullptr)
										{
											component->uuid = uniqueId;
											component->init();

											for (const auto& variable : type->getVariableNames())
												getState(conn, frames, timestamp, component, variable);

											result.emplace_back(component);
										}
									}
								}
							}

							frameIter->second.first->unlock();
						}

						sceneIter->second.first.unlock();
					}

					dataLock.unlock();

					return result;
				}

				template <>
				static std::list<std::shared_ptr<ComponentBase>> getState(const std::shared_ptr<IO::Connection>& conn, const Util::UUID& sceneId, unsigned long long timestamp)
				{
					return getState(conn, sceneId, timestamp);
				}

				static void applyState(const std::shared_ptr<IO::Connection>& conn, const Util::UUID& sceneId, unsigned long long timestamp);

				static void log(const Util::UUID& sceneId, unsigned long long timestamp);

				static void remove(const Util::UUID& sceneId, unsigned long long timestamp);

				static void removeBefore(const Util::UUID& sceneId, unsigned long long timestamp);

				static void removeAfter(const Util::UUID& sceneId, unsigned long long timestamp);

				template <typename T>
				static std::unordered_map<Util::UUID, std::unordered_map<std::string, IO::BitStream>> getChanges(const Util::UUID& sceneId, unsigned long long timestamp)
				{
					std::unordered_map<Util::UUID, std::unordered_map<std::string, IO::BitStream>> result;

					dataLock.lock();

					auto sceneIter = data.find(sceneId);
					if (sceneIter != data.end())
					{
						auto& frameMutex = sceneIter->second.first;
						auto& frames = sceneIter->second.second;
						frameMutex.lock();
						dataLock.unlock();

						auto frameIter = frames.find(timestamp);
						if (frameIter != frames.end())
						{
							auto componentMutex = frameIter->second.first;
							auto& components = frameIter->second.second;
							componentMutex->lock();
							frameMutex.unlock();

							static uint32_t typeHash = Util::CRC32::checksum(Util::getQualifiedName<T>());

							for (auto& component : components)
							{
								auto& varMutex = std::get<0>(component.second);
								auto& varType = std::get<1>(component.second);
								auto& variables = std::get<2>(component.second);

								if (varType == typeHash)
								{
									std::unordered_map<std::string, IO::BitStream> varData;

									varMutex.lock();
									for (auto& variable : variables)
									{
										auto& streamMutex = variable.second.first;

										streamMutex.lock();
										auto stream = std::get<1>(variable.second.second);
										streamMutex.unlock();

										varData.emplace(variable.first, std::move(stream));
									}
									varMutex.unlock();

									result.emplace(component.first, std::move(varData));
								}
							}

							componentMutex->unlock();
						}
						else frameMutex.unlock();
					}
					else dataLock.unlock();

					return result;
				}

				template <>
				static std::unordered_map<Util::UUID, std::unordered_map<std::string, IO::BitStream>> getChanges<World::ComponentBase>(const Util::UUID& sceneId, unsigned long long timestamp)
				{
					return getChanges(sceneId, timestamp);
				}

				static std::unordered_map<Util::UUID, std::unordered_map<std::string, IO::BitStream>> getChanges(const Util::UUID& sceneId, unsigned long long timestamp);

				template <typename T>
				static std::unordered_map<Util::UUID, std::unordered_map<std::string, std::pair<unsigned long long, IO::BitStream>>> getChanges(const Util::UUID& sceneId, unsigned long long fromExclusive, unsigned long long toInclusive)
				{
					std::unordered_map<Util::UUID, std::unordered_map<std::string, std::pair<unsigned long long, IO::BitStream>>> result;

					if (toInclusive > fromExclusive)
					{
						dataLock.lock();

						auto sceneIter = data.find(sceneId);
						if (sceneIter != data.end())
						{
							auto& frameMutex = sceneIter->second.first;
							auto& frames = sceneIter->second.second;
							frameMutex.lock();
							dataLock.unlock();

							auto iter = std::reverse_iterator(frames.lower_bound(toInclusive));
							iter = iter == frames.rend() ? std::reverse_iterator(frames.begin()) : iter->first == toInclusive ? iter : iter != frames.rend() ? std::next(iter) : iter;
							auto from = std::reverse_iterator(frames.lower_bound(fromExclusive));
							from = from == frames.rend() || from->first == fromExclusive ? from : from != frames.rbegin() ? std::next(from) : frames.rend();

							if (iter != frames.rend())
							{
								static const uint32_t typeHash = Util::CRC32::checksum(Util::getQualifiedName<T>());

								for (; iter != from; ++iter)
								{
									auto componentMutex = iter->second.first;
									auto& components = iter->second.second;
									componentMutex->lock();

									for (auto& component : components)
									{
										auto& varType = std::get<1>(component.second);

										if (varType == typeHash)
										{
											auto& varMutex = std::get<0>(component.second);
											auto& variables = std::get<2>(component.second);

											varMutex.lock();
											for (auto& variable : variables)
											{
												auto& streamMutex = variable.second.first;

												streamMutex.lock();
												auto stream = std::get<1>(variable.second.second);
												streamMutex.unlock();

												result[component.first].try_emplace(variable.first, std::make_pair(iter->first, std::move(stream)));
											}
											varMutex.unlock();
										}
									}

									componentMutex->unlock();
								}
							}

							frameMutex.unlock();
						}
						else dataLock.unlock();
					}

					return result;
				}

				template <>
				static std::unordered_map<Util::UUID, std::unordered_map<std::string, std::pair<unsigned long long, IO::BitStream>>> getChanges<World::ComponentBase>(const Util::UUID& sceneId, unsigned long long fromExclusive, unsigned long long toInclusive)
				{
					return getChanges(sceneId, fromExclusive, toInclusive);
				}

				static std::unordered_map<Util::UUID, std::unordered_map<std::string, std::pair<unsigned long long, IO::BitStream>>> getChanges(const Util::UUID& sceneId, unsigned long long fromExclusive, unsigned long long toInclusive);

				static void applyChanges(Util::UUID const& componentId, std::unordered_map<std::string, std::pair<unsigned long long, IO::BitStream>> const& variables);

			private:
				using changeEntryType = std::tuple<unsigned long long, IO::BitStream>;																			// Record of delta since last change and new value
				using componentVarEntryType = std::unordered_map<std::string, std::pair<std::shared_ptr<std::recursive_mutex>, changeEntryType>>;				// For a given variable (by name) [uint32_t represents component type hash]
				using componentEntryType = std::unordered_map<Util::UUID, std::tuple<std::shared_ptr<std::recursive_mutex>, uint32_t, componentVarEntryType>>;	// For a given component (by component's UUID)
				using frameEntryType = std::map<unsigned long long, std::pair<std::shared_ptr<std::recursive_mutex>, componentEntryType>>;						// For a given frame (by closest server timestamp [sorted in ascending order])

				static std::unordered_map<Util::UUID, std::pair<std::recursive_mutex, frameEntryType>> data;													// For a given scene (by scene's UUID)
				static std::recursive_mutex dataLock;

				static std::unordered_map<Util::UUID, unsigned long long> lastChange;
				static std::recursive_mutex changeLock;

				// TODO: Possibly rethink this data structure to better support client-side as well as server-side (no stored connection instance client-side [nullptr may be used]).
				static std::unordered_map<Util::UUID, std::multimap<unsigned long long, std::shared_ptr<IO::Connection>>>& pendingUpdates();
				static std::mutex& getUpdatesLock();

				static void getState(const std::shared_ptr<IO::Connection>& conn, decltype(data)::iterator::value_type::second_type::second_type& frames, unsigned long long timestamp, std::shared_ptr<ComponentBase>& component, const std::string& variable);
		};
	}
}