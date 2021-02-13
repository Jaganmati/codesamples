#pragma once

#include <type_traits>
#include <unordered_map>
#include <unordered_set>

#include "BitStream.h"
#include "Hash.h"
#include "Util.h"

#define TOSTRING(str) #str
#define PREPARE_TYPE(type) friend struct TechDemo::Util::Registrant<type>;\
static TechDemo::Util::Registrant<type> _registrant
#define REGISTER(type) TechDemo::Util::Registrant<type> type::_registrant = TechDemo::Util::Registrant<type>();\
void TechDemo::Util::Registrant<type>::init()
#define ADDBASETO(targetType, baseType) TechDemo::Util::Registrant<targetType>::addBase<baseType>()
#define ADDBASE(baseType) ADDBASETO(type, baseType)
#define ADDVARIABLETO(targetType, varName) registerType<decltype(reinterpret_cast<targetType*>(0)->varName)>();\
TechDemo::Util::Registrant<targetType>::addVariable<decltype(reinterpret_cast<targetType*>(0)->varName)>(TOSTRING(varName), static_cast<int32_t>(reinterpret_cast<intptr_t>(&reinterpret_cast<targetType*>(0)->varName)))
#define ADDVARIABLE(varName) ADDVARIABLETO(type, varName)
#define ADDCOMPVARIABLETO(targetType, varName, comparator) registerType<decltype(reinterpret_cast<targetType*>(0)->varName)>();\
TechDemo::Util::Registrant<targetType>::addVariable<decltype(reinterpret_cast<targetType*>(0)->varName)>(TOSTRING(varName), static_cast<int32_t>(reinterpret_cast<intptr_t>(&reinterpret_cast<targetType*>(0)->varName)), comparator)
#define ADDCOMPVARIABLE(varName, comparator) ADDCOMPVARIABLETO(type, varName, comparator)
#define ADDINTERPVARIABLETO(targetType, varName, interpolator) registerType<decltype(reinterpret_cast<targetType*>(0)->varName)>();\
TechDemo::Util::Registrant<targetType>::addVariable<decltype(reinterpret_cast<targetType*>(0)->varName)>(TOSTRING(varName), static_cast<int32_t>(reinterpret_cast<intptr_t>(&reinterpret_cast<targetType*>(0)->varName)), interpolator)
#define ADDINTERPVARIABLE(varName, interpolator) ADDINTERPVARIABLETO(type, varName, interpolator)
#define ADDCOMPINTERPVARIABLETO(targetType, varName, comparator, interpolator) registerType<decltype(reinterpret_cast<targetType*>(0)->varName)>();\
TechDemo::Util::Registrant<targetType>::addVariable<decltype(reinterpret_cast<targetType*>(0)->varName)>(TOSTRING(varName), static_cast<int32_t>(reinterpret_cast<intptr_t>(&reinterpret_cast<targetType*>(0)->varName)), comparator, interpolator)
#define ADDCOMPINTERPVARIABLE(varName, comparator, interpolator) ADDCOMPINTERPVARIABLETO(type, varName, comparator, interpolator)
#define ADDCOMPARATORTO(targetType, varName, comparator) TechDemo::Util::Registrant<targetType>::setComparator(TOSTRING(varName), comparator)
#define ADDCOMPARATOR(varName, comparator) ADDCOMPARATORTO(type, varName, comparator)
#define ADDINTERPOLATORTO(targetType, varName, interpolator) TechDemo::Util::Registrant<targetType>::setInterpolator(TOSTRING(varName), interpolator)
#define ADDINTERPOLATOR(varName, interpolator) ADDINTERPOLATORTO(type, varName, interpolator)
#define ADDDIFFERENCEFUNCTO(targetType, varName, difference) TechDemo::Util::Registrant<targetType>::setDifference(TOSTRING(varName), difference)
#define ADDDIFFERENCEFUNC(varName, difference) ADDDIFFERENCEFUNCTO(type, varName, difference)
#define ADDACCUMULATORTO(targetType, varName, accumulator) TechDemo::Util::Registrant<targetType>::setAccumulator(TOSTRING(varName), accumulator)
#define ADDACCUMULATOR(varName, accumulator) ADDACCUMULATORTO(type, varName, accumulator)
#define ADDPREMODIFYCALLBACKTO(targetType, varName, callback) TechDemo::Util::Registrant<targetType>::setPreCallback(TOSTRING(varName), callback)
#define ADDPREMODIFYCALLBACK(varName, callback) ADDPREMODIFYCALLBACKTO(type, varName, callback)
#define ADDPOSTMODIFYCALLBACKTO(targetType, varName, callback) TechDemo::Util::Registrant<targetType>::setPostCallback(TOSTRING(varName), callback)
#define ADDPOSTMODIFYCALLBACK(varName, callback) ADDPOSTMODIFYCALLBACKTO(type, varName, callback)

namespace TechDemo
{
	namespace Util
	{
		template <typename T>
		struct Registrant;
		struct Serializable;

		class RTTI
		{
			template <typename T>
			friend struct Registrant;

			public:
				struct Type
				{
					uint32_t hash = 0;	// CRC32 hash for type
					size_t size = 0;	// Size of type in bits
					bool isSerializable = false;	// Inherits from Serializable class
					char* (*creator)() = nullptr;	// Creator function
					std::unordered_map<uint32_t, int32_t> bases;	// Base type hash -> offset
					std::unordered_map<std::string, std::tuple<uint32_t, int32_t, bool, bool (*)(IO::BitStream const&, IO::BitStream const&), void (*)(const char*, const char*, float, char*), void (*)(const char*, const char*, IO::BitStream&), void (*)(IO::BitStream const&, IO::BitStream const&, char*), void (*)(char*), void (*)(char*)>> variables;	// Var name -> (Var type hash, offset, is pointer, comparator function, interpolator function, difference function, accumulate function, pre-modify callback, post-modify callback)

					uint32_t getHash() const;

					size_t getSize() const;

					char* create() const;
					
					std::list<const Type*> getBases() const;

					bool hasBase(uint32_t baseHash) const;

					std::pair<bool, int32_t> getBaseOffset(uint32_t baseHash) const;

					bool hasVariable(const std::string& variable) const;

					std::tuple<bool, int32_t, const Type*, bool, bool(*)(IO::BitStream const&, IO::BitStream const&), void (*)(const char*, const char*, float, char*), void (*)(const char*, const char*, IO::BitStream&), void (*)(IO::BitStream const&, IO::BitStream const&, char*), void(*)(char*), void(*)(char*)> getVariable(const std::string& variable) const;

					std::tuple<bool, char*, const Type*, bool, bool(*)(IO::BitStream const&, IO::BitStream const&), void (*)(const char*, const char*, float, char*), void (*)(const char*, const char*, IO::BitStream&), void (*)(IO::BitStream const&, IO::BitStream const&, char*), void(*)(char*), void(*)(char*)> getVariable(char* data, const std::string& variable) const;

					std::unordered_map<std::string, std::tuple<char*, const Type*, bool, bool(*)(IO::BitStream const&, IO::BitStream const&), void (*)(const char*, const char*, float, char*), void (*)(const char*, const char*, IO::BitStream&), void (*)(IO::BitStream const&, IO::BitStream const&, char*), void(*)(char*), void(*)(char*)>> getVariables(char* data, bool onlyLeaves = false) const;

					std::unordered_set<std::string> getVariableNames(bool onlyLeaves = false) const;

					std::unordered_set<std::string> getVariableNames(char* data, bool onlyLeaves = false) const;
				};

				static const Type* getType(uint32_t typeHash);

			private:
				static std::unordered_map<uint32_t, Type>& getTypes();
		};

		template <typename T>
		struct Registrant
		{
			using type = T;

			Registrant()
			{
				registerType<T>();
				init();
			}

			void init();

			template <typename B>
			static void addBase()
			{
				static_assert(std::is_base_of_v<B, T> && !std::is_same_v<B, T>, "An invalid base class was provided to the RTTI registrant.");
				
				static const uint32_t key = CRC32::checksum(getQualifiedName<T>());
				static const uint32_t base = CRC32::checksum(getQualifiedName<B>());

				char* (*creator)() = nullptr;
				if constexpr (std::is_trivially_default_constructible_v<B>)
					creator = []() -> char* { return reinterpret_cast<char*>(new B()); };

				RTTI::getTypes().try_emplace(base, RTTI::Type{ base, bitSize<B>(), std::is_base_of_v<Serializable, B>, creator });
				RTTI::getTypes()[key].bases.try_emplace(base, static_cast<int32_t>(reinterpret_cast<intptr_t>(dynamic_cast<B*>(reinterpret_cast<T*>(0)))));
			}

			template <typename V>
			static void addVariable(const std::string& name, int32_t offset, bool (*comparator)(IO::BitStream const&, IO::BitStream const&) = IO::BitStream::equals)
			{
				using W = std::remove_pointer_t<V>;
				static constexpr bool isPointer = std::is_pointer_v<V>;
				static const uint32_t key = CRC32::checksum(getQualifiedName<T>());
				static const uint32_t var = CRC32::checksum(getQualifiedName<W>());

				RTTI::getTypes()[key].variables.try_emplace(name, std::make_tuple(var, offset, isPointer, comparator, nullptr, nullptr, nullptr, nullptr, nullptr));
			}

			template <typename V>
			static void addVariable(const std::string& name, int32_t offset, void (*interpolator)(const char*, const char*, float, char*))
			{
				using W = std::remove_pointer_t<V>;
				static constexpr bool isPointer = std::is_pointer_v<V>;
				static const uint32_t key = CRC32::checksum(getQualifiedName<T>());
				static const uint32_t var = CRC32::checksum(getQualifiedName<W>());

				RTTI::getTypes()[key].variables.try_emplace(name, std::make_tuple(var, offset, isPointer, IO::BitStream::equals, interpolator, nullptr, nullptr, nullptr, nullptr));
			}

			template <typename V>
			static void addVariable(const std::string& name, int32_t offset, bool (*comparator)(IO::BitStream const&, IO::BitStream const&), void (*interpolator)(const char*, const char*, float, char*))
			{
				using W = std::remove_pointer_t<V>;
				static constexpr bool isPointer = std::is_pointer_v<V>;
				static const uint32_t key = CRC32::checksum(getQualifiedName<T>());
				static const uint32_t var = CRC32::checksum(getQualifiedName<W>());

				RTTI::getTypes()[key].variables.try_emplace(name, std::make_tuple(var, offset, isPointer, comparator, interpolator, nullptr, nullptr, nullptr, nullptr));
			}

			static void setComparator(const std::string& name, bool (*comparator)(IO::BitStream const&, IO::BitStream const&))
			{
				static const uint32_t key = CRC32::checksum(getQualifiedName<T>());

				std::get<3>(RTTI::getTypes()[key].variables[name]) = comparator;
			}

			static void setInterpolator(const std::string& name, void (*interpolator)(const char*, const char*, float, char*))
			{
				static const uint32_t key = CRC32::checksum(getQualifiedName<T>());

				std::get<4>(RTTI::getTypes()[key].variables[name]) = interpolator;
			}

			static void setDifference(const std::string& name, void (*difference)(const char*, const char*, IO::BitStream&))
			{
				static const uint32_t key = CRC32::checksum(getQualifiedName<T>());

				std::get<5>(RTTI::getTypes()[key].variables[name]) = difference;
			}

			static void setAccumulator(const std::string& name, void (*accumulator)(IO::BitStream const&, IO::BitStream const&, char*))
			{
				static const uint32_t key = CRC32::checksum(getQualifiedName<T>());

				std::get<6>(RTTI::getTypes()[key].variables[name]) = accumulator;
			}

			static void setPreCallback(const std::string& name, void(*callback)(char*))
			{
				static const uint32_t key = CRC32::checksum(getQualifiedName<T>());

				std::get<7>(RTTI::getTypes()[key].variables[name]) = callback;
			}

			static void setPostCallback(const std::string& name, void(*callback)(char*))
			{
				static const uint32_t key = CRC32::checksum(getQualifiedName<T>());

				std::get<8>(RTTI::getTypes()[key].variables[name]) = callback;
			}

			template <typename R = T>
			static void registerType()
			{
				static const uint32_t key = CRC32::checksum(getQualifiedName<R>());
				char* (*creator)() = nullptr;
				if constexpr (std::is_trivially_default_constructible_v<R>)
					creator = []() -> char* { return reinterpret_cast<char*>(new R()); };
				RTTI::getTypes().try_emplace(key, RTTI::Type{ key, bitSize<R>(), std::is_base_of_v<Serializable, R>, creator });
			}
		};
	}
}