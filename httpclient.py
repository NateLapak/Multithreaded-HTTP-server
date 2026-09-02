from sys import argv
import socket

'''
This file implements a custom HTTP/1.1 client that establishes a TCP connection to an HTTP server, 
constructs and sends GET/POST requests, receives the server's HTTP responses, and parses the response status code and body.

'''


def help():
    print("httpclient.py [GET/POST] [URL] [key1] [value1] [key2] [value2] ...\n")

# Defines the structure of an
class HTTPResponse:
    def __init__(self, code=200, body=""):
        self.code = code
        self.body = body

class HTTPClient:
    
    def connect(self, host, port):

        # This approach creates a temporary socket and closes once the with block ends
        # with socket.socket(socket.AF_INET,socket.SOCK_STREAM) as sock:
        #     sock.connect((HOST,PORT))
        #     print(f"connected to server at host {HOST} and port {PORT}")

        # This approach creates a persistent socket and the socket closes once socket.close() is ran.
        # This approach only works for IPv4. Thus, I need to implement a socket that takes in IPv4 and IPv6
        # self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        # self.socket.connect((host, int(port)))
        # print(f"Connected to server at host {host} and port {port}")

        # Since IPv4 and IPv6 uses different address families, we must use a different socket structure for both
        # Check if host is IPv6 (enclosed in brackets or contains ":")
        if ":" in host:

            # Remove brackets if present
            host = host.strip("[]")
            self.socket = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)

        # Otherwise, IPv4 is used
        else:
            self.socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        # Connect to server
        self.socket.connect((host, int(port)))
        print(f"Connected to server at host {host} and port {port}")

    # Get and parse the code part after getting the response from the socket
    def get_code(self, data):

        # The first line looks like: HTTP/1.1 200 OK
        first_line = data.split("\r\n", 1)[0]
        parts = first_line.split(" ")
        if len(parts) >= 2:
            return int(parts[1])  
        return None

    # Get and parse the headers part after getting the response from the socket
    def get_headers(self,data):
        
        # Split headers from body at the first double CRLF
        header_text = data.split("\r\n\r\n", 1)[0]
        header_lines = header_text.split("\r\n")[1:]  # skip status line
        headers = {}
        for line in header_lines:
            if ": " in line:
                key, value = line.split(": ", 1)
                headers[key] = value
        return headers

    # Get and parse the headers body after getting the response from the socket.
    def get_body(self, data):
        
        # Everything after the first double CRLF is the body
        parts = data.split("\r\n\r\n", 1)
        if len(parts) > 1:
            return parts[1]
        return ""
    
    def sendall(self, data):
        self.socket.sendall(data.encode('utf-8'))
    
    # Close socket connection to server
    def close(self):
        self.socket.close()

    # Receive the response (code taken from lab instructions)
    def read_response(self):
        response = b""
        with self.socket.makefile('rb') as sock_file:  
            response = sock_file.read()
        return response

    '''
        Handles GET request
        I used ChatGPT to structure and understand what I was doing. I asked the LLM the prompt: What do I put in this GET function after parsing the URL
    '''
    def GET(self, url, args=None):

        # Parse URL and save the components of URL as into seperate variables
        host, port, path = self.parseURL(url)

        # Percent encode path
        path = self.percent_encodePATH(path)

        # Add queries to path
        if args:
            path = path + "?" + self.encode_queries(args)

        if path == "":
            path = "/"

        self.connect(host, port)

        # Build GET request using the following format
        '''
            GET {path} HTTP/1.1
            Host: {host}
            User-Agent: CustomClient/1.0
            Connection: close
            Accept: text/html or text/css
        '''

        get_req = (
            f"GET {path} HTTP/1.1\r\n"
            f"Host: {host}\r\n"
            f"User-Agent: CustomClient/1.0\r\n"
            f"Connection: close\r\n"
            f"\r\n"
        )
        
        # Call send all function to send get_req
        self.sendall(get_req)

        # Read response from socket
        # If you try passing it without decoding, you get this error: TypeError: a bytes-like object is required, not 'str'
        res_bytes = self.read_response() 
        res = res_bytes.decode("utf-8", errors="ignore") 
        
        # Parse code and body from res
        code = self.get_code(res)
        body = self.get_body(res)

        # Close socket
        self.close()

        return HTTPResponse(code, body)


    '''
        Handles POST request
        I used ChatGPT to structure and understand what I was doing. I asked the LLM the prompt: Is the POST function similar to the GET function?
    '''
    def POST(self, url, args=None):

        # Parse URL and save the components of URL as into seperate variables
        host, port, path = self.parseURL(url)

        # Percent encode path and body
        path = self.percent_encodePATH(path)


        # Open socket
        self.connect(host, port)

        # Build HTTP Post body in the format expected by application/x-www-form-urlencoded
        body = self.encode_form_data(args)
        body_bytes = body.encode("utf-8")
        
        # Build POST request using the HTTP post format, and encode it

        '''
            POST /submit HTTP/1.1
            Host: example.com
            Content-Type: application/x-www-form-urlencoded
            Content-Length: 23
            Connection: close
            Body
        '''

        post_req = (
            f"POST {path} HTTP/1.1\r\n"
            f"Host: {host}\r\n"
            f"Content-Type: application/x-www-form-urlencoded\r\n"
            f"Content-Length: {len(body_bytes)}\r\n"
            f"Connection: close\r\n"
            f"\r\n"
            f"{body}"
        )

        # Send request
        self.sendall(post_req)

        # Read response from socket
        # If you try passing it without decoding, you get this error: TypeError: a bytes-like object is required, not 'str'
        res_bytes = self.read_response() 
        res = res_bytes.decode("utf-8", errors="ignore") 

        # Parse code and body
        code = self.get_code(res)
        body = self.get_body(res)

        # Close socket
        self.close()

        return HTTPResponse(code, body)

    '''
        Parses URL into it's seperate components: Scheme, host, port, path
        Function created by Nathan Lapak
    '''
    def parseURL(self, url):

        # URL must start with http://
        if url.startswith("http://"):
            url = url[7:]
        else:
            raise ValueError("URL must start with http://")

        host = ""

        # Host is IPv6
        if url[0] == "[":
            for i in range(1, len(url)):
                if url[i] == "]":
                    url = url[i + 1:]
                    break
                host += url[i]

        # Host is IPv4
        elif url[0].isdigit():
            for i in range(len(url)):
                if url[i] == ":" or url[i] == "/":
                    url = url[i:]
                    break
                host += url[i]

        # Host is a domain
        else:
            for i in range(len(url)):
                if url[i] == ":" or url[i] == "/":
                    url = url[i:]
                    break
                host += url[i]

        # Get port number if present
        port = ""
        if url.startswith(":"):
            url = url[1:]  # skip ":"
            for i in range(len(url)):
                if not url[i].isdigit():
                    url = url[i:]  # remainder is path
                    break
                port += url[i]
        if port == "":
            port = "80"

        # Get path
        path = "/"
        if url.startswith("/"):
            path = url  # everything remaining is the path

        # Return host, port, path
        return host, port, path
    
    #  Percent-encode arguments for POST requests as application/x-www-form-urlencoded body.
    def encode_form_data(self, args = None):

        if not args:
            return ""
        
        pairs = []
        for key, value in args.items():

            # Percent encode key and value
            encode_key = self.percent_encodeBODY(key)
            encode_value = self.percent_encodeBODY(value)
            pairs.append(f"{encode_key}={encode_value}")

        return "&".join(pairs)
    
    # Percent encode all GET requests 
    def percent_encodePATH(self, s):
        chars_allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~/"
        encoded_str = ""
        for char in s:
            if char in chars_allowed:
                encoded_str += char

            else:
                # Convers a char into percent-encoded form (ord returns unicode, :02x formats hexadecimal number)
                #encoded_str += f"%{ord(char):02X}" # Handles ASCII characters (not unicode characters. )
                for byte in char.encode("utf-8"):
                    encoded_str += f"%{byte:02X}"

        return encoded_str
    
    # Same as GET, but do not include / in characters allowed
    def percent_encodeBODY(self, s):
        chars_allowed = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~"
        encoded_str = ""
        for char in s:
            if char in chars_allowed:
                encoded_str += char

            else:

                # Convers a char into percent-encoded form (ord returns unicode, :02x formats hexadecimal number)
                #encoded_str += f"%{ord(char):02X}" # Handles ASCII characters (not unicode characters. )
                for byte in char.encode("utf-8"):
                    encoded_str += f"%{byte:02X}"

        return encoded_str
    
    
    # Percent encode queries passed in the GET request
    def encode_queries(self, args):

        # No args passed, encoding not needed
        if not args:
            return ""
        
        pairs = []

        # Iterate through args
        for key, value in args.items():

            # Encode key value pairs
            pairs.append(f"{self.percent_encodePATH(key)}={self.percent_encodePATH(value)}")

        # Join pairs and return
        return "&".join(pairs)
        

    def command(self, command, url, args):
        assert isinstance(url, str)
        assert isinstance(args, dict)
        if command == "POST":
            return  self.POST(url, args)
        elif command == "GET":
            return  self.GET(url, args)
        else:
            raise ValueError("not get or post")
    


if __name__ == "__main__":
    client = HTTPClient()
    if len(argv) < 3:
        help()
    else:
        method = argv[1]
        url = argv[2]
    key = None
    args = dict()
    for arg in argv[3:]:
        if key is None:
            key = arg
        else:
            args[key] = arg
            key = None
    if key is not None:
        args[key] = ""
    result = client.command(method, url, args)