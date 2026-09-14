#pragma once
#include <typeinfo>
#include <typeindex>
#include <unordered_map>
#include <map>
#include <any>
#include <memory>
#include <memory_resource>
#include <list>
#include <vector>
#include <queue>
#include <stdexcept>
#include <type_traits>
#include <string>
#include <algorithm>
#include <iostream>
#include <optional>
#include <format>
#include <ranges>
#include <functional>
#include <utility>
using namespace std::string_literals;
/*
* Varian<T> map
* 使用std::pmr::list优化，遍历效率提升，插入效率略微下降
* 优点：无需提前声明类型，可解决某些场景下提前声明和类实现循环依赖的问题，无需手动管理顺序指针
*/
template<typename Base>
class VarianTmap {
public:
	VarianTmap() : pool(std::make_unique<std::pmr::unsynchronized_pool_resource>()) {}

	void copyHelper(const VarianTmap<Base>& other) {
		this->DataContainer.reserve(other.DataContainer.size());
		std::unordered_map<Base*, Base*> PointerMap{};
		for (const auto& TypeElem : other.DataContainer) {
			publicTypeOperation.Operation.at(TypeElem.first).DeepCopyHelper(*this->pool, this->DataContainer[TypeElem.first], TypeElem.second, this->DataFinder, other.DataFinder, PointerMap, true);
		}
		for (const auto& KeyElem : other.Key) {
			this->Key.emplace(KeyElem.first, PointerMap.at(KeyElem.second));
		}
		for (const auto& elem : other.Order) {
			Base* NewBasePointer = PointerMap.at(elem);
			this->Order.push_back(NewBasePointer);
			this->DataFinder.at(NewBasePointer).Order = std::prev(this->Order.end());
		}
	}

	VarianTmap(const VarianTmap<Base>& other) : pool(std::make_unique<std::pmr::unsynchronized_pool_resource>()) {
		copyHelper(other);
	}

	VarianTmap<Base>& operator=(const VarianTmap<Base>& other) {
		if (this != &other) {
			this->clear();
			copyHelper(other);
		}
		return *this;
	}

	void moveHelper(VarianTmap<Base>&& other) {
		this->pool = std::move(other.pool);
		this->Order = std::move(other.Order);
		this->Key = std::move(other.Key);
		this->DataFinder = std::move(other.DataFinder);
		this->DataContainer = std::move(other.DataContainer);
	}

	VarianTmap(VarianTmap<Base>&& other) noexcept {
		moveHelper(std::move(other));
	}

	VarianTmap<Base>& operator=(VarianTmap<Base>&& other) noexcept {
		if (this != &other) {
			this->clear();
			moveHelper(std::move(other));
		}
		return *this;
	}
private:
	//自动转换指针

	/*
	* 可被构造器接受的类型：
	- `Base*`
	- `Derived*`
	- `std::pmr::list<Base*>::const_iterator`
	- `std::pmr::list<Derived>::iterator`

	* 自动将上述类型转换为`Base*`以供函数统一处理，用于函数参数
	*/
	template<typename T>
	static constexpr bool isDerivedType = std::is_base_of_v<Base, T>;

	std::unique_ptr<std::pmr::unsynchronized_pool_resource> pool;

	class auto_cast_pointer {
		friend class VarianTmap;
	private:
		Base* pointer = nullptr;//数据指针，即数据池中的地址
		std::pmr::list<Base*>::const_iterator orderIter;
		//特别地，把std::pmr::list<Base*>::const_iterator单独列出来，是因为insert时可能传入合法的x.order().end()，而x.order().end()无法直接解引用，需要后续判断使用场景后再决定
	public:
		auto_cast_pointer(Base* pointer) :pointer(pointer) {}

		template<
			typename T,
			typename = std::enable_if_t<
			VarianTmap<Base>::template isDerivedType<std::remove_const_t<T>>
			>
		>
		auto_cast_pointer(T* pointer) :pointer(static_cast<Base*>(const_cast<std::remove_const_t<T>*>(pointer))) {}

		auto_cast_pointer(std::pmr::list<Base*>::const_iterator pointer) :orderIter(pointer) {}

		template<
			typename U,
			typename = std::void_t<typename std::iterator_traits<U>::value_type>,
			typename T = typename std::iterator_traits<U>::value_type,
			typename = std::enable_if_t<
			!std::is_same_v<T, Base*>&&
			std::is_same_v<typename std::pmr::list<T>::iterator, U>&&
			VarianTmap<Base>::template isDerivedType<T>
			>
		>
		auto_cast_pointer(U pointer) :pointer(static_cast<Base*>(std::addressof(*pointer))) {}
	};



	std::pmr::list<Base*> Order;
	std::unordered_map<std::string, Base*> Key;
	class DataPointerStruct {
		friend class VarianTmap;
	private:
		std::type_index TypeIndex=std::type_index(typeid(void));
		std::string Key;
		std::any/*store std::pmr::list<T>::iterator in std::any*/ Data;
		std::pmr::list<Base*>::iterator Order;
	public:
		DataPointerStruct(std::type_index TypeIndex, std::string Key, std::any Data, std::pmr::list<Base*>::iterator Order) :
			TypeIndex(TypeIndex), Key(std::move(Key)), Data(std::move(Data)), Order(std::move(Order)) {
		};

		DataPointerStruct(std::type_index TypeIndex, std::string Key, std::any Data) :
			TypeIndex(TypeIndex), Key(std::move(Key)), Data(std::move(Data)) {
		};
	};

	std::unordered_map<Base*, DataPointerStruct>DataFinder;

	std::unordered_map<std::type_index,std::any/*store std::shared_ptr<std::pmr::list<T>> in std::any*/>DataContainer;



	//类型管理

	class PublicTypeOperationStruct {
		friend class VarianTmap;
		
		class OperationStruct {
			friend class VarianTmap;
			using DestructorFunc = void(*)(std::any&, std::any&);
			using DeepCopyHelperFunc = void(*)(std::pmr::unsynchronized_pool_resource&, std::any&, const std::any&, std::unordered_map<Base*, DataPointerStruct>&, const std::unordered_map<Base*, DataPointerStruct>&, std::unordered_map<Base*, Base*>&, bool);
			using MoveExtractFunc = void(*)(std::any&, std::any&, std::any&, std::unordered_map<Base*, DataPointerStruct>&, std::pmr::list<Base*>&, const std::string&, Base*&);
			using CreateEmptyFunc = void(*)(std::pmr::unsynchronized_pool_resource&, std::any&);

			/*
			* 学习记录：原实现使用 std::function 是合法且合理的。std::function 能统一保存
			* 普通函数、lambda 和函数对象等 callable，并提供 callable type erasure。
			* 这里实际注册的 callable 全部是无捕获 lambda，而无捕获 lambda 可以直接转换为
			* 普通函数指针；因此改用函数指针只是减少一层不必要的抽象，并非原实现有误。
			*
			* 原实现：
			* std::function<void(std::any&, std::any&)> Destructor;
			* std::function<void(std::pmr::unsynchronized_pool_resource&, std::any&, const std::any&, std::unordered_map<Base*, DataPointerStruct>&, const std::unordered_map<Base*, DataPointerStruct>&, std::unordered_map<Base*, Base*>&, bool)> DeepCopyHelper;
			* std::function<void(std::any&, std::any&, std::any&, std::unordered_map<Base*, DataPointerStruct>&, std::pmr::list<Base*>&, const std::string&, Base*&)> MoveExtract;
			* std::function<void(std::pmr::unsynchronized_pool_resource&, std::any&)> CreateEmpty;
			*/
			DestructorFunc Destructor = nullptr;
			DeepCopyHelperFunc DeepCopyHelper = nullptr;
			MoveExtractFunc MoveExtract = nullptr;
			CreateEmptyFunc CreateEmpty = nullptr;
		public:
			OperationStruct() = default;
		};

		template<typename T>
		static void destructorImpl(std::any& Data, std::any& Iter) {
			auto& DataList = *std::any_cast<std::shared_ptr<std::pmr::list<T>>&>(Data);
			DataList.erase(std::any_cast<typename std::pmr::list<T>::iterator&>(Iter));
		}

		template<typename T>
		static void createEmptyImpl(std::pmr::unsynchronized_pool_resource& pool, std::any& Data) {
			Data = std::make_shared<std::pmr::list<T>>(
				std::pmr::polymorphic_allocator<T>(static_cast<std::pmr::memory_resource*>(&pool))
			);
		}

		template<typename T>
		static void deepCopyHelperImpl(std::pmr::unsynchronized_pool_resource& pool, std::any& DstData, const std::any& SrcData, std::unordered_map<Base*, DataPointerStruct>& DstFinder, const std::unordered_map<Base*, DataPointerStruct>& SrcFinder, std::unordered_map<Base*, Base*>& PointerMap, bool CreateNew) {
			const auto& SrcList = *std::any_cast<const std::shared_ptr<std::pmr::list<T>>&>(SrcData);
			if (CreateNew)
				DstData = std::make_shared<std::pmr::list<T>>(
					std::pmr::polymorphic_allocator<T>(static_cast<std::pmr::memory_resource*>(&pool))
				);
			auto& DstList = *std::any_cast<std::shared_ptr<std::pmr::list<T>>&>(DstData);
			for (auto& elem : SrcList) {
				DstList.push_back(elem);
				typename std::pmr::list<T>::iterator DstIter = std::prev(DstList.end());
				Base* SrcBasePointer = const_cast<T*>(std::addressof(elem));
				Base* DstBasePointer = static_cast<Base*>(std::addressof(*DstIter));
				const DataPointerStruct& SrcDataPointer = SrcFinder.at(SrcBasePointer);
				DstFinder.emplace(DstBasePointer, DataPointerStruct(std::type_index(typeid(T)), SrcDataPointer.Key, DstIter));
				PointerMap.emplace(SrcBasePointer, DstBasePointer);
			}
		}

		template<typename T>
		static void moveExtractImpl(std::any& SrcData, std::any& SrcIter, std::any& DstData, std::unordered_map<Base*, DataPointerStruct>& DstFinder, std::pmr::list<Base*>& DstOrder, const std::string& Key, Base*& OutNewBase) {
			auto& SrcList = *std::any_cast<std::shared_ptr<std::pmr::list<T>>&>(SrcData);
			auto& SrcDataIter = std::any_cast<typename std::pmr::list<T>::iterator&>(SrcIter);
			auto& DstList = *std::any_cast<std::shared_ptr<std::pmr::list<T>>&>(DstData);
			DstList.push_back(std::move(*SrcDataIter));
			SrcList.erase(SrcDataIter);
			typename std::pmr::list<T>::iterator DstIter = std::prev(DstList.end());
			T* DstTyped = std::addressof(*DstIter);
			Base* DstBase = static_cast<Base*>(DstTyped);
			DstOrder.push_back(DstBase);
			typename std::pmr::list<Base*>::iterator DstOrderIter = std::prev(DstOrder.end());
			DstFinder.emplace(DstBase, DataPointerStruct(std::type_index(typeid(T)), Key, DstIter, DstOrderIter));
			OutNewBase = DstBase;
		}
	public:
		std::unordered_map<std::type_index,OperationStruct>Operation;

		template<typename T>
		void registerType() {
			std::type_index TypeIndex = std::type_index(typeid(T));
			auto iter = Operation.find(TypeIndex);
			if (iter == Operation.end()) {
				auto& op = Operation[TypeIndex];
				op.Destructor = &destructorImpl<T>;
				op.CreateEmpty = &createEmptyImpl<T>;
				op.DeepCopyHelper = &deepCopyHelperImpl<T>;
				op.MoveExtract = &moveExtractImpl<T>;
			}
		}
	};

	inline static PublicTypeOperationStruct publicTypeOperation;

	template<typename T>
	void ensureTypeRegistered() {
		static_assert(VarianTmap<Base>::template isDerivedType<T>);
		std::type_index TypeIndex = std::type_index(typeid(T));
		auto iter = DataContainer.find(TypeIndex);
		if (iter == DataContainer.end()) {
			publicTypeOperation.template registerType<T>();
			DataContainer[TypeIndex] = std::make_shared<std::pmr::list<T>>(
				std::pmr::polymorphic_allocator<T>(static_cast<std::pmr::memory_resource*>(pool.get()))
			);
		}
	}

	void ensureTypeRegistered(const std::type_index& TypeIndex) {
		if (DataContainer.find(TypeIndex) == DataContainer.end()) {
			std::any NewData;
			publicTypeOperation.Operation.at(TypeIndex).CreateEmpty(*pool, NewData);
			DataContainer.emplace(TypeIndex, std::move(NewData));
		}
	}

public:

	template<typename T>
	bool isTypeRegistered() const {
		std::type_index TypeIndex = std::type_index(typeid(T));
		return DataContainer.find(TypeIndex) != DataContainer.end();
	}

	bool isTypeRegistered(const std::type_index& TypeIndex) const {
		return DataContainer.find(TypeIndex) != DataContainer.end();
	}

	//插入、添加数据

private:
	template<typename Rollback>
	class RollbackGuard {
		Rollback rollback;
		bool active = true;
	public:
		explicit RollbackGuard(Rollback&& rollback) : rollback(std::move(rollback)) {}
		RollbackGuard(const RollbackGuard&) = delete;
		RollbackGuard& operator=(const RollbackGuard&) = delete;

		~RollbackGuard() noexcept {
			if (active)
				rollback();
		}

		void release() noexcept {
			active = false;
		}
	};

	template<typename Rollback>
	static auto makeRollbackGuard(Rollback&& rollback) {
		return RollbackGuard<std::decay_t<Rollback>>(std::forward<Rollback>(rollback));
	}

	void checkInsertable(auto_cast_pointer& Where) {
		if (Where.pointer != nullptr) {
			if (!DataFinder.count(Where.pointer)) {
				throw std::runtime_error("[VarianTmap::insert] Pointer not found.\n");
			}
		}
		else {
			if (Where.orderIter != Order.end()) {
				Where.pointer = *Where.orderIter;
				if (!DataFinder.count(Where.pointer)) {
					throw std::runtime_error("[VarianTmap::insert] Pointer not found.\n");
				}
			}
			//else pointer=nullptr; 若为Order.end()，pointer仍保持nullptr
		}
	}

	template<typename T, typename InsertDataFunc, typename InsertOrderFunc, typename = std::enable_if_t<isDerivedType<T>>>
	inline T* insertHelper(const std::string& key, InsertDataFunc&& insertDataFunc, InsertOrderFunc&& insertOrderFunc) {
		ensureTypeRegistered<T>();
		std::type_index TypeIndex = std::type_index(typeid(T));
		if (Key.count(key)) {
			throw std::runtime_error("[VarianTmap::insert] Key already exists.\n  Key: "s + key + "\n");
		}
		auto& list_ptr = std::any_cast<std::shared_ptr<std::pmr::list<T>>&>(DataContainer.at(TypeIndex));
		std::pmr::list<T>& DataList = *list_ptr;
		const typename std::pmr::list<T>::iterator DataIter = insertDataFunc(DataList);
		auto DataRollback = makeRollbackGuard([&]() noexcept { DataList.erase(DataIter); });
		T* TypedPointer = std::addressof(*DataIter);
		Base* BasePointer = static_cast<Base*>(TypedPointer);
		typename std::pmr::list<Base*>::iterator OrderPointer = insertOrderFunc(BasePointer);
		auto OrderRollback = makeRollbackGuard([&]() noexcept { Order.erase(OrderPointer); });
		DataFinder.emplace(BasePointer, DataPointerStruct(TypeIndex, key, DataIter, OrderPointer));
		auto FinderRollback = makeRollbackGuard([&]() noexcept { DataFinder.erase(BasePointer); });
		if (key != "")
			Key.emplace(key, BasePointer);
		FinderRollback.release();
		OrderRollback.release();
		DataRollback.release();
		return TypedPointer;
	}

	void mergeHelper(const VarianTmap<Base>& other, std::unordered_map<Base*, Base*>& PointerMap) {
		for (auto& KeyElem : other.Key) {
			if (this->Key.count(KeyElem.first)) {
				throw std::runtime_error("[VarianTmap::merge] Key already exists.\n  Key: "s + KeyElem.first + "\n");
			}
		}
		for (const auto& TypeElem : other.DataContainer) {
			const std::type_index& TypeIndex = TypeElem.first;
			bool NeedCreate = !isTypeRegistered(TypeIndex);
			publicTypeOperation.Operation.at(TypeIndex).DeepCopyHelper(*this->pool, this->DataContainer[TypeIndex], TypeElem.second, this->DataFinder, other.DataFinder, PointerMap, NeedCreate);
		}
		for (auto& KeyElem : other.Key) {
			this->Key.emplace(KeyElem.first, PointerMap.at(KeyElem.second));
		}
	}
public:

	template<typename U = void, typename V>
	auto push_back(const std::string& key, V&& value) {
		using T = std::conditional_t<std::is_same_v<U, void>, std::remove_cvref_t<V>, U>;
		return insertHelper<T>(
			key,
			[&](std::pmr::list<T>& DataList)->typename std::pmr::list<T>::iterator {
				DataList.push_back(std::forward<V>(value));
				return std::prev(DataList.end());
			},
			[&](Base* BasePointer)->typename std::pmr::list<Base*>::iterator {
				Order.push_back(BasePointer);
				return std::prev(Order.end());
			}
		);
	}

	template<typename U = void, typename V>
	auto push_back(V&& value) {
		return push_back<U>("", std::forward<V>(value));
	}

	template<typename U = void, typename V>
	auto insert(auto_cast_pointer Where, const std::string& key, V&& value) {
		using T = std::conditional_t<std::is_same_v<U, void>, std::remove_cvref_t<V>, U>;
		return insertHelper<T>(
			key,
			[&](std::pmr::list<T>& DataList)->typename std::pmr::list<T>::iterator {
				DataList.push_back(std::forward<V>(value));
				return std::prev(DataList.end());
			},
			[&](Base* BasePointer)->typename std::pmr::list<Base*>::iterator {
				checkInsertable(Where);
				if (Where.pointer == nullptr) {
					Order.push_back(BasePointer);
					return std::prev(Order.end());
				}
				else {
					DataPointerStruct& DataPointer = DataFinder.at(Where.pointer);
					typename std::pmr::list<Base*>::iterator& WhereOrderIter = DataPointer.Order;
					return Order.insert(WhereOrderIter, BasePointer);
				}
			}
		);
	}

	template<typename U = void, typename V>
	auto insert(auto_cast_pointer Where, V&& value) {
		return insert<U>(Where, "", std::forward<V>(value));
	}

	template<typename U = void, typename V>
	auto push_front(const std::string& key, V&& value) {
		using T = std::conditional_t<std::is_same_v<U, void>, std::remove_cvref_t<V>, U>;
		return insertHelper<T>(
			key,
			[&](std::pmr::list<T>& DataList)->typename std::pmr::list<T>::iterator {
				//数据池中的数据无序，这里直接向数据池中push_back即可，顺序仅靠Order维护，不是写错
				DataList.push_back(std::forward<V>(value));
				return std::prev(DataList.end());
			},
			[&](Base* BasePointer)->typename std::pmr::list<Base*>::iterator {
				Order.push_front(BasePointer);
				return Order.begin();
			}
		);
	}

	template<typename U = void, typename V>
	auto push_front(V&& value) {
		return push_front<U>("", std::forward<V>(value));
	}


	template<typename T, typename... Args>
	auto emplace(auto_cast_pointer Where, const std::string& key="", Args&&... args) {
		checkInsertable(Where);
		return insertHelper<T>(
			key,
			[&](std::pmr::list<T>& DataList)->typename std::pmr::list<T>::iterator {
				DataList.emplace_back(std::forward<Args>(args)...);
				return std::prev(DataList.end());
			},
			[&](Base* BasePointer)->typename std::pmr::list<Base*>::iterator {
				if (Where.pointer == nullptr) {
					Order.push_back(BasePointer);
					return std::prev(Order.end());
				}
				else {
					DataPointerStruct& DataPointer = DataFinder.at(Where.pointer);
					typename std::pmr::list<Base*>::iterator& WhereOrderIter = DataPointer.Order;
					return Order.insert(WhereOrderIter, BasePointer);
				}
			}
		);
	}

	void merge(const VarianTmap<Base>& other) {
		if (this == &other) {
			throw std::runtime_error("[VarianTmap::merge] Cannot merge a VarianTmap with itself.\n");
		}
		std::unordered_map<Base*, Base*> PointerMap{};
		mergeHelper(other, PointerMap);
		for (auto& elem : other.Order) {
			Base* NewBasePointer = PointerMap.at(elem);
			this->Order.push_back(NewBasePointer);
			this->DataFinder.at(NewBasePointer).Order = std::prev(Order.end());
		}
	}

	void merge(auto_cast_pointer Where, const VarianTmap<Base>& other) {
		if (this == &other) {
			throw std::runtime_error("[VarianTmap::merge] Cannot merge a VarianTmap with itself.\n");
		}
		checkInsertable(Where);
		std::unordered_map<Base*, Base*> PointerMap{};
		mergeHelper(other, PointerMap);
		typename std::pmr::list<Base*>::iterator OrderIter;
		if (Where.pointer == nullptr) {
			OrderIter = Order.end();
		}
		else {
			DataPointerStruct& DataPointer = DataFinder.at(Where.pointer);
			OrderIter = DataPointer.Order;
		}
		for (auto& elem : std::views::reverse(other.Order)) {
			Base* NewBasePointer = PointerMap.at(elem);
			OrderIter = this->Order.insert(OrderIter, NewBasePointer);
			this->DataFinder.at(NewBasePointer).Order = OrderIter;
		}
	}



	//遍历管理

	const std::pmr::list<Base*>& order() const {
		return Order;
	}

	const std::pmr::list<Base*>& iterate() const {
		return Order;
	}

	const auto riterate() const {
		return std::views::reverse(Order);
	}

	template<
		typename Compare,
		typename = std::enable_if_t<
			std::is_invocable_r_v<bool, Compare&, Base*, Base*>
		>
	>
	void sort(Compare&& comp) {
		// std::list::sort 只重排节点，不会使 DataFinder 保存的 Order iterator 失效。
		Order.sort(std::forward<Compare>(comp));
	}

	void reverse() noexcept {
		// std::list::reverse 只重排节点，不会使 DataFinder 保存的 Order iterator 失效。
		Order.reverse();
	}

	/*
	* iterate<T>() 是有意保留的 O(1) 低层接口：允许遍历并修改已有元素内容。
	* 不要通过返回的 list 直接调用 push_back、push_front、insert、erase、clear、splice
	* 等改变容器结构的操作，否则会绕过 VarianTmap 的维护逻辑，导致 DataContainer、
	* DataFinder、Order 和 Key 失去同步。这是该接口明确接受的 trade-off。
	*/
	template<typename T>
	std::pmr::list<T>& iterate() {
		ensureTypeRegistered<T>();
		std::type_index TypeIndex = std::type_index(typeid(T));
		return *std::any_cast<std::shared_ptr<std::pmr::list<T>>&>(DataContainer.at(TypeIndex));
	}

	template<typename T>
	const std::pmr::list<T>& iterate() const {
		if (!isTypeRegistered<T>()) {
			throw std::runtime_error("[VarianTmap::iterate(const)] Type not found.\n  Type: "s + typeid(T).name() + "\n");
		}
		std::type_index TypeIndex = std::type_index(typeid(T));
		return *std::any_cast<std::shared_ptr<std::pmr::list<T>>&>(DataContainer.at(TypeIndex));
	}

	typename std::pmr::list<Base*>::const_iterator begin() const {
		return Order.begin();
	}

	typename std::pmr::list<Base*>::const_iterator end() const {
		return Order.end();
	}



	//查找数据
private:
	void getRealPointer(auto_cast_pointer& Pointer) const {
		if (Pointer.pointer == nullptr) {
			if (Pointer.orderIter == Order.end())return;
			Pointer.pointer = *Pointer.orderIter;
		}
	}
public:
	Base* find(const std::string& key) const {
		auto KeyFindIter = Key.find(key);
		if (KeyFindIter == Key.end())
			return nullptr;
		else return static_cast<Base*>(KeyFindIter->second);
	}

	template<typename T>
	T* find(const std::string& key) const {
		if (!isTypeRegistered<T>()) {
			return nullptr;
		}
		std::type_index TypeIndex = std::type_index(typeid(T));
		auto KeyFindIter = Key.find(key);
		if (KeyFindIter == Key.end())
			return nullptr;
		auto DataIter = DataFinder.find(KeyFindIter->second);
		if (DataIter == DataFinder.end())
			return nullptr;
		if (DataIter->second.TypeIndex != TypeIndex)
			return nullptr;
		return static_cast<T*>(KeyFindIter->second);
	}

	Base* find(auto_cast_pointer Pointer) const {
		getRealPointer(Pointer);
		if (Pointer.pointer == nullptr) return nullptr;
		return DataFinder.count(Pointer.pointer) ? Pointer.pointer : nullptr;
	}

	template<typename T>
	T* find(auto_cast_pointer Pointer) const {
		if (!isTypeRegistered<T>()) {
			return nullptr;
		}
		std::type_index TypeIndex = std::type_index(typeid(T));
		getRealPointer(Pointer);
		if (Pointer.pointer == nullptr) return nullptr;
		auto DataIter = DataFinder.find(Pointer.pointer);
		if (DataIter == DataFinder.end())
			return nullptr;
		if (DataIter->second.TypeIndex != TypeIndex)
			return nullptr;
		return static_cast<T*>(Pointer.pointer);
	}

	std::pmr::list<Base*>::const_iterator find_order(const std::string& key) const {
		auto KeyFindIter = Key.find(key);
		if (KeyFindIter == Key.end())
			return Order.end();
		else {
			auto DataIter = DataFinder.find(KeyFindIter->second);
			return DataIter->second.Order;
		}
	}

	std::pmr::list<Base*>::const_iterator find_order(auto_cast_pointer Pointer) const {
		getRealPointer(Pointer);
		if (Pointer.pointer == nullptr) return Order.end();
		auto DataIter = DataFinder.find(Pointer.pointer);
		if (DataIter == DataFinder.end()) {
			return Order.end();
		}
		else {
			return DataIter->second.Order;
		}
	}

	std::string find_key(auto_cast_pointer Pointer) const {
		getRealPointer(Pointer);
		if (Pointer.pointer == nullptr) return "";
		auto DataIter = DataFinder.find(Pointer.pointer);
		if (DataIter == DataFinder.end())
			return "";
		else {
			const DataPointerStruct& DataPointer = DataIter->second;
			return DataPointer.Key;
		}
	}

	std::string find_key(const std::string& key) const {
		auto KeyFindIter = Key.find(key);
		if (KeyFindIter == Key.end())
			return "";
		else {
			return KeyFindIter->first;
		}
	}

	std::type_index find_type_index(auto_cast_pointer Pointer) const {
		getRealPointer(Pointer);
		if (Pointer.pointer == nullptr) return std::type_index(typeid(void));
		auto DataIter = DataFinder.find(Pointer.pointer);
		if (DataIter == DataFinder.end()) {
			return std::type_index(typeid(void));
		}
		else {
			const DataPointerStruct& DataPointer = DataIter->second;
			return DataPointer.TypeIndex;
		}
	}



	//访问数据

	template<typename T>
	const T& at(const std::string& key) const {
		T* pointer = find<T>(key);
		if (pointer == nullptr) {
			throw std::runtime_error("[VarianTmap::at] Data not found.\n  Key: "s + key + "\n");
		}
		else return *pointer;
	}

	template<typename T>
	T& at(const std::string& key) {
		return const_cast<T&>(std::as_const(*this).template at<T>(key));
	}

	const Base* operator[](const std::string& key) const {
		Base* pointer = find(key);
		if (pointer == nullptr) {
			throw std::runtime_error("[VarianTmap::operator[]] Data not found.\n  Key: "s + key + "\n");
		}
		return pointer;
	}

	Base* operator[](std::string key) {
		return const_cast<Base*>(std::as_const(*this)[key]);
	}

	template<typename T>
	T& get(const std::string& key) {
		T* pointer = find<T>(key);
		if (pointer == nullptr)
			return *push_back<T>(key, T{});
		else return *pointer;
	}
	
	template<typename T>
	Base* operator()(const std::string& key,const T& defaultValue) {
		T* pointer = find<T>(key);
		if (pointer == nullptr)
			return push_back<T>(key, defaultValue);
		else return pointer;
	}



	//重命名

	void rename(const std::string& key, const std::string& NewKey) {
		auto KeyFindIter = Key.find(key);
		if (KeyFindIter == Key.end()) {
			throw std::runtime_error("[VarianTmap::rename] Data not found.\n  Key: "s + key + "\n  New Key: " + NewKey + "\n");
		}
		if (Key.count(NewKey)) {
			throw std::runtime_error("[VarianTmap::rename] New key already exists.\n  Key: "s + key + "\n  New Key: " + NewKey + "\n");
		}
		Base* BasePointer = KeyFindIter->second;
		DataPointerStruct& DataPointer = DataFinder.at(BasePointer);
		DataPointer.Key = NewKey;
		auto node = Key.extract(KeyFindIter);
		node.key() = NewKey;
		Key.insert(std::move(node));
	}

	void rename(auto_cast_pointer Pointer, const std::string& NewKey) {
		getRealPointer(Pointer);
		if (Pointer.pointer == nullptr) {
			throw std::runtime_error("[VarianTmap::rename] Data not found.\n  Type: Unknown\n  Pointer: "s + std::format("0x{:x}", reinterpret_cast<uintptr_t>(Pointer.pointer)) + "\n  New Key: " + NewKey + "\n");
		}
		auto iter = DataFinder.find(Pointer.pointer);
		if (iter == DataFinder.end()) {
			throw std::runtime_error("[VarianTmap::rename] Data not found.\n  Type: Unknown\n  Pointer: "s + std::format("0x{:x}", reinterpret_cast<uintptr_t>(Pointer.pointer)) + "\n  New Key: " + NewKey + "\n");
		}
		DataPointerStruct& DataPointer = iter->second;
		if (Key.count(NewKey)) {
			throw std::runtime_error("[VarianTmap::rename] New key already exists.\n  Type: Unknown\n  Pointer: "s + std::format("0x{:x}", reinterpret_cast<uintptr_t>(Pointer.pointer)) + "\n  New Key: " + NewKey + "\n");
		}
		if (DataPointer.Key != "") {
			auto node = Key.extract(DataPointer.Key);
			node.key() = NewKey;
			Key.insert(std::move(node));
		}
		else {
			Key.emplace(NewKey, Pointer.pointer);
		}
		DataPointer.Key = NewKey;
	}

	//删除数据

	void erase(const std::string& key) {
		auto KeyFindIter = Key.find(key);
		if (KeyFindIter == Key.end()) {
			throw std::runtime_error("[VarianTmap::erase] Data not found.\n  Key: "s + key + "\n");
		}
		Base* BasePointer = KeyFindIter->second;
		DataPointerStruct& DataPointer = DataFinder.at(BasePointer);
		std::type_index TypeIndex = DataPointer.TypeIndex;
		Order.erase(DataPointer.Order);
		Key.erase(KeyFindIter);
		publicTypeOperation.Operation.at(TypeIndex).Destructor(DataContainer.at(TypeIndex), DataPointer.Data);
		DataFinder.erase(BasePointer);
	}

	void erase(auto_cast_pointer Pointer) {
		getRealPointer(Pointer);
		if (Pointer.pointer == nullptr) {
			throw std::runtime_error("[VarianTmap::erase] Data not found.\n  Type: Unknown\n  Pointer: "s + std::format("0x{:x}", reinterpret_cast<uintptr_t>(Pointer.pointer)) + "\n");
		}
		auto iter = DataFinder.find(Pointer.pointer);
		if (iter == DataFinder.end()) {
			throw std::runtime_error("[VarianTmap::erase] Data not found.\n  Type: Unknown\n  Pointer: "s + std::format("0x{:x}", reinterpret_cast<uintptr_t>(Pointer.pointer)) + "\n");
		}
		DataPointerStruct& DataPointer = iter->second;
		std::type_index TypeIndex = DataPointer.TypeIndex;
		Base* BasePointer = Pointer.pointer;
		Order.erase(DataPointer.Order);
		if (DataPointer.Key != "") {
			Key.erase(DataPointer.Key);
		}
		publicTypeOperation.Operation.at(TypeIndex).Destructor(DataContainer.at(TypeIndex), DataPointer.Data);
		DataFinder.erase(iter);
	}



	//抽取数据

	VarianTmap<Base> extract(const std::string& key, size_t count) {
		VarianTmap<Base> result;
		auto KeyFindIter = Key.find(key);
		if (KeyFindIter == Key.end()) {
			throw std::runtime_error("[VarianTmap::extract] Data not found.\n  Key: "s + key + "\n");
		}
		Base* StartPointer = KeyFindIter->second;
		DataPointerStruct& StartData = DataFinder.at(StartPointer);
		auto OrderIter = StartData.Order;
		for (size_t i = 0; i < count && OrderIter != Order.end(); ++i) {
			Base* CurrentPointer = *OrderIter;
			DataPointerStruct& CurrentData = DataFinder.at(CurrentPointer);
			std::type_index TypeIndex = CurrentData.TypeIndex;
			result.ensureTypeRegistered(TypeIndex);
			// 在 result 中分配空间
			Base* NewBase = nullptr;
			publicTypeOperation.Operation.at(TypeIndex).MoveExtract(
				DataContainer.at(TypeIndex), CurrentData.Data,
				result.DataContainer.at(TypeIndex),
				result.DataFinder, result.Order,
				CurrentData.Key, NewBase
			);
			if (CurrentData.Key != "") {
				result.Key.emplace(CurrentData.Key, NewBase);
				Key.erase(CurrentData.Key);
			}
			OrderIter = Order.erase(OrderIter);
			DataFinder.erase(CurrentPointer);
		}
		return result;
	}

	VarianTmap<Base> extract(auto_cast_pointer Pointer, size_t count) {
		VarianTmap<Base> result;
		getRealPointer(Pointer);
		if (Pointer.pointer == nullptr) {
			throw std::runtime_error("[VarianTmap::extract] Data not found.\n  Type: Unknown\n  Pointer: "s + std::format("0x{:x}", reinterpret_cast<uintptr_t>(Pointer.pointer)) + "\n");
		}
		auto iter = DataFinder.find(Pointer.pointer);
		if (iter == DataFinder.end()) {
			throw std::runtime_error("[VarianTmap::extract] Data not found.\n  Type: Unknown\n  Pointer: "s + std::format("0x{:x}", reinterpret_cast<uintptr_t>(Pointer.pointer)) + "\n");
		}
		{
			auto OrderIter = iter->second.Order;
			for (size_t i = 0; i < count && OrderIter != Order.end(); ++i) {
				Base* CurrentPointer = *OrderIter;
				DataPointerStruct& CurrentData = DataFinder.at(CurrentPointer);
				std::type_index TypeIndex = CurrentData.TypeIndex;
				result.ensureTypeRegistered(TypeIndex);
				Base* NewBase = nullptr;
				publicTypeOperation.Operation.at(TypeIndex).MoveExtract(
					DataContainer.at(TypeIndex), CurrentData.Data,
					result.DataContainer.at(TypeIndex),
					result.DataFinder, result.Order,
					CurrentData.Key, NewBase
				);
				if (CurrentData.Key != "") {
					result.Key.emplace(CurrentData.Key, NewBase);
					Key.erase(CurrentData.Key);
				}
				OrderIter = Order.erase(OrderIter);
				DataFinder.erase(CurrentPointer);
			}
		}
		return result;
	}
/*
	VarianTmap<Base> extract(auto_cast_pointer From, auto_cast_pointer To) {
		//return extract(Pointer);
	}
		*/
	//其他函数

	size_t size() const {
		return Order.size();
	}

	bool empty() const {
		return Order.size() == 0;
	}

	void clear() {
		this->Order.clear();
		this->Key.clear();
		this->DataFinder.clear();
		this->DataContainer.clear();
	}
	
	template<typename T>
	void clear() {
		std::type_index TypeIndex = std::type_index(typeid(T));
		if (!isTypeRegistered<T>()) {
			return;
		}
		auto& DataList = *std::any_cast<std::shared_ptr<std::pmr::list<T>>&>(DataContainer.at(TypeIndex));
		for (auto& elem : DataList) {
			auto iter = DataFinder.find(&elem);
			Order.erase(iter->second.Order);
			Key.erase(iter->second.Key);
			DataFinder.erase(iter);
		}
		DataList.clear();
	}
};
