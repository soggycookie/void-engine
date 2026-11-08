#ifdef __clang__
#pragma once
#include "sparse_set.h"
#include "logger/log.h"
#endif // __clang__

namespace VoidEngine::ECS
{
template <typename T>
void SparseSet<T>::init(Vallocator &vallocator, uint64_t capacity)
{
    ENGINE_ASSERT(capacity > 0, "Capacity must be greater than 0!");
    dense_array.init(vallocator, capacity);
    sparse_pages.init(vallocator, capacity);
    dense_array.push_back(vallocator, 0);
    alive_count = 1;
}

template <typename T>
uint32_t SparseSet<T>::get_page_index(uint32_t low_id) const
{
    return low_id >> SPARSE_PAGE_BIT;
}

template <typename T>
uint32_t SparseSet<T>::get_page_offset(uint32_t low_id) const
{
    return low_id & (SPARSE_PAGE_COUNT - 1);
}

template <typename T>
SparseSet<T>::Page *SparseSet<T>::get_sparse_page_no_alloc(uint32_t low_id)
{
    uint32_t page_idx = get_page_index(low_id);
    if (sparse_pages.get_count() < page_idx + 1)
    {
        return nullptr;
    }

    Page *page = &sparse_pages[page_idx];
    if (!page->dense_indices || !page->data)
    {
        return nullptr;
    }
    return page;
}

template <typename T>
const SparseSet<T>::Page *SparseSet<T>::get_sparse_page_no_alloc(uint32_t low_id) const
{
    uint32_t page_idx = get_page_index(low_id);
    if (sparse_pages.get_count() < page_idx + 1)
    {
        return nullptr;
    }

    const Page *page = &sparse_pages[page_idx];
    if (!page->dense_indices || !page->data)
    {
        return nullptr;
    }
    return page;
}

template <typename T>
SparseSet<T>::Page *SparseSet<T>::get_sparse_page(Vallocator &vallocator, uint32_t low_id)
{
    uint32_t page_idx = get_page_index(low_id);
    if (sparse_pages.get_count() < page_idx + 1)
    {
        return nullptr;
    }

    Page *page = &sparse_pages[page_idx];
    // lazy load page allocation
    if (!page->dense_indices && !page->data)
    {
        alloc_page(vallocator, page);
    }

    return page;
}

template <typename T>
SparseSet<T>::Page *SparseSet<T>::create_sparse_page(Vallocator &vallocator, uint32_t low_id)
{
    uint32_t page_idx = get_page_index(low_id);
    ENGINE_ASSERT(sparse_pages.get_count() < (page_idx + 1), "Page count is already sufficient!");
    uint64_t additional_page_count = (page_idx + 1) - sparse_pages.get_count();
    uint64_t count = sparse_pages.get_count();
    for (uint64_t idx = 0; idx < additional_page_count; ++idx)
    {
        sparse_pages.push_back(vallocator, Page());
    }
    Page *new_page = &sparse_pages[page_idx];
    ENGINE_ASSERT(new_page, "Failed to allocate new sparse page!");
    alloc_page(vallocator, new_page);

    return new_page;
}

template <typename T>
SparseSet<T>::Page *SparseSet<T>::create_or_get_sparse_page(Vallocator &vallocator, uint32_t low_id)
{
    Page *page = get_sparse_page(vallocator, low_id);

    if (!page)
    {
        page = create_sparse_page(vallocator, low_id);
    }

    return page;
}

template <typename T>
void SparseSet<T>::alloc_page(Vallocator &vallocator, Page *page)
{
    ENGINE_ASSERT(page, "Sparse page must not be null");
    page->dense_indices = SCAST(uint32_t *, vallocator.valloc(SPARSE_PAGE_COUNT * sizeof(uint32_t), alignof(uint32_t)));
    ENGINE_ASSERT(page->dense_indices, "Failed to allocate dense index array");
    std::memset(page->dense_indices, 0, sizeof(uint32_t) * SPARSE_PAGE_COUNT);
    page->data = SCAST(T *, vallocator.valloc(sizeof(T) * SPARSE_PAGE_COUNT, alignof(T)));
    ENGINE_ASSERT(page->data, "Failed to allocate data array");
}

template <typename T>
template <typename U>
uint64_t SparseSet<T>::push(Vallocator &vallocator, uint32_t new_id, U &&val, bool try_reuse)
{
    static_assert(std::is_constructible_v<T, U &&>, "Arg type cannot be used to construct T!");
    ENGINE_ASSERT(alive_count > 0, "Alive count must be greater than 0!");
    ENGINE_ASSERT(dense_array.get_count() >= alive_count,
                  "Alive count must be equal or smaller to dense count!");
    ENGINE_ASSERT(dense_array.get_count() != UINT32_MAX, "Reach maximum id value!");
    ENGINE_ASSERT(!is_existing_skip_version(new_id), "Id already exists!");
    bool reuse_success = false;
    uint64_t id = new_id;
    if (try_reuse)
    {
        uint64_t reused_id = get_used_id();
        if (reused_id != 0)
        {
            id = reused_id;
            reuse_success = true;
        }
    }
    uint32_t low_id = SCAST(uint32_t, id);
    uint32_t page_idx = get_page_index(low_id);
    uint32_t page_offset = get_page_offset(low_id);

    Page *page = create_or_get_sparse_page(vallocator, low_id);
    ENGINE_ASSERT(page, "Failed to acquire page!");
    uint32_t dense_idx = page->dense_indices[page_offset];
    ENGINE_ASSERT(dense_idx == 0, "The id is already occupied!");

    uint32_t dense_count = dense_array.get_count();
    if (!reuse_success)
    {
        dense_array.push_back(vallocator, id);
        swap_dense(dense_count, alive_count, false);
    }
    else
    {
        id = bump_high_32bit(id);
        dense_array[alive_count] = id;
    }

    page->dense_indices[page_offset] = alive_count;
    new (&page->data[page_offset]) T(std::forward<U>(val));
    ++alive_count;

    return id;
}

template <typename T>
bool SparseSet<T>::is_existing(uint64_t id) const
{
    uint32_t low_id = low_32bit(id);
    if (low_id == 0)
    {
        return false;
    }
    const Page *page = get_sparse_page_no_alloc(low_id);
    if (!page)
    {
        return false;
    }
    uint32_t dense_idx = page->dense_indices[get_page_offset(low_id)];
    if (dense_idx == 0)
    {
        return false;
    }
    uint64_t actual_id = dense_array[dense_idx];
    return actual_id == id;
}

template <typename T>
bool SparseSet<T>::is_existing_skip_version(uint32_t low_id) const
{
    if (low_id == 0)
    {
        return false;
    }
    const Page *page = get_sparse_page_no_alloc(low_id);
    if (!page)
    {
        return false;
    }
    uint32_t dense_idx = page->dense_indices[get_page_offset(low_id)];
    if (dense_idx == 0)
    {
        return false;
    }
    uint64_t actual_id = dense_array[dense_idx];
    return low_32bit(actual_id) == low_id;
}

template <typename T>
uint64_t SparseSet<T>::get_full_id(uint32_t low_id) const
{
    const Page *page = get_sparse_page_no_alloc(low_id);
    ENGINE_ASSERT(page, "Id is not existing!");
    uint32_t dense_idx = page->dense_indices[get_page_offset(low_id)];

    return dense_array[dense_idx];
}

template <typename T>
T *SparseSet<T>::get(uint64_t id)
{
    ENGINE_ASSERT(is_existing(id), "Id does not exist!");
    uint32_t low_id = SCAST(uint32_t, id);
    Page *page = get_sparse_page_no_alloc(low_id);
    if (!page)
    {
        return nullptr;
    }
    return page->data + get_page_offset(low_id);
}

template <typename T>
T &SparseSet<T>::operator[](uint64_t id)
{
    ENGINE_ASSERT(is_existing(id), "Id does not exist!");
    uint32_t low_id = SCAST(uint32_t, id);
    Page *page = get_sparse_page_no_alloc(low_id);
    ENGINE_ASSERT(page, "Page is not initialized!");
    ENGINE_ASSERT(page->dense_indices, "Sparse page dense index array is not initialized!");
    ENGINE_ASSERT(page->data, "Sparse page data array is not initialized!");

    return page->data[get_page_offset(low_id)];
}

template <typename T>
const T &SparseSet<T>::operator[](uint64_t id) const
{
    ENGINE_ASSERT(is_existing(id), "Id does not exist!");
    uint32_t low_id = SCAST(uint32_t, id);
    const Page *page = get_sparse_page_no_alloc(low_id);
    ENGINE_ASSERT(page, "Page is not initialized!");
    ENGINE_ASSERT(page->dense_indices, "Sparse page dense index array is not initialized!");
    ENGINE_ASSERT(page->data, "Sparse page data array is not initialized!");

    return page->data[get_page_offset(low_id)];
}

template <typename T>
const DynamicArray<uint64_t> &SparseSet<T>::get_dense_array() const
{
    return dense_array;
}

template <typename T>
void SparseSet<T>::remove(uint64_t id)
{
    uint32_t low_id = SCAST(uint32_t, id);
    ENGINE_ASSERT(low_id > 0, "Default value 0 at index 0 cannot be removed!");
    Page *page = get_sparse_page_no_alloc(low_id);
    ENGINE_ASSERT(page, "Id is not existing, cannot remove!");

    uint32_t dense_idx = page->dense_indices[get_page_offset(low_id)];
    if (dense_idx == 0)
    {
        return;
    }

    ENGINE_ASSERT(dense_idx < alive_count, "Index is out of alive bound!");
    swap_dense(dense_idx, alive_count - 1, true);

    if constexpr (!std::is_trivially_destructible_v<T>)
    {
        T *data = get(low_id);
        data->~T();
    }

    page->dense_indices[get_page_offset(low_id)] = 0;
    --alive_count;
}

/// <summary>
/// Swap Page Dense is for reused id flow
/// where the page dense is set correctly before
/// it just has to swap the dense array to work
/// </summary>
/// <typeparam name="T"></typeparam>
/// <param name="srcIndex"></param>
/// <param name="destIndex"></param>
/// <param name="swapPageDense"></param>
template <typename T>
void SparseSet<T>::swap_dense(uint32_t src_idx, uint32_t dst_idx, bool swap_page)
{
    ENGINE_ASSERT(dst_idx > 0 && src_idx > 0, "Indices must be greater than 0!");
    ENGINE_ASSERT(src_idx < dense_array.get_count(), "Index is out of bound!");
    ENGINE_ASSERT(dst_idx < dense_array.get_count(), "Index is out of bound!");
    if (src_idx == dst_idx)
    {
        return;
    }

    uint64_t src_id = dense_array[src_idx];
    uint64_t dst_id = dense_array[dst_idx];
    dense_array[src_idx] = dst_id;
    dense_array[dst_idx] = src_id;

    if (swap_page)
    {
        Page *src_page = get_sparse_page_no_alloc(SCAST(uint32_t, src_id));
        Page *dst_page = get_sparse_page_no_alloc(SCAST(uint32_t, dst_id));
        ENGINE_ASSERT(src_page, "Page must not be null!");
        ENGINE_ASSERT(dst_page, "Page must not be null!");
        src_page->dense_indices[get_page_offset(src_id)] = dst_idx;
        dst_page->dense_indices[get_page_offset(dst_id)] = src_idx;
    }
}

// template <typename T>
// void SparseSet<T>::PrintAllDense()
// {
//     for (uint32_t i = 0; i < dense_array.GetCount(); i++)
//     {
//         std::cout << SCAST(uint64_t *, dense_array.GetArray())[i] << ", ";
//     }
//     std::cout << std::endl;
// }
//
// template <typename T>
// void SparseSet<T>::PrintAliveDense()
// {
//     for (uint32_t i = 0; i <= alive_count; i++)
//     {
//         std::cout << SCAST(uint64_t *, dense_array.GetArray())[i] << ", ";
//     }
//     std::cout << std::endl;
// }
//
// template <typename T>
// void SparseSet<T>::PrintDeadDense()
// {
//     for (uint32_t i = alive_count + 1; i < dense_array.GetCount(); i++)
//     {
//         std::cout << SCAST(uint64_t *, dense_array.GetArray())[i] << ", ";
//     }
//     std::cout << std::endl;
// }

template <typename T>
uint64_t SparseSet<T>::get_used_id() const
{
    if (alive_count < dense_array.get_count())
    {
        return dense_array[alive_count];
    }
    return 0;
}

template <typename T>
void SparseSet<T>::destroy(Vallocator &vallocator)
{
    if constexpr (!std::is_trivially_destructible_v<T>)
    {
        for (uint64_t idx = 1; idx <= alive_count; idx++)
        {
            uint64_t id = dense_array[idx];
            T *data = get(id);
            ENGINE_ASSERT(data, "Data is null!");
            data->~T();
        }
    }

    for (uint64_t idx = 0; idx < sparse_pages.get_count(); ++idx)
    {
        Page &page = sparse_pages[idx];
        if (page.dense_indices && page.data)
        {
            vallocator.vfree(sizeof(uint32_t) * SPARSE_PAGE_COUNT, page.dense_indices);
            vallocator.vfree(sizeof(T) * SPARSE_PAGE_COUNT, page.data);
        }
    }
    sparse_pages.destroy(vallocator);
    dense_array.destroy(vallocator);
    alive_count = 0;
}

} // namespace VoidEngine::ECS
