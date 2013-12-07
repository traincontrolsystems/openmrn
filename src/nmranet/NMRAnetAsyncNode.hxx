/** \copyright
 * Copyright (c) 2013, Balazs Racz
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are  permitted provided that the following conditions are met:
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
 * \file NMRAnetAsyncNode.hxx
 *
 * Node definition for asynchronous NMRAnet nodes.
 *
 * @author Balazs Racz
 * @date 7 December 2013
 */

#ifndef _NMRAnetAsyncNode_hxx_
#define _NMRAnetAsyncNode_hxx_

#include "nmranet/NMRAnetIf.hxx"  // for NodeID

namespace NMRAnet
{

class AsyncIf;

/** Base class for NMRAnet nodes conforming to the asynchronous interface.
 *
 * It is important for this interface to contain no data members, since certain
 * implementations might need to be very lightweight (e.g. a command station
 * might have hundreds of train nodes.)
 */
class AsyncNode
{
public:
    virtual ~AsyncNode() {}
    // @returns the 48-bit NMRAnet node id for this node.
    virtual NodeID node_id() = 0;
    // @returns the interface this virtual node is bound to.
    virtual AsyncIf* interface() = 0;
    /** @returns true if the node is in the initialized state.
     *
     * Nodes not in initialized state may not send traffic to the bus. */
    virtual bool is_initialized() = 0;
};

} // namespace NMRAnet

#endif // _NMRAnetAsyncNode_hxx_
