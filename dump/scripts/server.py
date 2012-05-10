import can
from http import Server

can.open()

CAN_MASTER_ADDR = 0x8000

def get_sn_response():
 action = can.get_sn_action(CAN_MASTER_ADDR)
 return can.perform_action(action)
 
def get_event_response():
 action = can.get_event_action(CAN_MASTER_ADDR,100)
 return can.perform_action(action) 
 
def set_flash_addr_response(addr,flash_addr):
 return can.perform_action( can.set_flash_addr_action(addr,flash_addr) )
 
def write_flash_data_response(addr,data):
 return can.perform_action( can_set_flash_addr_action(addr,data) )

def dump_packets(wfile,packets):
 [wfile.write(str(packet)+'\n') for packet in packets] 
 
def index(self,method):
 self.ok('text/html')  
 self.wfile.write(open('index.html').read()) 
 
def write_flash(self,method):
 block_len = 64
 filename = 'akp_v0_53e.chx'

 if method == 'GET': QUERY = self.query_GET()
 elif method == 'POST': QUERY = self.query_POST()

 try:
  addr = int(QUERY['addr'],16)
  flash_addr = int(QUERY['flash_addr'])
 except KeyError as err:
  self.bad_request()
  print err
  return 
 
 data = open(filename).read()
  
 self.ok('text/plain')
 self.wfile.write(str(len(data))+'\n') 
 
 dump_packets(self.wfile, can.perform_action( can.set_flash_addr_action(addr,flash_addr) ) )
 
 packet_former = can.set_flash_data_packet_former(addr,data[0:64])
 for i in range(10):
  for packet in packet_former():
   dump_packets(self.wfile, can.perform_action( lambda sock: packet.send(sock) ))
   #can.perform_action( lambda sock: packet.send(sock) )
 
 #dump_packets(self.wfile, can.perform_action( can.write_flash_data_action(addr,data[0:64]) ) )
 
 
def get_event(self,method):
 if method == 'GET': QUERY = self.query_GET()
 elif method == 'POST': QUERY = self.query_POST()
 try:
  addr = int(QUERY['addr'],16)
  event_id = int(QUERY['event_id'])
 except KeyError as err:
  self.bad_request()
  print err
  return
 
 self.ok('text/plain') 
 packets = can.perform_action(can.get_event_action(addr,event_id))
 [self.wfile.write(str(packet)+'\n') for packet in packets]
 
methods =  { '/'             : index,
             '/get_event'    : get_event,
			 '/write_flash'   : write_flash}
Server.run(methods)



