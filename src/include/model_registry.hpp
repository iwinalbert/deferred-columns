#pragma once

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <pthread.h>

namespace duckdb {

class SharedMutex {
	pthread_rwlock_t rwlock;

public:
	SharedMutex() {
		pthread_rwlock_init(&rwlock, NULL);
	}
	~SharedMutex() {
		pthread_rwlock_destroy(&rwlock);
	}
	void lock_shared() {
		pthread_rwlock_rdlock(&rwlock);
	}
	void unlock_shared() {
		pthread_rwlock_unlock(&rwlock);
	}
	void lock() {
		pthread_rwlock_wrlock(&rwlock);
	}
	void unlock() {
		pthread_rwlock_unlock(&rwlock);
	}
};

template <typename Mutex>
class SharedLockGuard {
	Mutex &m;

public:
	explicit SharedLockGuard(Mutex &m_) : m(m_) {
		m.lock_shared();
	}
	~SharedLockGuard() {
		m.unlock_shared();
	}
};

template <typename Mutex>
class UniqueLockGuard {
	Mutex &m;

public:
	explicit UniqueLockGuard(Mutex &m_) : m(m_) {
		m.lock();
	}
	~UniqueLockGuard() {
		m.unlock();
	}
};

struct ModelMetadata {
	std::string name;
	int version;
	double rmse;
	bool is_active;
	std::string file_path;
};

class ModelRegistry {
public:
	static ModelRegistry &GetInstance();

	void RegisterModel(const std::string &name, int version, double rmse, const std::string &file_path = "",
	                   bool is_active = true);
	ModelMetadata GetModel(const std::string &name, int version = -1); // -1 means latest active

	SharedMutex &GetMutex() {
		return lock_;
	}

private:
	ModelRegistry() = default;
	SharedMutex lock_;
	std::unordered_map<std::string, std::vector<ModelMetadata>> models_;
};

} // namespace duckdb
