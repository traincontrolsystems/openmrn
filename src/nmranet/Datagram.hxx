/** \copyright
 * Copyright (c) 2014, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  - Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 *  - Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * \file Datagram.hxx
 *
 * Interface for datagram functionality in Async NMRAnet implementation.
 *
 * @author Balazs Racz
 * @date 25 Jan 2014
 */

#ifndef _NMRANET_DATAGRAM_HXX_
#define _NMRANET_DATAGRAM_HXX_

#include "utils/NodeHandlerMap.hxx"
#include "utils/Queue.hxx"
#include "nmranet/If.hxx"

namespace nmranet
{

struct IncomingDatagram;

/// Defines how long to wait for a Datagram_OK / Datagram_Rejected message.
extern long long DATAGRAM_RESPONSE_TIMEOUT_NSEC;

// cont
typedef Payload DatagramPayload;

/// Message structure for incoming datagram handlers.
struct IncomingDatagram
{
    /// Originator of the incoming datagram.
    NodeHandle src;
    /// Virtual node that the datagram was addressed to.
    Node *dst;
    /// Owned by the current IncomingDatagram object. Includes the datagram ID
    /// as the first byte.
    DatagramPayload payload;
};

/// Allocator to be used for Buffer<IncomingDatagram> objects.
extern Pool *const g_incoming_datagram_allocator;

/** Base class for datagram handlers.
 *
 * The datagram handler needs to listen to the incoming queue for arriving
 * datagrams. It is okay to derive a datagram handler from DatagramHandlerFlow
 * as well (they ar ecompatible). */
typedef FlowInterface<Buffer<IncomingDatagram>> DatagramHandler;
typedef StateFlow<Buffer<IncomingDatagram>, QList<1>> DatagramHandlerFlow;

/** Use this class to send datagrams */
class DatagramClient : public QMember
{
public:
    virtual ~DatagramClient()
    {
    }

    /** Triggers sending a datagram.
     *
     * @param b is the datagra buffer.
     * @param priority is the priority of the datagram client in the executor.
     *
     * Callers should set the done closure of the Buffer.  After that closure
     * is notified, the caller must ensure that the datagram client is released
     * back to the freelist.
     *
     * @TODO(balazs.racz): revisit the type of DatagramPayload and ensure that
     * there will be no extra copy of the data happening.
     */
    virtual void write_datagram(Buffer<NMRAnetMessage> *b,
                                unsigned priority = UINT_MAX) = 0;

    /** Requests cancelling the datagram send operation. Will notify the done
     * callback when the canceling is completed. */
    virtual void cancel() = 0;

    /** Returns a bitmask of ResultCodes for the transmission operation. */
    uint32_t result()
    {
        return result_;
    }

    /// Known result codes from the DatagramClient. Some of these are
    /// duplicates from the general result codes; others (the ones above 16
    /// bit) are specific to the DatagramClient.
    enum ResultCodes
    {
        PERMANENT_ERROR = 0x1000,
        RESEND_OK = 0x2000,
        // Resend OK errors
        TRANSPORT_ERROR = 0x4000,
        BUFFER_UNAVAILABLE = 0x0020,
        OUT_OF_ORDER = 0x0040,
        // Permanent error error bits
        SOURCE_NOT_PERMITTED = 0x0020,
        DATAGRAMS_NOT_ACCEPTED = 0x0040,

        // Internal error codes generated by the send flow
        OPERATION_SUCCESS = 0x10000, //< set when the Datagram OK arrives
        OPERATION_PENDING = 0x20000, //< cleared when done is called.
        DST_NOT_FOUND = 0x40000,     //< on CAN. Permanent error code.
        TIMEOUT = 0x80000,           //< Timeout waiting for ack/nack.
        DST_REBOOT = 0x100000,       //< Target node has rebooted.

        // The top byte of result_ is the response flags from Datagram_OK
        // response.
        RESPONSE_FLAGS_SHIFT = 24,
        RESPONSE_CODE_MASK = (1<<RESPONSE_FLAGS_SHIFT) - 1,
        OK_REPLY_PENDING = (1 << 31),
    };

    // These values are okay in the respond_ok flags byte.
    enum ResponseFlag
    {
        REPLY_PENDING = 0x80,
        REPLY_TIMEOUT_SEC = 0x1,
        REPLY_TIMEOUT_MASK = 0xf,
    };

protected:
    uint32_t result_;
};

/** Transport-agnostic dispatcher of datagrams.
 *
 * There will be typically one instance of this for each interface with virtual
 * nodes. This class is responsible for maintaining the registered datagram
 * handlers, and taking the datagram MTI from the incoming messages and routing
 * them to the datagra for datagram handlers.
 *
 * The datagram handler needs to listen to the incoming queue for arriving
 * datagrams. */
class DatagramService : public Service
{
public:
    typedef TypedNodeHandlerMap<Node, DatagramHandler> Registry;

    /** Creates a datagram dispatcher.
     *
     * @param interface is the async interface to which to bind.
     * @param num_registry_entries is the size of the registry map (how
     * many datagram handlers can be registered)
     */
    DatagramService(If *iface, size_t num_registry_entries);
    ~DatagramService();

    /// @returns the registry of datagram handlers.
    Registry *registry()
    {
        return dispatcher_.registry();
    }

    /** Datagram clients.
     *
     * Use control flows from this allocator to send datagrams to remote nodes.
     * When the client flow completes, it is the caller's responsibility to
     * return it to this allocator, once the client is done examining the
     * result codes. */
    TypedQAsync<DatagramClient> *client_allocator()
    {
        return &clients_;
    }

    If *iface()
    {
        return iface_;
    }

private:
    /** Class for routing incoming datagram messages to the datagram handlers.
     *
     * Keeps a registry of datagram handlers. Listens to incoming MTI_DATAGRAM
     * messages coming from the If, and routes them to the appropriate datagram
     * handlerflow.
     *
     * The dispatcher is a state flow that is using the If as the base service
     * and not the DatagramService that owns it. */
    class DatagramDispatcher : public IncomingMessageStateFlow
    {
    public:
        DatagramDispatcher(If *iface, size_t num_registry_entries)
            : IncomingMessageStateFlow(iface)
            , registry_(num_registry_entries)
        {
        }

        ~DatagramDispatcher()
        {
        }

        /// @returns the registry of datagram handlers.
        Registry *registry()
        {
            return &registry_;
        }

    private:
        virtual Action entry();

        Action incoming_datagram_allocated();
        Action respond_rejection();

        Buffer<IncomingDatagram> *d_; //< Datagram to send to handler.
        uint16_t resultCode_;         //< Rejection reason

        /// Maintains the registered datagram handlers.
        Registry registry_;

        // TypedAllocator<IncomingMessageHandler> lock_;
    };

    /// Interface on which we are registered.
    If *iface_;

    /// Datagram clients.
    TypedQAsync<DatagramClient> clients_;

    /// Datagram dispatch handler.
    DatagramDispatcher dispatcher_;
};

} // namespace nmranet

#endif // _NMRANET_DATAGRAM_HXX_
