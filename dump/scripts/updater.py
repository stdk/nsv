#!/bin/python

from can import open as can_open,perform_action,set_flash_data_packet_generator,delayed_update_packet,set_flash_addr_packet,init_control_packet,receive_packet
from time import time
import asyncore

S_OK = 1

class CANUpdater(asyncore.dispatcher):
 def __init__(self, host, port):
  from socket import AF_INET,SOCK_STREAM
  asyncore.dispatcher.__init__(self)
  self.create_socket(AF_INET, SOCK_STREAM)
  self.connect( (host, port) )
  init_control_packet().send(self)
  self.result = False
  self.addr = None
 
 def run(self,addr,data):
  self.addr = addr
  self.begin = time()
  
  self.generator = self.packet_generator(addr,data)
  self.start = time()
  self.generator.next().send(self)
  asyncore.loop()
  return self.result
 
 @staticmethod
 def packet_generator(addr,data):
  block_len = 64
  block_number = ( len(data) / block_len ) + (1 if len(data) % block_len else 0)
  
  progress = 0
  
  for i in range(block_number):
   flash_addr = block_len*i
   print '%i' % (100 * i / block_number)
   yield set_flash_addr_packet(addr,flash_addr)
   block_packets_generator = set_flash_data_packet_generator(addr,data[flash_addr:flash_addr+block_len])
   for packet in block_packets_generator():
    #print 'sending' + str(packet)
    yield packet
  
  print '100'  
  yield delayed_update_packet(addr) 
 
 def handle_close(self):
  self.close()

 def handle_read(self):
  pack = receive_packet(self)
  #print str(pack)
  if pack.addr == self.addr and pack.func == S_OK:
   try:
    self.generator.next().send(self)
   except StopIteration:
    end = time()
    print 'Completed in [%i]' % (end - self.begin)
    self.result = True
    self.close()
  #else:   print 'OUT:',str(pack)

if __name__=='__main__':
 from sys import argv,exit

 can_host = '127.0.0.1'
 can_port = 7552
 
 print argv
 
 try:
  addr = int(argv[1],16)
  firmware = argv[2]
 except:
  print 'Wrong arguments'
  print 'Format: updater.py <address> <firmware>'
  exit(2)
 
 print 'Updating firmware[%s] on device[%s]' % ( hex(addr),firmware )
 
 updater = CANUpdater(can_host,can_port)
 data = open(firmware).read()
 updater.run(addr,data)