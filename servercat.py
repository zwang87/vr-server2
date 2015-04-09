import socket
import struct

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM, socket.IPPROTO_UDP)
sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
sock.bind(('192.168.1.3', 1611))

while True:
	#print "%d %02f %02f %02f %02f %02f %02f %02f" % struct.unpack("ifffffff", sock.recv(32))
	print sock.recv(1024)
