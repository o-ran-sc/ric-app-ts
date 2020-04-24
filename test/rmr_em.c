// vim: ts=4 sw=4 noet :
/*
==================================================================================
       Copyright (c) 2020 Nokia
       Copyright (c) 2020 AT&T Intellectual Property.

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
==================================================================================
*/

/*
	Mnemonic:	rmr_em.c
	Abstract:	RMR emulation for testing

	Date:		20 March	
	Author:		E. Scott Daniels
*/

#include <unistd.h>
#include <string.h>
#include <malloc.h>


/*
	CAUTION:  this is a copy of what is in RMR and it is needed as some of the framework
	functions access the 'public' fields like state and mtype.
*/
typedef struct {
    int state;                  // state of processing
    int mtype;                  // message type
    int len;                    // length of data in the payload (send or received)
    unsigned char* payload;     // transported data
    unsigned char* xaction;     // pointer to fixed length transaction id bytes
    int sub_id;                 // subscription id
    int     tp_state;           // transport state (errno) valid only if state != RMR_OK, and even then may not be valid

                                // these things are off limits to the user application
    void*   tp_buf;             // underlying transport allocated pointer (e.g. nng message)
    void*   header;             // internal message header (whole buffer: header+payload)
    unsigned char* id;          // if we need an ID in the message separate from the xaction id
    int     flags;              // various MFL_ (private) flags as needed
    int     alloc_len;          // the length of the allocated space (hdr+payload)

    void*   ring;               // ring this buffer should be queued back to
    int     rts_fd;             // SI fd for return to sender

    int     cookie;             // cookie to detect user misuse of free'd msg
} rmr_mbuf_t;

typedef struct {
	char meid[32];
	char src[32];
} header_t;


void* rmr_init( char* port, int flags ) {
	return malloc( sizeof( char ) * 100 );
}

rmr_mbuf_t* rmr_alloc_msg( void* mrc, int payload_len ) {
	rmr_mbuf_t*	mbuf;
	char* p;				// the tp buffer; also the payload
	header_t* h;
	int len;

	len = (sizeof( char ) * payload_len) + sizeof( header_t );
	p = (char *) malloc( len );
	if( p == NULL ) {
		return NULL;
	}
	h = (header_t *) p;
	memset( p, 0, len );

	mbuf = (rmr_mbuf_t *) malloc( sizeof( rmr_mbuf_t ) );
	if( mbuf == NULL ) {
		free( p );
		return NULL;
	}
	memset( mbuf, 0, sizeof( rmr_mbuf_t ) );

	mbuf->tp_buf = p;
	mbuf->payload = (char *)p + sizeof( header_t );


	mbuf->len = 0;
	mbuf->alloc_len = payload_len;
	mbuf->payload = (void *) p;
	strncpy( h->src, "host:ip", 8 );
	strncpy( h->meid, "EniMeini", 9 );

	return mbuf;
}

void rmr_free_msg( rmr_mbuf_t* mbuf ) {
	if( mbuf ) {
		if( mbuf->tp_buf ) {
			free( mbuf->tp_buf );
		}
		free( mbuf );
	}
}

char* rmr_get_meid( rmr_mbuf_t* mbuf, char* m ) {
	header_t* h;

	if( mbuf != NULL ) {
		if( m == NULL ) {
			m = (char *) malloc( sizeof( char ) * 32 );		
		}
		h = (header_t *) mbuf->tp_buf;
		memcpy( m, h->meid, 32 );
	}

	return m;
}

int rmr_payload_size( rmr_mbuf_t* mbuf ) {

	if( mbuf != NULL ) {
		return mbuf->alloc_len;
	}

	return 0;
}

char *rmr_get_src( rmr_mbuf_t* mbuf, char *m ) {
	header_t*  h;

	if( mbuf != NULL ) {
		if( m == NULL ) {
			m = (char *) malloc( sizeof( char ) * 32 );		
		}
		h = (header_t *) mbuf->tp_buf;
		memcpy( m, h->src, 32 );
	}

	return m;
}

int rmr_str2meid( rmr_mbuf_t* mbuf, unsigned char* s ) {
	header_t*  h;

	if( mbuf != NULL ) {
		if( s == NULL ) {
			return 1;
		}

		if( strlen( s ) > 31 ) {
			return 1;
		}

		h = (header_t *) mbuf->tp_buf;
		strncpy( h->meid, s, 32 );
		return 0;
	}

	return 1;
}

rmr_mbuf_t* rmr_send_msg( void* mrc, rmr_mbuf_t* mbuf ) {

	if( mbuf != NULL ) {
		mbuf->state = 0;	
	}
	
	return mbuf;
}

rmr_mbuf_t* rmr_rts_msg( void* mrc, rmr_mbuf_t* mbuf ) {

	if( mbuf != NULL ) {
		mbuf->state = 0;	
	}
	
	return mbuf;
}

rmr_mbuf_t* rmr_realloc_payload( rmr_mbuf_t* mbuf, int payload_len, int copy, int clone ) {             // ensure message is large enough
	rmr_mbuf_t* nmb;
	unsigned char* payload;

	if( mbuf == NULL ) {
		return NULL;
	}

	nmb = rmr_alloc_msg( NULL, payload_len );
	if( copy ) {
		memcpy( nmb->payload, mbuf->payload, mbuf->len );
		nmb->len = mbuf->len;
	} else {
		nmb->len = 0;
	}
	nmb->state = mbuf->state;

	if( ! clone ) {
		free( mbuf );
	}
	return  nmb;
}

void rmr_close( void*  mrc ) {
	return;
}

rmr_mbuf_t* rmr_torcv_msg( void* mrc, rmr_mbuf_t* mbuf, int timeout ) {
	static int max2receive = 500;
	static int mtype = 0;

	if( mbuf == NULL ) {
		mbuf = rmr_alloc_msg( NULL, 2048 );
	}

	if( max2receive <= 0 ) {
		mbuf->state = 12;
		mbuf->len = 0;
		mbuf->mtype = -1;
		mbuf->sub_id = -1;
	}

	max2receive--;

	mbuf->state = 0;
	mbuf->len = 80;
	mbuf->mtype = mtype;
	mbuf->sub_id = -1;

	mtype++;
	if( mtype > 100 ) {
		mtype = 0;
	}

	return mbuf;
}

int rmr_ready( void* mrc ) {
	static int state = 0;

	if( ! state )  {
		state = 1;
		return 0;
	} 

	return 1;
}

