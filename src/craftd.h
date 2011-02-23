#ifndef CRAFTD_CRAFTD_H
#define CRAFTD_CRAFTD_H

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

#include <pthread.h>
#include <sys/queue.h>

#include <event2/event.h>
#include <event2/dns.h>
#include <bstrlib.h>

#include "algos/set.h"
#include "craftd-config.h"


/* Public data */
/**
 * Declare a global variable to determine if the 
 * service is running as proxy or game
 */
enum ServiceType {
  GAME,
  PROXY
};
enum ServiceType MODE;


/**
 * Declare the player list as a singly-linked list 
 */
pthread_rwlock_t PL_rwlock;
int PL_count;
SLIST_HEAD(PL_slisthead, PL_entry) PL_head;
struct event_base* base;
struct evdns_base* dns_base;

/**
 * This is a player entry in the singly-linked list
 * TODO: move this into a player.h
 */

struct PL_entry
{
  pthread_rwlock_t rwlock;
  evutil_socket_t fd;
  struct bufferevent *bev;
  pthread_rwlock_t inlock;
  pthread_rwlock_t outlock;
#ifdef USE_CDPROXY
  pthread_rwlock_t sevinlock;
  pthread_rwlock_t sevoutlock;
  struct bufferevent *sev;
#endif
  char ip[128];
  //uint64_t mapseed;
  //uint8_t dimension;
  int eid;
  bstring username;
  struct
  {
    pthread_rwlock_t rwlock;
    double x;
    double y;
    double z;
    float yaw;
    float pitch;
  } position;
  Set_T loadedchunks; // The working set of chunks that we have sent
    
  SLIST_ENTRY(PL_entry) PL_entries; // Pointer to the next player entry
};

/**
 * The worker pool condition variable notifies available workers
 * The cv mutex protects the shared data access
 */
pthread_cond_t worker_cv;
pthread_mutex_t worker_cvmutex;

/**
 * Declare a Work Queue as a singly-linked tail queue for player work reqs
 * Uses the worker_cv and worker_cvmutex for syncronization
 */
STAILQ_HEAD(WQ_stailqhead, WQ_entry) WQ_head;


/**
 * This is a work queue entry in the singly-linked tail queue
 * 
 * 	GAME: Client to Server
 * 	PROXY: Server to clieny
 * WQ_*_INPUT is a work request that gets called on network input
 * 	Depending on packet type, packets may skip the process stage
 * 	and get populated into an tempory buffer before network output
 * 
 * WQ_OUTPUT is a work request that gets created on packet creation
 *       to write to the network
 * 
 * WQ_PROCESS is a work request to handle various functions like 
 *       Chunk loading and unloading and handling packet data
 *       Some values are null for this type of WQ (lock)
 */
enum WQ_type {WQ_PROXY_INPUT,
	      WQ_GAME_INPUT,
	      WQ_PROCESS,
	      WQ_OUTPUT};

struct WQ_process_data
{
  void *packet;
  size_t pktlen;
  uint8_t pkttype;
};
struct WQ_entry
{
  struct bufferevent *bev;
  struct PL_entry *player;
  enum WQ_type worktype;
  pthread_rwlock_t *lock;
  void *workdata;
  STAILQ_ENTRY(WQ_entry) WQ_entries; // Pointer to next work item
};

/* Public methods */
void errorcb(struct bufferevent *bev, short error, void *ctx);
void craftd_version(const char *progname);

#endif
