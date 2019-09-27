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
#ifndef INCLUDED_GHEX_COMMON_AWAIT_FUTURES_HPP
#define INCLUDED_GHEX_COMMON_AWAIT_FUTURES_HPP

#include <vector>

namespace gridtools {

    namespace ghex {
        
        template<typename FutureRange, typename Continuation>
        void await_futures(FutureRange& range, Continuation&& cont)
        {
            int size = range.size();
            std::vector<int> index_list(size);
            for (int i = 0; i < size; ++i)
                index_list[i] = i;
            while(size>0)
            {
                for (int j = 0; j < size; ++j)
                {
                    const auto k = index_list[j];
                    if (range[k].test())
                    {
                        if (j < --size)
                            index_list[j--] = index_list[size];
                                cont(range[k].get());
                    }
                }
            }
        }

    }
}

#endif // INCLUDED_GHEX_COMMON_AWAIT_FUTURES_HPP
