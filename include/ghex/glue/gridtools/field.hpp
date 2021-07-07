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
#ifndef INCLUDED_GHEX_GLUE_GRIDTOOLS_FIELD_HPP
#define INCLUDED_GHEX_GLUE_GRIDTOOLS_FIELD_HPP

#include "../../structured/simple_field_wrapper.hpp"
#include "../../arch_traits.hpp"
#include <gridtools/storage/data_store.hpp>
#include <gridtools/meta/list_to_iseq.hpp>
#ifdef __CUDACC__
#include <gridtools/storage/storage_cuda/cuda_storage.hpp>
#endif

namespace gridtools {


        namespace _impl {

            template<typename Halo, typename S, typename I, I... Is>
            array<S, sizeof...(Is)>
            get_begin(std::integer_sequence<I,Is...>)
            {
                return {Halo::template at<Is>()...};
            }

            template<typename Seq>
            struct get_layout_map;

            template<template <class T, T...> class Seq, typename I, I... Is>
            struct get_layout_map<Seq<I, Is...>>
            {
                using type = layout_map<Is...>;
            };

            template<typename Layout>
            struct get_unmasked_layout_map;

            template<int... Args>
            struct get_unmasked_layout_map<layout_map<Args...>>
            {
                using args          = ::gridtools::meta::list<std::integral_constant<int, Args>...>;
                using unmasked_args = ::gridtools::meta::filter<_layout_map::not_negative, args>;
                using integer_seq   = ::gridtools::meta::list_to_iseq<unmasked_args>;
                using type          = typename get_layout_map<integer_seq>::type;
            };
        } // namespace _impl
    namespace ghex {
        namespace _impl {

            template<typename T, typename Arch, typename DomainDescriptor, typename Seq>
            struct get_simple_field_wrapper_type;

            template<typename T, typename Arch, typename DomainDescriptor, template <class J, J...> class Seq, typename I, I... Is>
            struct get_simple_field_wrapper_type<T,Arch,DomainDescriptor, Seq<I, Is...>>
            {
                using type = structured::simple_field_wrapper<T,Arch,DomainDescriptor,Is...>;
            };

            template<typename Storage>
            struct get_arch
            {
                using type = ::gridtools::ghex::cpu;
            };
#ifdef __CUDACC__
            template<typename DataType>
            struct get_arch<cuda_storage<DataType>>
            {
                using type = ::gridtools::ghex::gpu;
            };
#endif
        } // namespace _impl

        template <typename Grid, typename Storage, typename StorageInfo>
        auto wrap_gt_field(Grid& grid, const ::gridtools::data_store<Storage,StorageInfo>& ds, typename arch_traits<typename _impl::get_arch<Storage>::type>::device_id_type device_id = 0)
        {
            using domain_id_type    = typename Grid::domain_id_type;
            using data_store_t      = ::gridtools::data_store<Storage,StorageInfo>;
            using arch_t          = typename _impl::get_arch<Storage>::type;
            using value_t           = typename data_store_t::data_t;
            using layout_t          = typename StorageInfo::layout_t;
            using integer_seq       = typename ::gridtools::_impl::get_unmasked_layout_map<layout_t>::integer_seq;
            using uint_t            = decltype(layout_t::masked_length);
            using dimension         = std::integral_constant<uint_t, layout_t::masked_length>;
            using halo_t            = typename StorageInfo::halo_t;

            using sfw_t             = typename _impl::get_simple_field_wrapper_type<
                                          value_t,
                                          arch_t,
                                          structured::domain_descriptor<domain_id_type, dimension::value>,
                                          integer_seq>::type;

            value_t* ptr        = ds.get_storage_ptr()->get_target_ptr();
            const auto& info    = ds.info();
            const auto& extents = info.total_lengths();
            const auto& origin  = ::gridtools::_impl::get_begin<halo_t, uint_t>(std::make_index_sequence<dimension::value>());
            auto strides        = info.strides();

            for (unsigned int i=0u; i<dimension::value; ++i)
            {
                strides[i] *= sizeof(value_t);
            }

            return sfw_t(grid.m_domains[0].domain_id(), ptr, origin, extents, strides, device_id);
        }

    } // namespace ghex

} // namespace gridtools

#endif /* INCLUDED_GHEX_GLUE_GRIDTOOLS_FIELD_HPP */

