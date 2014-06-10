//
//  NetworkPacket.cpp
//  libraries/networking/src
//
//  Created by Brad Hefta-Gaub on 8/9/13.
//  Copyright 2013 High Fidelity, Inc.
//
//  Distributed under the Apache License, Version 2.0.
//  See the accompanying file LICENSE or http://www.apache.org/licenses/LICENSE-2.0.html
//

#include <cassert>
#include <cstring>
#include <QtDebug>

#include "SharedUtil.h"

#include "NetworkPacket.h"

void NetworkPacket::copyContents(const SharedNodePointer& sendingNode, const QByteArray& packet) {
    if (packet.size() && packet.size() <= MAX_PACKET_SIZE) {
        _sendingNode = sendingNode;
        _byteArray = packet;
    } else {
        qDebug(">>> NetworkPacket::copyContents() unexpected length = %d", packet.size());
    }
}

NetworkPacket::NetworkPacket(const NetworkPacket& packet) {
    copyContents(packet.getSendingNode(), packet.getByteArray());
}

NetworkPacket::NetworkPacket(const SharedNodePointer& sendingNode, const QByteArray& packet) {
    copyContents(sendingNode, packet);
};

// copy assignment 
NetworkPacket& NetworkPacket::operator=(NetworkPacket const& other) {
    copyContents(other.getSendingNode(), other.getByteArray());
    return *this;
}

#ifdef HAS_MOVE_SEMANTICS
// move, same as copy, but other packet won't be used further
NetworkPacket::NetworkPacket(NetworkPacket && packet) {
    copyContents(packet.getSendingNode(), packet.getByteArray());
}

// move assignment
NetworkPacket& NetworkPacket::operator=(NetworkPacket&& other) {
    copyContents(other.getSendingNode(), other.getByteArray());
    return *this;
}
#endif
