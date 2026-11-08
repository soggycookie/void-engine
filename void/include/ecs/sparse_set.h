#pragma once
#include "dynamic_array.h"
#include "vallocator.h"

namespace VoidEngine::ECS
{

// NOTE: Sparse array only support maximum 2^32 id value
template <typename T>
class SparseSet
{
private:
    struct Page
    {
        uint32_t *dense_indices;
        T *data;
    };

    constexpr static uint64_t SPARSE_PAGE_BIT = 6;
    constexpr static uint64_t SPARSE_PAGE_COUNT = 1 << SPARSE_PAGE_BIT;

    DynamicArray<uint64_t> dense_array;
    DynamicArray<Page> sparse_pages;
    uint32_t alive_count; // Alive id
public:
    SparseSet() : dense_array(), sparse_pages(), alive_count(0)
    {
        static_assert(std::is_copy_constructible_v<T> || std::is_move_constructible_v<T>,
                      "T must be copy-constructible or move-constructible");
        static_assert(std::is_destructible_v<T>);
    }

    void init(Vallocator &vallocator, uint64_t capacity);
    void destroy(Vallocator &vallocator);

    uint32_t get_count() const { return alive_count; }

    bool is_existing(uint64_t id) const;
    bool is_existing_skip_version(uint32_t low_id) const;

    uint64_t get_full_id(uint32_t low_id) const;

    T *get(uint64_t id);

    template <typename U>
    uint64_t push(Vallocator &vallocator, uint32_t new_id, U &&val, bool try_reuse = true);

    void remove(uint64_t id);

    uint64_t get_used_id() const;
    T &operator[](uint64_t id);
    const T &operator[](uint64_t id) const;

    const DynamicArray<uint64_t> &get_dense_array() const;

    // void PrintAllDense();
    // void PrintAliveDense();
    // void PrintDeadDense();

private:
    void swap_dense(uint32_t src_idx, uint32_t dst_idx, bool swap_page);

    uint32_t get_page_index(uint32_t low_id) const;
    uint32_t get_page_offset(uint32_t low_id) const;

    void alloc_page(Vallocator &vallocator, Page *page);

    Page *get_sparse_page(Vallocator &vallocator, uint32_t low_id);
    Page *get_sparse_page_no_alloc(uint32_t low_id);
    const Page *get_sparse_page_no_alloc(uint32_t low_id) const;
    Page *create_sparse_page(Vallocator &vallocator, uint32_t low_id);
    Page *create_or_get_sparse_page(Vallocator &vallocator, uint32_t low_id);

};

} // namespace VoidEngine::ECS
