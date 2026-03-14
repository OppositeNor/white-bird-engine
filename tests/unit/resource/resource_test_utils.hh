
#include "core/allocator/i_allocator.hh"
#include "resource/resource_chunk/resource_chunk.hh"
#include <functional>
#include <iostream>
#include <memory>
#include <vector>
#include <string>

namespace WBE = WhiteBirdEngine;

class MockResourceChunk final : public WBE::ResourceChunk {
public:

    struct Builder {
        std::function<WBE::RefRes<char>(const std::string&)> get_resource_callback = nullptr;
        std::function<size_t()> get_size_callback = nullptr;
        std::function<WBE::RefRes<char>(size_t, const void*)> push_data_callback = nullptr;
        std::function<void*(WBE::MemID)> get_data_callback = nullptr;
        std::function<const void*(WBE::MemID)> get_data_const_callback = nullptr;
        std::function<std::vector<std::string>()> get_all_keys_callback = nullptr;
        std::function<bool(const std::string&)> contains_callback = nullptr;
        std::function<WBE::RefRes<char>(const std::string&, const void*, size_t)> push_res_unit_ptr_callback = nullptr;
        std::function<void(void**, const std::string&)> create_ref_to_callback = nullptr;

        Builder& set_get_resource_callback(
            std::function<WBE::RefRes<char>(const std::string&)> p_callback) {
            get_resource_callback = p_callback;
            return *this;
        }

        Builder& set_get_size_callback(std::function<size_t()> p_callback) {
            get_size_callback = p_callback;
            return *this;
        }

        Builder& set_push_data_callback(std::function<WBE::RefRes<char>(size_t, const void*)> p_callback) {
            push_data_callback = p_callback;
            return *this;
        }

        Builder& set_get_data_callback(std::function<void*(WBE::MemID)> p_callback) {
            get_data_callback = p_callback;
            return *this;
        }

        Builder& set_get_data_const_callback(std::function<const void*(WBE::MemID)> p_callback) {
            get_data_const_callback = p_callback;
            return *this;
        }

        Builder& set_get_all_keys_callback(std::function<std::vector<std::string>()> p_callback) {
            get_all_keys_callback = p_callback;
            return *this;
        }

        Builder& set_contains_callback(std::function<bool(const std::string&)> p_callback) {
            contains_callback = p_callback;
            return *this;
        }

        Builder& set_push_res_unit_ptr_callback(std::function<WBE::RefRes<char>(const std::string&, const void*, size_t)> p_callback) {
            push_res_unit_ptr_callback = p_callback;
            return *this;
        }

        Builder& set_create_ref_to_callback(std::function<void(void**, const std::string&)> p_callback) {
            create_ref_to_callback = p_callback;
            return *this;
        }

        std::unique_ptr<MockResourceChunk> build(std::ostream& p_os) {
            return std::make_unique<MockResourceChunk>(
                get_resource_callback,
                get_size_callback,
                push_data_callback,
                get_data_callback,
                get_data_const_callback,
                get_all_keys_callback,
                contains_callback,
                push_res_unit_ptr_callback,
                create_ref_to_callback,
                p_os
            );
        }
    };

    MockResourceChunk(
        std::function<WBE::RefRes<char>(const std::string&)> p_get_resource_cb,
        std::function<size_t()> p_get_size_cb,
        std::function<WBE::RefRes<char>(size_t, const void*)> p_push_data_cb,
        std::function<void*(WBE::MemID)> p_get_data_cb,
        std::function<const void*(WBE::MemID)> p_get_data_const_cb,
        std::function<std::vector<std::string>()> p_get_all_keys_cb,
        std::function<bool(const std::string&)> p_contains_cb,
        std::function<WBE::RefRes<char>(const std::string&, const void*, size_t)> p_push_res_unit_ptr_cb,
        std::function<void(void**, const std::string&)> p_create_ref_to_cb,
        std::ostream& p_os
    ) : get_resource_callback(p_get_resource_cb),
        get_size_callback(p_get_size_cb),
        push_data_callback(p_push_data_cb),
        get_data_callback(p_get_data_cb),
        get_data_const_callback(p_get_data_const_cb),
        get_all_keys_callback(p_get_all_keys_cb),
        contains_callback(p_contains_cb),
        push_res_unit_ptr_callback(p_push_res_unit_ptr_cb),
        create_ref_to_callback(p_create_ref_to_cb),
        os(&p_os) {}

    virtual WBE::RefRes<char> get_resource(const std::string& p_id) const override {
        WBE::RefRes<char> ret = WBE::MEM_NULL;
        if (get_resource_callback) {
            ret = get_resource_callback(p_id);
        }
        (*os) << "get_resource " << p_id << '\n';
        return ret;
    }

    virtual size_t get_size() const override {
        size_t ret = 0;
        if (get_size_callback) {
            ret = get_size_callback();
        }
        (*os) << "get_size" << '\n';
        return ret;
    }

    virtual WBE::RefRes<char> push_data(size_t p_res_size, const void* p_res) override {
        WBE::RefRes<char> ret = WBE::MEM_NULL;
        if (push_data_callback) {
            ret = push_data_callback(p_res_size, p_res);
        }
        (*os) << "push_data " << p_res_size << " p_res" << '\n';
        return ret;
    }

    virtual void* get_data(WBE::MemID p_mem_id) override {
        void* ret = nullptr;
        if (get_data_callback) {
            ret = get_data_callback(p_mem_id);
        }
        (*os) << "get_data " << p_mem_id << '\n';
        return ret;
    }

    virtual const void* get_data(WBE::MemID p_mem_id) const override {
        const void* ret = nullptr;
        if (get_data_const_callback) {
            ret = get_data_const_callback(p_mem_id);
        }
        (*os) << "get_data " << p_mem_id << '\n';
        return ret;
    }

    virtual std::vector<std::string> get_all_keys() const override {
        std::vector<std::string> ret = {};
        if (get_all_keys_callback) {
            ret = get_all_keys_callback();
        }
        (*os) << "get_all_keys" << '\n';
        return ret;
    }

    virtual bool contains(const std::string& p_res_key) const override {
        bool ret = false;
        if (contains_callback) {
            ret = contains_callback(p_res_key);
        }
        (*os) << "contains " << p_res_key << '\n';
        return ret;
    }

    virtual void create_ref_to(void** p_ptr, const std::string& p_key) override {
        if (create_ref_to_callback) {
            create_ref_to_callback(p_ptr, p_key);
        }
        (*os) << "create_ref_to p_ptr " << p_key << '\n';
    }

protected:
    virtual WBE::RefRes<char> push_res_unit_ptr(const std::string& p_res_key, const void* p_res, size_t p_size) override {
        WBE::RefRes<char> ret = WBE::MEM_NULL;
        if (push_res_unit_ptr_callback) {
            ret = push_res_unit_ptr_callback(p_res_key, p_res, p_size);
        }
        (*os) << "push_res_unit_ptr " << p_res_key << " p_res " << p_size << '\n';
        return ret;
    }

private:
    std::function<WBE::RefRes<char>(const std::string&)> get_resource_callback;
    std::function<size_t()> get_size_callback;
    std::function<WBE::RefRes<char>(size_t, const void*)> push_data_callback;
    std::function<void*(WBE::MemID)> get_data_callback;
    std::function<const void*(WBE::MemID)> get_data_const_callback;
    std::function<std::vector<std::string>()> get_all_keys_callback;
    std::function<bool(const std::string&)> contains_callback;
    std::function<WBE::RefRes<char>(const std::string&, const void*, size_t)> push_res_unit_ptr_callback;
    std::function<void(void**, const std::string&)> create_ref_to_callback;
    std::ostream* os;
};

