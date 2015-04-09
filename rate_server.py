from twisted.internet.protocol import DatagramProtocol
from twisted.internet import reactor
from twisted.internet import task
from lxml import etree
import logging
from io import BytesIO
import time

CONSTANT = .001
print CONSTANT

class XmlRelay(DatagramProtocol):
	def __init__(self):
		self.sendXmlTimer = None
		self.clients = [("192.168.1.3", 1610)]
		self.frame_data = ''

	def datagramReceived(self, data, (in_host, in_port)):
		self.frame_data = data
		self.sendXml()

	def sendXml(self):
		if self.sendXmlTimer and self.sendXmlTimer.active():
			self.sendXmlTimer.reset(CONSTANT)
		else:
			self.sendXmlTimer = reactor.callLater(CONSTANT, self.sendXml)
		for host, port in self.clients:
			self.transport.write(self.frame_data, (host, port))

relay = XmlRelay()
reactor.listenUDP(1615, relay)
relay.sendXmlTimer = reactor.callLater(CONSTANT, relay.sendXml)
reactor.run()
