/*
 * GridTools
 *
 * Copyright (c) 2019, ETH Zurich
 * All rights reserved.
 *
 * Please, refer to the LICENSE file in the root directory.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef INCLUDED_GHEX_TL_UCX_COMMUNICATOR_HPP
#define INCLUDED_GHEX_TL_UCX_COMMUNICATOR_HPP

#include <iostream>
#include <map>
#include <functional>
#include <deque>

#include <ucp/api/ucp.h>

#include "../communicator.hpp"
#include "../../common/debug.hpp"

#ifdef USE_PMIX
#define USE_PMI
#include "../util/pmi/pmix/pmi.hpp"
using PmiType = gridtools::ghex::tl::pmi<gridtools::ghex::tl::pmix_tag>;
#endif

#include "ucp_lock.hpp"
#include "request.hpp"

#define ghex_likely(x)   __builtin_expect(x, 1)
#define ghex_unlikely(x) __builtin_expect(x, 0)

namespace gridtools
{
    namespace ghex
    {
	namespace tl
	{

	    /*
	     * GHEX tag structure:
	     *
	     * 01234567 01234567 01234567 01234567 01234567 01234567 01234567 01234567
	     *                                    |
	     *      message tag (32)              |   source rank (32)
	     *                                    |
	     */
#define GHEX_TAG_BITS                       32
#define GHEX_RANK_BITS                      32
#define GHEX_TAG_MASK                       0xffffffff00000000ul
#define GHEX_SOURCE_MASK                    0x00000000fffffffful

#define GHEX_MAKE_SEND_TAG(_tag, _dst)			\
	    ((((uint64_t) (_tag) ) << GHEX_RANK_BITS) |	\
	     (((uint64_t) (_dst) )))


#define GHEX_MAKE_RECV_TAG(_ucp_tag, _ucp_tag_mask, _tag, _src)		\
	    {								\
		_ucp_tag_mask = GHEX_SOURCE_MASK | GHEX_TAG_MASK;	\
		_ucp_tag = ((((uint64_t) (_tag) ) << GHEX_RANK_BITS) |	\
			    (((uint64_t) (_src) )));			\
	    }								\

#define GHEX_GET_SOURCE(_ucp_tag)		\
	    ((_ucp_tag) & GHEX_SOURCE_MASK)


#define GHEX_GET_TAG(_ucp_tag)			\
	    ((_ucp_tag) >> GHEX_RANK_BITS)


	    /* local definitions - request and future related things */
	    namespace ucx
	    {
		static std::size_t   ucp_request_size; // size in bytes required for a request by the UCX library

		void empty_send_cb(void *, ucs_status_t ) {}
		void empty_recv_cb(void *, ucs_status_t , ucp_tag_recv_info_t *) {}
		void ghex_request_init_cb(void *request){
		    bzero(request, GHEX_REQUEST_SIZE);
		}
	    }


	    /** Class that provides the functions to send and receive messages. A message
	     * is an object with .data() that returns a pointer to `unsigned char`
	     * and .size(), with the same behavior of std::vector<unsigned char>.
	     * Each message will be sent and received with a tag, bot of type int
	     */

	    template<>
	    class communicator<ucx_tag>
	    {
	    public:
		using tag_type  = ucp_tag_t;
		using rank_type = int;
		using size_type = int;
		using request   = ucx::request;
                using traits    = int;

		/* these are static, i.e., shared by threads */
		static rank_type m_rank;
		static rank_type m_size;

	    protected:

		/* these are static, i.e., shared by threads */
		static ucp_context_h ucp_context;
		static ucp_worker_h  ucp_worker;

		/* these are per-thread */
		ucp_worker_h  ucp_worker_send;

#ifdef USE_PMI
		/** PMI interface to obtain peer addresses */
		/* per-communicator instance used to store/query connections */
		PmiType pmi_impl;

		/* global instance used to init/finalize the library */
		static PmiType pmi_impl_static;
#endif
		/** known connection pairs <rank, endpoint address>,
		    created as rquired by the communication pattern
		    Has to be per-thread
		*/
		std::map<rank_type, ucp_ep_h> connections;

	    public:
		
                rank_type rank() const noexcept { return m_rank; }
                size_type size() const noexcept { return m_size; }

		~communicator()
		{
		    ucp_worker_destroy(ucp_worker_send);
		}

		static void finalize ()
		{
		    if(ucp_worker) {
			ucp_worker_destroy(ucp_worker);
			ucp_cleanup(ucp_context);
			ucp_worker = nullptr;
			ucp_context = nullptr;
		    }
		}

		static void initialize()
		{
#ifdef USE_PMI
		    // communicator rank and world size
		    m_rank = pmi_impl_static.rank();
		    m_size = pmi_impl_static.size();
#endif

#ifdef THREAD_MODE_SERIALIZED
#ifndef USE_OPENMP_LOCKS
		    LOCK_INIT(ucp_lock);
#endif
#endif

		    // UCX initialization
		    ucs_status_t status;
		    ucp_params_t ucp_params;
		    ucp_config_t *config = NULL;
		    ucp_worker_params_t worker_params;

		    status = ucp_config_read(NULL, NULL, &config);
		    if(UCS_OK != status) ERR("ucp_config_read failed");

		    /* Initialize UCP */
		    {
			memset(&ucp_params, 0, sizeof(ucp_params));

			/* pass features, request size, and request init function */
			ucp_params.field_mask =
			    UCP_PARAM_FIELD_FEATURES          |
			    UCP_PARAM_FIELD_REQUEST_SIZE      |
			    UCP_PARAM_FIELD_TAG_SENDER_MASK   |
			    UCP_PARAM_FIELD_MT_WORKERS_SHARED |
			    UCP_PARAM_FIELD_ESTIMATED_NUM_EPS |
			    UCP_PARAM_FIELD_REQUEST_INIT      ;

			/* request transport support for tag matching */
			ucp_params.features =
			    UCP_FEATURE_TAG ;

			// TODO: templated request type - how do we know the size??
			ucp_params.request_size = GHEX_REQUEST_SIZE;

			/* this should be true if we have per-thread workers
			   otherwise, if one worker is shared by all thread, it should be false
			   This requires benchmarking. */
			ucp_params.mt_workers_shared = true;

			/* estimated number of end-points -
			   affects transport selection criteria and theresulting performance */
			ucp_params.estimated_num_eps = m_size;

			/* Mask which specifies particular bits of the tag which can uniquely identify
			   the sender (UCP endpoint) in tagged operations. */
			ucp_params.tag_sender_mask = GHEX_SOURCE_MASK;

			/* Needed to zero the memory region. Otherwise segfaults occured
			   when a std::function destructor was called on an invalid object */
			ucp_params.request_init = ucx::ghex_request_init_cb;

#if (GHEX_DEBUG_LEVEL == 2)
			if(0 == m_rank){
			    LOG("ucp version %s", ucp_get_version_string());
			    LOG("ucp features %lx", ucp_params.features);
			    ucp_config_print(config, stdout, NULL, UCS_CONFIG_PRINT_CONFIG);
			}
#endif

			status = ucp_init(&ucp_params, config, &ucp_context);
			ucp_config_release(config);

			if(UCS_OK != status) ERR("ucp_config_init");
			if(0 == m_rank) LOG("UCX initialized");
		    }

		    /* ask for UCP request size - non-templated version for the futures */
		    {
			ucp_context_attr_t attr = {};
			attr.field_mask = UCP_ATTR_FIELD_REQUEST_SIZE;
			ucp_context_query (ucp_context, &attr);

			/* UCP request size */
			ucx::ucp_request_size = attr.request_size;
		    }

		    /* create a worker */
		    {
			memset(&worker_params, 0, sizeof(worker_params));

			/* this should not be used if we have a single worker per thread */
			worker_params.field_mask  = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
#ifdef THREAD_MODE_MULTIPLE
			worker_params.thread_mode = UCS_THREAD_MODE_MULTI;
#elif defined THREAD_MODE_SERIALIZED
			worker_params.thread_mode = UCS_THREAD_MODE_SERIALIZED;
#else
			worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;
#endif

			status = ucp_worker_create (ucp_context, &worker_params, &ucp_worker);
			if(UCS_OK != status) ERR("ucp_worker_create failed");
			if(0 == m_rank) LOG("UCP worker created");
		    }

#ifdef USE_PMI
		    /* obtain the worker endpoint address and post it to PMI */
		    {
			ucp_address_t *worker_address;
			size_t address_length;

			status = ucp_worker_get_address(ucp_worker, &worker_address, &address_length);
			if(UCS_OK != status) ERR("ucp_worker_get_address failed");
			if(0 == m_rank) LOG("UCP worker addres length %zu", address_length);

			/* update pmi with local address information */
			std::vector<char> data((const char*)worker_address, (const char*)worker_address + address_length);
			pmi_impl_static.set("ghex-rank-address", data);
			ucp_worker_release_address(ucp_worker, worker_address);

			/* invoke global pmi data exchange */
			// pmi_exchange();
		    }
#endif
		}

		communicator(const traits& t = traits{})
		{

		    /* create a per-thread send worker */
		    ucs_status_t status;
		    ucp_worker_params_t worker_params;
		    memset(&worker_params, 0, sizeof(worker_params));

		    /* this should not be used if we have a single worker per thread */
		    worker_params.field_mask  = UCP_WORKER_PARAM_FIELD_THREAD_MODE;
		    worker_params.thread_mode = UCS_THREAD_MODE_SINGLE;

		    status = ucp_worker_create (ucp_context, &worker_params, &ucp_worker_send);
		    if(UCS_OK != status) ERR("ucp_worker_create failed");
		    if(0 == m_rank) LOG("UCP worker created");
		}

		ucp_ep_h connect(ucp_address_t *worker_address)
		{
		    ucs_status_t status;
		    ucp_ep_params_t ep_params;
		    ucp_ep_h ucp_ep;

		    /* create endpoint */
		    memset(&ep_params, 0, sizeof(ep_params));
		    ep_params.field_mask = UCP_EP_PARAM_FIELD_REMOTE_ADDRESS;
		    ep_params.address    = worker_address;
		    status = ucp_ep_create (ucp_worker_send, &ep_params, &ucp_ep);
		    if(UCS_OK != status) ERR("ucp_ep_create failed");

#if (GHEX_DEBUG_LEVEL == 2)
		    ucp_ep_print_info(ucp_ep, stdout);
		    ucp_worker_print_info(ucp_worker_send, stdout);
#endif

		    LOG("UCP connection established");
		    return ucp_ep;
		}

		ucp_ep_h rank_to_ep(const rank_type &rank)
		{
		    ucp_ep_h ep;

		    /* look for a connection to a given peer
		       create it if it does not yet exist */
		    auto conn = connections.find(rank);
		    if(ghex_unlikely( conn == connections.end() )){

			ucp_address_t *worker_address;
#ifdef USE_PMI
			/* get peer address - we have ownership of the address */
			std::vector<char> data = pmi_impl.get_bytes(rank, "ghex-rank-address");
			worker_address = (ucp_address_t*)data.data();
#else
			ERR("PMI is not enabled. Don't know how to obtain peer address.");
#endif

			ep = connect(worker_address);
			connections.emplace(rank, ep);
		    } else {

			/* found an existing connection - return the corresponding endpoint handle */
			ep = conn->second;
		    }

		    return ep;
		}

		/** Send a message to a destination with the given tag.
		 * It returns a future that can be used to check when the message is available
		 * again for the user.
		 *
		 * @tparam MsgType message type (this could be a std::vector<unsigned char> or a message found in message.hpp)
		 *
		 * @param msg Const reference to a message to send
		 * @param dst Destination of the message
		 * @param tag Tag associated with the message
		 *
		 * @return A future that will be ready when the message can be reused (e.g., filled with new data to send)
		 */
		template <typename MsgType>
		[[nodiscard]] request send(const MsgType &msg, rank_type dst, tag_type tag)
		{
		    ucp_ep_h ep;
		    ucs_status_ptr_t status;
		    request req;

		    ep = rank_to_ep(dst);

		    /* send */
		    status = ucp_tag_send_nb(ep, msg.data(), msg.size(), ucp_dt_make_contig(1),
					     GHEX_MAKE_SEND_TAG(tag, m_rank), ucx::empty_send_cb);

		    if(ghex_unlikely( UCS_OK == (uintptr_t)status )){

			/* send completed immediately */
			req.m_req = nullptr;
		    } else if(ghex_likely( !UCS_PTR_IS_ERR(status) )) {

			/* return the request */
			req.m_req = (request::req_type)(status);
			req.m_req->m_ucp_worker = ucp_worker;
			req.m_req->m_ucp_worker_send = ucp_worker_send;
			req.m_req->m_type = ucx::REQ_SEND;
		    } else {
			ERR("ucp_tag_recv_nb failed");
		    }

		    return req;
		}


		/** Receive a message from a destination with the given tag.
		 * It returns a future that can be used to check when the message is available
		 * to be read.
		 *
		 * @tparam MsgType message type (this could be a std::vector<unsigned char> or a message found in message.hpp)
		 *
		 * @param msg Const reference to a message that will contain the data
		 * @param src Source of the message
		 * @param tag Tag associated with the message
		 *
		 * @return A future that will be ready when the message can be read
		 */
		template <typename MsgType>
		[[nodiscard]] request recv(MsgType &msg, rank_type src, tag_type tag) {
		    ucp_tag_t ucp_tag, ucp_tag_mask;
		    ucs_status_ptr_t status;
		    request req;

		    CRITICAL_BEGIN(ucp_lock) {

			/* recv */
			GHEX_MAKE_RECV_TAG(ucp_tag, ucp_tag_mask, tag, src);
			status = ucp_tag_recv_nb(ucp_worker, msg.data(), msg.size(), ucp_dt_make_contig(1),
						 ucp_tag, ucp_tag_mask, ucx::empty_recv_cb);

			if(ghex_likely( !UCS_PTR_IS_ERR(status) )) {

			    ucs_status_t rstatus;
			    rstatus = ucp_request_check_status (status);
			    if(ghex_unlikely( rstatus != UCS_INPROGRESS )){

				/* recv completed immediately */
				req.m_req = nullptr;

		    		/* we need to free the request here, not in the callback */
		    		ucp_request_free(status);
			    } else {

				/* return the request */
				req.m_req = (request::req_type)(status);
				req.m_req->m_ucp_worker = ucp_worker;
				req.m_req->m_ucp_worker_send = ucp_worker_send;
				req.m_req->m_type = ucx::REQ_RECV;
			    }
			} else {
			    ERR("ucp_tag_send_nb failed");
			}
		    } CRITICAL_END(ucp_lock);

		    return req;
		}


		/** Function to invoke to poll the transport layer and check for the completions
		 * of the operations without a future associated to them (that is, they are associated
		 * to a call-back). When an operation completes, the corresponfing call-back is invoked
		 * with the rank and tag associated with that request.
		 *
		 * @return unsigned Non-zero if any communication was progressed, zero otherwise.
		 */
		unsigned progress()
		{
		    int p = 0;

		    p+= ucp_worker_progress(ucp_worker_send);
		    p+= ucp_worker_progress(ucp_worker_send);
		    p+= ucp_worker_progress(ucp_worker_send);
		    CRITICAL_BEGIN(ucp_lock) {
			p+= ucp_worker_progress(ucp_worker);
			p+= ucp_worker_progress(ucp_worker);
		    } CRITICAL_END(ucp_lock);

		    return p;
		}
	    };

	    /** static communicator properties, shared between threads */

#ifdef USE_PMI
	    PmiType communicator<ucx_tag>::pmi_impl_static;
#endif
	    communicator<ucx_tag>::rank_type communicator<ucx_tag>::m_rank;
	    communicator<ucx_tag>::rank_type communicator<ucx_tag>::m_size;
	    ucp_context_h communicator<ucx_tag>::ucp_context = 0;
	    ucp_worker_h  communicator<ucx_tag>::ucp_worker = 0;

	} // namespace tl
    } // namespace ghex
} // namespace gridtools

#endif /* INCLUDED_GHEX_TL_UCX_COMMUNICATOR_HPP */
