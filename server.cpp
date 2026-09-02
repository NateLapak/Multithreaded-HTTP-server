#include <iostream>

/*
This file sets up a basic multithreaded HTTP/1.1 server in C++ that connects to HTTPclient.py. 
*/

int main() {
    std::cout << "Hello World" << std::endl;
    return 0;
}


// import socketserver
// import pathlib

// HOST = "0.0.0.0"
// PORT = 8000
// BUFSIZE = 4096
// LINE_ENDING='\r\n'
// SERVE_PATH = pathlib.Path('www').resolve()
// HTTP_1_1 = 'HTTP/1.1'

// class LabServer(socketserver.TCPServer):
//     allow_reuse_address = True

// class LabServerTCPHandler(socketserver.StreamRequestHandler):
//     def __init__(self, *args, **kwargs):
//         self.charset = "UTF-8"
//         self.serve_path = pathlib.Path("www").resolve()
//         super().__init__(*args, **kwargs)

//     def recieve_line(self):
//         return self.rfile.readline().strip().decode(self.charset, 'ignore')
    
//     def send_line(self, line):
//         self.wfile.write((line + LINE_ENDING).encode(self.charset, 'ignore'))
    
//     def handle(self):
//         start_line = self.recieve_line()
//         print("<", start_line)

// '''
//     Class that sets the server-wide options of server
//     Code taken from lab instructions in task 2
// '''
// class LabHttpTcpServer(socketserver.TCPServer):
//    allow_reuse_address = True

// '''
//     Class that handles one client connection
//     Code taken from lab instructions in task 2
// '''

// class LabHttpTCPHandler(socketserver.StreamRequestHandler):

//     # No need for __init__ function since the class inherits from StreamRequestHandler
    
//     '''
//         Purpose: Handle the functionality of the client connecting
//     '''
//     def handle(self):

//         try:
//             # Read the HTTP request
//             req_line = self.rfile.readline().decode('utf-8').strip()

//             # Parse the request line into it's method, path and version components
//             parsed_line = req_line.split() 

//             # Req_line must be host and path at minimum
//             if len(parsed_line) < 2:
//                 self.send_error(400) 
//                 return
            
//             # Save HTTP method (GET, POST, PUT, etc)
//             method = parsed_line[0]

//             # Save req path to handler (like /index.html)
//             path = parsed_line[1]


//             '''
//                 This code reads all the request lines until it hits a blank line
//             '''

//             # Store headers from request_line here
//             headers = {}
//             while True:
//                 req_line = self.rfile.readline().decode('utf-8').strip()

//                 # Break if line is empty
//                 if req_line == "":
//                     break
                
//                 k, v = req_line.split(":", 1)
//                 headers[k.strip()] = v.strip()

//             # Serve file if HTTP method is a GET request, otherwise send 405 error
//             if method == "GET":
//                 self.serve_file(path)
//             else:
//                 self.error(405) 

//         except:
//             print("An error botting up the server occured")
//             self.error(500)


//     # HTTP req is valid, serve file using path passed as the parameter
//     def serve_file(self, path = "index.html"):

//         decode_path = self.percent_decode(path)
        
//         # Path is root
//         if decode_path == "/":
//             file_path = SERVE_PATH / "index.html"

//         else:
            
//             # Non root directory
//             file_path = SERVE_PATH / decode_path.lstrip("/")

//             # File path is a directory (like http://127.0.0.1:8000/deep/, server would see /deep/)
//             if file_path.is_dir():

//                 # Path doesn't end with '/', 301 error
//                 if not path.endswith("/"):
//                     self.error(301, path)

//                 # Path ends with '/', serve index.html inside the directory
//                 file_path = file_path / "index.html"

//         # Check for 403 error (don't have the necessary permissions to access requested source)
//         try:
//             file_path = file_path.resolve()
//             SERVE_PATH.resolve()
//             file_path.relative_to(SERVE_PATH.resolve())

//         except ValueError:
//             self.error(403)
//             return

//         # Check if file_path exists and is an actual file, then read it's the file content
//         if file_path.exists() and file_path.is_file():
//             with open(file_path, "rb") as f:
//                 body = f.read()

//             # Change all instances of \r\n to \n
//             body = body.replace(b"\r\n", b"\n")

//             content_type = "text/html"

//             # Determine content type based on file extension
//             if file_path.suffix == ".css":
//                 content_type = "text/css; charset=utf-8"
            
//             # Send HTTP response in HTTP response format
//             self.wfile.write(f"{HTTP_1_1} 200 OK\r\n".encode())
//             self.wfile.write(f"Content-Length: {len(body)}\r\n".encode())
//             self.wfile.write(f"Content-Type: {content_type}; charset=utf-8\r\n".encode())
//             self.wfile.write(f"Connection: Close\r\n".encode())
//             self.wfile.write(b"\r\n")
//             self.wfile.write(body)

//         # File does not exist, send 404 error
//         else:
//             self.error(404)

//     # HTTP request is invalid, print out error code
//     def error(self, err_code, path = "/"):

//         reason = ""

//         '''
//         HTTP/1.1 301 Moved Permanently
//         Location: /path/
//         Content-Length: 0
//         Connection: Close
//         '''
//         if err_code == 301:
//             new_location = path + "/"
//             self.wfile.write(f"{HTTP_1_1} 301 Moved Permanently\r\n".encode())
//             self.wfile.write(f"Location: {new_location}\r\n".encode())
//             self.wfile.write(b"Content-Length: 0\r\n")
//             self.wfile.write(b"Connection: Close\r\n\r\n")
//             return

//         # 400 error code
//         elif err_code == 400:
//             reason = str(err_code) + " Bad request"
        
//         # 403 error code
//         elif err_code == 403:
//             reason = str(err_code) + " Forbidden error"

//         # 404 error code
//         elif err_code == 404:
//             reason = str(err_code) + " Not found"

//         # 500 err code
//         elif err_code == 500:
//             reason = str(err_code) + " Internel Server Error"

//         # 405 err code
//         elif err_code == 405:
//             reason = str(err_code) + " Method not allowed"

//         else:
//             reason = str(err_code)

//         # Body of the error page
//         body = f"<html><body><h1>{err_code} {reason}</h1></body></html>".encode("utf-8")

//         # Write HTTP response back to HTTP
//         self.wfile.write(f"{HTTP_1_1} {err_code} {reason}\r\n".encode())
//         self.wfile.write(f"Content-Length: {len(body)}\r\n".encode())
//         self.wfile.write(f"Content-Type: text/html; charset=utf-8 \r\n".encode())
//         self.wfile.write(f"Connection: Close \r\n".encode())
//         self.wfile.write(b"\r\n")
   
//         # Encode error code, then write to status line
//         self.wfile.write(body)

//     # Decode any percent encoded string given by the request_line
//     def percent_decode(self, s):

//         decoded_str = ""
//         i = 0

//         # Loop over each char in s
//         while i < len(s):

//             # Current char is % and 2 chars follow, there is a percent encoded sequence
//             if s[i] == "%" and i + 2 < len(s):

//                 # Algorithm to decode percent-encoded sequence:
//                 # Take the next two characters (s[i+1:i+3])
//                 # Convert them from hexadecimal to an integer
//                 # Then convert that integer to its ASCII character
//                 # Append this decoded character to 'decoded'


//                 decoded_str += chr(int(s[i+1:i+3], 16)) 
//                 i += 3

//             # Otherwise, just append char to decoded_str
//             else:
//                 decoded_str += s[i]
//                 i += 1
//         return decoded_str


// def main():
    
//     # From https://docs.python.org/3/library/socketserver.html, The Python Software Foundation, downloaded 2024-01-07
//     print("Starting server")
//     with LabServer((HOST, PORT), LabHttpTCPHandler) as server:
//         server.serve_forever()


// if __name__ == "__main__":
//     main()