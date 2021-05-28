# ==================================================================================
#	Copyright (c) 2021 AT&T Intellectual Property.
# 	Copyright (c) 2021 Alexandre Huff.
#
#    Licensed under the Apache License, Version 2.0 (the "License");
#    you may not use this file except in compliance with the License.
#    You may obtain a copy of the License at
#
#        http://www.apache.org/licenses/LICENSE-2.0
#
#    Unless required by applicable law or agreed to in writing, software
#    distributed under the License is distributed on an "AS IS" BASIS,
#    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#    See the License for the specific language governing permissions and
#    limitations under the License.
# ==================================================================================
#
#
# 	Mnemonic:	echo-server.py
# 	Abstract:   Implements a naive echo server just for testing REST calls from
#               TS xApp. Its goal is to run an effortless REST server.
#
# 	Date:		22 May 2021
# 	Author:		Alexandre Huff


from flask import Flask, jsonify, request

app = Flask(__name__)

@app.route('/api/echo', methods=['POST'])
def echo():
    data = request.get_json()
    # just returning the received data
    return jsonify(data)

if __name__ == "__main__":
    app.run()
