#pragma once
#include <iostream>
#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <queue>
#include <string>
#include <any>
#include <optional>
#include <functional>
#include <array>
#include <type_traits>
struct EventBase {};

template<typename T>
inline constexpr bool isEventType = std::is_base_of_v<EventBase, T>;

class Event {
	friend class EventQueue;
private:
	std::type_index Type;
	std::any Data;
	Event(std::type_index Type, std::any Data) :Type(std::move(Type)), Data(std::move(Data)) {}
public:
	template<typename T, typename = std::enable_if_t<isEventType<T>>>
	const T* getIf() const {
		if (std::type_index(typeid(T)) == Type)
			return std::any_cast<T>(&Data);
		else return nullptr;
	}
	template<typename T>
	bool is() const {
		return std::type_index(typeid(T)) == Type;
	}
};
class EventQueue {
	friend class Event;
private:
	std::queue<std::type_index> Order;

	class TypeStruct {
		friend class EventQueue;
	private:
		std::any/*store std::queue<T> in std::any*/ Data;
	public:
		TypeStruct() = default;
	};
	std::unordered_map<std::type_index, TypeStruct> Type;



	//类型管理

	class TypeOperationStruct {
		friend class EventQueue;
		class OperationStruct {
			friend class EventQueue;
			std::function<Event(std::any&)>extractHelper;
		public:
			OperationStruct() = default;
		};
	public:
		std::unordered_map<std::type_index, OperationStruct> Operation;
		template<typename T>
		void registerType() {
			std::type_index TypeIndex = std::type_index(typeid(T));
			auto iter = Operation.find(TypeIndex);
			if (iter == Operation.end()) {
				Operation[TypeIndex].extractHelper = [](std::any& Data)->Event {
					auto& DataQueue = std::any_cast<std::queue<T>&>(Data);
					Event event(std::type_index(typeid(T)), std::move(DataQueue.front()));
					DataQueue.pop();
					return event;
					};
			}
		}
	};
	inline static TypeOperationStruct TypeOperation;
	template<typename T>
	std::type_index getTypeIndexAutoCreate() {
		std::type_index TypeIndex = std::type_index(typeid(T));
		auto iter = Type.find(TypeIndex);
		if (iter == Type.end()) {
			Type.emplace(TypeIndex, TypeStruct{});
			Type[TypeIndex].Data = std::queue<T>{};
			TypeOperation.registerType<T>();
			return TypeIndex;
		}
		else return TypeIndex;
	}
	template<typename T>
	std::optional<std::type_index> getTypeIndex() const {
		std::type_index TypeIndex = std::type_index(typeid(T));
		auto iter = Type.find(TypeIndex);
		if (iter == Type.end()) {
			return std::nullopt;
		}
		else return TypeIndex;
	}
	template<typename T>
	static constexpr bool isDerivedType = std::is_base_of_v<EventBase, T>;



	//添加数据

	template<typename T, typename InsertDataFunc, typename = std::enable_if_t<isDerivedType<T>>>
	inline void insertHelper(InsertDataFunc&& insertDataFunc) {
		std::type_index TypeIndex = getTypeIndexAutoCreate<T>();
		std::queue<T>& DataQueue = std::any_cast<std::queue<T>&>(Type[TypeIndex].Data);
		insertDataFunc(DataQueue);
		Order.push(TypeIndex);
	}
public:
	template<typename U = void, typename V>
	void push(V&& value) {
		using T = std::conditional_t<std::is_same_v<U, void>, std::remove_cvref_t<V>, U>;
		return insertHelper<T>(
			[&](std::queue<T>& DataQueue) {
				DataQueue.push(std::forward<V>(value));
			}
		);
	}
	template<typename T, typename... Args>
	auto emplace(Args&&... args) {
		return insertHelper<T>(
			[&](std::queue<T>& DataQueue) {
				DataQueue.emplace(std::forward<Args>(args)...);
			}
		);
	}



	//访问数据

	const std::optional<Event> pollEvent() {
		if (Order.empty())return std::nullopt;
		std::type_index TypeIndex = Order.front();
		Order.pop();
		return TypeOperation.Operation[TypeIndex].extractHelper(Type[TypeIndex].Data);
	}



	//其他函数

	size_t size() const {
		return Order.size();
	}
	bool empty() const {
		return Order.size() == 0;
	}
	void clear() {
		while (!Order.empty()) {
			Order.pop();
		}
		this->Type.clear();
	}
};
