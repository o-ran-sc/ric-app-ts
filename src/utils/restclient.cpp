// vi: ts=4 sw=4 noet:
/*
==================================================================================
    Copyright (c) 2022 Alexandre Huff

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
    Mnemonic:	restclient.cpp
    Abstract:	Implements a tiny wrapper on top of libcurl to handle with rest requests.

    Date:       8 Apr 2022
    Author:     Alexandre Huff
*/


#include "restclient.hpp"
#include <mutex>
#include <string.h>
#include <memory>

namespace restclient {

// callback to handle http responses
static size_t http_response_callback( const char *in, size_t size, size_t num, std::string *out ) {
    if( out == NULL ) {
        return 0;
    }
    const size_t totalBytes( size * num );
    out->append( in, totalBytes );
    return totalBytes;
}

/*
    Create a RestClient instance to exchange messages with
    a given rest api available on baseUrl, which consists of
    scheme://domain[:port]
*/
RestClient::RestClient( std::string baseUrl ) {
    this->baseUrl = baseUrl;

    if( ! init() ) {
         fprintf( stderr, "unable to initialize RestClient\n" );
    }
}

RestClient::~RestClient( ) {
    curl_slist_free_all( headers );
    curl_easy_cleanup( curl );
}

std::string RestClient::getBaseUrl( ) {
    return baseUrl;
}

bool RestClient::init( ) {
    static std::mutex curl_mutex;

    {   // scoped mutex to make curl_global_init thread-safe
        const std::lock_guard<std::mutex> lock( curl_mutex );
        curl_global_init( CURL_GLOBAL_DEFAULT );
    }

    curl = curl_easy_init();
    if( curl == NULL ) {
        return false;
    }

    // curl_easy_setopt( curl, CURLOPT_VERBOSE, 1L );
    curl_easy_setopt( curl, CURLOPT_TIMEOUT, 5 );

    /* provide a buffer to store errors in */
    if( curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf) != CURLE_OK ) {
        fprintf( stderr, "unable to set CURLOPT_ERRORBUFFER\n" );
        return false;
    }
    errbuf[0] = 0;

    if( curl_easy_setopt( curl, CURLOPT_WRITEFUNCTION, http_response_callback ) != CURLE_OK ) {
        fprintf( stderr, "unable to set CURLOPT_WRITEFUNCTION\n" );
        return false;
    }

    if( curl_easy_setopt( curl, CURLOPT_WRITEDATA, &response.body ) != CURLE_OK ) {
        fprintf( stderr, "unable to set CURLOPT_WRITEDATA\n" );
        return false;
    }

    headers = curl_slist_append( headers, "Accept: application/json" );
    headers = curl_slist_append( headers, "Content-Type: application/json" );
    if( curl_easy_setopt( curl, CURLOPT_HTTPHEADER, headers ) != CURLE_OK ) {
        fprintf( stderr, "unable to set CURLOPT_HTTPHEADER\n" );
        return false;
    }

    return true;
}

/*
    Executes a GET request at the path of this RestClient instance.
    Returns the HTTP status code and the correspoding message body.
*/
response_t RestClient::do_get( std::string path ) {
    response = { 0, "" };

    const std::string endpoint = baseUrl + path;

    if( curl_easy_setopt( curl, CURLOPT_URL, endpoint.c_str() ) != CURLE_OK ) {
        fprintf( stderr, "unable to set CURLOPT_URL\n" );
        return response;
    }
    if( curl_easy_setopt( curl, CURLOPT_HTTPGET, 1L ) != CURLE_OK ) {
        fprintf( stderr, "unable to set CURLOPT_HTTPGET\n" );
        return response;
    }

    CURLcode res = curl_easy_perform( curl );
    if( res == CURLE_OK ) {
        if( curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &response.status_code ) != CURLE_OK ) {
            fprintf( stderr, "unable to get CURLINFO_RESPONSE_CODE\n" );
        }
    } else {
        size_t len = strlen( errbuf );
        fprintf( stderr, "unable to complete the request url=%s ", endpoint.c_str() );
        if(len)
            fprintf( stderr, "error=%s%s", errbuf,
                    ( ( errbuf[len - 1] != '\n' ) ? "\n" : "") );
        else
            fprintf( stderr, "error=%s\n", curl_easy_strerror( res ) );
    }

    return response;
}

/*
    Executes a POST request of a json message at the path of this RestClient instance.
    Returns the HTTP status code and the correspoding message body.
*/
response_t RestClient::do_post( std::string path, std::string json ) {
    response = { 0, "" };

    const std::string endpoint = baseUrl + path;

    if( curl_easy_setopt( curl, CURLOPT_URL, endpoint.c_str() ) != CURLE_OK ) {
        fprintf( stderr, "unable to set CURLOPT_URL\n" );
        return response;
    }
    if( curl_easy_setopt( curl, CURLOPT_POST, 1L ) != CURLE_OK ) {
        fprintf( stderr, "unable to set CURLOPT_POST\n" );
        return response;
    }
    if( curl_easy_setopt( curl, CURLOPT_POSTFIELDS, json.c_str() ) != CURLE_OK ) {
        fprintf( stderr, "unable to set CURLOPT_POSTFIELDS\n" );
        return response;
    }

    CURLcode res = curl_easy_perform( curl );
    if( res == CURLE_OK ) {
        if( curl_easy_getinfo( curl, CURLINFO_RESPONSE_CODE, &response.status_code ) != CURLE_OK ) {
            fprintf( stderr, "unable to get CURLINFO_RESPONSE_CODE\n" );
        }
    } else {
        size_t len = strlen( errbuf );
        fprintf( stderr, "unable to complete the request url=%s ", endpoint.c_str() );
        if(len)
            fprintf( stderr, "error=%s%s", errbuf,
                    ( (errbuf[len - 1] != '\n' ) ? "\n" : "") );
        else
            fprintf( stderr, "error=%s\n", curl_easy_strerror( res ) );
    }

    return response;
}

} // namespace
