/**
 * Copyright (c) 2023 Hailo Technologies Ltd. All rights reserved.
 * Distributed under the MIT license (https://opensource.org/licenses/MIT)
**/
/**
 * @file mapped_buffer.cpp
 * @brief Vdma mapped buffer implementation
 **/

#include "mapped_buffer.hpp"

#include "vdma/vdma_device.hpp"


namespace hailort {
namespace vdma {

Expected<MappedBuffer> MappedBuffer::create(HailoRTDriver &driver,
    std::shared_ptr<DmaAbleBuffer> buffer, HailoRTDriver::DmaDirection data_direction)
{
    auto status = HAILO_UNINITIALIZED;
    auto result = MappedBuffer(driver, buffer, data_direction, status);
    CHECK_SUCCESS_AS_EXPECTED(status);

    return result;
}

Expected<MappedBufferPtr> MappedBuffer::create_shared(HailoRTDriver &driver, std::shared_ptr<DmaAbleBuffer> buffer,
    HailoRTDriver::DmaDirection data_direction)
{
    auto dma_mapped_buffer = create(driver, buffer, data_direction);
    CHECK_EXPECTED(dma_mapped_buffer);

    auto result = make_shared_nothrow<MappedBuffer>(dma_mapped_buffer.release());
    CHECK_NOT_NULL_AS_EXPECTED(result, HAILO_OUT_OF_HOST_MEMORY);

    return result;
}

Expected<MappedBuffer> MappedBuffer::create(HailoRTDriver &driver,
    HailoRTDriver::DmaDirection data_direction, size_t size, void *user_address)
{
    auto buffer = DmaAbleBuffer::create(driver, size, user_address);
    CHECK_EXPECTED(buffer);

    return create(driver, buffer.release(), data_direction);
}

Expected<MappedBufferPtr> MappedBuffer::create_shared(HailoRTDriver &driver,
    HailoRTDriver::DmaDirection data_direction, size_t size, void *user_address)
{
    auto dma_mapped_buffer = create(driver, data_direction, size, user_address);
    CHECK_EXPECTED(dma_mapped_buffer);

    auto result = make_shared_nothrow<MappedBuffer>(dma_mapped_buffer.release());
    CHECK_NOT_NULL_AS_EXPECTED(result, HAILO_OUT_OF_HOST_MEMORY);

    return result;
}

MappedBuffer::MappedBuffer(HailoRTDriver &driver, std::shared_ptr<DmaAbleBuffer> buffer,
                           HailoRTDriver::DmaDirection data_direction, hailo_status &status) :
    m_driver(driver),
    m_buffer(buffer),
    m_mapping_handle(HailoRTDriver::INVALID_DRIVER_VDMA_MAPPING_HANDLE_VALUE),
    m_data_direction(data_direction)
{
    auto expected_handle = driver.vdma_buffer_map(m_buffer->user_address(), m_buffer->size(), m_data_direction,
        m_buffer->buffer_identifier());
    if (!expected_handle) {
        LOGGER__ERROR("Mapping address {} to dma failed", m_buffer->user_address());
        status = expected_handle.status();
        return;
    }

    m_mapping_handle = expected_handle.release();
    status = HAILO_SUCCESS;
}

MappedBuffer::~MappedBuffer()
{
    if (HailoRTDriver::INVALID_DRIVER_VDMA_MAPPING_HANDLE_VALUE != m_mapping_handle) {
        m_driver.vdma_buffer_unmap(m_mapping_handle);
        m_mapping_handle = HailoRTDriver::INVALID_DRIVER_VDMA_MAPPING_HANDLE_VALUE;
    }
}

MappedBuffer::MappedBuffer(MappedBuffer &&other) noexcept :
    m_driver(other.m_driver),
    m_buffer(std::move(other.m_buffer)),
    m_mapping_handle(std::exchange(other.m_mapping_handle, HailoRTDriver::INVALID_DRIVER_VDMA_MAPPING_HANDLE_VALUE)),
    m_data_direction(other.m_data_direction)
{}

void* MappedBuffer::user_address()
{
    return m_buffer->user_address();
}

size_t MappedBuffer::size() const
{
    return m_buffer->size();
}

HailoRTDriver::VdmaBufferHandle MappedBuffer::handle()
{
    return m_mapping_handle;
}

hailo_status MappedBuffer::synchronize(HailoRTDriver::DmaSyncDirection sync_direction)
{
    static constexpr auto BUFFER_START = 0;
    return synchronize(BUFFER_START, size(), sync_direction);
}

hailo_status MappedBuffer::synchronize(size_t offset, size_t count, HailoRTDriver::DmaSyncDirection sync_direction)
{
    CHECK(offset + count <= size(), HAILO_INVALID_ARGUMENT,
        "Synchronizing {} bytes starting at offset {} will overflow (buffer size {})",
        offset, count, size());
    return m_driver.vdma_buffer_sync(m_mapping_handle, sync_direction, offset, count);
}

} /* namespace vdma */
} /* namespace hailort */