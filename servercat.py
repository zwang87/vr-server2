import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('127.0.0.1', 1615))

while True:
	#print "%d %02f %02f %02f %02f %02f %02f %02f" % struct.unpack("ifffffff", sock.recv(32))
	print sock.recv(1024)
