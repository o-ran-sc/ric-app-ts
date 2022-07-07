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
    Mnemonic:	restclient.hpp
    Abstract:	Header for the libcurl wrapper.

    Date:       8 Apr 2022
    Author:     Alexandre Huff
*/



#ifndef _CURL_WRAPPER_HPP
#define _CURL_WRAPPER_HPP

#include <curl/curl.h>
#include <string>
#include <stdexcept>

namespace restclient {

typedef struct resp {
    long status_code;
    std::string body;
} response_t;

class RestClient {
    private:
        std::string baseUrl;
        CURL *curl = NULL;
        struct curl_slist *headers = NULL;
        char errbuf[CURL_ERROR_SIZE];

        void init( );

    public:
        RestClient( std::string baseUrl );
        ~RestClient();
        std::string getBaseUrl();
        response_t do_get( std::string path );
        response_t do_post( std::string path, std::string json );
};

class RestClientException : public std::runtime_error {
    public:
        RestClientException( const std::string &error )
            : std::runtime_error{ error.c_str() } { }
};

} // namespace


#endif
