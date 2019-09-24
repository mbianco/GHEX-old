/*
 * GridTools
 *
 * Copyright (c) 2014-2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 *
 */
#ifndef INCLUDED_SIMPLE_FIELD_WRAPPER_HPP
#define INCLUDED_SIMPLE_FIELD_WRAPPER_HPP

#include "./structured_domain_descriptor.hpp"
#include <iostream>
#include <cstring>

namespace gridtools {
namespace ghex {
    namespace detail {
        template<int D, int I>
        struct compute_strides_impl
        {
            template<typename Layout, typename Coordinate>
            static void apply(const Coordinate& extents, Coordinate& strides)
            {
                const auto last_idx = Layout::template find<I>();
                const auto idx      = Layout::template find<I-1>();
                strides[idx]        = strides[last_idx]*extents[last_idx];
                compute_strides_impl<D,I-1>::template apply<Layout>(extents,strides);
            }
        };
        template<int D>
        struct compute_strides_impl<D,0>
        {
            template<typename Layout, typename Coordinate>
            static void apply(const Coordinate&, Coordinate&)
            {
            }
        };
        template<int D>
        struct compute_strides
        {
            template<typename Layout, typename Coordinate>
            static void apply(const Coordinate& extents, Coordinate& strides)
            {
                const auto idx      = Layout::template find<D-1>();
                strides[idx]        = 1;
                compute_strides_impl<D,D-1>::template apply<Layout>(extents,strides);
            }
        };
    } // namespace detail

    // forward declaration
    template<typename T, typename Device, typename DomainDescriptor, int... Order>
    class simple_field_wrapper;

    /** @brief wraps a contiguous N-dimensional array and implements the field descriptor concept
     * @tparam T field value type
     * @tparam Device device type the data lives on
     * @tparam DomainIdType domain id type
     * @tparam Dimension N
     * @tparam Order permutation of the set {0,...,N-1} indicating storage layout (N-1 -> stride=1)*/
    template<typename T, typename Device, typename DomainIdType, int Dimension, int... Order>
    class simple_field_wrapper<T,Device,structured_domain_descriptor<DomainIdType,Dimension>, Order...>
    {
    public: // member types
        using value_type             = T;
        using device_type            = Device;
        using device_id_type         = typename device_type::id_type;
        using domain_descriptor_type = structured_domain_descriptor<DomainIdType,Dimension>;
        using dimension              = typename domain_descriptor_type::dimension;
        using layout_map             = gridtools::layout_map<Order...>;
        using domain_id_type         = DomainIdType;
        using coordinate_type        = typename domain_descriptor_type::halo_generator_type::coordinate_type;

    private: // members
        domain_id_type  m_dom_id;
        value_type*     m_data;
        coordinate_type m_strides;
        coordinate_type m_offsets;
        coordinate_type m_extents;
        device_id_type  m_device_id;

    public: // ctors
        /** @brief construcor
         * @tparam Array coordinate-like type
         * @param dom_id local domain id
         * @param data pointer to data
         * @param offsets coordinate of first physical coordinate (not buffer) from the orign of the wrapped N-dimensional array
         * @param extents extent of the wrapped N-dimensional array (including buffer regions)*/
        template<typename Array>
        simple_field_wrapper(domain_id_type dom_id, value_type* data, const Array& offsets, const Array& extents, device_id_type d_id = 0)
        : m_dom_id(dom_id), m_data(data), m_strides(1), m_device_id(d_id)
        {
            std::copy(offsets.begin(), offsets.end(), m_offsets.begin());
            std::copy(extents.begin(), extents.end(), m_extents.begin());
            // compute strides
            detail::compute_strides<dimension::value>::template apply<layout_map>(m_extents,m_strides);
        }
        simple_field_wrapper(simple_field_wrapper&&) noexcept = default;
        simple_field_wrapper(const simple_field_wrapper&) noexcept = default;

    public: // member functions
        typename device_type::id_type device_id() const { return m_device_id; }
        domain_id_type domain_id() const { return m_dom_id; }

        const coordinate_type& extents() const noexcept { return m_extents; }
        const coordinate_type& offsets() const noexcept { return m_offsets; }

        value_type* data() const { return m_data; }

        /** @brief access operator
         * @param x coordinate vector with respect to offset specified in constructor
         * @return reference to value */
        value_type& operator()(const coordinate_type& x) { return m_data[dot(x,m_strides)]; }
        const value_type& operator()(const coordinate_type& x) const { return m_data[dot(x,m_strides)]; }

        /** @brief access operator
         * @param is coordinates with respect to offset specified in constructor
         * @return reference to value */
        template<typename... Is>
        value_type& operator()(Is&&... is) { return m_data[dot(coordinate_type{is...}+m_offsets,m_strides)]; }
        template<typename... Is>
        const value_type& operator()(Is&&... is) const { return m_data[dot(coordinate_type{is...}+m_offsets,m_strides)]; }

        template<typename IndexContainer>
        void pack(T* buffer, const IndexContainer& c)
        {
            for (const auto& is : c)
            {
                detail::for_loop_pointer_arithmetic<dimension::value,dimension::value,layout_map>::apply(
                    [this,buffer](auto o_data, auto o_buffer)
                    {
                        buffer[o_buffer] = m_data[o_data];
                    },
                    is.local().first(),
                    is.local().last(),
                    m_extents,
                    m_offsets
                    );
                buffer += is.size();
            }
        }

        template<typename IndexContainer>
        void unpack(const T* buffer, const IndexContainer& c)
        {
            for (const auto& is : c)
            {
                detail::for_loop_pointer_arithmetic<dimension::value,dimension::value,layout_map>::apply(
                    [this,buffer](auto o_data, auto o_buffer)
                    {
                        m_data[o_data] = buffer[o_buffer];
                    },
                    is.local().first(),
                    is.local().last(),
                    m_extents,
                    m_offsets
                    );
                buffer += is.size();
            }
        }
    };

    /** @brief wrap a N-dimensional array (field) of contiguous memory
     * @tparam Device device type the data lives on
     * @tparam Order permutation of the set {0,...,N-1} indicating storage layout (N-1 -> stride=1)
     * @tparam DomainIdType domain id type
     * @tparam T field value type
     * @tparam Array coordinate-like type
     * @param dom_id local domain id
     * @param data pointer to data
     * @param offsets coordinate of first physical coordinate (not buffer) from the orign of the wrapped N-dimensional array
     * @param extents extent of the wrapped N-dimensional array (including buffer regions)
     * @return wrapped field*/
    template<typename Device, int... Order, typename DomainIdType, typename T, typename Array>
    simple_field_wrapper<T,Device,structured_domain_descriptor<DomainIdType,sizeof...(Order)>, Order...>
    wrap_field(DomainIdType dom_id, T* data, const Array& offsets, const Array& extents, typename Device::id_type device_id = 0)
    {
        return {dom_id, data, offsets, extents, device_id};
    }
} // namespace ghex
} // namespace gridtools

#endif /* INCLUDED_SIMPLE_FIELD_WRAPPER_HPP */

