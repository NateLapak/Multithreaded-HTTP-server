Server.py was singlethreaded because of this line:
  allow_reuse_address = True

server.py = the HTTP server, speaks HTTP/1.1 by constructing HTTP/1.1 responses
httpclient.py = a program that talks to the HTTP server, speaks HTTP/1.1 by constructing requests and parsing the server's responses.
