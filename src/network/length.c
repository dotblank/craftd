/*
 * Copyright (c) 2010-2011 Kevin M. Bowling, <kevin.bowling@kev009.com>, USA
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <config.h>

#include <assert.h>
#include <errno.h>
#include <stdlib.h>

#include "network/network.h"
#include "network/network-private.h"
#include "network/packets.h"

/**
 * A utility function for the length state machine to return the correct length
 * or exception value.
 * 
 * @param inlen length of the packet received so far
 * @param totalsize size of the entire packet
 * @return totalsize on good read, -EAGAIN on incomplete data, else -EILSEQ
 */
static inline int
len_returncode(int inlen, int totalsize)
{
  if (inlen >= totalsize)
    return totalsize;
  else if (inlen < totalsize)
    return -EAGAIN;
  else
    return -EILSEQ;
}

/** 
 * Get and return length of full packet.
 * Otherwise EAGAIN EILSEQ on exception (need more data or bad input)
 * NOTE that we negate the error values since the positives could be valid len
 * e.g. return -EAGAIN;  it must be negated again if < 0 and used as an errno
 *
 * @remarks
 * A potential for optimization is to send in the bufferevent instead and use a
 * setwatermark() so we only run the machine when the current variable amount of
 * data has arrived.  The complexity with this is then resetting the watermark
 * in the packet router.
 *
 * @param pkttype packet type byte
 * @param input input evbuffer
 * @return total length or -EAGAIN, -EILSEQ on exception
*/
int
len_statemachine(uint8_t pkttype, struct evbuffer* input)
{
    size_t inlen;
    inlen = evbuffer_get_length(input);

    switch (pkttype)
    {
    case PID_KEEPALIVE: // Keepalive packet 0x00
    {
        return len_returncode(inlen, packet_keepalivesz);
    }
    case PID_LOGIN: // Login packet 0x01
    {
      struct evbuffer_ptr ptr;
      uint16_t ulen;  // Size of the username string
      uint16_t plen; // Size of the password string
      int totalsize;
      int status;

      if(evbuffer_ptr_set(input, &ptr, packet_loginsz.str1offset, 
		EVBUFFER_PTR_SET) != 0)
	return -EAGAIN;
      
      status = CRAFTD_evbuffer_copyout_from(input, &ulen, sizeof(ulen), &ptr);
      if (status != 0)
	return -status;
      ulen = ntohs(ulen);

      if(evbuffer_ptr_set(input, &ptr, packet_loginsz.str2offset + ulen, 
		EVBUFFER_PTR_SET) != 0)
	return -EAGAIN;
	
      status = CRAFTD_evbuffer_copyout_from(input, &plen, sizeof(plen), &ptr);
      if (status != 0)
	return -status;
      plen = ntohs(plen);

      // Packet type + 2(strlen + varstring) + etc
      totalsize = packet_loginsz.base + ulen + plen;
      return len_returncode(inlen, totalsize);
    }
    case PID_HANDSHAKE: // Handshake packet 0x02
    {
      struct evbuffer_ptr ptr;
      uint16_t ulen;
      int totalsize;
      int status;

	if(evbuffer_ptr_set(input, &ptr, packet_handshakesz.str1offset, 
		  EVBUFFER_PTR_SET) != 0)
	  return -EAGAIN;
	
      status = CRAFTD_evbuffer_copyout_from(input, &ulen, sizeof(ulen), &ptr);
      if (status != 0)
	return -status;

      ulen = ntohs(ulen);

      // Packet type + short string len + string
      totalsize = packet_handshakesz.base + ulen;
      return len_returncode(inlen, totalsize);
    }
    case PID_CHAT: // Chat packet 0x03
    {
        struct evbuffer_ptr ptr;
	uint16_t mlen;
	int totalsize;
	int status;
	
	if(evbuffer_ptr_set(input, &ptr, packet_chatsz.str1offset, 
		  EVBUFFER_PTR_SET) != 0)
	  return -EAGAIN;
	
	status = CRAFTD_evbuffer_copyout_from(input, &mlen, sizeof(mlen), &ptr);
	if (status != 0)
	  return -status;
	
	mlen = ntohs(mlen);
	
	totalsize = packet_chatsz.base + mlen;
	return len_returncode(inlen, totalsize);
    }
    case PID_TIMEUPDATE: // Update client time packet 0x04
    {
      return len_returncode(inlen, packet_timesz);
    }
    case PID_ENTITYEQUIPMENT: // Entity equipment packet 0x05
    {
      return len_returncode(inlen, packet_entityequipmentsz);
    }
    case PID_SPAWNPOS: // Spawn position packet 0x06
    {
        return len_returncode(inlen,packet_spawnpossz);
    }
    case PID_USEENTITY: // Use entity packet 0x07
    {
	return len_returncode(inlen, packet_useentitysz);
    }
    case PID_PLAYERHEALTH: // Health update packet 0x08
    {
      return len_returncode(inlen, packet_playerhealthsz);
    }
    case PID_RESPAWN: // Repawn packet 0x09
    {
	return len_returncode(inlen, packet_respawnsz);
    }
    case PID_PLAYERFLY: // "Flying"/Player packet 0x0A
    {
        return len_returncode(inlen, packet_playerflysz);
    }
    case PID_PLAYERPOS: // Player position packet 0x0B
    {
        return len_returncode(inlen, packet_playerpossz);
    }
    case PID_PLAYERLOOK: // Player look packet 0x0C
    {
        return len_returncode(inlen, packet_looksz);
    }
    case PID_PLAYERMOVELOOK: // Player move+look packet 0x0D
    {
	return len_returncode(inlen, packet_movelooksz);
    }
    case PID_PLAYERDIG: // Block dig packet 0x0E
    {
        return len_returncode(inlen, packet_digsz);
    }
    case PID_BLOCKPLACE: // Place packet 0x0F
    {
      struct evbuffer_ptr ptr;
      int status;
      int16_t itemid;

      if(evbuffer_ptr_set(input, &ptr, packet_blockplacesz.itemidoffset, 
		EVBUFFER_PTR_SET) != 0)
	return -EAGAIN;
      
      status = CRAFTD_evbuffer_copyout_from(input, &itemid, sizeof(itemid), 
                                            &ptr);
      
      if ( status != 0)
        return -status;

      itemid = ntohs(itemid);
      if (itemid >= 0)
        return len_returncode(inlen, packet_blockplacesz.place);
      else if (itemid == -1)
        return len_returncode(inlen, packet_blockplacesz.emptyplace);
      else
	return -EILSEQ;
    }
    case PID_HOLDCHANGE: // Block/item switch packet 0x10
    {
        return len_returncode(inlen, packet_holdchangesz);
    }
    case PID_PACKET11: //unknown packet 0x11
    {
	return len_returncode(inlen, packet_11sz);
    }
    case PID_ARMANIMATE: // Arm animate 0x12
    {
        return len_returncode(inlen, packet_armanimatesz);
    }
    case PID_ENTITYACTION: // Entity action (crouch) 0x13
    {
	return len_returncode(inlen, packet_entityactionsz);
    }
    case PID_NAMEDENTITYSPAWN: // Entity spawn packet (named) 0x14
    {
	struct evbuffer_ptr ptr;
	uint16_t mlen;
	int totalsize;
	int status;
	
	if(evbuffer_ptr_set(input, &ptr, packet_namedentityspawnsz.str1offset, 
		  EVBUFFER_PTR_SET) != 0)
	  return -EAGAIN;
	
	status = CRAFTD_evbuffer_copyout_from(input, &mlen, sizeof(mlen), &ptr);
	if (status != 0)
	  return -status;
	
	mlen = ntohs(mlen);
	
	totalsize = packet_namedentityspawnsz.base + mlen;
	return len_returncode(inlen, totalsize);
    }
    case PID_PICKUPSPAWN: // pickup item spawn packet 0x15
    {
        return len_returncode(inlen, packet_pickupspawnsz);
    }
    case PID_COLLECTITEM: // Collect Item packet 0x16
    {
	return len_returncode(inlen,packet_collectitemsz);
    }
    case PID_SPAWNOBJECT: // Spawn objects (ex minecarts) 0x17
    {
        return len_returncode(inlen, packet_spawnobjectsz);
    }
    case PID_SPAWNMOB: // Spawn mob 0x18
    {
      	struct evbuffer_ptr ptr;
	uint8_t byte = 0;
	int status;
	

	int size = 1;
	do //this loop should always run at least once
	{
	  if(evbuffer_ptr_set(input, &ptr, packet_spawnmobszbase+size, 
		    EVBUFFER_PTR_SET) != 0)
	    return -EAGAIN;
	  //evbuffer_enable_locking(b, NULL);
	  //evbuffer_lock(b);
	  /* TODO: check locking semantics wrt multiple threads */ 
	  status = CRAFTD_evbuffer_copyout_from(input, &byte, sizeof(MCbyte), &ptr);
	  //evbuffer_unlock(b);
	  if (status != 0)
	    return -status;
	  LOG(LOG_DEBUG,"Decoding metadata %d : %d", size, byte);
	  size++;
	}
        while(byte != 127);
	
        return len_returncode(inlen, packet_spawnmobszbase + size);
    }
    case PID_PAINTING: // Entity painting 0x19
    {
	struct evbuffer_ptr ptr;
	uint16_t mlen;
	int totalsize;
	int status;
	
	if(evbuffer_ptr_set(input, &ptr, packet_paintingsz.str1offset, 
		  EVBUFFER_PTR_SET) != 0)
	  return -EAGAIN;
	
	status = CRAFTD_evbuffer_copyout_from(input, &mlen, sizeof(mlen), &ptr);
	if (status != 0)
	  return -status;
	
	mlen = ntohs(mlen);
	
	totalsize = packet_paintingsz.base + mlen;
	return len_returncode(inlen, totalsize);
    }
    case PID_PACKET1B: //Unknown packet ox1B
    {
	return len_returncode(inlen,packet_1Bsz);
    }
    case PID_ENTITYVELOCITY: // 0x1C
    {
	return len_returncode(inlen, packet_entityvelocitysz);
    }
    case PID_ENTITYDESTROY: // 0x1D
    {
        return len_returncode(inlen, packet_entitydestroysz);
    }
    case PID_ENTITYINIT: // 0x1E
    {
	return len_returncode(inlen, packet_entityinitsz);
    }
    case PID_ENTITYRELMOVE: // 0x1F
    {
	return len_returncode(inlen, packet_entityrelmovesz);
    }
    case PID_ENTITYLOOK: // 0x20
    {
	return len_returncode(inlen, packet_entitylooksz);
    }
    case PID_ENTITYLOOKMOVE: // 0x21
    {
	return len_returncode(inlen, packet_entitylookmovesz);
    }
    case PID_ENTITYPOS: // 0x22
    {
	return len_returncode(inlen, packet_entitypossz);
    }
    case PID_ENTITYSTATUS: // 0x26
    {
	return len_returncode(inlen, packet_entitystatussz);
    }
    case PID_ENTITYATTACH: // 0x27
    {
	return len_returncode(inlen, packet_entityattachsz);
    }
    case PID_MULTIBLOCKCHANGE: // 0x34
    {
        struct evbuffer_ptr ptr;
	int16_t asize;
	int totalsize;
	int status;
	
	if(evbuffer_ptr_set(input, &ptr, packet_multiblockchangeszbase, 
		  EVBUFFER_PTR_SET) != 0)
	  return -EAGAIN;
	
	status = CRAFTD_evbuffer_copyout_from(input, &asize, sizeof(asize), &ptr);
	if (status != 0)
	  return -status;
	
	asize = ntohs(asize);
	
	totalsize = packet_multiblockchangeszbase + sizeof(asize) 
	          + (asize*(sizeof(MCshort)+2*sizeof(MCbyte)));
	return len_returncode(inlen, totalsize);
    }
    case PID_BLOCKCHANGE: // 0x35
    {
	return len_returncode(inlen,packet_blockchangesz);
    }
    case PID_ENTITYMETA: // 0x28
    {
      	struct evbuffer_ptr ptr;
	uint8_t byte = 0;
	int status;	

	int size = 0;
	do //this loop should always run at least once
	{
	  if(evbuffer_ptr_set(input, &ptr, packet_entitymetaszbase+size, 
		    EVBUFFER_PTR_SET) != 0)
	    return -EAGAIN;
	  //evbuffer_enable_locking(b, NULL);
	  //evbuffer_lock(b);
	  /* TODO: check locking semantics wrt multiple threads */ 
	  status = CRAFTD_evbuffer_copyout_from(input, &byte, sizeof(byte), &ptr);
	  //LOG(LOG_DEBUG,"decode meta data byte: %d",byte);
	  //evbuffer_unlock(b);
	  if (status != 0)
	    return -status;
	  size++; // even increment after an 0xf7 byte 
	}
        while(byte != 127);
	
	return len_returncode(inlen, packet_entitymetaszbase + size);
    }
    case PID_PRECHUNK: // Prechunk packet 0x32
    {
        return len_returncode(inlen,packet_prechunksz);
    }
    case PID_MAPCHUNK: // Map chunk 0x33
    {
        struct evbuffer_ptr ptr;
	MCint mlen;
	int totalsize;
	int status;
	
	evbuffer_ptr_set(input, &ptr, packet_mapchunksz.sizelocation, 
			 EVBUFFER_PTR_SET);
	
	status = CRAFTD_evbuffer_copyout_from(input, &mlen, sizeof(mlen), &ptr);
	if (status != 0)
	  return -status;
	
	mlen = ntohl(mlen);
	
	totalsize = packet_mapchunksz.base + mlen;
	return len_returncode(inlen, totalsize);	
    }
    case PID_OPENWINDOW: // Open window 0x64
    {
	struct evbuffer_ptr ptr;
	uint16_t mlen;
	int totalsize;
	int status;
	
	if(evbuffer_ptr_set(input, &ptr, packet_openwindowsz.str1offset, 
		  EVBUFFER_PTR_SET) != 0)
	  return -EAGAIN;
	
	status = CRAFTD_evbuffer_copyout_from(input, &mlen, sizeof(mlen), &ptr);
	if (status != 0)
	  return -status;
	
	mlen = ntohs(mlen);
	
	totalsize = packet_openwindowsz.base + mlen;
	return len_returncode(inlen, totalsize);
    }
    case PID_CLOSEWINDOW: // Close window 0x65
    {
        return len_returncode(inlen, packet_closewindowsz);
    }
    case PID_WINDOWCLICK: // Window click 0x66
    {
      struct evbuffer_ptr ptr;
      int status;
      int16_t itemid;

      if(evbuffer_ptr_set(input, &ptr, packet_windowclicksz.itemidoffset, 
		EVBUFFER_PTR_SET) != 0)
	return -EAGAIN;

      status = CRAFTD_evbuffer_copyout_from(input, &itemid, sizeof(itemid), 
                                            &ptr);
      if ( status != 0)
        return -status;

      itemid = ntohs(itemid);
      if (itemid == -1)
        return len_returncode(inlen, packet_windowclicksz.clicknull);
      else
        return len_returncode(inlen, packet_windowclicksz.click);
    }
    case PID_SETSLOT: // 0x67
    {
	struct evbuffer_ptr ptr;
	int status;
	MCshort itemid;
	
	evbuffer_ptr_set(input,&ptr,packet_setslotsz.itemidoffset,
			 EVBUFFER_PTR_SET);
	status =  CRAFTD_evbuffer_copyout_from(input,&itemid,sizeof(itemid),
					       &ptr);
	if(status != 0)
	  return -status;
	if(itemid == -1)
	  return len_returncode(inlen,packet_setslotsz.itemnull);
	else
	  return len_returncode(inlen,packet_setslotsz.item);
    }
    case PID_WINDOWITEMS:
    {
      	struct evbuffer_ptr ptr;
	int status;
	MCshort itemcountraw;
	MCshort itemcount;
	MCshort itemid;
	 
	if(evbuffer_ptr_set(input, &ptr, packet_windowitemssz.itemcountoffset, 
			    EVBUFFER_PTR_SET) != 0)
	  return -EAGAIN;
	
	status =  CRAFTD_evbuffer_copyout_from(input, &itemcountraw, 
                                               sizeof(itemcountraw), &ptr);
	
	if(status != 0)
	  return -status;
	itemcount = ntohs(itemcountraw);
	LOG(LOG_DEBUG,"Number of window items: %d", itemcount);
	
	if(itemcount == 0)
	  return len_returncode(inlen, packet_windowitemssz.itemcountoffset +
				sizeof(itemcount));
	
        int runningsize = 0; // running size total
	for(int i = 0; i < itemcount; i++)
	{
	  if(evbuffer_ptr_set(input, &ptr, packet_windowitemssz.itemcountoffset 
                + 2 + runningsize, EVBUFFER_PTR_SET) != 0)
	    return -EAGAIN;

	  status =  CRAFTD_evbuffer_copyout_from(input,&itemid,sizeof(itemid),
					       &ptr);
	  if(status != 0)
	    return -status;
	  
	  if(itemid == -1) // add only 2 bytes (MCshort) to total size if itemid is -1
	    runningsize += packet_windowitemssz.itemnullsize; 
	  else
	    runningsize += packet_windowitemssz.itemsize;
	}
	return len_returncode(inlen, packet_windowitemssz.itemcountoffset
                              + runningsize + 2);
    }
    case PID_UPDATEPROGBAR: // 0x69
    {
	return len_returncode(inlen,packet_updateprogbarsz);
    }
    case PID_TRANSACTION: // 0x6A
    {
	return len_returncode(inlen,packet_transactionsz);
    }
    case PID_UPDATESIGN: // 0x82
    {
	struct evbuffer_ptr ptr;
	uint16_t len1;  // Size of the line 1 string
	uint16_t len2;  // Size of the line 2 string
	uint16_t len3;  // Size of the line 3 string
	uint16_t len4;  // Size of the line 4 string
	int totalsize;
	int status;

	if(evbuffer_ptr_set(input, &ptr, packet_updatesignsz.str1offset, 
                            EVBUFFER_PTR_SET) != 0)
	  return -EAGAIN;
	
	status = CRAFTD_evbuffer_copyout_from(input, &len1, sizeof(len1), &ptr);
	if (status != 0)
	  return -status;
	len1 = ntohs(len1);

	if(evbuffer_ptr_set(input, &ptr, packet_updatesignsz.str2offset + len1, 
		            EVBUFFER_PTR_SET) != 0)
	  return -EAGAIN;
	
	status = CRAFTD_evbuffer_copyout_from(input, &len2, sizeof(len2), &ptr);
	if (status != 0)
	  return -status;
	len2 = ntohs(len2);
	
	if(evbuffer_ptr_set(input, &ptr, packet_updatesignsz.str3offset + len1 + len2,
			    EVBUFFER_PTR_SET) != 0)
	  return -EAGAIN;
	
	status = CRAFTD_evbuffer_copyout_from(input, &len3, sizeof(len3), &ptr);
	if (status != 0)
	  return -status;
	len3 = ntohs(len3);
	
	if(evbuffer_ptr_set(input, &ptr, packet_updatesignsz.str4offset+len1+len2+len3, 
			    EVBUFFER_PTR_SET) != 0)
	  return -EAGAIN;
	
	status = CRAFTD_evbuffer_copyout_from(input, &len4, sizeof(len4), &ptr);
	if (status != 0)
	  return -status;
	len4 = ntohs(len4);	

	// Packet type + 2(strlen + varstring) + etc
	totalsize = packet_updatesignsz.base + len1 + len2 + len3 + len4;
	return len_returncode(inlen, totalsize);
    }
    case PID_DISCONNECT: // Disconnect packet 0xFF
    {
        struct evbuffer_ptr ptr;
	uint16_t mlen;
	int totalsize;
	int status;
	
	if(evbuffer_ptr_set(input, &ptr, packet_disconnectsz.str1offset, 
		  EVBUFFER_PTR_SET) != 0)
	  return -EAGAIN;
	
	status = CRAFTD_evbuffer_copyout_from(input, &mlen, sizeof(mlen), &ptr);
	if (status != 0)
	  return -status;
	
	mlen = ntohs(mlen);
	
	totalsize = packet_disconnectsz.base + mlen;
	return len_returncode(inlen, totalsize);
    }
    default:
    {
        LOG(LOG_ERR, "Unknown packet type: %x, len: %d\n!", pkttype, 
	    (int) inlen);
        // Close connection
        return -EILSEQ;
    }
    }
    
    /* We should never get here; a coding error occurred if so */ 
    bool unterminated_length_case = false;
    assert(unterminated_length_case);
}
