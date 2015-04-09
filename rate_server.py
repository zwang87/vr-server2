from twisted.internet.protocol import DatagramProtocol
from twisted.internet import reactor
from twisted.internet import task
from lxml import etree
import logging
from io import BytesIO
import time

class XmlRelay(DatagramProtocol):
        def __init__(self, _sc):
                self.sc = _sc
                self.sendXmlTimer = None
                self.clients = [("192.168.1.3", 1610)]
                self.frame_data = None

        def datagramReceived(self, data, (in_host, in_port)):
                #print data
                self.frame_data = data
                self.sendXml()

        def sendXml(self):
                # Every 10 milliseconds
                if self.sendXmlTimer and self.sendXmlTimer.active():
                        self.sendXmlTimer.reset(.01)
                else:
                        self.sendXmlTimer = reactor.callLater(.01, self.sendXml)
                for host, port in self.clients:
                        sc.sendMsg(self.frame_data, (host, port))

class ServerConnection(DatagramProtocol):
     def sendMsg(self, data, (host, port)):
         self.transport.write(data, (host, port))
 
sc = ServerConnection()
relay = XmlRelay(sc)
reactor.listenUDP(0, sc, interface='192.168.1.3', maxPacketSize=65507)
reactor.listenUDP(1615, relay, interface='127.0.0.1', maxPacketSize=65507)
reactor.run()
